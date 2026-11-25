#pragma once

#include <array>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

#include "protocol.h"

namespace agar 
{
struct Player 
{
    std::uint8_t id = 0;
    std::string name;
    std::uint8_t skin = 0;
    float x = 0, y = 0;     
    float tx = 0, ty = 0;   
    std::uint32_t weight = 1;
    float aliveTime = 0;    
    bool active = false;

    float diameter() const { return diameterForWeight(weight); }
    float radius() const { return diameter() / 2.f; }
};

struct Food 
{
    float x = 0, y = 0;
    std::uint8_t type = 0;  
};

struct DeathInfo 
{
    std::uint8_t id = 0;
    std::string name;
    std::uint32_t weight = 1;
    float aliveTime = 0;
    std::uint8_t killerId = 0;
};

class World 
{
public:
    explicit World(std::uint32_t seed = 12345);

    int addPlayer(const std::string& name, std::uint8_t skin);
    bool removePlayer(std::uint8_t id);
    bool nameTaken(const std::string& name) const;  

    void setTarget(std::uint8_t id, float x, float y);
    void step(float dt, std::vector<DeathInfo>& deaths);

    const Player* player(std::uint8_t id) const;
    const std::array<Player, kMaxPlayers>& players() const { return players_; }
    const std::vector<Food>& foods() const { return foods_; }
    int playerCount() const;

    void collectFoodsNear(float x, float y, float halfW, float halfH, int maxCount,
                          std::vector<Food>& out) const;

    static float speedFor(float diameter);

    Player* mutablePlayer(std::uint8_t id);
    std::vector<Food>& mutableFoods() { return foods_; }

private:
    float rnd(float a, float b);
    void respawnFood(Food& f);
    void findSpawn(float& x, float& y);

    std::mt19937 rng_;
    std::array<Player, kMaxPlayers> players_;
    std::vector<Food> foods_;
};
}  
