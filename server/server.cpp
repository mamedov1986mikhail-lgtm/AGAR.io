#include "server.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace agar {

namespace {

void logf(const char* fmt, const std::string& a = "", const std::string& b = "") {
    std::printf(fmt, a.c_str(), b.c_str());
    std::printf("\n");
    std::fflush(stdout);
}

}  // namespace

Server::Server(uv_loop_t* loop, ServerConfig cfg)
    : loop_(loop),
      cfg_(std::move(cfg)),
      world_(static_cast<std::uint32_t>(
          std::chrono::steady_clock::now().time_since_epoch().count())),
      records_(cfg_.recordsFile) {
    byPlayer_.fill(nullptr);
    rngState_ = static_cast<std::uint32_t>(
                    std::chrono::steady_clock::now().time_since_epoch().count()) | 1u;
}

Server::~Server() = default;

// ------------------------------------------------------------------ запуск

bool Server::start() {
    records_.load();

    sockaddr_in tcpAddr{};
    sockaddr_in udpAddr{};
    if (uv_ip4_addr(cfg_.host.c_str(), cfg_.tcpPort, &tcpAddr) != 0 ||
        uv_ip4_addr(cfg_.host.c_str(), cfg_.udpPort, &udpAddr) != 0) {
        std::fprintf(stderr, "Неверный адрес/порт: %s\n", cfg_.host.c_str());
        return false;
    }

    uv_tcp_init(loop_, &listener_);
    listener_.data = this;
    int r = uv_tcp_bind(&listener_, reinterpret_cast<const sockaddr*>(&tcpAddr), 0);
    if (r == 0) r = uv_listen(reinterpret_cast<uv_stream_t*>(&listener_), 64, &Server::onConnection);
    if (r != 0) {
        std::fprintf(stderr, "TCP %d: %s\n", cfg_.tcpPort, uv_strerror(r));
        uv_close(reinterpret_cast<uv_handle_t*>(&listener_), nullptr);
        return false;
    }

    uv_udp_init(loop_, &udp_);
    udp_.data = this;
    r = uv_udp_bind(&udp_, reinterpret_cast<const sockaddr*>(&udpAddr), UV_UDP_REUSEADDR);
    if (r == 0) r = uv_udp_recv_start(&udp_, &Server::onAlloc, &Server::onUdpRecv);
    if (r != 0) {
        std::fprintf(stderr, "UDP %d: %s\n", cfg_.udpPort, uv_strerror(r));
        uv_close(reinterpret_cast<uv_handle_t*>(&listener_), nullptr);
        uv_close(reinterpret_cast<uv_handle_t*>(&udp_), nullptr);
        return false;
    }

    uv_timer_init(loop_, &timer_);
    timer_.data = this;
    lastTickNs_ = uv_hrtime();
    uv_timer_start(&timer_, &Server::onTick, 0, 1000 / kTickHz);
    started_ = true;

    world_.ensureBots(cfg_.bots);
    std::printf("AGAR server: TCP %s:%d, UDP %s:%d, ботов: %d\n", cfg_.host.c_str(), cfg_.tcpPort,
                cfg_.host.c_str(), cfg_.udpPort, cfg_.bots);
    std::fflush(stdout);
    return true;
}

void Server::onGenericClosed(uv_handle_t*) {}

void Server::stop() {
    if (stopping_) return;
    stopping_ = true;
    records_.save();
    if (started_) {
        uv_timer_stop(&timer_);
        uv_close(reinterpret_cast<uv_handle_t*>(&timer_), &Server::onGenericClosed);
        uv_udp_recv_stop(&udp_);
        uv_close(reinterpret_cast<uv_handle_t*>(&udp_), &Server::onGenericClosed);
        uv_close(reinterpret_cast<uv_handle_t*>(&listener_), &Server::onGenericClosed);
    }
    std::vector<Conn*> all(conns_.begin(), conns_.end());
    for (Conn* c : all) closeConn(c);
}

// ------------------------------------------------------------------ TCP

void Server::onConnection(uv_stream_t* s, int status) {
    if (status < 0) return;
    static_cast<Server*>(s->data)->acceptConn();
}

