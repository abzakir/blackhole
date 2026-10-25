// blackhole.frag - step 6: the accretion disk.
//
// Same ray tracer as step 5, plus a thin disk of hot gas in the y = 0 plane. After
// every RK4 step the ray checks whether it crossed that plane. If it did, inside the
// disk, it picks up the light the gas gives off there, and partly or fully stops.
//
// Units: r_s = 1 and c = 1. The black hole sits at the origin, y is up.
#version 120

uniform vec2 u_resolution;
uniform vec3 u_camPos;
uniform vec3 u_camRight;
uniform vec3 u_camUp;
uniform vec3 u_camForward;
uniform float u_tanHalfFov;
uniform bool u_showGrid;
uniform int u_shiftMode;  // 0 plain, 1 + gravitational redshift, 2 + Doppler as well
uniform float u_time;     // seconds, turns the disk

const float RS = 1.0;
const float ESCAPE_R = 1000.0;
const float STEP_SCALE = 0.05;
const int MAX_STEPS = 1000;
const float PI = 3.14159265;

const float DISK_IN = 3.0;        // innermost stable circular orbit: gas inside it falls in
const float DISK_OUT = 12.0;
const float DISK_PEAK_T = 5500.0; // kelvin, at the hottest radius (chosen for looks)
const float EXPOSURE = 0.7;       // overall disk brightness before tone mapping
const float TIME_SCALE = 4.0;     // speeds up the orbits so you can see them move

// ---- light bending and RK4: unchanged from step 5 ----

vec3 acceleration(vec3 x, float h2) {
    float r2 = dot(x, x);
    return -1.5 * RS * h2 * x / (r2 * r2 * sqrt(r2));
}

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

// ---- the sky: unchanged from step 5 ----

float hash(vec3 p) {
    return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453);
}

float noise(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(mix(hash(i), hash(i + vec3(1.0, 0.0, 0.0)), f.x),
                   mix(hash(i + vec3(0.0, 1.0, 0.0)), hash(i + vec3(1.0, 1.0, 0.0)), f.x), f.y),
               mix(mix(hash(i + vec3(0.0, 0.0, 1.0)), hash(i + vec3(1.0, 0.0, 1.0)), f.x),
                   mix(hash(i + vec3(0.0, 1.0, 1.0)), hash(i + vec3(1.0, 1.0, 1.0)), f.x), f.y), f.z);
}

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

vec3 stars(vec3 dir) {
    vec3 color = vec3(0.0);
    for (int layer = 0; layer < 4; ++layer) {
        vec3 cell = floor(dir * (25.0 + 20.0 * float(layer)));
        vec3 seed = cell + 17.0 * float(layer);
        if (hash(seed) > 0.8) {
            vec3 star = cell + 0.5 + 0.5 * (vec3(hash(seed + 1.7), hash(seed + 4.3), hash(seed + 9.1)) - 0.5);
            float angle = length(dir - normalize(star));
            float brightness = 0.12 + 2.5 * pow(hash(seed + 7.7), 8.0);
            vec3 tint = mix(vec3(1.0, 0.78, 0.6), vec3(0.7, 0.82, 1.0), hash(seed + 3.1));
            color += tint * brightness * exp(-angle * angle * 3.0e5);
        }
    }
    return color;
}

vec3 galaxy(vec3 dir) {
    float height = dot(dir, normalize(vec3(0.0, 1.0, 0.45)));
    float glow = exp(-height * height / 0.02) * (0.3 + 0.9 * clouds(dir * 6.0));
    float dust = exp(-height * height / 0.002) * smoothstep(0.4, 0.7, clouds(dir * 12.0 + 3.0));
    return vec3(0.55, 0.5, 0.7) * 0.4 * glow * (1.0 - 0.85 * dust);
}

vec3 skyGrid(vec3 dir) {
    float theta = acos(clamp(dir.y, -1.0, 1.0));
    float phi = atan(dir.z, dir.x);
    float checker = mod(floor(theta / (PI / 12.0)) + floor(phi / (PI / 12.0)), 2.0);
    vec3 base = dir.y > 0.0 ? vec3(0.25, 0.45, 0.85) : vec3(0.85, 0.45, 0.2);
    return base * (0.25 + 0.3 * checker);
}

// ---- the accretion disk ----

// Colour of a glowing body at a temperature in kelvin, scaled so the brightest channel
// is 1. A curve fit to the real blackbody colours (Tanner Helland's), good for 1000-40000 K.
vec3 blackbody(float kelvin) {
    float t = clamp(kelvin, 1000.0, 40000.0) / 100.0;
    vec3 color;
    color.r = t <= 66.0 ? 1.0 : 1.2929 * pow(t - 60.0, -0.1332);
    color.g = t <= 66.0 ? 0.3901 * log(t) - 0.6318 : 1.1299 * pow(t - 60.0, -0.0755);
    color.b = t >= 66.0 ? 1.0 : (t <= 19.0 ? 0.0 : 0.5432 * log(t - 10.0) - 1.1963);
    return clamp(color, 0.0, 1.0);
}

