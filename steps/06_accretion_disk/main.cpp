// Step 6 - The accretion disk
//
// A thin disk of hot gas orbits the black hole in the y = 0 plane, from the
// innermost stable circular orbit (3 r_s) out to 12 r_s. Rays from step 5 now
// check every step whether they crossed that plane, and if so pick up the light
// the gas gives off there.
//
// What the gas looks like depends on three things, all in blackhole.frag:
//   - its temperature, from the standard thin-disk model (hottest near 4 r_s),
//   - Doppler shift: it orbits at up to half the speed of light, so the side
//     coming towards you is bluer and much brighter than the side going away,
//   - gravitational redshift: light climbing out of the hole loses energy.
// Press D to switch between them and see what each one does.
//
// New compared with step 5: time and colour-mode uniforms, D and Space keys, and a
// camera that starts further out and just above the disk.
//
// Command line: --distance D, --grid, --spin, --mode N (0, 1 or 2), plus demo.hpp's
// --shot, --record and --clean. For clips:
//   --tour          the camera follows a 14-second scripted move (loops in the window)
//   --video folder  renders the tour at 1920x1080, 30 fps, into folder/frame_0000.jpg ...
//   --no-titles     leaves the titles out of the video

#include "demo.hpp"

#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

namespace {

constexpr float kFovY = 60.f * 3.14159265f / 180.f;
const std::string kShaderPath = std::string(STEP_DIR) + "/blackhole.frag";

// How the disk's colour is worked out (the u_shiftMode uniform).
const char* const kModeNames[] = {
    "plain colours (no relativity)",
    "+ gravitational redshift",
    "+ gravitational redshift + Doppler",
};

struct OrbitCamera {
    float distance = 20.f;
    float azimuth = 0.f;
    float elevation = 0.1f;  // just above the disk, so it's seen nearly edge-on

    sf::Vector3f position() const {
        return {distance * std::cos(elevation) * std::cos(azimuth),
                distance * std::sin(elevation),
                distance * std::cos(elevation) * std::sin(azimuth)};
    }
};

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

constexpr float kTourSeconds = 14.f;
constexpr int kVideoFps = 30;
const sf::Vector2u kVideoSize{1920, 1080};

// The scripted camera move for clips: glide in from far away while orbiting a little
// and sinking towards the disk plane, so its far side rises up over the shadow.
void tourCamera(OrbitCamera& camera, float seconds) {
    const float u = std::clamp(seconds / kTourSeconds, 0.f, 1.f);
    const float eased = u * u * (3.f - 2.f * u);  // smoothstep: starts and stops gently
    camera.distance = 30.f + (13.f - 30.f) * eased;
    camera.azimuth = -0.5f + 0.8f * u;
    camera.elevation = 0.32f + (0.06f - 0.32f) * eased;
}

// Titles burned into the video (LinkedIn plays videos muted), over a dark band so
// they stay readable on top of the bright disk. They fade in during the second second.
void drawVideoTitles(sf::RenderTarget& target, const sf::Font& font, float seconds) {
    const sf::Vector2f size(target.getSize());
    const float fade = std::clamp(seconds - 1.f, 0.f, 1.f);
    const auto alpha = [fade](float full) { return static_cast<std::uint8_t>(full * fade); };

    sf::VertexArray band(sf::PrimitiveType::TriangleStrip, 4);
    band[0] = sf::Vertex{{0.f, size.y - 300.f}, sf::Color(0, 0, 0, 0)};
    band[1] = sf::Vertex{{size.x, size.y - 300.f}, sf::Color(0, 0, 0, 0)};
    band[2] = sf::Vertex{{0.f, size.y}, sf::Color(0, 0, 0, alpha(170.f))};
    band[3] = sf::Vertex{{size.x, size.y}, sf::Color(0, 0, 0, alpha(170.f))};
    target.draw(band);

    const float margin = 64.f;
    sf::Text title(font, "Ray traced in real time on the GPU", 46);
    title.setFillColor(sf::Color(240, 240, 245, alpha(255.f)));
    title.setPosition({margin, size.y - margin - 112.f});
    target.draw(title);

    sf::Text subtitle(font, "A Schwarzschild black hole and its accretion disk, with Doppler shift and gravitational redshift", 24);
    subtitle.setFillColor(sf::Color(185, 187, 200, alpha(255.f)));
    subtitle.setPosition({margin, size.y - margin - 44.f});
    target.draw(subtitle);

    sf::Text link(font, "github.com/abzakir/blackhole", 24);
    link.setFillColor(sf::Color(242, 181, 107, alpha(255.f)));
    link.setPosition({size.x - margin - link.getLocalBounds().size.x, size.y - margin - 44.f});
    target.draw(link);
}

} // namespace

