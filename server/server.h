#pragma once
#include <uv.h>
#include <array>
#include <cstdint>
#include <deque>
#include <set>
#include <string>
#include <vector>
#include "game_world.h"
#include "protocol.h"
#include "records.h"

namespace agar 
{

struct ServerConfig 
{
    std::string host = "0.0.0.0";
    int tcpPort = kDefaultTcpPort;
    int udpPort = kDefaultUdpPort;
    std::string recordsFile = "records.txt";
};

class Server 
{
public:
    Server(uv_loop_t* loop, ServerConfig cfg);
    ~Server();

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    bool start();
    void stop();

private:
    enum class State { Connected, Queued, Playing };

    struct Conn 
    {
        uv_tcp_t tcp;
        Server* server = nullptr;
        std::string inbuf;
        State state = State::Connected;
        int playerId = -1;
        std::uint32_t token = 0;
        std::string pendingName;
        std::uint8_t pendingSkin = 0;
        sockaddr_storage udpAddr{};
        bool hasUdp = false;
    };

    struct WriteReq 
    {
        uv_write_t req;
        std::string data;
    };

    struct UdpSendReq 
    {
        uv_udp_send_t req;
        std::vector<std::uint8_t> data;
    };

    // libuv-колбэки
    static void onConnection(uv_stream_t* server, int status);
    static void onAlloc(uv_handle_t* h, size_t suggested, uv_buf_t* buf);
    static void onRead(uv_stream_t* s, ssize_t nread, const uv_buf_t* buf);
    static void onWritten(uv_write_t* req, int status);
    static void onConnClosed(uv_handle_t* h);
    static void onUdpRecv(uv_udp_t* h, ssize_t nread, const uv_buf_t* buf,
                          const sockaddr* addr, unsigned flags);
    static void onUdpSent(uv_udp_send_t* req, int status);
    static void onTick(uv_timer_t* t);
    static void onGenericClosed(uv_handle_t* h);

    // TCP-часть
    void acceptConn();
    void closeConn(Conn* c);
    void sendLine(Conn* c, const std::string& line);
    void handleLine(Conn* c, const std::string& line);
    void handleReg(Conn* c, const std::vector<std::string>& w);
    void sendRecords(Conn* c);

    // Игровая логика
    bool nameInUse(const std::string& name) const;
    void admit(Conn* c);
    void promoteQueue();
    void finishGame(Conn* c, const char* verb, std::uint32_t weight, int seconds);
    void leaveGame(Conn* c, const char* verb);
    Conn* connByPlayer(int playerId) const;

    // UDP-часть и такт
    void tick();
    void handleInput(const std::uint8_t* data, std::size_t n, const sockaddr* addr);
    void sendState(Conn* c, std::uint32_t tickNo, const std::vector<std::uint8_t>& playersBlock,
                   std::uint8_t playersCount);

    uv_loop_t* loop_;
    ServerConfig cfg_;
    uv_tcp_t listener_{};
    uv_udp_t udp_{};
    uv_timer_t timer_{};
    bool started_ = false;
    bool stopping_ = false;

    World world_;
    Records records_;
    std::set<Conn*> conns_;
    std::deque<Conn*> queue_;
    std::array<Conn*, kMaxPlayers> byPlayer_{};
    std::uint32_t tickNo_ = 0;
    std::uint64_t lastTickNs_ = 0;
    std::uint32_t rngState_;
};

}  