// Temperature across a thin disk, as a fraction of the peak: T ~ r^(-3/4) (1 - sqrt(r_in / r))^(1/4).
// It is zero at the inner edge and hottest at 49/36 r_in (about 4.1 r_s); 0.4878 is the peak value.
float temperatureProfile(float r) {
    float x = DISK_IN / r;
    return pow(x, 0.75) * pow(1.0 - sqrt(x), 0.25) / 0.4878;
}

// g = (frequency the camera sees) / (frequency the gas sent). Everything relativistic about
// the disk's colour is in this one number.
//   uT    the orbiting gas's clock runs slow: gravity plus its own speed, 1 / sqrt(1 - 1.5 r_s / r)
//   omega how fast the gas goes round (Kepler): sqrt(r_s / (2 r^3))
//   ell   the photon's angular momentum about the disk's axis divided by its energy. Both are
//         conserved along the ray, so the camera end works it out once per pixel.
//   camF  1 - r_s / r at the camera: the hovering camera's own clock runs slow too.
float frequencyShift(float r, float ell, float camF) {
    if (u_shiftMode == 0) {
        return 1.0;
    }
    float uT = 1.0 / sqrt(1.0 - 1.5 * RS / r);
    float omega = sqrt(0.5 * RS / (r * r * r));
    float doppler = u_shiftMode == 2 ? 1.0 - omega * ell : 1.0;
    return 1.0 / (sqrt(camF) * uT * doppler);
}

// Light given off where a ray crosses the disk: rgb is what the camera sees, a is how
// much of the ray the gas blocks (1 = opaque).
vec4 diskLight(vec3 p, float ell, float camF) {
    float r = length(p);
    if (r < DISK_IN || r > DISK_OUT) {
        return vec4(0.0);
    }

    // Swirls that orbit at each radius's own speed, so inner rings overtake outer ones.
    // The gas moves towards -phi (see frequencyShift's sign), so the pattern is sampled at phi + omega t.
    float omega = sqrt(0.5 * RS / (r * r * r));
    float angle = atan(p.z, p.x) + omega * u_time * TIME_SCALE;
    float swirl = clouds(vec3(2.0 * cos(angle), 2.0 * sin(angle), 4.0 * r));

    // A blackbody at temperature T seen with shift g looks like a blackbody at g * T,
    // and its brightness goes as T^4, so g^4 in all. That's why one side is so much brighter.
    float g = frequencyShift(r, ell, camF);
    float seenT = DISK_PEAK_T * temperatureProfile(r) * g;
    float brightness = pow(seenT / DISK_PEAK_T, 4.0) * (0.45 + 1.1 * swirl);
    vec3 light = blackbody(seenT) * brightness * EXPOSURE;

    float edges = smoothstep(DISK_IN, DISK_IN + 0.3, r) * smoothstep(DISK_OUT, DISK_OUT - 4.0, r);
    float opacity = edges * clamp(0.35 + 0.8 * swirl, 0.0, 1.0);
    return vec4(light * opacity, opacity);
}

// Squeezes any brightness into 0..1 with a soft shoulder, like a film camera (ACES fit by K. Narkowicz).
vec3 toneMap(vec3 c) {
    return clamp(c * (2.51 * c + 0.03) / (c * (2.43 * c + 0.59) + 0.14), 0.0, 1.0);
}

void main() {
    // 1. Pixel -> direction (as in step 5).
    vec2 uv = (2.0 * gl_FragCoord.xy - u_resolution) / u_resolution.y;
    vec3 dir = normalize(u_camForward + u_tanHalfFov * (uv.x * u_camRight + uv.y * u_camUp));

    // 2. Hovering-camera correction (as in step 5).
    vec3 x = u_camPos;
    vec3 radial = normalize(x);
    float camF = 1.0 - RS / length(x);
    vec3 v = normalize(dir + (sqrt(camF) - 1.0) * dot(dir, radial) * radial);
    vec3 angular = cross(x, v);
    float h2 = dot(angular, angular);

    // Conserved along the ray: its energy E (from the null condition) and its angular
    // momentum about the disk's axis. We trace backwards, so the real photon travels
    // along -v and its angular momentum is -angular.y.
    float vr = dot(v, radial);
    float energy = sqrt(vr * vr + camF * h2 / dot(x, x));
    float ell = -angular.y / energy;

    // 3. Follow the ray, collecting disk light every time it crosses y = 0.
    vec3 color = vec3(0.0);
    float transmittance = 1.0;  // how much of the ray hasn't been blocked yet
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
        vec3 before = x;
        rk4Step(x, v, h2, STEP_SCALE * r / length(v));

        if (before.y * x.y < 0.0) {
            vec3 hit = mix(before, x, before.y / (before.y - x.y));  // where the step crossed y = 0
            vec4 disk = diskLight(hit, ell, camF);
            color += transmittance * disk.rgb;
            transmittance *= 1.0 - disk.a;
            if (transmittance < 0.01) {
                break;
            }
        }
    }

    if (escaped) {
        vec3 skyDir = normalize(v);
        color += transmittance * (u_showGrid ? skyGrid(skyDir) : galaxy(skyDir) + stars(skyDir));
    }
    gl_FragColor = vec4(toneMap(color), 1.0);
}