void Server::acceptConn() {
    if (stopping_) return;
    Conn* c = new Conn();
    c->server = this;
    uv_tcp_init(loop_, &c->tcp);
    c->tcp.data = c;
    if (uv_accept(reinterpret_cast<uv_stream_t*>(&listener_),
                  reinterpret_cast<uv_stream_t*>(&c->tcp)) != 0) {
        uv_close(reinterpret_cast<uv_handle_t*>(&c->tcp), &Server::onConnClosed);
        return;
    }
    uv_tcp_nodelay(&c->tcp, 1);
    conns_.insert(c);
    uv_read_start(reinterpret_cast<uv_stream_t*>(&c->tcp), &Server::onAlloc, &Server::onRead);
}

void Server::onAlloc(uv_handle_t*, size_t suggested, uv_buf_t* buf) {
    buf->base = static_cast<char*>(std::malloc(suggested));
    buf->len = buf->base ? static_cast<decltype(buf->len)>(suggested) : 0;
}

void Server::onRead(uv_stream_t* s, ssize_t nread, const uv_buf_t* buf) {
    Conn* c = static_cast<Conn*>(s->data);
    Server* self = c->server;
    if (nread < 0) {
        std::free(buf->base);
        self->closeConn(c);  // EOF или ошибка
        return;
    }
    if (nread > 0) {
        c->inbuf.append(buf->base, static_cast<size_t>(nread));
        std::free(buf->base);
        if (c->inbuf.size() > 4096) {  // защита от мусора без переводов строки
            self->closeConn(c);
            return;
        }
        std::size_t pos;
        // handleLine может закрыть соединение, поэтому проверяем его наличие на каждой итерации.
        while (self->conns_.count(c) && (pos = c->inbuf.find('\n')) != std::string::npos) {
            std::string line = c->inbuf.substr(0, pos);
            c->inbuf.erase(0, pos + 1);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (!line.empty()) self->handleLine(c, line);
        }
        return;
    }
    std::free(buf->base);
}

void Server::sendLine(Conn* c, const std::string& line) {
    if (!conns_.count(c)) return;
    WriteReq* w = new WriteReq();
    w->data = line + "\n";
    uv_buf_t b = uv_buf_init(&w->data[0], static_cast<unsigned int>(w->data.size()));
    if (uv_write(&w->req, reinterpret_cast<uv_stream_t*>(&c->tcp), &b, 1, &Server::onWritten) != 0)
        delete w;
}

void Server::onWritten(uv_write_t* req, int) {
    delete reinterpret_cast<WriteReq*>(req);  // req - первый член WriteReq
}

void Server::onConnClosed(uv_handle_t* h) { delete static_cast<Conn*>(h->data); }

// Закрытие соединения = выход игрока (если он играл) или уход из очереди.
void Server::closeConn(Conn* c) {
    if (!conns_.count(c)) return;
    if (c->state == State::Playing) {
        leaveGame(c, nullptr);  // без ответа: канал уже закрывается
    } else if (c->state == State::Queued) {
        queue_.erase(std::remove(queue_.begin(), queue_.end(), c), queue_.end());
        c->state = State::Connected;
    }
    conns_.erase(c);
    uv_read_stop(reinterpret_cast<uv_stream_t*>(&c->tcp));
    uv_close(reinterpret_cast<uv_handle_t*>(&c->tcp), &Server::onConnClosed);
    promoteQueue();
}

void Server::handleLine(Conn* c, const std::string& line) {
    std::vector<std::string> w = splitWords(line);
    if (w.empty()) return;
    const std::string& cmd = w[0];

    if (cmd == "REG") {
        handleReg(c, w);
    } else if (cmd == "LEAVE") {
        if (c->state == State::Playing) {
            leaveGame(c, "BYE");
        } else if (c->state == State::Queued) {
            queue_.erase(std::remove(queue_.begin(), queue_.end(), c), queue_.end());
            c->state = State::Connected;
            sendLine(c, "BYE 0 0 0 0");
        } else {
            sendLine(c, "BYE 0 0 0 0");
        }
    } else if (cmd == "RECORDS") {
        sendRecords(c);
    } else {
        sendLine(c, "ERR UNKNOWN Неизвестная команда");
    }
}

