// shadow_check.cpp - numerical check of the original geodesic.comp physics.
//
// Shoots light rays from a camera at r = 5 r_s (the original's starting distance)
// toward the black hole and finds the edge of its shadow: the angle off the line
// of sight below which rays fall in and above which they escape. Compares the
// textbook Schwarzschild equations with the two mistakes found in geodesic.comp.
//
// Units: r_s = 1 and c = 1. Rays stay in the equatorial plane (theta = pi/2),
// which loses nothing because the black hole is spherically symmetric.

#include <cmath>
#include <cstdio>

namespace {

constexpr double kPi = 3.14159265358979323846;

// Position (r, phi) and velocity (dr, dphi) along the ray, per unit affine parameter lambda.
struct State {
    double r, phi, dr, dphi;
};

struct Variant {
    const char* name;
    bool originalEnergy;  // E as in initRay: the "/ f" under the square root is missing
    bool originalRForce;  // r'' uses r * dphi^2 instead of (r - r_s) * dphi^2
    bool euler;           // one Euler step per iteration, as geodesic.comp's rk4Step really does
    double step;
    long maxSteps;
};

State add(const State& s, const State& d, double h) {
    return {s.r + h * d.r, s.phi + h * d.phi, s.dr + h * d.dr, s.dphi + h * d.dphi};
}

// Right-hand side of the geodesic equations: d/dlambda of every state component.
State derivatives(const State& s, double E, const Variant& v) {
    const double r = s.r;
    const double f = 1.0 - 1.0 / r;
    const double dt = E / f;
    const double angular = (v.originalRForce ? r : r - 1.0) * s.dphi * s.dphi;
    return {
        s.dr,
        s.dphi,
        -(1.0 / (2.0 * r * r)) * f * dt * dt + (1.0 / (2.0 * r * r * f)) * s.dr * s.dr + angular,
        -2.0 * s.dr * s.dphi / r,
    };
}

// True if a ray leaving r0 at angle alpha from the inward direction falls through the horizon.
bool falls_in(double alpha, double r0, const Variant& v) {
    State s{r0, 0.0, -std::cos(alpha), std::sin(alpha) / r0};
    const double f = 1.0 - 1.0 / r0;
    const double E = v.originalEnergy
        ? f * std::sqrt(s.dr * s.dr / f + r0 * r0 * s.dphi * s.dphi)
        : std::sqrt(s.dr * s.dr + f * r0 * r0 * s.dphi * s.dphi);

    const double h = v.step;
    for (long i = 0; i < v.maxSteps; ++i) {
        if (s.r <= 1.0) {
            return true;
        }
        if (s.r > 100.0 && s.dr > 0.0) {
            return false;
        }

        const State k1 = derivatives(s, E, v);
        if (v.euler) {
            s = add(s, k1, h);
            continue;
        }
        const State k2 = derivatives(add(s, k1, h / 2), E, v);
        const State k3 = derivatives(add(s, k2, h / 2), E, v);
        const State k4 = derivatives(add(s, k3, h), E, v);
        s.r    += h / 6 * (k1.r    + 2 * k2.r    + 2 * k3.r    + k4.r);
        s.phi  += h / 6 * (k1.phi  + 2 * k2.phi  + 2 * k3.phi  + k4.phi);
        s.dr   += h / 6 * (k1.dr   + 2 * k2.dr   + 2 * k3.dr   + k4.dr);
        s.dphi += h / 6 * (k1.dphi + 2 * k2.dphi + 2 * k3.dphi + k4.dphi);
    }
    return false;
}

// Bisects for the shadow edge, in degrees from the line of sight.
double shadow_edge_deg(double r0, const Variant& v) {
    double inside = 0.0;      // straight at the hole: falls in
    double outside = kPi / 2; // sideways: escapes
    for (int i = 0; i < 40; ++i) {
        const double mid = 0.5 * (inside + outside);
        (falls_in(mid, r0, v) ? inside : outside) = mid;
    }
    return inside * 180.0 / kPi;
}

} // namespace

int main() {
    const double r0 = 5.0;
    const double originalStep = 1e7 / 1.269e10;  // D_LAMBDA / SagA_rs from geodesic.comp
    const double halfFov = 30.0;                 // the original's 60 degree vertical field of view

    const Variant variants[] = {
        {"Textbook equations, RK4",              false, false, false, 1e-3, 2000000},
        {"Textbook equations, Euler (his steps)", false, false, true,  originalStep, 60000},
        {"Only the energy mistake, RK4",         true,  false, false, 1e-3, 2000000},
        {"Only the r-force mistake, RK4",        false, true,  false, 1e-3, 2000000},
        {"Both mistakes, RK4",                   true,  true,  false, 1e-3, 2000000},
        {"Exactly geodesic.comp",                true,  true,  true,  originalStep, 60000},
    };

    // Exact answer: rays with impact parameter below b = (3*sqrt(3)/2) r_s are captured.
    // An observer standing still at r0 sees that edge at sin(a) = b * sqrt(f) / r0.
    // Like initRay, falls_in() aims rays with flat-space directions (dr = -cos a, r*dphi = sin a),
    // and in that convention the same edge sits at tan(a') = tan(a) / sqrt(f).
    const double f0 = 1.0 - 1.0 / r0;
    const double observed = std::asin(1.5 * std::sqrt(3.0) * std::sqrt(f0) / r0);
    const double flatAim = std::atan(std::tan(observed) / std::sqrt(f0));
    std::printf("Camera at r = %.0f r_s. Exact shadow edge: %.2f deg as a still observer sees it,\n"
                "which is %.2f deg in the flat-space aiming that initRay (and this table) uses.\n\n",
                r0, observed * 180.0 / kPi, flatAim * 180.0 / kPi);
    std::printf("%-40s %11s %20s\n", "Variant", "edge (deg)", "shadow / screen h");
    for (const Variant& v : variants) {
        const double edge = shadow_edge_deg(r0, v);
        const double fraction = std::tan(edge * kPi / 180.0) / std::tan(halfFov * kPi / 180.0);
        std::printf("%-40s %11.2f %19.0f%%\n", v.name, edge, 100.0 * fraction);
    }
    return 0;
}
