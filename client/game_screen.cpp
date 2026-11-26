#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "screens.h"

namespace agar 
{
namespace 
{

constexpr float kInputPeriod = 1.f / 30.f;

void drawWorldText(sf::RenderTarget& target, const sf::Font& font, const std::string& utf8, sf::Vector2f center,
                   unsigned size, sf::Color color) 
{
    sf::Text t;
    t.setFont(font);
    t.setString(U(utf8));
    t.setCharacterSize(size);
    t.setFillColor(color);
    t.setOutlineColor(sf::Color(0, 0, 0, 200));
    t.setOutlineThickness(std::max(1.f, static_cast<float>(size) / 12.f));
    sf::FloatRect b = t.getLocalBounds();
    t.setOrigin(b.left + b.width / 2.f, b.top + b.height / 2.f);
    t.setPosition(center);
    target.draw(t);
}

sf::Color foodColor(int type) 
{
    switch (type) 
    {
        case 0: return sf::Color(190, 235, 90);
        case 1: return sf::Color(255, 170, 60);
        default: return sf::Color(255, 90, 190);
    }
}
} 

GameScreen::~GameScreen() 
{
    if (tcp_.connected() && (phase_ == Phase::Playing || phase_ == Phase::Queued)) tcp_.sendLine("LEAVE");
    udp_.close();
    tcp_.close();
}

void GameScreen::connect(App& app) 
{
    if (!tcp_.connect(app.profile.host, app.profile.tcpPort, 3.f)) 
    {
        message_ = "Не удалось подключиться к серверу " + app.profile.host + ":" +
                   std::to_string(app.profile.tcpPort);
        errorCode_ = "NET";
        phase_ = Phase::Error;
        return;
    }
    tcp_.sendLine("REG " + app.profile.name + " " + std::to_string(app.profile.skin));
    message_ = "Вход на сервер...";
}

void GameScreen::handleLine(App& app, const std::string& line) 
{
    std::vector<std::string> w = splitWords(line);
    if (w.empty()) return;
    const std::string& cmd = w[0];

    if (cmd == "OK" && w.size() >= 6) 
    {
        myId_ = std::atoi(w[1].c_str());
        token_ = static_cast<std::uint32_t>(std::strtoul(w[2].c_str(), nullptr, 10));
        int udpPort = std::atoi(w[3].c_str());
        worldW_ = static_cast<float>(std::atoi(w[4].c_str()));
        worldH_ = static_cast<float>(std::atoi(w[5].c_str()));
        if (udpPort <= 0 || udpPort > 65535 || !udp_.open(app.profile.host, static_cast<unsigned short>(udpPort))) 
        {
            message_ = "Не удалось открыть UDP-канал к серверу.";
            errorCode_ = "NET";
            phase_ = Phase::Error;
            return;
        }
        phase_ = Phase::Playing;
        playTime_ = 0;
        sinceState_ = 0;
        inputTimer_ = kInputPeriod;
        message_.clear();
    } 
    else if (cmd == "FULL") 
    {
        std::size_t sp = line.find(' ');
        message_ = sp == std::string::npos ? std::string(kServerFullMessage) : line.substr(sp + 1);
        phase_ = Phase::Queued;
    } 
    else if (cmd == "ERR" && w.size() >= 2) 
    {
        errorCode_ = w[1];
        std::size_t p = line.find(w[1]);
        std::size_t sp = p == std::string::npos ? std::string::npos : line.find(' ', p);
        message_ = sp == std::string::npos ? "Ошибка сервера" : line.substr(sp + 1);
        phase_ = Phase::Error;
        udp_.close();
    } 
    else if ((cmd == "DEAD" || cmd == "BYE") && w.size() >= 5) 
    {
        finalWeight_ = static_cast<unsigned>(std::atoi(w[1].c_str()));
        finalSeconds_ = std::atoi(w[2].c_str());
        newRecord_ = w[3] == "1";
        recordPlace_ = std::atoi(w[4].c_str());
        udp_.close();
        if (cmd == "DEAD" && phase_ != Phase::Leaving) 
        {
            phase_ = Phase::Over;  
        } 
        else 
        {
            app.goTo(std::make_unique<MenuScreen>());  
            phase_ = Phase::Leaving;
        }
    }
}

void GameScreen::requestLeave() 
{
    if (phase_ == Phase::Playing || phase_ == Phase::Queued) 
    {
        tcp_.sendLine("LEAVE");
        phase_ = Phase::Leaving;
        leaveTimer_ = 0;
    }
}

void GameScreen::handleEvent(App& app, const sf::Event& e) 
{
    if (e.type == sf::Event::MouseMoved) 
    {
        mouse_ = sf::Vector2f(static_cast<float>(e.mouseMove.x), static_cast<float>(e.mouseMove.y));
        return;
    }
    if (e.type != sf::Event::KeyPressed) return;

    if (e.key.code == sf::Keyboard::Escape) 
    {
        if (phase_ == Phase::Playing || phase_ == Phase::Queued) requestLeave();
        else if (phase_ == Phase::Connecting || phase_ == Phase::Over || phase_ == Phase::Error)
            app.goTo(std::make_unique<MenuScreen>());
    } 
    else if (e.key.code == sf::Keyboard::Return) 
    {
        if (phase_ == Phase::Over) 
        {
            app.goTo(std::make_unique<GameScreen>());  
        } 
        else if (phase_ == Phase::Error) 
        {
            if (errorCode_ == "TAKEN" || errorCode_ == "BADNAME" || errorCode_ == "BADSKIN")
                app.goTo(std::make_unique<CreatePlayerScreen>(app));
            else
                app.goTo(std::make_unique<MenuScreen>());
        }
    }
}

sf::Vector2f GameScreen::cameraCenter() const 
{
    auto it = shown_.find(static_cast<std::uint8_t>(myId_));
    if (it != shown_.end()) return it->second;
    return sf::Vector2f(worldW_ / 2.f, worldH_ / 2.f);
}

float GameScreen::zoom() const 
{
    std::uint32_t weight = 1;
    for (const NetPlayer& p : state_.players)
        if (p.id == myId_) weight = p.weight;
    return 1.f + std::min(0.18f, (diameterForWeight(weight) - kStartDiameter) / 800.f);
}

sf::Vector2f GameScreen::mouseInWorld() const 
{
    sf::Vector2f c = cameraCenter();
    float z = zoom();
    return sf::Vector2f(c.x + (mouse_.x - kWinW / 2.f) * z, c.y + (mouse_.y - kWinH / 2.f) * z);
}

void GameScreen::smoothPositions(float dt) 
{
    float k = std::min(1.f, dt * 18.f);
    std::map<std::uint8_t, sf::Vector2f> next;
    for (const NetPlayer& p : state_.players) {
        sf::Vector2f goal(p.x, p.y);
        auto it = shown_.find(p.id);
        sf::Vector2f cur = it == shown_.end() ? goal : it->second;
        float dx = goal.x - cur.x, dy = goal.y - cur.y;
        if (dx * dx + dy * dy > 300.f * 300.f) cur = goal;  
        else cur += sf::Vector2f(dx * k, dy * k);
        next[p.id] = cur;
    }
    shown_.swap(next);
}

void GameScreen::update(App& app, float dt) 
{
    if (phase_ == Phase::Connecting) 
    {
        if (++ticks_ == 2) connect(app);
        if (phase_ == Phase::Connecting && ticks_ < 2) return;
        if (phase_ == Phase::Connecting) 
        {
            leaveTimer_ += dt; 
            if (leaveTimer_ > 5.f) 
            {
                message_ = "Сервер не ответил.";
                errorCode_ = "NET";
                phase_ = Phase::Error;
                tcp_.close();
                return;
            }
        }
    }

    if (tcp_.connected()) 
    {
        std::vector<std::string> lines;
        bool alive = tcp_.poll(lines);
        for (const std::string& l : lines) handleLine(app, l);
        if (!alive && phase_ != Phase::Over && phase_ != Phase::Error && phase_ != Phase::Leaving) 
        {
            message_ = "Соединение с сервером потеряно.";
            errorCode_ = "NET";
            phase_ = Phase::Error;
            udp_.close();
        } 
        else if (!alive && phase_ == Phase::Leaving) 
        {
            app.goTo(std::make_unique<MenuScreen>());
        }
    }

    if (phase_ == Phase::Leaving) 
    {
        leaveTimer_ += dt;
        if (leaveTimer_ > 1.5f) app.goTo(std::make_unique<MenuScreen>()); 
        return;
    }

    if (phase_ != Phase::Playing) return;

    playTime_ += dt;
    sinceState_ += dt;

    StatePacket st;
    if (udp_.receive(st)) 
    {
        state_ = std::move(st);
        haveState_ = true;
        sinceState_ = 0;
    }
    smoothPositions(dt);

    inputTimer_ += dt;
    if (inputTimer_ >= kInputPeriod) 
    {
        inputTimer_ = 0;
        sf::Vector2f t = mouseInWorld();
        udp_.sendInput(static_cast<std::uint8_t>(myId_), token_, t.x, t.y);
    }
}

void GameScreen::drawWorld(App& app, sf::RenderTarget& target) 
{
    float z = zoom();
    sf::View view(cameraCenter(), sf::Vector2f(kWinW * z, kWinH * z));
    target.setView(view);

    sf::RectangleShape field(sf::Vector2f(worldW_, worldH_));
    field.setTexture(&app.road);
    field.setTextureRect(sf::IntRect(0, 0, static_cast<int>(worldW_), static_cast<int>(worldH_)));
    target.draw(field);

    sf::RectangleShape border(sf::Vector2f(worldW_, worldH_));
    border.setFillColor(sf::Color::Transparent);
    border.setOutlineThickness(10.f);
    border.setOutlineColor(sf::Color(200, 60, 60));
    target.draw(border);

    sf::Vector2f c = view.getCenter(), half(view.getSize().x / 2.f + 20.f, view.getSize().y / 2.f + 20.f);

    sf::CircleShape dot(1.f, 20);
    for (const NetFood& f : state_.foods) 
    {
        float x = static_cast<float>(f.x), y = static_cast<float>(f.y);
        if (std::abs(x - c.x) > half.x || std::abs(y - c.y) > half.y) continue;
        float r = foodSize(f.type) / 2.f;
        dot.setRadius(r);
        dot.setOrigin(r, r);
        dot.setPosition(x, y);
        dot.setFillColor(foodColor(f.type));
        target.draw(dot);
    }

    std::vector<const NetPlayer*> order;
    for (const NetPlayer& p : state_.players) order.push_back(&p);
    std::sort(order.begin(), order.end(),
              [](const NetPlayer* a, const NetPlayer* b) { return a->weight < b->weight; });

    for (const NetPlayer* p : order) 
    {
        auto it = shown_.find(p->id);
        sf::Vector2f pos = it == shown_.end() ? sf::Vector2f(p->x, p->y) : it->second;
        float diameter = diameterForWeight(p->weight), r = diameter / 2.f;
        bool mine = p->id == myId_;

        sf::CircleShape blob(r, 72);
        blob.setOrigin(r, r);
        blob.setPosition(pos);
        app.skins.apply(blob, p->skin);
        blob.setOutlineThickness(mine ? 4.f : 3.f);
        blob.setOutlineColor(mine ? sf::Color(255, 255, 255) : darker(app.skins.colorOf(p->skin), 0.6f));
        target.draw(blob);

        unsigned wsize = static_cast<unsigned>(std::clamp(diameter * 0.42f, 12.f, 48.f));
        drawWorldText(target, app.font, std::to_string(p->weight), pos, wsize, sf::Color::White);
        drawWorldText(target, app.font, p->name, sf::Vector2f(pos.x, pos.y - r - 14.f), 16,
                      mine ? sf::Color(255, 240, 150) : sf::Color(235, 235, 240));
    }
}

void GameScreen::drawHud(App& app, sf::RenderTarget& target) 
{
    target.setView(target.getDefaultView());

    std::uint32_t weight = 1;
    for (const NetPlayer& p : state_.players)
        if (p.id == myId_) weight = p.weight;

    sf::RectangleShape plate(sf::Vector2f(230.f, 56.f));
    plate.setOrigin(115.f, 0.f);
    plate.setPosition(kWinW / 2.f, 10.f);
    plate.setFillColor(sf::Color(0, 0, 0, 140));
    target.draw(plate);
    drawText(target, app.font, "Вес: " + std::to_string(weight), kWinW / 2.f, 16.f, 34, sf::Color::White,
             Align::Center, true);

    drawText(target, app.font, "Время: " + formatTime(static_cast<int>(playTime_)), 18.f, 14.f, 24,
             sf::Color(255, 255, 255));
    drawText(target, app.font, "Esc — выход", 18.f, kWinH - 36.f, 18, sf::Color(220, 220, 230));

    std::vector<const NetPlayer*> top;
    for (const NetPlayer& p : state_.players) top.push_back(&p);
    std::sort(top.begin(), top.end(), [](const NetPlayer* a, const NetPlayer* b) { return a->weight > b->weight; });
    if (top.size() > 5) top.resize(5);

    sf::RectangleShape panel(sf::Vector2f(250.f, 38.f + 28.f * static_cast<float>(top.size())));
    panel.setPosition(kWinW - 268.f, 14.f);
    panel.setFillColor(sf::Color(0, 0, 0, 120));
    target.draw(panel);
    drawText(target, app.font, "Лидеры", kWinW - 143.f, 18.f, 22, sf::Color::White, Align::Center, true);
    for (std::size_t i = 0; i < top.size(); i++) 
    {
        float y = 50.f + 28.f * static_cast<float>(i);
        sf::Color col = top[i]->id == myId_ ? sf::Color(255, 240, 150) : sf::Color(230, 230, 238);
        drawText(target, app.font, std::to_string(i + 1) + ". " + top[i]->name, kWinW - 256.f, y, 19, col);
        drawText(target, app.font, std::to_string(top[i]->weight), kWinW - 30.f, y, 19, col, Align::Right);
    }

    if (sinceState_ > 2.5f)
        drawText(target, app.font, "Нет данных от сервера (UDP)...", kWinW / 2.f, kWinH - 70.f, 22,
                 sf::Color(255, 150, 120), Align::Center);
}

void GameScreen::drawOverlay(App& app, sf::RenderTarget& target, const std::string& title,
    const std::vector<std::string>& lines, sf::Color titleColor)
{
    target.setView(target.getDefaultView());
    sf::RectangleShape dim(sf::Vector2f(kWinW, kWinH));
    dim.setFillColor(sf::Color(0, 0, 0, 150));
    target.draw(dim);

    std::vector<std::string> titleLines;
    std::size_t start = 0;
    std::size_t end = 0;
    while ((end = title.find('\n', start)) != std::string::npos)
    {
        titleLines.push_back(title.substr(start, end - start));
        start = end + 1;
    }
    titleLines.push_back(title.substr(start));

    const bool multiline = titleLines.size() > 1;
    const unsigned titleSize = multiline ? 28 : 40;
    const float titleStep = 40.f;
    const float extra = titleStep * static_cast<float>(titleLines.size() - 1);

    float h = 110.f + extra + 40.f * static_cast<float>(lines.size());
    sf::RectangleShape box(sf::Vector2f(700.f, h));
    box.setOrigin(350.f, h / 2.f);
    box.setPosition(kWinW / 2.f, kWinH / 2.f);
    box.setFillColor(sf::Color(30, 34, 44));
    box.setOutlineThickness(2.f);
    box.setOutlineColor(sf::Color(90, 96, 112));
    target.draw(box);

    float y = kWinH / 2.f - h / 2.f + 22.f;
    for (const std::string& t : titleLines)
    {
        drawText(target, app.font, t, kWinW / 2.f, y, titleSize, titleColor, Align::Center, true);
        y += titleStep;
    }
    y += 32.f;

    for (const std::string& l : lines)
    {
        drawText(target, app.font, l, kWinW / 2.f, y, 24, sf::Color(225, 228, 238), Align::Center);
        y += 40.f;
    }
}

void GameScreen::draw(App& app, sf::RenderTarget& target) 
{
    switch (phase_) 
    {
        case Phase::Connecting:
            drawOverlay(app, target, "Подключение...", {app.profile.host + ":" + std::to_string(app.profile.tcpPort)},
                        sf::Color::White);
            break;
        case Phase::Queued:
        {
            std::string text = message_;

            std::size_t pos = text.find(": ");     
            if (pos == std::string::npos)
                pos = text.find(". ");              

            if (pos != std::string::npos)
                text[pos + 1] = '\n';            

            drawOverlay(app, target, text,
                { "Вход произойдёт автоматически.", "Esc — выйти из очереди" },
                sf::Color(255, 200, 90));
            break;
        }
            break;
        case Phase::Playing:
            if (haveState_) 
            {
                drawWorld(app, target);
                drawHud(app, target);
            }
            else 
            {
                drawOverlay(app, target, "Загрузка игры...", {}, sf::Color::White);
            }
            break;
        case Phase::Leaving:
            drawOverlay(app, target, "Выход...", {}, sf::Color::White);
            break;
        case Phase::Over: 
        {
            std::vector<std::string> lines = 
            {
                "Вес: " + std::to_string(finalWeight_) + "     Время: " + formatTime(finalSeconds_),
                newRecord_ ? "Новый рекорд! Место в таблице: " + std::to_string(recordPlace_)
                           : "Результат не попал в таблицу рекордов",
                "Enter — играть снова     Esc — в меню"};
            drawOverlay(app, target, "Вы проиграли", lines,
                        newRecord_ ? sf::Color(255, 215, 90) : sf::Color(255, 120, 110));
            break;
        }
        case Phase::Error: 
        {
            bool fixable = errorCode_ == "TAKEN" || errorCode_ == "BADNAME" || errorCode_ == "BADSKIN";
            drawOverlay(app, target, "Не удалось войти в игру",
                        {message_, fixable ? "Enter — изменить игрока     Esc — в меню" : "Enter или Esc — в меню"},
                        sf::Color(255, 120, 110));
            break;
        }
    }
    target.setView(target.getDefaultView());
}

}  
