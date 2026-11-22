#pragma once

#include <SFML/Graphics.hpp>

#include <memory>
#include <string>

#include "protocol.h"
#include "skins.h"
#include "ui.h"

namespace agar 
{
struct Profile 
{
    std::string name;  
    int skin = 0;
    std::string host = "127.0.0.1";
    unsigned short tcpPort = kDefaultTcpPort;

    bool created() const { return !name.empty(); }
};

class App;

class Screen 
{
public:
    virtual ~Screen() = default;
    virtual void handleEvent(App& app, const sf::Event& e) = 0;
    virtual void update(App& app, float dt) = 0;
    virtual void draw(App& app, sf::RenderTarget& target) = 0;
};

class App 
{
public:
    App(int argc, char** argv);

    int run();
    void goTo(std::unique_ptr<Screen> next) { next_ = std::move(next); }
    void quit() { window.close(); }

    void loadProfile();
    void saveProfile() const;

    sf::RenderWindow window;
    sf::Font font;
    Skins skins;
    sf::Texture road;
    Profile profile;

private:
    std::unique_ptr<Screen> screen_;
    std::unique_ptr<Screen> next_;
};

}  
