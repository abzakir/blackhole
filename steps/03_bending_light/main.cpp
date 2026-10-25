// Step 3: light bends around the black hole.
// Rays follow the Schwarzschild geodesic equations, moved with Euler's method.
// The faint ring is the photon sphere at 1.5 r_s.

#include "demo.hpp"

#include <SFML/Graphics.hpp>

#include <cmath>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <vector>

namespace {

constexpr double kRs = 1.0;                // Schwarzschild radius (event horizon)
constexpr double kStep = 0.1;              // step size in lambda
constexpr int kStepsPerFrame = 2;
constexpr double kEscape = 20.0;           // past every edge of the screen
constexpr std::size_t kTrailLength = 800;

const sf::Color kBackground(6, 7, 12);
const sf::Color kLight(255, 190, 90);

// Ray state in polar coordinates. geodesic() returns its derivative in the same struct.
struct State {
    double r, phi;    // position
    double dr, dphi;  // velocity
};

// Geodesic equations for light in the equatorial plane. E is the ray's energy (constant).
State geodesic(const State& s, double E) {
    const double r = s.r;
    const double f = 1.0 - kRs / r;  // 1 far away, 0 at the horizon
    const double dtdl = E / f;       // dt/dlambda
    State d;
    d.r = s.dr;
    d.phi = s.dphi;
    d.dr = -(kRs / (2.0 * r * r)) * f * dtdl * dtdl     // gravity
         + (kRs / (2.0 * r * r * f)) * s.dr * s.dr      // radial term
         + (r - kRs) * s.dphi * s.dphi;                 // sideways motion
    d.dphi = -2.0 * s.dr * s.dphi / r;
    return d;
}

// Euler step: use the slope at the start of the step.
void eulerStep(State& s, double E, double h) {
    const State d = geodesic(s, E);
    s.r += d.r * h;
    s.phi += d.phi * h;
    s.dr += d.dr * h;
    s.dphi += d.dphi * h;
}

struct Ray {
    State state{};
    double energy = 0.0;
    std::deque<sf::Vector2f> trail;
    bool active = true;
    bool fellIn = false;

    // Starts at a point with a unit direction, both in x, y.
    Ray(sf::Vector2f position, sf::Vector2f direction) {
        const double x = position.x, y = position.y;
        state.r = std::hypot(x, y);
        state.phi = std::atan2(y, x);
        // radial and angular velocity
        state.dr = (x * direction.x + y * direction.y) / state.r;
        state.dphi = (x * direction.y - y * direction.x) / (state.r * state.r);
        // energy from the null condition ds^2 = 0
        const double f = 1.0 - kRs / state.r;
        energy = std::sqrt(state.dr * state.dr + f * state.r * state.r * state.dphi * state.dphi);
        trail.push_back(position);
    }

    void update() {
        // The equations divide by zero at the horizon, so stop a ray that would reach it
        // this step. It can't escape from there anyway.
        if (state.r + state.dr * kStep <= kRs) {
            fallIn();
        } else {
            eulerStep(state, energy, kStep);
            if (!(state.r > kRs)) {  // also catches NaN
                fallIn();
            } else if (state.r > kEscape && state.dr > 0.0) {  // escaped
                active = false;
            }
        }
        trail.push_back(position());
        if (trail.size() > kTrailLength) {
            trail.pop_front();
        }
    }

    void fallIn() {
        state.r = kRs;  // end the trail on the horizon
        active = false;
        fellIn = true;
    }

    sf::Vector2f position() const {
        return {static_cast<float>(state.r * std::cos(state.phi)),
                static_cast<float>(state.r * std::sin(state.phi))};
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
    const float rs = static_cast<float>(kRs);
    sf::CircleShape horizon(rs, 128);
    horizon.setOrigin({rs, rs});
    horizon.setFillColor(sf::Color::Black);
    horizon.setOutlineColor(sf::Color(255, 120, 50, 170));
    horizon.setOutlineThickness(0.04f);
    target.draw(horizon);
}

// Photon sphere at 1.5 r_s.
void drawPhotonSphere(sf::RenderTarget& target) {
    const float radius = 1.5f * static_cast<float>(kRs);
    sf::CircleShape ring(radius, 128);
    ring.setOrigin({radius, radius});
    ring.setFillColor(sf::Color::Transparent);
    ring.setOutlineColor(sf::Color(255, 255, 255, 45));
    ring.setOutlineThickness(0.02f);
    target.draw(ring);
}

} // namespace

int main(int argc, char* argv[]) {
    const demo::Options options = demo::parseOptions(argc, argv);
    sf::RenderWindow window;
    demo::openWindow(window, "Step 3 - Bending light (Euler)", options);
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

        int fellIn = 0;
        window.clear(kBackground);
        drawPhotonSphere(window);
        for (const Ray& ray : rays) {
            drawTrail(window, ray.trail, kLight);
            if (ray.active) drawHead(window, ray.position());
            fellIn += ray.fellIn ? 1 : 0;
        }
        drawBlackHole(window);
        caption.label(window, {1.06f, 1.06f}, "photon sphere, 1.5 r_s");

        caption.draw(window, "Step 3 - Bending light (Euler's method)",
                     "Fell in: " + std::to_string(fellIn) + " of " + std::to_string(rays.size()) +
                     "     Space pause   R restart   Esc quit");
        demo::present(window, options, frame);
    }
    return 0;
}
