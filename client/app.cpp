#include "app.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>

#include "screens.h"

namespace agar 
{

namespace 
{

const char* kProfileFile = "player.cfg";

std::string findAssetDir(sf::Font& font) 
{
    const std::string candidates[] = {"assets", AGAR_ASSET_DIR};
    for (const std::string& dir : candidates) 
    {
        if (font.loadFromFile(dir + "/fonts/DejaVuSans.ttf")) return dir;
    }
    std::fprintf(stderr, "Не найден шрифт assets/fonts/DejaVuSans.ttf - текст не будет показан.\n");
    return AGAR_ASSET_DIR;
}

}  

App::App(int argc, char** argv)
    : window(sf::VideoMode(kWinW, kWinH), "AGAR.io", sf::Style::Titlebar | sf::Style::Close) 
{
    std::string assetDir = findAssetDir(font);
    road = makeRoadTexture(assetDir);
    loadProfile();

    if (argc > 1) 
    {
        std::string a = argv[1];
        std::size_t colon = a.rfind(':');
        if (colon != std::string::npos) 
        {
            int port = std::atoi(a.c_str() + colon + 1);
            if (port > 0 && port < 65536) profile.tcpPort = static_cast<unsigned short>(port);
            a.erase(colon);
        }
        if (!a.empty()) profile.host = a;
    }
}

void App::loadProfile() 
{
    std::ifstream in(kProfileFile);
    std::string line;
    while (std::getline(in, line)) 
    {
        std::size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq), val = line.substr(eq + 1);
        if (!val.empty() && val.back() == '\r') val.pop_back();
        if (key == "name" && isValidName(val)) 
        {
            profile.name = val;
        } 
        else if (key == "skin") 
        {
            int s = std::atoi(val.c_str());
            if (s >= 0 && s < kSkinCount) profile.skin = s;
        } 
        else if (key == "host" && !val.empty()) 
        {
            profile.host = val;
        } 
        else if (key == "port") 
        {
            int p = std::atoi(val.c_str());
            if (p > 0 && p < 65536) profile.tcpPort = static_cast<unsigned short>(p);
        }
    }
}

void App::saveProfile() const 
{
    std::ofstream out(kProfileFile, std::ios::trunc);
    if (!out) return;
    out << "name=" << profile.name << "\n"
        << "skin=" << profile.skin << "\n"
        << "host=" << profile.host << "\n"
        << "port=" << profile.tcpPort << "\n";
}

int App::run() 
{
    window.setFramerateLimit(60);
    screen_ = std::make_unique<MenuScreen>();

    sf::Clock clock;
    while (window.isOpen()) 
    {
        sf::Event e;
        while (window.pollEvent(e)) 
        {
            if (e.type == sf::Event::Closed) 
            {
                window.close();
                break;
            }
            if (screen_) screen_->handleEvent(*this, e);
        }
        float dt = std::min(clock.restart().asSeconds(), 0.1f);
        if (!window.isOpen()) break;
        if (screen_) screen_->update(*this, dt);

        window.clear(sf::Color(24, 27, 33));
        if (screen_) screen_->draw(*this, window);
        window.display();

        if (next_) 
        {
            screen_ = std::move(next_);
            window.setView(window.getDefaultView());
        }
    }
    screen_.reset();  
    return 0;
}

} 
