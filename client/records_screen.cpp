#include <cstdlib>

#include "screens.h"

namespace agar 
{
void RecordsScreen::handleEvent(App& app, const sf::Event& e) 
{
    if (e.type == sf::Event::KeyPressed &&
        (e.key.code == sf::Keyboard::Escape || e.key.code == sf::Keyboard::Return))
        app.goTo(std::make_unique<MenuScreen>());
    else if (e.type == sf::Event::MouseButtonPressed && e.mouseButton.button == sf::Mouse::Left)
        app.goTo(std::make_unique<MenuScreen>());
}

void RecordsScreen::update(App& app, float dt) 
{
    if (state_ == State::Connecting) 
    {
        if (++ticks_ < 2) return;
        if (!tcp_.connect(app.profile.host, app.profile.tcpPort, 2.f)) 
        {
            error_ = "Не удалось подключиться к серверу " + app.profile.host + ":" +
                     std::to_string(app.profile.tcpPort);
            state_ = State::Failed;
            return;
        }
        tcp_.sendLine("RECORDS");
        state_ = State::Waiting;
        return;
    }
    if (state_ != State::Waiting) return;

    waited_ += dt;
    std::vector<std::string> lines;
    bool alive = tcp_.poll(lines);
    for (const std::string& line : lines) 
    {
        std::vector<std::string> w = splitWords(line);
        if (w.empty()) continue;
        if (w[0] == "RECORDS" && w.size() >= 2) 
        {
            expected_ = std::atoi(w[1].c_str());
            rows_.clear();
        } 
        else if (w[0] == "R" && w.size() >= 4) 
        {
            Row r;
            r.name = w[1];
            r.weight = static_cast<unsigned>(std::atoi(w[2].c_str()));
            r.seconds = std::atoi(w[3].c_str());
            rows_.push_back(r);
        }
    }
    if (expected_ >= 0 && static_cast<int>(rows_.size()) >= expected_) 
    {
        state_ = State::Ready;
        tcp_.close();
    } 
    else if (!alive) 
    {
        error_ = "Соединение с сервером потеряно.";
        state_ = State::Failed;
    } 
    else if (waited_ > 4.f) 
    {
        error_ = "Сервер не ответил.";
        state_ = State::Failed;
        tcp_.close();
    }
}

void RecordsScreen::draw(App& app, sf::RenderTarget& target) 
{
    drawText(target, app.font, "Таблица рекордов", kWinW / 2.f, 50.f, 52, sf::Color(240, 240, 245), Align::Center, true);

    if (state_ == State::Connecting || state_ == State::Waiting) 
    {
        drawText(target, app.font, "Подключение к серверу...", kWinW / 2.f, 320.f, 28,
                 sf::Color(190, 195, 210), Align::Center);
    } 
    else if (state_ == State::Failed) 
    {
        drawText(target, app.font, error_, kWinW / 2.f, 320.f, 26, sf::Color(255, 120, 110), Align::Center);
    } 
    else 
    {
        const float top = 150.f, step = 44.f;
        const float xNum = 330.f, xName = 400.f, xWeight = 820.f, xTime = 950.f;
        sf::Color head(150, 156, 172);
        drawText(target, app.font, "№", xNum, top - 36.f, 20, head, Align::Right);
        drawText(target, app.font, "Игрок", xName, top - 36.f, 20, head);
        drawText(target, app.font, "Вес", xWeight, top - 36.f, 20, head, Align::Right);
        drawText(target, app.font, "Время", xTime, top - 36.f, 20, head, Align::Right);

        for (int i = 0; i < kRecordsCount; i++) 
        {
            float y = top + static_cast<float>(i) * step;
            sf::RectangleShape band(sf::Vector2f(660.f, step - 4.f));
            band.setPosition(300.f, y - 2.f);
            band.setFillColor(i % 2 == 0 ? sf::Color(36, 40, 50) : sf::Color(30, 34, 43));
            target.draw(band);

            sf::Color c = i == 0 ? sf::Color(255, 215, 90) : sf::Color(225, 228, 238);
            drawText(target, app.font, std::to_string(i + 1), xNum, y + 3.f, 24, c, Align::Right);
            if (i < static_cast<int>(rows_.size())) 
            {
                const Row& r = rows_[static_cast<std::size_t>(i)];
                drawText(target, app.font, r.name, xName, y + 3.f, 24, c);
                drawText(target, app.font, std::to_string(r.weight), xWeight, y + 3.f, 24, c, Align::Right);
                drawText(target, app.font, formatTime(r.seconds), xTime, y + 3.f, 24, c, Align::Right);
            } 
            else 
            {
                drawText(target, app.font, "—", xName, y + 3.f, 24, sf::Color(90, 96, 110));
            }
        }
    }
    drawText(target, app.font, "Esc или Enter — назад", kWinW / 2.f, kWinH - 46.f, 20, sf::Color(150, 156, 172),
             Align::Center);
}
}  
