#pragma once

#include <SFML/Network.hpp>

#include <string>
#include <vector>

#include "protocol.h"

namespace agar 
{
class TcpLink 
{
public:
    bool connect(const std::string& host, unsigned short port, float timeoutSec);
    void sendLine(const std::string& line);
    bool poll(std::vector<std::string>& lines);
    void close();
    bool connected() const { return connected_; }

private:
    sf::TcpSocket sock_;
    std::string buf_;
    bool connected_ = false;
};

class UdpLink 
{
public:
    bool open(const std::string& host, unsigned short port);
    void sendInput(std::uint8_t id, std::uint32_t token, float tx, float ty);
    bool receive(StatePacket& out);
    void close();

private:
    sf::UdpSocket sock_;
    sf::IpAddress server_ = sf::IpAddress::None;
    unsigned short port_ = 0;
    std::uint32_t lastTick_ = 0;
    bool open_ = false;
};
}  
