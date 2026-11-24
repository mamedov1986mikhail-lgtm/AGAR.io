#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>

#include "game_world.h"
#include "protocol.h"
#include "records.h"

using namespace agar;

static int failures = 0;
#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) 
        {                                                       \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);      \
            ++failures;                                                      \
        }                                                                    \
    } while (0)


static void clearFood(World& w) 
        {
    for (Food& f : w.mutableFoods()) 
    {
        f.x = 1.f;
        f.y = 1.f;
    }
}

static void testStart() 
{
    World w(1);
    int id = w.addPlayer("Alice", 3, false);
    CHECK(id >= 0);
    const Player* p = w.player(static_cast<std::uint8_t>(id));
    CHECK(p && p->weight == 1);
    CHECK(std::fabs(p->diameter() - 30.f) < 1e-3f);  
    CHECK(w.addPlayer("alice", 0, false) < 0);       
}

static void testPlayerLimit() 
{
    World w(2);
    for (int i = 0; i < kMaxPlayers; ++i)
        CHECK(w.addPlayer("P" + std::to_string(i), 0, false) >= 0);
    CHECK(w.addPlayer("Extra", 0, false) < 0);  
    CHECK(w.playerCount() == kMaxPlayers);
}

static void testFood() 
{
    World w(3);
    clearFood(w);
    int id = w.addPlayer("Eater", 0, false);
    Player* p = w.mutablePlayer(static_cast<std::uint8_t>(id));
    p->x = 500;
    p->y = 500;
    p->tx = 500;
    p->ty = 500;
    w.mutableFoods()[0] = Food{500.f + 22.f, 500.f, 2};
    w.mutableFoods()[1] = Food{500.f, 500.f + 25.f, 0};
    std::vector<DeathInfo> d;
    w.step(0.f, d);
    CHECK(w.player(static_cast<std::uint8_t>(id))->weight == 1 + 3);
    CHECK(std::fabs(w.player(static_cast<std::uint8_t>(id))->diameter() - 33.f) < 1e-3f);
    CHECK(w.foods().size() == static_cast<std::size_t>(kFoodCount));
    CHECK(!(w.foods()[0].x == 522.f && w.foods()[0].y == 500.f));
    CHECK(w.foods()[1].x == 500.f && w.foods()[1].y == 525.f);
}

static void testEatPlayer() 
{
    World w(4);
    clearFood(w);
    int a = w.addPlayer("Big", 0, false);
    int b = w.addPlayer("Small", 1, false);
    Player* pa = w.mutablePlayer(static_cast<std::uint8_t>(a));
    Player* pb = w.mutablePlayer(static_cast<std::uint8_t>(b));
    pa->weight = 11;
    pa->x = pa->tx = 1000;
    pa->y = pa->ty = 1000;
    pb->weight = 4;
    pb->x = pb->tx = 1000;
    pb->y = pb->ty = 1005;  
    std::vector<DeathInfo> d;
    w.step(0.f, d);
    CHECK(d.size() == 1 && d[0].name == "Small" && d[0].killerId == a);
    CHECK(w.player(static_cast<std::uint8_t>(a))->weight == 15);  
    CHECK(w.player(static_cast<std::uint8_t>(b)) == nullptr);

    World w2(5);
    clearFood(w2);
    int c = w2.addPlayer("C", 0, false);
    int e = w2.addPlayer("E", 0, false);
    Player* pc = w2.mutablePlayer(static_cast<std::uint8_t>(c));
    Player* pe = w2.mutablePlayer(static_cast<std::uint8_t>(e));
    pc->x = pc->tx = pe->x = pe->tx = 300;
    pc->y = pc->ty = pe->y = pe->ty = 300;
    d.clear();
    w2.step(0.f, d);
    CHECK(d.empty());
}

static void testMovement() 
{
    World w(6);
    clearFood(w);
    int id = w.addPlayer("Runner", 0, false);
    Player* p = w.mutablePlayer(static_cast<std::uint8_t>(id));
    p->x = p->tx = 100;
    p->y = p->ty = 100;
    w.setTarget(static_cast<std::uint8_t>(id), 1000.f, 100.f);
    std::vector<DeathInfo> d;
    for (int i = 0; i < 30; ++i) w.step(1.f / 30.f, d);
    float x = w.player(static_cast<std::uint8_t>(id))->x;
    CHECK(x > 250.f && x < 450.f); 
    w.setTarget(static_cast<std::uint8_t>(id), 99999.f, -5.f);  
    CHECK(w.player(static_cast<std::uint8_t>(id))->tx == kWorldW);
    CHECK(w.player(static_cast<std::uint8_t>(id))->ty == 0.f);
}

static void testProtocol() 
{
    InputPacket in;
    in.id = 5;
    in.token = 0xDEADBEEF;
    in.tx = 123.5f;
    in.ty = -7.25f;
    std::vector<std::uint8_t> bytes = encodeInput(in);
    InputPacket out;
    CHECK(decodeInput(bytes.data(), bytes.size(), out));
    CHECK(out.id == 5 && out.token == 0xDEADBEEF && out.tx == 123.5f && out.ty == -7.25f);
    CHECK(!decodeInput(bytes.data(), bytes.size() - 1, out)); 

    ByteWriter w;
    w.u8(kPktState);
    w.u32(42);
    w.u8(2);
    w.u8(2);
    w.u16(1);
    NetPlayer p;
    p.id = 2;
    p.skin = 9;
    p.x = 10.5f;
    p.y = 20.5f;
    p.weight = 77;
    p.name = "Zed99";
    encodePlayer(w, p);
    p.id = 3;
    p.name = "Q";
    encodePlayer(w, p);
    encodeFood(w, NetFood{1200, 3999, 2});
    StatePacket st;
    CHECK(decodeState(w.data().data(), w.data().size(), st));
    CHECK(st.tick == 42 && st.yourId == 2 && st.players.size() == 2 && st.foods.size() == 1);
    CHECK(st.players[0].name == "Zed99" && st.players[0].weight == 77 && st.players[0].skin == 9);
    CHECK(st.foods[0].x == 1200 && st.foods[0].y == 3999 && st.foods[0].type == 2);
    CHECK(!decodeState(w.data().data(), w.data().size() - 2, st));

    CHECK(isValidName("Player1") && isValidName("a"));
    CHECK(!isValidName("") && !isValidName("имя") && !isValidName("a b") &&
          !isValidName("1234567890123456"));
}

static void testRecords() 
{
    const char* path = "test_records.tmp";
    std::remove(path);
    Records r(path);
    r.load();
    for (int i = 0; i < 10; ++i) CHECK(r.add("p" + std::to_string(i), 10 + i, 5) > 0);
    CHECK(r.entries().size() == 10);
    CHECK(!r.qualifies(5, 100));         
    CHECK(r.add("low", 5, 100) == 0);
    CHECK(r.add("top", 100, 1) == 1);    
    CHECK(r.entries().size() == 10 && r.entries().back().weight == 11);  
    r.save();
    Records r2(path);
    r2.load();
    CHECK(r2.entries().size() == 10 && r2.entries()[0].name == "top");
    std::remove(path);
}

int main() 
{
    testStart();
    testPlayerLimit();
    testFood();
    testEatPlayer();
    testMovement();
    testBots();
    testProtocol();
    testRecords();
    if (failures == 0) std::printf("Все тесты пройдены\n");
    return failures == 0 ? 0 : 1;
}
