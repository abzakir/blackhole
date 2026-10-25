// Step 5: traces one light ray per pixel around the black hole.
// Units: r_s = 1, c = 1. Black hole at the origin, y up.
#version 120

uniform vec2 u_resolution;   // window size in pixels
uniform vec3 u_camPos;       // camera position
uniform vec3 u_camRight;     // camera axes, unit length
uniform vec3 u_camUp;
uniform vec3 u_camForward;
uniform float u_tanHalfFov;  // tan(vertical field of view / 2)
uniform bool u_showGrid;     // checkerboard sky instead of stars

const float RS = 1.0;
const float ESCAPE_R = 1000.0;  // rays past this, heading out, count as escaped
const float STEP_SCALE = 0.05;  // step = 5% of the distance to the hole
const int MAX_STEPS = 1000;
const float PI = 3.14159265;

// Light bending in Cartesian form. With h = |x cross v| constant this traces the same
// paths as the polar geodesic equations: r'' = (r - 1.5 r_s) phi'^2.
vec3 acceleration(vec3 x, float h2) {
    float r2 = dot(x, x);
    return -1.5 * RS * h2 * x / (r2 * r2 * sqrt(r2));
}

// One RK4 step for position and velocity.
void rk4Step(inout vec3 x, inout vec3 v, float h2, float dt) {
    vec3 k1x = v;
    vec3 k1v = acceleration(x, h2);
    vec3 k2x = v + 0.5 * dt * k1v;
    vec3 k2v = acceleration(x + 0.5 * dt * k1x, h2);
    vec3 k3x = v + 0.5 * dt * k2v;
    vec3 k3v = acceleration(x + 0.5 * dt * k2x, h2);
    vec3 k4x = v + dt * k3v;
    vec3 k4v = acceleration(x + dt * k3x, h2);
    x += dt / 6.0 * (k1x + 2.0 * k2x + 2.0 * k3x + k4x);
    v += dt / 6.0 * (k1v + 2.0 * k2v + 2.0 * k3v + k4v);
}

// Pseudo-random value in [0, 1).
float hash(vec3 p) {
    return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453);
}

// Smooth value noise.
float noise(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(mix(hash(i), hash(i + vec3(1.0, 0.0, 0.0)), f.x),
                   mix(hash(i + vec3(0.0, 1.0, 0.0)), hash(i + vec3(1.0, 1.0, 0.0)), f.x), f.y),
               mix(mix(hash(i + vec3(0.0, 0.0, 1.0)), hash(i + vec3(1.0, 0.0, 1.0)), f.x),
                   mix(hash(i + vec3(0.0, 1.0, 1.0)), hash(i + vec3(1.0, 1.0, 1.0)), f.x), f.y), f.z);
}

// A few octaves of noise.
float clouds(vec3 p) {
    float sum = 0.0;
    float amplitude = 0.5;
    for (int i = 0; i < 4; ++i) {
        sum += amplitude * noise(p);
        p *= 2.03;
        amplitude *= 0.5;
    }
    return sum;
}

// Procedural stars: some grid cells hold one star.
vec3 stars(vec3 dir) {
    vec3 color = vec3(0.0);
    for (int layer = 0; layer < 4; ++layer) {
        vec3 cell = floor(dir * (25.0 + 20.0 * float(layer)));
        vec3 seed = cell + 17.0 * float(layer);
        if (hash(seed) > 0.8) {
            vec3 star = cell + 0.5 + 0.5 * (vec3(hash(seed + 1.7), hash(seed + 4.3), hash(seed + 9.1)) - 0.5);
            float angle = length(dir - normalize(star));         // in radians, roughly
            float brightness = 0.12 + 2.5 * pow(hash(seed + 7.7), 8.0);
            vec3 tint = mix(vec3(1.0, 0.78, 0.6), vec3(0.7, 0.82, 1.0), hash(seed + 3.1));
            color += tint * brightness * exp(-angle * angle * 3.0e5);  // about 1 pixel wide
        }
    }
    return color;
}

// Faint galaxy band with a dust lane, passing behind the black hole.
vec3 galaxy(vec3 dir) {
    float height = dot(dir, normalize(vec3(0.0, 1.0, 0.45)));  // distance from the band's centre line
    float glow = exp(-height * height / 0.02) * (0.3 + 0.9 * clouds(dir * 6.0));
    float dust = exp(-height * height / 0.002) * smoothstep(0.4, 0.7, clouds(dir * 12.0 + 3.0));
    return vec3(0.55, 0.5, 0.7) * 0.4 * glow * (1.0 - 0.85 * dust);
}

// Checkerboard sky: blue above the horizon, orange below.
vec3 skyGrid(vec3 dir) {
    float theta = acos(clamp(dir.y, -1.0, 1.0));  // 0 straight up, PI straight down
    float phi = atan(dir.z, dir.x);
    float checker = mod(floor(theta / (PI / 12.0)) + floor(phi / (PI / 12.0)), 2.0);
    vec3 base = dir.y > 0.0 ? vec3(0.25, 0.45, 0.85) : vec3(0.85, 0.45, 0.2);
    return base * (0.25 + 0.3 * checker);
}

void main() {
    // pixel to ray direction
    vec2 uv = (2.0 * gl_FragCoord.xy - u_resolution) / u_resolution.y;
    vec3 dir = normalize(u_camForward + u_tanHalfFov * (uv.x * u_camRight + uv.y * u_camUp));

    // The camera hovers in place, so the radial part of the direction is scaled by sqrt(f).
    vec3 x = u_camPos;
    vec3 radial = normalize(x);
    float f = 1.0 - RS / length(x);
    vec3 v = normalize(dir + (sqrt(f) - 1.0) * dot(dir, radial) * radial);
    vec3 angular = cross(x, v);
    float h2 = dot(angular, angular);

    // trace until the ray falls in or escapes
    bool escaped = false;
    for (int i = 0; i < MAX_STEPS; ++i) {
        float r = length(x);
        if (r < RS) {
            break;
        }
        if (r > ESCAPE_R && dot(x, v) > 0.0) {
            escaped = true;
            break;
        }
        rk4Step(x, v, h2, STEP_SCALE * r / length(v));
    }

    // rays that don't escape stay black
    vec3 color = vec3(0.0);
    if (escaped) {
        vec3 skyDir = normalize(v);
        color = u_showGrid ? skyGrid(skyDir) : galaxy(skyDir) + stars(skyDir);
    }
    gl_FragColor = vec4(color, 1.0);
}