bool Server::nameInUse(const std::string& name) const {
    if (world_.nameTaken(name)) return true;
    auto same = [&](const std::string& a) {
        if (a.size() != name.size()) return false;
        for (std::size_t i = 0; i < a.size(); ++i)
            if (std::tolower(static_cast<unsigned char>(a[i])) !=
                std::tolower(static_cast<unsigned char>(name[i])))
                return false;
        return true;
    };
    for (const Conn* q : queue_)
        if (same(q->pendingName)) return true;
    return false;
}

void Server::handleReg(Conn* c, const std::vector<std::string>& w) {
    if (c->state != State::Connected) {
        sendLine(c, "ERR ALREADY Игрок уже зарегистрирован");
        return;
    }
    if (w.size() != 3 || !isValidName(w[1])) {
        sendLine(c, "ERR BADNAME Имя: латинские буквы и цифры, до 15 символов");
        return;
    }
    int skin = std::atoi(w[2].c_str());
    if (skin < 0 || skin >= kSkinCount) {
        sendLine(c, "ERR BADSKIN Неверный номер цвета/текстуры");
        return;
    }
    if (nameInUse(w[1])) {
        sendLine(c, "ERR TAKEN Игрок с таким именем уже есть");
        return;
    }
    c->pendingName = w[1];
    c->pendingSkin = static_cast<std::uint8_t>(skin);

    if (world_.humanCount() >= kMaxPlayers) {  // мест нет: ставим в очередь
        c->state = State::Queued;
        queue_.push_back(c);
        sendLine(c, std::string("FULL ") + kServerFullMessage);
        logf("В очередь: %s", c->pendingName);
        return;
    }
    admit(c);
}

void Server::admit(Conn* c) {
    if (world_.playerCount() >= kMaxPlayers) world_.removeOneBot();  // боты уступают место людям
    int id = world_.addPlayer(c->pendingName, c->pendingSkin, false);
    if (id < 0) {  // не должно случаться: имя проверено, место освобождено
        c->state = State::Connected;
        sendLine(c, "ERR TAKEN Не удалось добавить игрока");
        return;
    }
    rngState_ = rngState_ * 1664525u + 1013904223u;
    c->token = rngState_ ^ static_cast<std::uint32_t>(uv_hrtime());
    c->playerId = id;
    c->hasUdp = false;
    c->state = State::Playing;
    byPlayer_[static_cast<std::size_t>(id)] = c;
    sendLine(c, "OK " + std::to_string(id) + " " + std::to_string(c->token) + " " +
                    std::to_string(cfg_.udpPort) + " " +
                    std::to_string(static_cast<int>(kWorldW)) + " " +
                    std::to_string(static_cast<int>(kWorldH)));
    logf("Вход: %s", c->pendingName);
}

void Server::promoteQueue() {
    while (!stopping_ && !queue_.empty() && world_.humanCount() < kMaxPlayers) {
        Conn* c = queue_.front();
        queue_.pop_front();
        admit(c);
    }
}

Server::Conn* Server::connByPlayer(int id) const {
    if (id < 0 || id >= kMaxPlayers) return nullptr;
    return byPlayer_[static_cast<std::size_t>(id)];
}

// Завершение игры игрока: сохраняем рекорд, подтверждаем выход, освобождаем место.
void Server::finishGame(Conn* c, const char* verb, std::uint32_t weight, int seconds) {
    int place = records_.add(c->pendingName, weight, seconds);
    if (place > 0) records_.save();
    if (c->playerId >= 0) byPlayer_[static_cast<std::size_t>(c->playerId)] = nullptr;
    c->playerId = -1;
    c->state = State::Connected;
    c->hasUdp = false;
    logf("Выход: %s %s", c->pendingName, place > 0 ? "(рекорд)" : "");
    if (verb) {
        sendLine(c, std::string(verb) + " " + std::to_string(weight) + " " +
                        std::to_string(seconds) + " " + (place > 0 ? "1" : "0") + " " +
                        std::to_string(place));
    }
}

void Server::leaveGame(Conn* c, const char* verb) {
    const Player* p = world_.player(static_cast<std::uint8_t>(c->playerId));
    std::uint32_t weight = p ? p->weight : 1;
    int seconds = p ? static_cast<int>(p->aliveTime) : 0;
    world_.removePlayer(static_cast<std::uint8_t>(c->playerId));
    finishGame(c, verb, weight, seconds);
    world_.ensureBots(cfg_.bots);
    promoteQueue();
}