int main(int argc, char* argv[]) {
    const demo::Options options = demo::parseOptions(argc, argv);
    OrbitCamera camera;
    bool showGrid = false;
    bool spin = false;
    bool tour = false;
    bool showTitles = true;
    std::optional<std::string> videoDir;
    int shiftMode = 2;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--distance" && i + 1 < argc) {
            camera.distance = static_cast<float>(std::atof(argv[++i]));
        } else if (arg == "--mode" && i + 1 < argc) {
            shiftMode = std::clamp(std::atoi(argv[++i]), 0, 2);
        } else if (arg == "--grid") {
            showGrid = true;
        } else if (arg == "--spin") {
            spin = true;
        } else if (arg == "--tour") {
            tour = true;
        } else if (arg == "--video" && i + 1 < argc) {
            videoDir = argv[++i];
        } else if (arg == "--no-titles") {
            showTitles = false;
        }
    }

    sf::RenderWindow window;
    demo::openWindow(window, "Step 6 - The accretion disk", options);
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

    // Video mode renders into an off-screen texture: it can be bigger than the screen,
    // and every frame is saved from it while the window shows a scaled-down preview.
    std::optional<sf::RenderTexture> video;
    sf::Font titleFont;
    if (videoDir) {
        tour = true;
        demo::hideCaptions = true;
        std::filesystem::create_directories(*videoDir);
        video.emplace(kVideoSize);
        showTitles = showTitles && titleFont.openFromFile("C:/Windows/Fonts/segoeui.ttf");
    }

    bool paused = false;
    bool dragging = false;
    sf::Vector2i lastMouse;
    float simTime = 0.f;  // drives the disk's rotation
    sf::Clock frameClock;
    sf::Clock fpsClock;
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
                if (key->code == sf::Keyboard::Key::D) shiftMode = (shiftMode + 1) % 3;
                if (key->code == sf::Keyboard::Key::Space) paused = !paused;
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

        // Recordings advance a fixed time per frame, so they play back at the real speed.
        if (video) {
            if (frame >= static_cast<int>(kTourSeconds) * kVideoFps) {
                std::printf("Recorded %d frames to %s\n", frame, videoDir->c_str());
                break;
            }
            simTime = static_cast<float>(frame) / kVideoFps;
        } else {
            const float dt = options.offline() ? 1.f / 60.f : frameClock.restart().asSeconds();
            if (!paused) {
                simTime += dt;
            }
        }
        if (spin) {
            camera.azimuth += 0.006f;
        }
        if (tour) {
            tourCamera(camera, video ? simTime : std::fmod(simTime, kTourSeconds));
        }

        sf::RenderTarget& target = video ? static_cast<sf::RenderTarget&>(*video) : window;
        const sf::Vector2u targetSize = target.getSize();
        setCameraUniforms(shader, camera, targetSize);
        shader.setUniform("u_showGrid", showGrid);
        shader.setUniform("u_shiftMode", shiftMode);
        shader.setUniform("u_time", simTime);

        target.clear();
        target.draw(sf::RectangleShape(sf::Vector2f(targetSize)), &shader);

        if (video) {
            if (showTitles) {
                drawVideoTitles(*video, titleFont, simTime);
            }
            video->display();
            char name[32];
            std::snprintf(name, sizeof name, "/frame_%04d.jpg", frame);
            if (!video->getTexture().copyToImage().saveToFile(*videoDir + name)) {
                return 1;
            }
            window.clear();
            sf::Sprite preview(video->getTexture());
            preview.setScale({static_cast<float>(window.getSize().x) / static_cast<float>(targetSize.x),
                              static_cast<float>(window.getSize().y) / static_cast<float>(targetSize.y)});
            window.draw(preview);
            window.display();
            continue;
        }

        ++fpsFrames;
        if (fpsClock.getElapsedTime() >= sf::seconds(0.5f)) {
            fps = static_cast<float>(fpsFrames) / fpsClock.restart().asSeconds();
            fpsFrames = 0;
        }
        char status[128];
        std::snprintf(status, sizeof status, "D: %s   camera at r = %.1f r_s   %.0f fps",
                      kModeNames[shiftMode], camera.distance, fps);
        caption.draw(window, "Step 6 - The accretion disk",
                     std::string(status) + "     Drag orbit   Scroll zoom   Space pause   G grid   F5 reload   Esc quit");
        demo::present(window, options, frame);
        if (!window.isOpen()) {
            std::printf("Rendered %d frames in %.2f s (%.0f fps on average)\n", frame + 1,
                        runClock.getElapsedTime().asSeconds(), (frame + 1) / runClock.getElapsedTime().asSeconds());
        }
    }
    return 0;
}
