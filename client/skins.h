#pragma once
#include <SFML/Graphics.hpp>
#include <array>
#include "protocol.h"

namespace agar 
{
class Skins 
{
public:
    Skins();

    void apply(sf::CircleShape& shape, int skin) const;
    sf::Color colorOf(int skin) const;

private:
    std::array<sf::Texture, kSkinCount - kColorSkins> textures_;
};

sf::Texture makeRoadTexture(const std::string& assetDir);

}  
