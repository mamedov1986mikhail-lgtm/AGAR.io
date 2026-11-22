// Внешний вид капель: 8 цветов и 3 процедурные текстуры; текстура игрового поля "дорога".
#pragma once

#include <SFML/Graphics.hpp>

#include <array>

#include "protocol.hpp"

namespace agar {

class Skins {
public:
    Skins();

    // Применяет цвет или текстуру номер skin (0..kSkinCount-1) к фигуре.
    void apply(sf::CircleShape& shape, int skin) const;
    // Характерный цвет скина (для списка лидеров и контуров).
    sf::Color colorOf(int skin) const;

private:
    std::array<sf::Texture, kSkinCount - kColorSkins> textures_;
};

// Текстура дороги: assets/road.png, если файл есть, иначе рисуется программно.
sf::Texture makeRoadTexture(const std::string& assetDir);

}  // namespace agar
