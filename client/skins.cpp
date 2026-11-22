#include "skins.hpp"

#include <cstdint>

namespace agar {

namespace {

const sf::Color kColors[kSkinCount] = {
    sf::Color(230, 70, 70),   sf::Color(240, 150, 50), sf::Color(240, 220, 60),
    sf::Color(80, 200, 90),   sf::Color(60, 200, 210), sf::Color(70, 110, 240),
    sf::Color(160, 90, 220),  sf::Color(240, 120, 180),
    // представители текстур
    sf::Color(230, 90, 90),   sf::Color(90, 190, 90),  sf::Color(90, 120, 220)};

sf::Texture makeSkinTexture(int index) {
    const unsigned N = 64;
    sf::Image img;
    img.create(N, N, sf::Color::White);
    for (unsigned y = 0; y < N; ++y) {
        for (unsigned x = 0; x < N; ++x) {
            sf::Color c;
            if (index == 0) {  // диагональные полосы
                c = ((x + y) / 8) % 2 == 0 ? sf::Color(230, 80, 80) : sf::Color(250, 235, 235);
            } else if (index == 1) {  // горошек
                float dx = static_cast<float>(x % 16) - 8.f, dy = static_cast<float>(y % 16) - 8.f;
                c = dx * dx + dy * dy < 14.f ? sf::Color(250, 220, 70) : sf::Color(70, 170, 90);
            } else {  // шашечки
                c = ((x / 16) + (y / 16)) % 2 == 0 ? sf::Color(70, 110, 220) : sf::Color(235, 240, 250);
            }
            img.setPixel(x, y, c);
        }
    }
    sf::Texture t;
    t.loadFromImage(img);
    t.setSmooth(true);
    return t;
}

}  // namespace

Skins::Skins() {
    for (std::size_t i = 0; i < textures_.size(); ++i) textures_[i] = makeSkinTexture(static_cast<int>(i));
}

void Skins::apply(sf::CircleShape& shape, int skin) const {
    if (skin < 0 || skin >= kSkinCount) skin = 0;
    if (skin < kColorSkins) {
        shape.setTexture(nullptr);
        shape.setFillColor(kColors[skin]);
    } else {
        shape.setTexture(&textures_[static_cast<std::size_t>(skin - kColorSkins)]);
        shape.setFillColor(sf::Color::White);
    }
}

sf::Color Skins::colorOf(int skin) const {
    if (skin < 0 || skin >= kSkinCount) skin = 0;
    return kColors[skin];
}

sf::Texture makeRoadTexture(const std::string& assetDir) {
    sf::Texture tex;
    if (tex.loadFromFile(assetDir + "/road.png") || tex.loadFromFile("assets/road.png")) {
        tex.setRepeated(true);
        tex.setSmooth(true);
        return tex;
    }

    // Асфальт с шумом и разметкой: пунктир по центру и крест из дорог.
    const unsigned N = 256;
    sf::Image img;
    img.create(N, N, sf::Color(58, 60, 64));
    std::uint32_t seed = 12345u;
    for (unsigned y = 0; y < N; ++y) {
        for (unsigned x = 0; x < N; ++x) {
            seed = seed * 1664525u + 1013904223u;
            int n = static_cast<int>((seed >> 24) % 17) - 8;
            int v = 60 + n;
            img.setPixel(x, y, sf::Color(static_cast<sf::Uint8>(v), static_cast<sf::Uint8>(v + 2),
                                         static_cast<sf::Uint8>(v + 6)));
        }
    }
    for (unsigned i = 0; i < N; ++i) {
        bool dash = (i / 24) % 2 == 0;
        if (!dash) continue;
        for (unsigned w = 0; w < 4; ++w) {
            img.setPixel(N / 2 - 2 + w, i, sf::Color(235, 200, 60));  // вертикальная разметка
            img.setPixel(i, N / 2 - 2 + w, sf::Color(235, 200, 60));  // горизонтальная разметка
        }
    }
    tex.loadFromImage(img);
    tex.setRepeated(true);
    tex.setSmooth(true);
    return tex;
}

}  // namespace agar
