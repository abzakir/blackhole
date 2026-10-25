// Step 4: RK4 vs Euler.
// Gold rays use RK4, blue rays use Euler, with the same step size. The white ray
// starts at the critical distance and circles the photon sphere a few times before
// escaping.

#include "demo.hpp"

#include <SFML/Graphics.hpp>

#include <cmath>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <vector>

namespace {

constexpr double kRs = 1.0;
constexpr double kStep = 0.1;
constexpr int kStepsPerFrame = 2;
constexpr double kEscape = 20.0;
constexpr double kStartX = -15.0;
constexpr std::size_t kTrailLength = 1000;

const sf::Color kBackground(6, 7, 12);
const sf::Color kRk4Color(255, 190, 90);
const sf::Color kEulerColor(80, 150, 255);
const sf::Color kOrbitColor(255, 255, 255);

struct State {
    double r, phi;
    double dr, dphi;
};

State operator+(const State& a, const State& b) {
    return {a.r + b.r, a.phi + b.phi, a.dr + b.dr, a.dphi + b.dphi};
}
State operator*(const State& s, double k) {
    return {s.r * k, s.phi * k, s.dr * k, s.dphi * k};
}

// Same geodesic equations as step 3.
State geodesic(const State& s, double E) {
    const double r = s.r;
    const double f = 1.0 - kRs / r;
    const double dtdl = E / f;
    State d;
    d.r = s.dr;
    d.phi = s.dphi;
    d.dr = -(kRs / (2.0 * r * r)) * f * dtdl * dtdl
         + (kRs / (2.0 * r * r * f)) * s.dr * s.dr
         + (r - kRs) * s.dphi * s.dphi;
    d.dphi = -2.0 * s.dr * s.dphi / r;
    return d;
}

// Euler: one slope per step.
State eulerStep(const State& s, double E, double h) {
    return s + geodesic(s, E) * h;
}

// RK4: slopes at the start, twice at the midpoint and at the end, weighted 1-2-2-1.
State rk4Step(const State& s, double E, double h) {
    const State k1 = geodesic(s, E);
    const State k2 = geodesic(s + k1 * (h / 2.0), E);
    const State k3 = geodesic(s + k2 * (h / 2.0), E);
    const State k4 = geodesic(s + k3 * h, E);
    return s + (k1 + k2 * 2.0 + k3 * 2.0 + k4) * (h / 6.0);
}

enum class Method { Euler, RK4 };

struct Ray {
    State state{};
    double energy = 0.0;
    Method method;
    int substeps;  // smaller steps per update
    std::deque<sf::Vector2f> trail;
    bool active = true;
    bool fellIn = false;

    // Takes doubles because the orbiting ray needs a very precise start height.
    Ray(double x, double y, double dirX, double dirY, Method method, int substeps = 1)
        : method(method), substeps(substeps) {
        state.r = std::hypot(x, y);
        state.phi = std::atan2(y, x);
        state.dr = (x * dirX + y * dirY) / state.r;
        state.dphi = (x * dirY - y * dirX) / (state.r * state.r);
        const double f = 1.0 - kRs / state.r;
        energy = std::sqrt(state.dr * state.dr + f * state.r * state.r * state.dphi * state.dphi);
        trail.push_back(position());
    }

    void update() {
        const double h = kStep / substeps;
        for (int i = 0; i < substeps; ++i) {
            // same horizon check as step 3, RK4 would otherwise sample f = 0 mid-step
            if (state.r + state.dr * h <= kRs) {
                fallIn();
                break;
            }
            state = method == Method::RK4 ? rk4Step(state, energy, h) : eulerStep(state, energy, h);
            if (!(state.r > kRs)) {
                fallIn();
                break;
            }
        }
        if (state.r > kEscape && state.dr > 0.0) {
            active = false;
        }
        trail.push_back(position());
        if (trail.size() > kTrailLength) {
            trail.pop_front();
        }
    }

    void fallIn() {
        state.r = kRs;
        active = false;
        fellIn = true;
    }

