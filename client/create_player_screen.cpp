#include <cmath>
#include <cstdlib>

#include "screens.h"

namespace agar 
{
namespace 
{
constexpr float kFieldX = 140.f, kFieldW = 440.f, kFieldH = 50.f;
constexpr float kNameY = 190.f, kServerY = 300.f, kSkinY = 410.f;
constexpr float kSwatchR = 24.f, kSwatchStep = 64.f;
constexpr int kSwatchCols = 6;

bool isAlnum(sf::Uint32 u) 
{
    return (u >= '0' && u <= '9') || (u >= 'a' && u <= 'z') || (u >= 'A' && u <= 'Z');
}

bool parseServer(const std::string& s, std::string& host, unsigned short& port, unsigned short defPort) 
{
    std::string h = s;
    unsigned short p = defPort;
    std::size_t colon = h.rfind(':');
    if (colon != std::string::npos) 
    {
        int v = std::atoi(h.c_str() + colon + 1);
        if (v <= 0 || v > 65535) return false;
        p = static_cast<unsigned short>(v);
        h.erase(colon);
    }
    if (h.empty()) return false;
    host = h;
    port = p;
    return true;
}
}  

CreatePlayerScreen::CreatePlayerScreen(App& app)
    : name_(app.profile.name),
      server_(app.profile.host + ":" + std::to_string(app.profile.tcpPort)),
      skin_(app.profile.skin) {}

sf::Vector2f CreatePlayerScreen::swatchCenter(int i) const 
{
    return sf::Vector2f(kFieldX + kSwatchR + static_cast<float>(i % kSwatchCols) * kSwatchStep,
                        kSkinY + 40.f + static_cast<float>(i / kSwatchCols) * kSwatchStep);
}

void CreatePlayerScreen::submit(App& app) 
{
    if (!isValidName(name_)) 
    {
        error_ = "Имя: только латинские буквы и цифры, от 1 до 15 символов.";
        focus_ = kName;
        return;
    }
    std::string host;
    unsigned short port = 0;
    if (!parseServer(server_, host, port, kDefaultTcpPort)) 
    {
        error_ = "Адрес сервера: host или host:port.";
        focus_ = kServer;
        return;
    }
    app.profile.name = name_;
    app.profile.skin = skin_;
    app.profile.host = host;
    app.profile.tcpPort = port;
    app.saveProfile();
    app.goTo(std::make_unique<MenuScreen>());
}

void CreatePlayerScreen::handleEvent(App& app, const sf::Event& e) 
{
    if (e.type == sf::Event::KeyPressed) 
    {
        switch (e.key.code) 
        {
            case sf::Keyboard::Escape:
                app.goTo(std::make_unique<MenuScreen>());
                break;
            case sf::Keyboard::Return:
                submit(app);
                break;
            case sf::Keyboard::Tab:
                focus_ = (focus_ + 1) % kFieldCount;
                break;
            case sf::Keyboard::Left:
                if (focus_ == kSkin) skin_ = (skin_ + kSkinCount - 1) % kSkinCount;
                break;
            case sf::Keyboard::Right:
                if (focus_ == kSkin) skin_ = (skin_ + 1) % kSkinCount;
                break;
            case sf::Keyboard::BackSpace:
                if (focus_ == kName && !name_.empty()) name_.pop_back();
                if (focus_ == kServer && !server_.empty()) server_.pop_back();
                break;
            default:
                break;
        }
    } 
    else if (e.type == sf::Event::TextEntered) 
    {
        sf::Uint32 u = e.text.unicode;
        if (focus_ == kName && isAlnum(u) && name_.size() < kMaxNameLen) 
        {
            name_.push_back(static_cast<char>(u));
            error_.clear();
        } 
        else if (focus_ == kServer && (isAlnum(u) || u == '.' || u == ':' || u == '-' || u == '_') &&
                   server_.size() < 60) {
            server_.push_back(static_cast<char>(u));
            error_.clear();
        }
    } 
    else if (e.type == sf::Event::MouseButtonPressed && e.mouseButton.button == sf::Mouse::Left) 
    {
        float mx = static_cast<float>(e.mouseButton.x), my = static_cast<float>(e.mouseButton.y);
        if (sf::FloatRect(kFieldX, kNameY, kFieldW, kFieldH).contains(mx, my)) focus_ = kName;
        if (sf::FloatRect(kFieldX, kServerY, kFieldW, kFieldH).contains(mx, my)) focus_ = kServer;
        for (int i = 0; i < kSkinCount; i++) 
        {
            sf::Vector2f c = swatchCenter(i);
            float dx = mx - c.x, dy = my - c.y;
            if (dx * dx + dy * dy <= kSwatchR * kSwatchR) 
            {
                skin_ = i;
                focus_ = kSkin;
            }
        }
    }
}

void CreatePlayerScreen::update(App&, float dt) { time_ += dt; }

void CreatePlayerScreen::draw(App& app, sf::RenderTarget& target) 
{
    drawText(target, app.font, "Создать игрока", kFieldX, 70.f, 48, sf::Color(240, 240, 245), Align::Left, true);

    auto field = [&](const char* label, float y, const std::string& value, bool focused, unsigned size) 
        {
        drawText(target, app.font, label, kFieldX, y - 32.f, 20, sf::Color(170, 176, 192));
        sf::RectangleShape box(sf::Vector2f(kFieldW, kFieldH));
        box.setPosition(kFieldX, y);
        box.setFillColor(sf::Color(35, 39, 48));
        box.setOutlineThickness(2.f);
        box.setOutlineColor(focused ? sf::Color(110, 170, 255) : sf::Color(80, 86, 100));
        target.draw(box);
        std::string shown = value;
        if (focused && std::fmod(time_, 1.f) < 0.5f) shown += "|";
        drawText(target, app.font, shown, kFieldX + 12.f, y + 9.f, size, sf::Color::White);
    };
    field("Имя игрока (латинские буквы и цифры, до 15 символов)", kNameY, name_, focus_ == kName, 26);
    field("Сервер (адрес или адрес:порт)", kServerY, server_, focus_ == kServer, 24);

    drawText(target, app.font, "Цвет или текстура капли (← → или мышь)", kFieldX, kSkinY - 32.f + 10.f, 20,
             focus_ == kSkin ? sf::Color(150, 200, 255) : sf::Color(170, 176, 192));
    for (int i = 0; i < kSkinCount; i++) 
    {
        sf::Vector2f c = swatchCenter(i);
        sf::CircleShape s(kSwatchR, 40);
        s.setOrigin(kSwatchR, kSwatchR);
        s.setPosition(c);
        app.skins.apply(s, i);
        s.setOutlineThickness(i == skin_ ? 4.f : 2.f);
        s.setOutlineColor(i == skin_ ? sf::Color::White : darker(app.skins.colorOf(i), 0.5f));
        target.draw(s);
    }

    sf::CircleShape preview(110.f, 80);
    preview.setOrigin(110.f, 110.f);
    preview.setPosition(930.f, 300.f);
    app.skins.apply(preview, skin_);
    preview.setOutlineThickness(5.f);
    preview.setOutlineColor(darker(app.skins.colorOf(skin_), 0.6f));
    target.draw(preview);
    drawText(target, app.font, name_.empty() ? "Игрок" : name_, 930.f, 440.f, 30, sf::Color::White, Align::Center, true);

    if (!error_.empty())
        drawText(target, app.font, error_, kFieldX, 615.f, 22, sf::Color(255, 120, 110));
    else if (!app.profile.created() || name_ != app.profile.name)
        drawText(target, app.font, "Если имя уже занято, сервер сообщит об этом при входе в игру.", kFieldX, 615.f,
                 18, sf::Color(120, 126, 142));

    drawText(target, app.font, "Enter — сохранить     Tab — следующее поле     Esc — назад", kWinW / 2.f,
             kWinH - 50.f, 20, sf::Color(150, 156, 172), Align::Center);
}

} 
