#pragma once
#include <SFML/Graphics.hpp>
#include <string>

namespace agar 
{
enum class Align { Left, Center, Right };

inline sf::String U(const std::string& utf8) { return sf::String::fromUtf8(utf8.begin(), utf8.end()); }

inline float drawText(sf::RenderTarget& target, const sf::Font& font, const std::string& utf8,
                      float x, float y, unsigned size, sf::Color color,
                      Align align = Align::Left, bool bold = false) 
{
    sf::Text text;
    text.setFont(font);
    text.setString(U(utf8));
    text.setCharacterSize(size);
    text.setFillColor(color);
    if (bold) text.setStyle(sf::Text::Bold);
    sf::FloatRect b = text.getLocalBounds();
    float ox = b.left;
    if (align == Align::Center) ox = b.left + b.width / 2.f;
    if (align == Align::Right) ox = b.left + b.width;
    text.setOrigin(ox, 0.f);
    text.setPosition(x, y);
    target.draw(text);
    return b.width;
}

inline sf::Color darker(sf::Color c, float k = 0.6f) 
{
    return sf::Color(static_cast<sf::Uint8>(c.r * k), static_cast<sf::Uint8>(c.g * k),
                     static_cast<sf::Uint8>(c.b * k), c.a);
}

constexpr unsigned kWinW = 1280;
constexpr unsigned kWinH = 720;

}  
