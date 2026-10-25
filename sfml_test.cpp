// Checks that SFML, OpenGL and shaders work.
// --auto saves a screenshot after 2 seconds and quits.

#include <SFML/Graphics.hpp>

#include <iostream>
#include <optional>
#include <string_view>

namespace {

constexpr std::string_view kRingShader = R"(
#version 120
uniform vec2 u_resolution;
uniform float u_time;

void main() {
    // centred coordinates, screen height = 1
    vec2 uv = (gl_FragCoord.xy - 0.5 * u_resolution) / u_resolution.y;
    float r = length(uv);

    // soft ring at r = 0.22 that pulses
    float ring = exp(-pow((r - 0.22) * 18.0, 2.0));
    float pulse = 0.85 + 0.15 * sin(u_time * 2.0);
    vec3 color = vec3(1.0, 0.55, 0.15) * ring * pulse;

    gl_FragColor = vec4(color, 1.0);
}
)";

} // namespace

int main(int argc, char* argv[]) {
    const bool autoClose = argc > 1 && std::string_view(argv[1]) == "--auto";

    sf::RenderWindow window(sf::VideoMode({800, 600}), "SFML test");
    window.setFramerateLimit(60);

    const sf::ContextSettings settings = window.getSettings();
    std::cout << "SFML " << SFML_VERSION_MAJOR << '.' << SFML_VERSION_MINOR << '.' << SFML_VERSION_PATCH
              << ", OpenGL " << settings.majorVersion << '.' << settings.minorVersion << '\n';

    if (!sf::Shader::isAvailable()) {
        std::cerr << "Shaders are not available on this GPU/driver.\n";
        return 1;
    }

    sf::Shader shader;
    if (!shader.loadFromMemory(kRingShader, sf::Shader::Type::Fragment)) {
        std::cerr << "Failed to compile the fragment shader.\n";
        return 1;
    }
    std::cout << "Fragment shader compiled OK\n";

    // full-window rectangle drawn with the shader
    const sf::Vector2f size(window.getSize());
    sf::RectangleShape screen(size);

    // black circle on top
    const float radius = 0.18f * size.y;
    sf::CircleShape horizon(radius, 128);
    horizon.setOrigin({radius, radius});
    horizon.setPosition(size / 2.f);
    horizon.setFillColor(sf::Color::Black);

    sf::Clock clock;
    while (window.isOpen()) {
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }
        }
        if (!window.isOpen()) {
            break;
        }

        shader.setUniform("u_resolution", size);
        shader.setUniform("u_time", clock.getElapsedTime().asSeconds());

        window.clear();
        window.draw(screen, &shader);
        window.draw(horizon);

        // capture before display()
        const bool done = autoClose && clock.getElapsedTime() > sf::seconds(2);
        if (done) {
            sf::Texture capture(window.getSize());
            capture.update(window);
            if (capture.copyToImage().saveToFile("sfml_test.png")) {
                std::cout << "Saved sfml_test.png\n";
            }
        }

        window.display();

        if (done) {
            window.close();
        }
    }
    return 0;
}
