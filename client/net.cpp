#include "net.h"

namespace agar 
{

// ------------------------------------------------------------------ TCP

bool TcpLink::connect(const std::string& host, unsigned short port, float timeoutSec) 
{
    close();
    sf::IpAddress ip(host);  
    if (ip == sf::IpAddress::None) return false;
    sock_.setBlocking(true);
    if (sock_.connect(ip, port, sf::seconds(timeoutSec)) != sf::Socket::Done) return false;
    sock_.setBlocking(false);
    connected_ = true;
    buf_.clear();
    return true;
}

void TcpLink::sendLine(const std::string& line) 
{
    if (!connected_) return;
    std::string data = line + "\n";
    std::size_t total = 0;
    int spins = 0;
    while (total < data.size() && spins < 1000) 
    {
        std::size_t sent = 0;
        sf::Socket::Status st = sock_.send(data.data() + total, data.size() - total, sent);
        total += sent;
        if (st == sf::Socket::Done) break;
        if (st == sf::Socket::Disconnected || st == sf::Socket::Error) 
        {
            connected_ = false;
            return;
        }
        spins++; 
        sf::sleep(sf::milliseconds(1));
    }
}

bool TcpLink::poll(std::vector<std::string>& lines) 
{
    if (!connected_) return false;
    char chunk[2048];
    while (true) 
    {
        std::size_t got = 0;
        sf::Socket::Status st = sock_.receive(chunk, sizeof chunk, got);
        if (st == sf::Socket::Done) 
        {
            buf_.append(chunk, got);
        } 
        else if (st == sf::Socket::NotReady) 
        {
            break;
        }
        else 
        {  
            connected_ = false;
            break;
        }
    }
    std::size_t pos;
    while ((pos = buf_.find('\n')) != std::string::npos) 
    {
        std::string line = buf_.substr(0, pos);
        buf_.erase(0, pos + 1);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) lines.push_back(line);
    }
    return connected_;
}

void TcpLink::close() 
{
    if (connected_) sock_.disconnect();
    connected_ = false;
    buf_.clear();
}

// ------------------------------------------------------------------ UDP

bool UdpLink::open(const std::string& host, unsigned short port) 
{
    close();
    server_ = sf::IpAddress(host);
    if (server_ == sf::IpAddress::None) return false;
    port_ = port;
    if (sock_.bind(sf::Socket::AnyPort) != sf::Socket::Done) return false;
    sock_.setBlocking(false);
    lastTick_ = 0;
    open_ = true;
    return true;
}

void UdpLink::sendInput(std::uint8_t id, std::uint32_t token, float tx, float ty) 
{
    if (!open_) return;
    InputPacket in;
    in.id = id;
    in.token = token;
    in.tx = tx;
    in.ty = ty;
    std::vector<std::uint8_t> bytes = encodeInput(in);
    sock_.send(bytes.data(), bytes.size(), server_, port_);
}

bool UdpLink::receive(StatePacket& out) 
{
    if (!open_) return false;
    bool any = false;
    std::uint8_t buf[2048];
    while (true) 
    {
        std::size_t got = 0;
        sf::IpAddress from;
        unsigned short fromPort = 0;
        if (sock_.receive(buf, sizeof buf, got, from, fromPort) != sf::Socket::Done) break;
        if (from != server_ || fromPort != port_) continue;  
        StatePacket st;
        if (!decodeState(buf, got, st)) continue;
        if (any && st.tick <= out.tick) continue;         
        if (!any && st.tick <= lastTick_) continue;
        out = std::move(st);
        any = true;
    }
    if (any) lastTick_ = out.tick;
    return any;
}

void UdpLink::close() 
{
    if (open_) sock_.unbind();
    open_ = false;
}
}  
