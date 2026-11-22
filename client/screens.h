#pragma once

#include <map>
#include <string>
#include <vector>

#include "app.h"
#include "net.h"

namespace agar 
{

// ---------------------------------------------------------------- главное меню
class MenuScreen : public Screen 
{
public:
    explicit MenuScreen(std::string message = "");
    void handleEvent(App& app, const sf::Event& e) override;
    void update(App& app, float dt) override;
    void draw(App& app, sf::RenderTarget& target) override;

private:
    sf::FloatRect itemRect(int i) const;
    int itemAt(float x, float y) const;
    void activate(App& app, int index);

    std::vector<std::string> items_;
    int sel_ = 0;
    std::string message_;
    float time_ = 0;
};

// ---------------------------------------------------------------- создание игрока
class CreatePlayerScreen : public Screen 
{
public:
    explicit CreatePlayerScreen(App& app);
    void handleEvent(App& app, const sf::Event& e) override;
    void update(App& app, float dt) override;
    void draw(App& app, sf::RenderTarget& target) override;

private:
    enum Field { kName = 0, kServer = 1, kSkin = 2, kFieldCount = 3 };

    sf::Vector2f swatchCenter(int i) const;
    void submit(App& app);

    std::string name_;
    std::string server_;
    int skin_ = 0;
    int focus_ = kName;
    std::string error_;
    float time_ = 0;
};

// ---------------------------------------------------------------- таблица рекордов
class RecordsScreen : public Screen 
{
public:
    RecordsScreen() = default;
    void handleEvent(App& app, const sf::Event& e) override;
    void update(App& app, float dt) override;
    void draw(App& app, sf::RenderTarget& target) override;

private:
    enum class State { Connecting, Waiting, Ready, Failed };

    struct Row 
    {
        std::string name;
        unsigned weight = 0;
        int seconds = 0;
    };

    TcpLink tcp_;
    State state_ = State::Connecting;
    int ticks_ = 0;
    float waited_ = 0;
    int expected_ = -1;
    std::vector<Row> rows_;
    std::string error_;
};

// ---------------------------------------------------------------- игра
class GameScreen : public Screen 
{
public:
    GameScreen() = default;
    ~GameScreen() override;
    void handleEvent(App& app, const sf::Event& e) override;
    void update(App& app, float dt) override;
    void draw(App& app, sf::RenderTarget& target) override;

private:
    enum class Phase { Connecting, Queued, Playing, Leaving, Over, Error };

    void connect(App& app);
    void handleLine(App& app, const std::string& line);
    void requestLeave();
    sf::Vector2f cameraCenter() const;
    float zoom() const;
    sf::Vector2f mouseInWorld() const;
    void smoothPositions(float dt);

    void drawWorld(App& app, sf::RenderTarget& target);
    void drawHud(App& app, sf::RenderTarget& target);
    void drawOverlay(App& app, sf::RenderTarget& target, const std::string& title,
                     const std::vector<std::string>& lines, sf::Color titleColor);

    TcpLink tcp_;
    UdpLink udp_;
    Phase phase_ = Phase::Connecting;
    int ticks_ = 0;

    int myId_ = -1;
    std::uint32_t token_ = 0;
    float worldW_ = kWorldW, worldH_ = kWorldH;

    StatePacket state_;
    bool haveState_ = false;
    float sinceState_ = 0;
    std::map<std::uint8_t, sf::Vector2f> shown_; 

    float inputTimer_ = 0;
    float playTime_ = 0;
    float leaveTimer_ = 0;
    sf::Vector2f mouse_{kWinW / 2.f, kWinH / 2.f};

    std::string message_;      
    std::string errorCode_;
    unsigned finalWeight_ = 1;
    int finalSeconds_ = 0;
    bool newRecord_ = false;
    int recordPlace_ = 0;
};

} 
