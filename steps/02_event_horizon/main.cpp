// Step 2: event horizon and polar coordinates.
// Each ray tracks r and phi around the black hole and stops once r <= r_s.
// G toggles a polar grid, and the mouse shows r and phi.

#include "demo.hpp"

#include <SFML/Graphics.hpp>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <optional>
#include <vector>

namespace {

constexpr float kPi = 3.14159265f;
constexpr float kHorizon = 1.0f;           // event horizon radius, r_s
constexpr float kStep = 0.1f;              // distance a ray moves per update
constexpr int kStepsPerFrame = 2;
constexpr float kEscape = 20.f;            // past every edge of the screen
constexpr std::size_t kTrailLength = 800;

const sf::Color kBackground(6, 7, 12);
const sf::Color kLight(255, 190, 90);

struct Ray {
    sf::Vector2f position;
    sf::Vector2f direction;
    float r = 0.f;    // distance from the black hole
    float phi = 0.f;  // angle from the +x axis, in radians
    std::deque<sf::Vector2f> trail;
    bool active = true;

    Ray(sf::Vector2f position, sf::Vector2f direction) : position(position), direction(direction) {}

    void update() {
        // polar coordinates around the black hole
        r = std::hypot(position.x, position.y);
        phi = std::atan2(position.y, position.x);

        if (r <= kHorizon) {  // inside the event horizon
            active = false;
            return;
        }
        if (r > kEscape) {
            active = false;
            return;
        }

        position += direction * kStep;
        trail.push_back(position);
        if (trail.size() > kTrailLength) {
            trail.pop_front();
        }
    }
};

std::vector<Ray> makeRays() {
    std::vector<Ray> rays;
    for (int i = 0; i <= 64; ++i) {
        const float y = -8.f + 0.25f * static_cast<float>(i);
        rays.emplace_back(sf::Vector2f(-15.f, y), sf::Vector2f(1.f, 0.f));
    }
    return rays;
}

void drawTrail(sf::RenderTarget& target, const std::deque<sf::Vector2f>& trail, sf::Color color) {
    sf::VertexArray line(sf::PrimitiveType::LineStrip, trail.size());
    for (std::size_t i = 0; i < trail.size(); ++i) {
        color.a = static_cast<std::uint8_t>(255.f * static_cast<float>(i + 1) / static_cast<float>(trail.size()));
        line[i] = sf::Vertex{trail[i], color};
    }
    target.draw(line, sf::BlendAdd);
}

void drawHead(sf::RenderTarget& target, sf::Vector2f position) {
    sf::CircleShape dot(0.06f, 16);
    dot.setOrigin({0.06f, 0.06f});
    dot.setPosition(position);
    dot.setFillColor(sf::Color(255, 240, 210));
    target.draw(dot);
}

void drawBlackHole(sf::RenderTarget& target) {
    sf::CircleShape horizon(kHorizon, 128);
    horizon.setOrigin({kHorizon, kHorizon});
    horizon.setFillColor(sf::Color::Black);
    horizon.setOutlineColor(sf::Color(255, 120, 50, 170));
    horizon.setOutlineThickness(0.04f);
    target.draw(horizon);
}

// Polar grid: a circle every 1 r_s and a line every 15 degrees.
void drawPolarGrid(sf::RenderTarget& target) {
    const sf::Color color(120, 140, 255, 30);
    sf::VertexArray lines(sf::PrimitiveType::Lines);
    constexpr int kSegments = 180;
    for (int ring = 1; ring <= 18; ++ring) {
        const float radius = static_cast<float>(ring);
        for (int i = 0; i < kSegments; ++i) {
            const float a0 = 2.f * kPi * static_cast<float>(i) / kSegments;
            const float a1 = 2.f * kPi * static_cast<float>(i + 1) / kSegments;
            lines.append({{radius * std::cos(a0), radius * std::sin(a0)}, color});
            lines.append({{radius * std::cos(a1), radius * std::sin(a1)}, color});
        }
    }
    for (int spoke = 0; spoke < 24; ++spoke) {
        const float a = 2.f * kPi * static_cast<float>(spoke) / 24.f;
        lines.append({{kHorizon * std::cos(a), kHorizon * std::sin(a)}, color});
        lines.append({{18.f * std::cos(a), 18.f * std::sin(a)}, color});
    }
    target.draw(lines);
}

} // namespace

int main(int argc, char* argv[]) {
    const demo::Options options = demo::parseOptions(argc, argv);
    sf::RenderWindow window;
    demo::openWindow(window, "Step 2 - The event horizon", options);
    window.setView(demo::worldView());
    const demo::Caption caption;

    std::vector<Ray> rays = makeRays();
    bool paused = false;
    bool showGrid = true;

    for (int frame = 0; window.isOpen(); ++frame) {
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            } else if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                if (key->code == sf::Keyboard::Key::Escape) window.close();
                if (key->code == sf::Keyboard::Key::Space) paused = !paused;
                if (key->code == sf::Keyboard::Key::R) rays = makeRays();
                if (key->code == sf::Keyboard::Key::G) showGrid = !showGrid;
            }
        }
        if (!window.isOpen()) {
            break;
        }

        if (!paused) {
            for (int s = 0; s < kStepsPerFrame; ++s) {
                for (Ray& ray : rays) {
                    if (ray.active) ray.update();
                }
            }
        }

        window.clear(kBackground);
        if (showGrid) {
            drawPolarGrid(window);
        }
        for (const Ray& ray : rays) {
            drawTrail(window, ray.trail, kLight);
            if (ray.active) drawHead(window, ray.position);
        }
        drawBlackHole(window);  // drawn last, it hides the last step inside the horizon

        // r and phi under the mouse
        const sf::Vector2i mousePixel = sf::Mouse::getPosition(window);
        const sf::Vector2u size = window.getSize();
        if (mousePixel.x >= 0 && mousePixel.y >= 0 && mousePixel.x < int(size.x) && mousePixel.y < int(size.y)) {
            const sf::Vector2f mouse = window.mapPixelToCoords(mousePixel);
            char readout[64];
            std::snprintf(readout, sizeof readout, "r = %.2f r_s   phi = %.0f deg",
                          std::hypot(mouse.x, mouse.y), std::atan2(mouse.y, mouse.x) * 180.f / kPi);
            caption.label(window, mouse, readout);
        }

        caption.draw(window, "Step 2 - The event horizon",
                     "Rays that reach r = r_s are gone for good.     G polar grid   Space pause   R restart   Esc quit");
        demo::present(window, options, frame);
    }
    return 0;
}
