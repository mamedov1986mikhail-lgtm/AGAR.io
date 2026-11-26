#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

namespace agar 
{
constexpr int kMaxPlayers = 16;            
constexpr float kWorldW = 4000.f;          
constexpr float kWorldH = 4000.f;
constexpr float kStartDiameter = 30.f;     
constexpr int kFoodCount = 1200;          
constexpr int kTickHz = 30;               
constexpr int kSkinCount = 11;             
constexpr int kColorSkins = 8;
constexpr std::size_t kMaxNameLen = 15;
constexpr int kRecordsCount = 10;          
constexpr std::uint16_t kDefaultTcpPort = 7777;
constexpr std::uint16_t kDefaultUdpPort = 7778;
constexpr float kViewHalfW = 760.f;        
constexpr float kViewHalfH = 460.f;
constexpr int kMaxFoodPerPacket = 150;     

constexpr const char* kServerFullMessage = "Сервер полон. Ожидайте своей очереди на вход.";
constexpr int kMinPlayers = 3;
constexpr const char* kWaitPlayersMessage =
    "Ожидайте подключения: для входа на сервер нужно три игрока.";
enum PacketType : std::uint8_t { kPktInput = 1, kPktState = 2 };

inline float foodSize(int type) { return 5.f * static_cast<float>(type + 1); }
inline std::uint32_t foodWeight(int type) { return static_cast<std::uint32_t>(type + 1); }

inline float diameterForWeight(std::uint32_t weight) {
    return kStartDiameter - 1.f + static_cast<float>(weight);
}

class ByteWriter 
{
public:
    void u8(std::uint8_t v) { buf_.push_back(v); }
    void u16(std::uint16_t v) 
    {
        buf_.push_back(static_cast<std::uint8_t>(v & 0xFF));
        buf_.push_back(static_cast<std::uint8_t>(v >> 8));
    }
    void u32(std::uint32_t v) 
    {
        for (int i = 0; i < 4; i++) buf_.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF));
    }
    void f32(float f) 
    {
        std::uint32_t v;
        std::memcpy(&v, &f, sizeof v);
        u32(v);
    }
    void str8(const std::string& s) 
    {
        std::size_t n = std::min<std::size_t>(s.size(), 255);
        u8(static_cast<std::uint8_t>(n));
        buf_.insert(buf_.end(), s.begin(), s.begin() + static_cast<std::ptrdiff_t>(n));
    }
    void bytes(const std::vector<std::uint8_t>& b) { buf_.insert(buf_.end(), b.begin(), b.end()); }
    const std::vector<std::uint8_t>& data() const { return buf_; }
    std::vector<std::uint8_t>& data() { return buf_; }

private:
    std::vector<std::uint8_t> buf_;
};

class ByteReader 
{
public:
    ByteReader(const std::uint8_t* d, std::size_t n) : d_(d), n_(n) {}
    bool ok() const { return ok_; }
    std::size_t remaining() const { return n_ - pos_; }
    std::uint8_t u8() { return need(1) ? d_[pos_++] : 0; }
    std::uint16_t u16() {
        if (!need(2)) return 0;
        std::uint16_t v = static_cast<std::uint16_t>(d_[pos_] | (d_[pos_ + 1] << 8));
        pos_ += 2;
        return v;
    }
    std::uint32_t u32() 
    {
        if (!need(4)) return 0;
        std::uint32_t v = 0;
        for (int i = 0; i < 4; i++) v |= static_cast<std::uint32_t>(d_[pos_ + i]) << (8 * i);
        pos_ += 4;
        return v;
    }
    float f32() 
    {
        std::uint32_t v = u32();
        float f;
        std::memcpy(&f, &v, sizeof f);
        return f;
    }
    std::string str8() 
    {
        std::size_t n = u8();
        if (!need(n)) return {};
        std::string s(reinterpret_cast<const char*>(d_ + pos_), n);
        pos_ += n;
        return s;
    }

private:
    bool need(std::size_t k) 
    {
        if (!ok_ || pos_ + k > n_) 
        {
            ok_ = false;
            return false;
        }
        return true;
    }
    const std::uint8_t* d_;
    std::size_t n_;
    std::size_t pos_ = 0;
    bool ok_ = true;
};

struct NetPlayer 
{
    std::uint8_t id = 0;
    std::uint8_t skin = 0;
    float x = 0, y = 0;
    std::uint32_t weight = 1;
    std::string name;
};

struct NetFood 
{
    std::uint16_t x = 0, y = 0;
    std::uint8_t type = 0;
};

struct StatePacket 
{
    std::uint32_t tick = 0;
    std::uint8_t yourId = 0;
    std::vector<NetPlayer> players;
    std::vector<NetFood> foods;
};

struct InputPacket 
{
    std::uint8_t id = 0;
    std::uint32_t token = 0;
    float tx = 0, ty = 0;
};

inline std::vector<std::uint8_t> encodeInput(const InputPacket& in) 
{
    ByteWriter w;
    w.u8(kPktInput);
    w.u8(in.id);
    w.u32(in.token);
    w.f32(in.tx);
    w.f32(in.ty);
    return w.data();
}

inline bool decodeInput(const std::uint8_t* d, std::size_t n, InputPacket& out) 
{
    ByteReader r(d, n);
    if (r.u8() != kPktInput) return false;
    out.id = r.u8();
    out.token = r.u32();
    out.tx = r.f32();
    out.ty = r.f32();
    return r.ok();
}

inline void encodePlayer(ByteWriter& w, const NetPlayer& p) 
{
    w.u8(p.id);
    w.u8(p.skin);
    w.f32(p.x);
    w.f32(p.y);
    w.u32(p.weight);
    w.str8(p.name);
}

inline void encodeFood(ByteWriter& w, const NetFood& f) 
{
    w.u16(f.x);
    w.u16(f.y);
    w.u8(f.type);
}

inline bool decodeState(const std::uint8_t* d, std::size_t n, StatePacket& out) 
{
    ByteReader r(d, n);
    if (r.u8() != kPktState) return false;
    out.tick = r.u32();
    out.yourId = r.u8();
    std::size_t np = r.u8();
    std::size_t nf = r.u16();
    if (!r.ok()) return false;
    out.players.clear();
    out.foods.clear();
    for (std::size_t i = 0; i < np; i++) 
    {
        NetPlayer p;
        p.id = r.u8();
        p.skin = r.u8();
        p.x = r.f32();
        p.y = r.f32();
        p.weight = r.u32();
        p.name = r.str8();
        if (!r.ok()) return false;
        out.players.push_back(std::move(p));
    }
    for (std::size_t i = 0; i < nf; i++) 
    {
        NetFood f;
        f.x = r.u16();
        f.y = r.u16();
        f.type = r.u8();
        if (!r.ok()) return false;
        out.foods.push_back(f);
    }
    return true;
}

inline bool isValidName(const std::string& s) 
{
    if (s.empty() || s.size() > kMaxNameLen) return false;
    for (unsigned char c : s) 
    {
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
        if (!ok) return false;
    }
    return true;
}

inline std::vector<std::string> splitWords(const std::string& line) 
{
    std::istringstream in(line);
    std::vector<std::string> out;
    std::string w;
    while (in >> w) out.push_back(w);
    return out;
}

inline std::string formatTime(int seconds) 
{
    char buf[32];
    std::snprintf(buf, sizeof buf, "%02d:%02d", seconds / 60, seconds % 60);
    return buf;
}

}  
