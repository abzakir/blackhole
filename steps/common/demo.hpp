// Shared helpers for all steps: window, captions, screenshots and frame recording.
#pragma once

#include <SFML/Graphics.hpp>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

namespace demo {

// Command-line options:
//   --shot file.png   save one frame and quit
//   --record folder   save frames as folder/frame_0000.png, ...
//   --frame N         frame to save, or last frame to record (default 300)
//   --every K         with --record, save every K-th frame (default 2)
//   --clean           hide the captions

inline bool hideCaptions = false;  // set by --clean

struct Options {
    std::optional<std::string> shotPath;
    std::optional<std::string> recordDir;
    int shotFrame = 300;
    int every = 2;

    bool offline() const { return shotPath || recordDir; }
};

inline Options parseOptions(int argc, char* argv[]) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--clean") {
            hideCaptions = true;
        }
    }
    for (int i = 1; i + 1 < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--shot") {
            options.shotPath = argv[++i];
        } else if (arg == "--record") {
            options.recordDir = argv[++i];
        } else if (arg == "--frame") {
            options.shotFrame = std::atoi(argv[++i]);
        } else if (arg == "--every") {
            options.every = std::max(1, std::atoi(argv[++i]));
        }
    }
    if (options.recordDir) {
        std::filesystem::create_directories(*options.recordDir);
    }
    return options;
}

// 1280x720 window with anti-aliasing. No frame limit while saving frames.
inline void openWindow(sf::RenderWindow& window, const std::string& title, const Options& options) {
    sf::ContextSettings settings;
    settings.antiAliasingLevel = 8;
    window.create(sf::VideoMode({1280, 720}), title, sf::Style::Titlebar | sf::Style::Close,
                  sf::State::Windowed, settings);
    window.setFramerateLimit(options.offline() ? 0 : 60);
}

// Call before display(), the back buffer is undefined after it.
inline bool saveFrame(const sf::RenderWindow& window, const std::string& path) {
    sf::Texture texture(window.getSize());
    texture.update(window);
    return texture.copyToImage().saveToFile(path);
}

// 32 x 18 r_s centred on the black hole. The negative height makes +y point up.
inline sf::View worldView() {
    return sf::View({0.f, 0.f}, {32.f, -18.f});
}

// Saves the frame if asked to, shows it, and closes the window after the last saved frame.
inline void present(sf::RenderWindow& window, const Options& options, int frame) {
    if (options.shotPath && frame == options.shotFrame && saveFrame(window, *options.shotPath)) {
        std::cout << "Saved " << *options.shotPath << '\n';
    }
    if (options.recordDir && frame % options.every == 0) {
        char name[32];
        std::snprintf(name, sizeof name, "/frame_%04d.png", frame / options.every);
        saveFrame(window, *options.recordDir + name);
    }
    window.display();
    if (options.offline() && frame >= options.shotFrame) {
        if (options.recordDir) {
            std::cout << "Recorded " << options.shotFrame / options.every + 1 << " frames to "
                      << *options.recordDir << '\n';
        }
        window.close();
    }
}

// Title, hint line and small labels, drawn in screen space.
class Caption {
public:
    Caption() {
        for (const char* path : {"C:/Windows/Fonts/segoeui.ttf", "C:/Windows/Fonts/arial.ttf"}) {
            if (font_.openFromFile(path)) {
                loaded_ = true;
                return;
            }
        }
        std::cerr << "No font found, captions are disabled.\n";
    }

    void draw(sf::RenderWindow& window, const std::string& title, const std::string& hints) const {
        text(window, title, {20.f, 14.f}, 24, sf::Color(235, 235, 240));
        text(window, hints, {20.f, 48.f}, 16, sf::Color(130, 132, 150));
    }

    // Small label next to a point in world coordinates.
    void label(sf::RenderWindow& window, sf::Vector2f world, const std::string& string) const {
        const sf::Vector2f pixel(window.mapCoordsToPixel(world));
        text(window, string, pixel + sf::Vector2f(8.f, -22.f), 14, sf::Color(160, 162, 180));
    }

private:
    void text(sf::RenderWindow& window, const std::string& string, sf::Vector2f pixel,
              unsigned size, sf::Color color) const {
        if (!loaded_ || hideCaptions) {
            return;
        }
        const sf::View world = window.getView();
        window.setView(window.getDefaultView());
        sf::Text text(font_, string, size);
        text.setPosition(pixel);
        text.setFillColor(color);
        window.draw(text);
        window.setView(world);
    }

    sf::Font font_;
    bool loaded_ = false;
};

} // namespace demo
