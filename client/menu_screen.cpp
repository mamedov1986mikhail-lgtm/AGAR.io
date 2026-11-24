#include <cmath>

#include "screens.h"

namespace agar 
{
namespace 
{
constexpr float kItemW = 380.f, kItemH = 56.f, kItemGap = 14.f, kItemTop = 250.f;
}

MenuScreen::MenuScreen(std::string message)
    : items_{"Создать игрока", "Игра", "Рекорды", "Выход"}, message_(std::move(message)) {}

sf::FloatRect MenuScreen::itemRect(int i) const 
{
    return sf::FloatRect((kWinW - kItemW) / 2.f, kItemTop + static_cast<float>(i) * (kItemH + kItemGap), kItemW, kItemH);
}

int MenuScreen::itemAt(float x, float y) const 
{
    for (int i = 0; i < static_cast<int>(items_.size()); i++)
        if (itemRect(i).contains(x, y)) return i;
    return -1;
}

void MenuScreen::activate(App& app, int index) 
{
    switch (index) 
    {
        case 0:
            app.goTo(std::make_unique<CreatePlayerScreen>(app));
            break;
        case 1:
            if (!app.profile.created()) 
            {
                message_ = "Сначала создайте игрока.";
                sel_ = 0;
            } 
            else 
            {
                app.goTo(std::make_unique<GameScreen>());
            }
            break;
        case 2:
            app.goTo(std::make_unique<RecordsScreen>());
            break;
        default:
            app.quit();
            break;
    }
}

void MenuScreen::handleEvent(App& app, const sf::Event& e) 
{
    const int n = static_cast<int>(items_.size());
    if (e.type == sf::Event::KeyPressed) 
    {
        if (e.key.code == sf::Keyboard::Up) sel_ = (sel_ + n - 1) % n;
        else if (e.key.code == sf::Keyboard::Down) sel_ = (sel_ + 1) % n;
        else if (e.key.code == sf::Keyboard::Return) activate(app, sel_);
        else if (e.key.code == sf::Keyboard::Escape) app.quit();
    } 
    else if (e.type == sf::Event::MouseMoved) 
    {
        int i = itemAt(static_cast<float>(e.mouseMove.x), static_cast<float>(e.mouseMove.y));
        if (i >= 0) sel_ = i;
    } 
    else if (e.type == sf::Event::MouseButtonPressed && e.mouseButton.button == sf::Mouse::Left) 
    {
        int i = itemAt(static_cast<float>(e.mouseButton.x), static_cast<float>(e.mouseButton.y));
        if (i >= 0) 
        {
            sel_ = i;
            activate(app, i);
        }
    }
}

void MenuScreen::update(App&, float dt) { time_ += dt; }

void MenuScreen::draw(App& app, sf::RenderTarget& target) 
{
    for (int i = 0; i < 7; i++) 
    {
        float fi = static_cast<float>(i);
        float r = 26.f + fi * 9.f;
        sf::CircleShape blob(r, 48);
        blob.setOrigin(r, r);
        blob.setPosition(kWinW * (0.12f + 0.13f * fi) + 30.f * std::sin(time_ * 0.6f + fi * 1.7f),
                         kWinH * (0.25f + 0.5f * std::fmod(fi * 0.37f, 1.f)) +
                             24.f * std::cos(time_ * 0.5f + fi));
        app.skins.apply(blob, i % kSkinCount);
        sf::Color c = blob.getFillColor();
        c.a = 70;
        blob.setFillColor(c);
        target.draw(blob);
    }

    drawText(target, app.font, "AGAR", kWinW / 2.f, 70.f, 84, sf::Color(240, 240, 245), Align::Center, true);

    for (int i = 0; i < static_cast<int>(items_.size()); i++) 
    {
        sf::FloatRect r = itemRect(i);
        sf::RectangleShape box(sf::Vector2f(r.width, r.height));
        box.setPosition(r.left, r.top);
        bool sel = i == sel_;
        box.setFillColor(sel ? sf::Color(70, 120, 220) : sf::Color(40, 45, 56));
        box.setOutlineThickness(2.f);
        box.setOutlineColor(sel ? sf::Color(170, 205, 255) : sf::Color(70, 76, 92));
        target.draw(box);
        drawText(target, app.font, items_[static_cast<std::size_t>(i)], r.left + r.width / 2.f,
                 r.top + 11.f, 28, sf::Color::White, Align::Center);
    }

    if (!message_.empty())
        drawText(target, app.font, message_, kWinW / 2.f, 585.f, 22, sf::Color(255, 120, 110), Align::Center);

    if (app.profile.created()) 
    {
        sf::CircleShape blob(16.f, 32);
        blob.setOrigin(16.f, 16.f);
        blob.setPosition(40.f, kWinH - 40.f);
        app.skins.apply(blob, app.profile.skin);
        target.draw(blob);
        drawText(target, app.font, "Игрок: " + app.profile.name + "    Сервер: " + app.profile.host + ":" + std::to_string(app.profile.tcpPort),
                 64.f, kWinH - 54.f, 20, sf::Color(190, 195, 210));
    } 
    else 
    {
        drawText(target, app.font, "Игрок ещё не создан", 30.f, kWinH - 54.f, 20, sf::Color(190, 195, 210));
    }
    drawText(target, app.font, "↑ ↓ Enter или мышь", kWinW - 30.f, kWinH - 54.f, 18,
             sf::Color(120, 126, 142), Align::Right);
}
}  
