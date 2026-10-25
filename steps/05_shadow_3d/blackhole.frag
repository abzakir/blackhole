// blackhole.frag - step 5: the black hole in 3D.
//
// The GPU runs main() once for every pixel. Each run:
//   1. turns its pixel into a light ray leaving the camera,
//   2. follows that ray backwards through curved spacetime with RK4,
//   3. paints the pixel black if the ray falls into the black hole, or with the
//      part of the sky the ray ends up pointing at if it escapes.
//
// Units: r_s = 1 and c = 1. The black hole sits at the origin, y is up.
#version 120

uniform vec2 u_resolution;   // window size in pixels
uniform vec3 u_camPos;       // camera position
uniform vec3 u_camRight;     // camera axes, unit length
uniform vec3 u_camUp;
uniform vec3 u_camForward;
uniform float u_tanHalfFov;  // tan(vertical field of view / 2)
uniform bool u_showGrid;     // checkerboard sky instead of stars

const float RS = 1.0;
const float ESCAPE_R = 1000.0;  // beyond this an outgoing ray is as good as straight
const float STEP_SCALE = 0.05;  // each step covers about 5% of the current distance
const int MAX_STEPS = 1000;
const float PI = 3.14159265;

// Light bending in Cartesian coordinates. With h = |x cross v| (constant along the
// ray), this pull towards the centre gives exactly the same paths as the polar
// geodesic equations of steps 3-4: both reduce to r'' = (r - 1.5 r_s) * phi'^2.
// No sin/cos, no special axis, and no division by zero except at r = 0.
vec3 acceleration(vec3 x, float h2) {
    float r2 = dot(x, x);
    return -1.5 * RS * h2 * x / (r2 * r2 * sqrt(r2));
}

// One RK4 step for position x and velocity v: the same 1-2-2-1 recipe as step 4.
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

// Pseudo-random number in [0, 1) from a point: the same input always gives the same output.
float hash(vec3 p) {
    return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453);
}

// Procedural stars. Space is cut into unit cells; a few cells hold one star near
// their centre, and a direction that passes close to a star lights up.
vec3 stars(vec3 dir) {
    vec3 color = vec3(0.0);
    for (int layer = 0; layer < 3; ++layer) {
        vec3 p = dir * (30.0 + 18.0 * float(layer));
        vec3 cell = floor(p);
        vec3 seed = cell + 17.0 * float(layer);
        if (hash(seed) > 0.85) {
            vec3 star = cell + 0.5 + 0.5 * (vec3(hash(seed + 1.7), hash(seed + 4.3), hash(seed + 9.1)) - 0.5);
            float d = length(p - star);
            float brightness = pow(hash(seed + 7.7), 3.0) * 2.0;
            vec3 tint = mix(vec3(1.0, 0.78, 0.6), vec3(0.7, 0.82, 1.0), hash(seed + 3.1));
            color += tint * brightness * exp(-d * d * 400.0);
        }
    }
    return color;
}

// A checkerboard painted on the sky (blue above the horizon, orange below), so you
// can see how the black hole distorts the view behind it.
vec3 skyGrid(vec3 dir) {
    float theta = acos(clamp(dir.y, -1.0, 1.0));  // 0 straight up, PI straight down
    float phi = atan(dir.z, dir.x);
    float checker = mod(floor(theta / (PI / 12.0)) + floor(phi / (PI / 12.0)), 2.0);
    vec3 base = dir.y > 0.0 ? vec3(0.25, 0.45, 0.85) : vec3(0.85, 0.45, 0.2);
    return base * (0.25 + 0.3 * checker);
}

void main() {
    // 1. Pixel -> direction. uv runs from -1 at the bottom of the screen to +1 at the
    //    top, and proportionally wider across.
    vec2 uv = (2.0 * gl_FragCoord.xy - u_resolution) / u_resolution.y;
    vec3 dir = normalize(u_camForward + u_tanHalfFov * (uv.x * u_camRight + uv.y * u_camUp));

    // 2. The camera hovers in place. Near the hole, space is stretched along r, so the
    //    radial part of the direction it sees is shorter by sqrt(f) in coordinates.
    //    Skipping this makes the shadow too big (see checks/shadow_check.cpp).
    vec3 x = u_camPos;
    vec3 radial = normalize(x);
    float f = 1.0 - RS / length(x);
    vec3 v = normalize(dir + (sqrt(f) - 1.0) * dot(dir, radial) * radial);
    vec3 angular = cross(x, v);
    float h2 = dot(angular, angular);

    // 3. Follow the ray until it falls in or gets far away heading outward.
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

    // Rays that never escape (fell in, or still circling after MAX_STEPS) stay black.
    vec3 color = vec3(0.0);
    if (escaped) {
        vec3 skyDir = normalize(v);
        color = u_showGrid ? skyGrid(skyDir) : stars(skyDir);
    }
    gl_FragColor = vec4(color, 1.0);
}