void Server::sendRecords(Conn* c) {
    const auto& e = records_.entries();
    std::string out = "RECORDS " + std::to_string(e.size());
    for (const RecordEntry& r : e)
        out += "\nR " + r.name + " " + std::to_string(r.weight) + " " + std::to_string(r.seconds);
    sendLine(c, out);
}

// ------------------------------------------------------------------ UDP

void Server::onUdpRecv(uv_udp_t* h, ssize_t nread, const uv_buf_t* buf, const sockaddr* addr,
                       unsigned) {
    Server* self = static_cast<Server*>(h->data);
    if (nread > 0 && addr) {
        self->handleInput(reinterpret_cast<const std::uint8_t*>(buf->base),
                          static_cast<std::size_t>(nread), addr);
    }
    std::free(buf->base);
}

void Server::handleInput(const std::uint8_t* data, std::size_t n, const sockaddr* addr) {
    InputPacket in;
    if (!decodeInput(data, n, in)) return;
    Conn* c = connByPlayer(in.id);
    if (!c || c->state != State::Playing || c->token != in.token) return;
    std::size_t len = addr->sa_family == AF_INET6 ? sizeof(sockaddr_in6) : sizeof(sockaddr_in);
    std::memcpy(&c->udpAddr, addr, len);
    c->hasUdp = true;
    world_.setTarget(in.id, in.tx, in.ty);
}

void Server::onUdpSent(uv_udp_send_t* req, int) { delete reinterpret_cast<UdpSendReq*>(req); }

void Server::sendState(Conn* c, std::uint32_t tickNo, const std::vector<std::uint8_t>& block,
                       std::uint8_t playersCount) {
    const Player* me = world_.player(static_cast<std::uint8_t>(c->playerId));
    if (!me || !c->hasUdp) return;

    std::vector<Food> near;
    world_.collectFoodsNear(me->x, me->y, kViewHalfW, kViewHalfH, kMaxFoodPerPacket, near);

    ByteWriter w;
    w.u8(kPktState);
    w.u32(tickNo);
    w.u8(static_cast<std::uint8_t>(c->playerId));
    w.u8(playersCount);
    w.u16(static_cast<std::uint16_t>(near.size()));
    w.bytes(block);
    for (const Food& f : near) {
        NetFood nf;
        nf.x = static_cast<std::uint16_t>(f.x);
        nf.y = static_cast<std::uint16_t>(f.y);
        nf.type = f.type;
        encodeFood(w, nf);
    }

    UdpSendReq* r = new UdpSendReq();
    r->data = std::move(w.data());
    uv_buf_t b = uv_buf_init(reinterpret_cast<char*>(r->data.data()),
                             static_cast<unsigned int>(r->data.size()));
    if (uv_udp_send(&r->req, &udp_, &b, 1, reinterpret_cast<const sockaddr*>(&c->udpAddr),
                    &Server::onUdpSent) != 0)
        delete r;
}

// ------------------------------------------------------------------ такт

void Server::onTick(uv_timer_t* t) { static_cast<Server*>(t->data)->tick(); }

void Server::tick() {
    if (stopping_) return;
    std::uint64_t now = uv_hrtime();
    float dt = static_cast<float>(static_cast<double>(now - lastTickNs_) / 1e9);
    lastTickNs_ = now;
    dt = std::min(dt, 0.1f);

    std::vector<DeathInfo> deaths;
    world_.step(dt, deaths);

    for (const DeathInfo& d : deaths) {
        if (d.bot) continue;
        Conn* c = connByPlayer(d.id);
        if (!c) continue;
        finishGame(c, "DEAD", d.weight, static_cast<int>(d.aliveTime));
    }
    world_.ensureBots(cfg_.bots);
    if (!deaths.empty()) promoteQueue();

    // Общий блок игроков собираем один раз, дальше он дописывается в пакет каждого клиента.
    ByteWriter block;
    std::uint8_t count = 0;
    for (const Player& p : world_.players()) {
        if (!p.active) continue;
        NetPlayer np;
        np.id = p.id;
        np.skin = p.skin;
        np.x = p.x;
        np.y = p.y;
        np.weight = p.weight;
        np.name = p.name;
        encodePlayer(block, np);
        ++count;
    }
    ++tickNo_;
    for (Conn* c : conns_)
        if (c->state == State::Playing) sendState(c, tickNo_, block.data(), count);
}

}  // namespace agar
