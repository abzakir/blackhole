// Step 1 - Straight light rays
//
// No physics yet. A column of light rays flies left to right in straight lines,
// leaving fading trails, and passes right through the black hole. This step sets
// up what every later step reuses: the window, the main loop, and drawing trails
// with a vertex array.
//
// Units: 1 world unit = 1 Schwarzschild radius (r_s), and light moves at c = 1.

#include "demo.hpp"

#include <SFML/Graphics.hpp>

#include <cstdint>
#include <deque>
#include <optional>
#include <vector>

namespace {

constexpr float kHorizon = 1.0f;           // event horizon radius, r_s
constexpr float kStep = 0.1f;              // distance a ray moves per update
constexpr int kStepsPerFrame = 2;
constexpr std::size_t kTrailLength = 800;  // points kept per trail

const sf::Color kBackground(6, 7, 12);
const sf::Color kLight(255, 190, 90);

struct Ray {
    sf::Vector2f position;
    sf::Vector2f direction;  // unit length, so the ray moves at c = 1
    std::deque<sf::Vector2f> trail;
    bool active = true;

    Ray(sf::Vector2f position, sf::Vector2f direction) : position(position), direction(direction) {}

    void update() {
        position += direction * kStep;
        trail.push_back(position);
        if (trail.size() > kTrailLength) {
            trail.pop_front();
        }
        if (position.x > 17.f) {  // past the right edge of the screen
            active = false;
        }
    }
};

// A column of parallel rays at the left edge, all heading right.
std::vector<Ray> makeRays() {
    std::vector<Ray> rays;
    for (int i = 0; i <= 64; ++i) {
        const float y = -8.f + 0.25f * static_cast<float>(i);
        rays.emplace_back(sf::Vector2f(-15.f, y), sf::Vector2f(1.f, 0.f));
    }
    return rays;
}

// Draws a trail as one connected line that fades from invisible at the tail to
// full brightness at the head.
void drawTrail(sf::RenderTarget& target, const std::deque<sf::Vector2f>& trail, sf::Color color) {
    sf::VertexArray line(sf::PrimitiveType::LineStrip, trail.size());
    for (std::size_t i = 0; i < trail.size(); ++i) {
        color.a = static_cast<std::uint8_t>(255.f * static_cast<float>(i + 1) / static_cast<float>(trail.size()));
        line[i] = sf::Vertex{trail[i], color};
    }
    target.draw(line, sf::BlendAdd);  // additive blending: overlapping light adds up and glows
}

void drawHead(sf::RenderTarget& target, sf::Vector2f position) {
    sf::CircleShape dot(0.06f, 16);
    dot.setOrigin({0.06f, 0.06f});
    dot.setPosition(position);
    dot.setFillColor(sf::Color(255, 240, 210));
    target.draw(dot);
}

// SFML has a ready-made circle, so unlike raw OpenGL there is no circle maths to write.
void drawBlackHole(sf::RenderTarget& target) {
    sf::CircleShape horizon(kHorizon, 128);
    horizon.setOrigin({kHorizon, kHorizon});
    horizon.setFillColor(sf::Color::Black);
    horizon.setOutlineColor(sf::Color(255, 120, 50, 170));
    horizon.setOutlineThickness(0.04f);
    target.draw(horizon);
}

} // namespace

int main(int argc, char* argv[]) {
    const demo::Options options = demo::parseOptions(argc, argv);
    sf::RenderWindow window;
    demo::openWindow(window, "Step 1 - Straight light rays", options);
    window.setView(demo::worldView());
    const demo::Caption caption;

    std::vector<Ray> rays = makeRays();
    bool paused = false;

    for (int frame = 0; window.isOpen(); ++frame) {
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            } else if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                if (key->code == sf::Keyboard::Key::Escape) window.close();
                if (key->code == sf::Keyboard::Key::Space) paused = !paused;
                if (key->code == sf::Keyboard::Key::R) rays = makeRays();
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
        drawBlackHole(window);  // drawn first, so the rays visibly cross it
        for (const Ray& ray : rays) {
            drawTrail(window, ray.trail, kLight);
            if (ray.active) drawHead(window, ray.position);
        }
        caption.draw(window, "Step 1 - Straight light rays",
                     "No gravity yet: light flies straight through the black hole.     Space pause   R restart   Esc quit");
        demo::present(window, options, frame);
    }
    return 0;
}