    sf::Vector2f position() const {
        return {static_cast<float>(state.r * std::cos(state.phi)),
                static_cast<float>(state.r * std::sin(state.phi))};
    }
};

// Start height for a ray from x0 with the critical impact parameter b = 3*sqrt(3)/2 r_s.
// b = L / E with L = y and E = sqrt(x0^2 + f*y^2) / r, solved by repeated substitution.
double criticalHeight(double x0) {
    const double critical = 1.5 * std::sqrt(3.0) * kRs;
    double y = critical;
    for (int i = 0; i < 50; ++i) {
        const double r = std::hypot(x0, y);
        const double f = 1.0 - kRs / r;
        y = critical * std::sqrt(x0 * x0 + f * y * y) / r;
    }
    return y;
}

std::vector<Ray> makeRays(Method method) {
    std::vector<Ray> rays;
    for (int i = 0; i <= 64; ++i) {
        rays.emplace_back(kStartX, -8.0 + 0.25 * i, 1.0, 0.0, method);
    }
    return rays;
}

// 1e-9 r_s outside the critical height, with 50 substeps for accuracy.
Ray makeOrbitRay() {
    return Ray(kStartX, criticalHeight(kStartX) + 1e-9, 1.0, 0.0, Method::RK4, 50);
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

void drawPhotonSphere(sf::RenderTarget& target) {
    const float radius = 1.5f * static_cast<float>(kRs);
    sf::CircleShape ring(radius, 128);
    ring.setOrigin({radius, radius});
    ring.setFillColor(sf::Color::Transparent);
    ring.setOutlineColor(sf::Color(255, 255, 255, 45));
    ring.setOutlineThickness(0.02f);
    target.draw(ring);
}

int countFellIn(const std::vector<Ray>& rays) {
    int count = 0;
    for (const Ray& ray : rays) {
        count += ray.fellIn ? 1 : 0;
    }
    return count;
}

} // namespace

int main(int argc, char* argv[]) {
    const demo::Options options = demo::parseOptions(argc, argv);
    sf::RenderWindow window;
    demo::openWindow(window, "Step 4 - RK4", options);
    window.setView(demo::worldView());
    const demo::Caption caption;

    std::vector<Ray> rk4Rays = makeRays(Method::RK4);
    std::vector<Ray> eulerRays = makeRays(Method::Euler);
    Ray orbitRay = makeOrbitRay();
    bool paused = false;
    bool showEuler = true;

    for (int frame = 0; window.isOpen(); ++frame) {
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            } else if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                if (key->code == sf::Keyboard::Key::Escape) window.close();
                if (key->code == sf::Keyboard::Key::Space) paused = !paused;
                if (key->code == sf::Keyboard::Key::E) showEuler = !showEuler;
                if (key->code == sf::Keyboard::Key::O) orbitRay = makeOrbitRay();
                if (key->code == sf::Keyboard::Key::R) {
                    rk4Rays = makeRays(Method::RK4);
                    eulerRays = makeRays(Method::Euler);
                    orbitRay = makeOrbitRay();
                }
            }
        }
        if (!window.isOpen()) {
            break;
        }

        if (!paused) {
            for (int s = 0; s < kStepsPerFrame; ++s) {
                for (Ray& ray : rk4Rays) {
                    if (ray.active) ray.update();
                }
                for (Ray& ray : eulerRays) {
                    if (ray.active) ray.update();
                }
                if (orbitRay.active) orbitRay.update();
            }
        }

        window.clear(kBackground);
        drawPhotonSphere(window);
        if (showEuler) {
            for (const Ray& ray : eulerRays) {
                drawTrail(window, ray.trail, kEulerColor);
            }
        }
        for (const Ray& ray : rk4Rays) {
            drawTrail(window, ray.trail, kRk4Color);
            if (ray.active) drawHead(window, ray.position());
        }
        drawTrail(window, orbitRay.trail, kOrbitColor);
        if (orbitRay.active) drawHead(window, orbitRay.position());
        drawBlackHole(window);
        caption.label(window, {1.06f, 1.06f}, "photon sphere, 1.5 r_s");

        caption.draw(window, "Step 4 - RK4 (gold) vs Euler (blue)",
                     "Fell in: RK4 " + std::to_string(countFellIn(rk4Rays)) + ", Euler " +
                     std::to_string(countFellIn(eulerRays)) +
                     "     E toggle Euler   O fire orbiting ray   Space pause   R restart   Esc quit");
        demo::present(window, options, frame);
    }
    return 0;
}
