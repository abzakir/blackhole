// Step 5: the black hole in 3D, on the GPU.
// blackhole.frag traces one light ray per pixel. This file sets up the window, the
// orbit camera and the shader uniforms.
// Options: --distance D (in r_s), --stars, --spin, plus demo.hpp's options.

#include "demo.hpp"

#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

namespace {

constexpr float kFovY = 60.f * 3.14159265f / 180.f;  // vertical field of view
const std::string kShaderPath = std::string(STEP_DIR) + "/blackhole.frag";

// Orbits the origin and always looks at it. y is up.
struct OrbitCamera {
    float distance = 12.f;    // from the centre, in r_s
    float azimuth = 0.f;      // angle around the y axis, radians
    float elevation = 0.15f;  // angle above the y = 0 plane, radians

    sf::Vector3f position() const {
        return {distance * std::cos(elevation) * std::cos(azimuth),
                distance * std::sin(elevation),
                distance * std::cos(elevation) * std::sin(azimuth)};
    }
};

// Camera position and axes for the shader.
void setCameraUniforms(sf::Shader& shader, const OrbitCamera& camera, sf::Vector2u windowSize) {
    const sf::Vector3f position = camera.position();
    const sf::Vector3f forward = (-position).normalized();
    const sf::Vector3f right = forward.cross({0.f, 1.f, 0.f}).normalized();
    const sf::Vector3f up = right.cross(forward);

    shader.setUniform("u_resolution", sf::Vector2f(windowSize));
    shader.setUniform("u_camPos", position);
    shader.setUniform("u_camRight", right);
    shader.setUniform("u_camUp", up);
    shader.setUniform("u_camForward", forward);
    shader.setUniform("u_tanHalfFov", std::tan(kFovY / 2.f));
}

bool loadShader(sf::Shader& shader) {
    if (!shader.loadFromFile(kShaderPath, sf::Shader::Type::Fragment)) {
        std::cerr << "Could not compile " << kShaderPath << " (the error is printed above)\n";
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char* argv[]) {
    const demo::Options options = demo::parseOptions(argc, argv);
    OrbitCamera camera;
    bool showGrid = true;
    bool spin = false;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--distance" && i + 1 < argc) {
            camera.distance = static_cast<float>(std::atof(argv[++i]));
        } else if (arg == "--stars") {
            showGrid = false;
        } else if (arg == "--spin") {
            spin = true;
        }
    }

    sf::RenderWindow window;
    demo::openWindow(window, "Step 5 - The black hole in 3D", options);
    const demo::Caption caption;

    std::cout << "GPU: " << reinterpret_cast<const char*>(glGetString(GL_RENDERER)) << '\n';
    if (!sf::Shader::isAvailable()) {
        std::cerr << "This GPU/driver does not support shaders.\n";
        return 1;
    }
    sf::Shader shader;
    if (!loadShader(shader)) {
        return 1;
    }

    // full-window rectangle, drawn with the shader
    const sf::RectangleShape screen(sf::Vector2f(window.getSize()));

    bool dragging = false;
    sf::Vector2i lastMouse;
    sf::Clock fpsClock;   // fps over half-second windows
    int fpsFrames = 0;
    float fps = 0.f;
    const sf::Clock runClock;

    for (int frame = 0; window.isOpen(); ++frame) {
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            } else if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                if (key->code == sf::Keyboard::Key::Escape) window.close();
                if (key->code == sf::Keyboard::Key::G) showGrid = !showGrid;
                if (key->code == sf::Keyboard::Key::F5 && loadShader(shader)) {
                    std::cout << "Reloaded " << kShaderPath << '\n';
                }
            } else if (const auto* press = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (press->button == sf::Mouse::Button::Left) {
                    dragging = true;
                    lastMouse = press->position;
                }
            } else if (const auto* release = event->getIf<sf::Event::MouseButtonReleased>()) {
                if (release->button == sf::Mouse::Button::Left) dragging = false;
            } else if (const auto* move = event->getIf<sf::Event::MouseMoved>()) {
                if (dragging) {
                    const sf::Vector2i delta = move->position - lastMouse;
                    lastMouse = move->position;
                    camera.azimuth += 0.005f * static_cast<float>(delta.x);
                    camera.elevation = std::clamp(camera.elevation + 0.005f * static_cast<float>(delta.y), -1.5f, 1.5f);
                }
            } else if (const auto* wheel = event->getIf<sf::Event::MouseWheelScrolled>()) {
                camera.distance = std::clamp(camera.distance * std::pow(0.9f, wheel->delta), 2.5f, 80.f);
            }
        }
        if (!window.isOpen()) {
            break;
        }

        if (spin) {
            camera.azimuth += 0.006f;
        }
        setCameraUniforms(shader, camera, window.getSize());
        shader.setUniform("u_showGrid", showGrid);

        window.clear();
        window.draw(screen, &shader);

        ++fpsFrames;
        if (fpsClock.getElapsedTime() >= sf::seconds(0.5f)) {
            fps = static_cast<float>(fpsFrames) / fpsClock.restart().asSeconds();
            fpsFrames = 0;
        }
        char status[96];
        std::snprintf(status, sizeof status, "camera at r = %.1f r_s   %.0f fps", camera.distance, fps);
        caption.draw(window, "Step 5 - The black hole in 3D (GPU ray tracing)",
                     std::string(status) + "     Drag orbit   Scroll zoom   G sky grid / stars   F5 reload shader   Esc quit");
        demo::present(window, options, frame);
        if (!window.isOpen()) {
            std::printf("Rendered %d frames in %.2f s (%.0f fps on average)\n", frame + 1,
                        runClock.getElapsedTime().asSeconds(), (frame + 1) / runClock.getElapsedTime().asSeconds());
        }
    }
    return 0;
}
