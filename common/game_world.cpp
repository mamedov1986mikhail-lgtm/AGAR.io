#include "game_world.h"

#include <algorithm>
#include <cmath>
#include <cctype>

namespace agar 
{
namespace 
{
bool equalNoCase(const std::string& a, const std::string& b) 
{
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); i++) 
    {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i])))
            return false;
    }
    return true;
}

float clampf(float v, float lo, float hi) { return std::max(lo, std::min(hi, v)); }
}  

World::World(std::uint32_t seed) : rng_(seed) 
{
    for (std::size_t i = 0; i < players_.size(); i++) players_[i].id = static_cast<std::uint8_t>(i);
    foods_.resize(kFoodCount);
    for (Food& f : foods_) respawnFood(f);
}

float World::rnd(float a, float b) 
{
    std::uniform_real_distribution<float> d(a, b);
    return d(rng_);
}

void World::respawnFood(Food& f) 
{
    f.x = rnd(20.f, kWorldW - 20.f);
    f.y = rnd(20.f, kWorldH - 20.f);
    float r = rnd(0.f, 1.f);
    f.type = r < 0.6f ? 0 : (r < 0.9f ? 1 : 2);
}

float World::speedFor(float diameter) 
{
    return std::max(70.f, 260.f * std::pow(kStartDiameter / diameter, 0.35f));
}

int World::playerCount() const 
{
    int n = 0;
    for (const Player& p : players_) n += p.active ? 1 : 0;
    return n;
}

bool World::nameTaken(const std::string& name) const 
{
    for (const Player& p : players_)
        if (p.active && equalNoCase(p.name, name)) return true;
    return false;
}

const Player* World::player(std::uint8_t id) const 
{
    if (id >= kMaxPlayers || !players_[id].active) return nullptr;
    return &players_[id];
}

Player* World::mutablePlayer(std::uint8_t id) 
{
    if (id >= kMaxPlayers || !players_[id].active) return nullptr;
    return &players_[id];
}

void World::findSpawn(float& x, float& y) 
{
    float bestD = -1.f;
    for (int i = 0; i < 24; ++i) {
        float cx = rnd(100.f, kWorldW - 100.f);
        float cy = rnd(100.f, kWorldH - 100.f);
        float minD = 1e9f;
        for (const Player& p : players_) 
        {
            if (!p.active) continue;
            minD = std::min(minD, std::hypot(p.x - cx, p.y - cy));
        }
        if (minD > bestD) 
        {
            bestD = minD;
            x = cx;
            y = cy;
        }
    }
}

int World::addPlayer(const std::string& name, std::uint8_t skin) 
{
    if (nameTaken(name)) return -1;
    for (Player& p : players_) {
        if (p.active) continue;
        std::uint8_t id = p.id;
        p = Player{};
        p.id = id;
        p.name = name;
        p.skin = skin;
        p.active = true;
        p.weight = 1;
        findSpawn(p.x, p.y);
        p.tx = p.x;
        p.ty = p.y;
        return id;
    }
    return -1;
}

bool World::removePlayer(std::uint8_t id) 
{
    if (id >= kMaxPlayers || !players_[id].active) return false;
    players_[id].active = false;
    return true;
}

void World::setTarget(std::uint8_t id, float x, float y) {
    Player* p = mutablePlayer(id);
    if (!p) return;
    p->tx = clampf(x, 0.f, kWorldW);
    p->ty = clampf(y, 0.f, kWorldH);
}

void World::step(float dt, std::vector<DeathInfo>& deaths) 
{
    for (Player& p : players_) 
    {
        if (!p.active) continue;
        float dx = p.tx - p.x, dy = p.ty - p.y;
        float dist = std::hypot(dx, dy);
        if (dist > 1.f) 
        {
            float v = speedFor(p.diameter()) * std::min(1.f, dist / 60.f);
            float step = std::min(dist, v * dt);
            p.x += dx / dist * step;
            p.y += dy / dist * step;
        }
        p.x = clampf(p.x, 0.f, kWorldW);
        p.y = clampf(p.y, 0.f, kWorldH);
        p.aliveTime += dt;
    }

    for (Player& p : players_) 
    {
        if (!p.active) continue;
        float r = p.radius();
        for (Food& f : foods_) 
        {
            float reach = r + foodSize(f.type) / 2.f;
            float dx = f.x - p.x, dy = f.y - p.y;
            if (dx * dx + dy * dy <= reach * reach) 
            {
                p.weight += foodWeight(f.type);
                respawnFood(f);
                r = p.radius();
            }
        }
    }

    std::array<Player*, kMaxPlayers> order{};
    std::size_t n = 0;
    for (Player& p : players_)
        if (p.active) order[n++] = &p;
    std::sort(order.begin(), order.begin() + static_cast<std::ptrdiff_t>(n),
              [](const Player* a, const Player* b) { return a->weight > b->weight; });
    std::array<bool, kMaxPlayers> dead{};
    for (std::size_t i = 0; i < n; i++) 
    {
        Player* big = order[i];
        if (dead[big->id]) continue;
        for (std::size_t j = i + 1; j < n; j++) 
        {
            Player* small = order[j];
            if (dead[small->id] || small->weight >= big->weight) continue;
            float d = std::hypot(small->x - big->x, small->y - big->y);
            if (d < big->radius()) {
                big->weight += small->weight;
                dead[small->id] = true;
                DeathInfo info;
                info.id = small->id;
                info.name = small->name;
                info.weight = small->weight;
                info.aliveTime = small->aliveTime;
                info.killerId = big->id;
                deaths.push_back(std::move(info));
            }
        }
    }
    for (Player& p : players_)
        if (p.active && dead[p.id]) p.active = false;
}

void World::collectFoodsNear(float x, float y, float halfW, float halfH, int maxCount,
                             std::vector<Food>& out) const 
{
    out.clear();
    for (const Food& f : foods_) 
    {
        if (std::fabs(f.x - x) <= halfW && std::fabs(f.y - y) <= halfH) 
        {
            out.push_back(f);
            if (static_cast<int>(out.size()) >= maxCount) break;
        }
    }
}

} 
