// Shared by every Celestials object shader: scene cbuffer, camera rays, noise, the procedural
// sky and the photon tracer used by the lensing objects.
// Units: the object sits at the origin; its radius (or Schwarzschild radius) is 1.

#ifndef CELESTIAL_HLSLI
#define CELESTIAL_HLSLI

cbuffer Scene : register(b0)
{
    float4 gCamPos;    // xyz eye, w = seconds since this object appeared
    float4 gCamRight;  // xyz, w = aspect
    float4 gCamUp;     // xyz, w = tan(fovY / 2)
    float4 gCamFwd;    // xyz, w = radians per pixel
    float4 gAxis;      // xyz spin / jet axis (unit), w = seed
    float4 gP0, gP1, gP2, gP3, gP4, gP5;   // per-object parameters (see each shader)
    float4 gC0, gC1, gC2, gC3, gC4, gC5;   // per-object colours
    float4 gSky;       // rgb nebula tint, w = sky brightness
    float4 gMisc;      // x = max march steps, y = accretion disk on, z = unused, w = spin angle (radians)
    float4 gSun;       // planets: xyz unit direction towards their star, w = its angular radius (0 = none)
    float4 gSunColor;  // rgb light colour (x intensity), w = disk brightness
};

struct PSIn
{
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

static const float kPi = 3.14159265;

float Time() { return gCamPos.w; }
float Sq(float x) { return x * x; }

float3 CameraRay(float2 uv)
{
    float2 ndc = float2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);
    float t = gCamUp.w;
    return normalize(gCamFwd.xyz + ndc.x * t * gCamRight.w * gCamRight.xyz + ndc.y * t * gCamUp.xyz);
}

// ---- hashing / noise (hash without sine, D. Hoskins) ----

float Hash31(float3 p)
{
    p = frac(p * float3(0.1031, 0.1030, 0.0973));
    p += dot(p, p.yxz + 33.33);
    return frac((p.x + p.y) * p.z);
}

float3 Hash33(float3 p)
{
    p = frac(p * float3(0.1031, 0.1030, 0.0973));
    p += dot(p, p.yxz + 33.33);
    return frac((p.xxy + p.yxx) * p.zyx);
}

float Noise3(float3 p)
{
    float3 i = floor(p);
    float3 f = frac(p);
    float3 u = f * f * (3.0 - 2.0 * f);
    float n000 = Hash31(i);
    float n100 = Hash31(i + float3(1, 0, 0));
    float n010 = Hash31(i + float3(0, 1, 0));
    float n110 = Hash31(i + float3(1, 1, 0));
    float n001 = Hash31(i + float3(0, 0, 1));
    float n101 = Hash31(i + float3(1, 0, 1));
    float n011 = Hash31(i + float3(0, 1, 1));
    float n111 = Hash31(i + float3(1, 1, 1));
    return lerp(lerp(lerp(n000, n100, u.x), lerp(n010, n110, u.x), u.y),
                lerp(lerp(n001, n101, u.x), lerp(n011, n111, u.x), u.y), u.z);
}

float Fbm(float3 p, int octaves)
{
    float v = 0.0, a = 0.5, norm = 0.0;
    [loop]
    for (int k = 0; k < octaves; ++k)
    {
        v += a * Noise3(p);
        norm += a;
        p = p * 2.03 + float3(1.7, 9.2, 5.4);
        a *= 0.5;
    }
    return v / norm;
}

// x = distance to the nearest feature point, y = second nearest, z = nearest cell's id (0..1).
// Feature points wander with `phase` so the cells boil.
float3 Voronoi(float3 p, float phase)
{
    float3 i = floor(p);
    float3 f = frac(p);
    float d1 = 8.0, d2 = 8.0, id = 0.0;
    [unroll]
    for (int z = -1; z <= 1; ++z)
    [unroll]
    for (int y = -1; y <= 1; ++y)
    [unroll]
    for (int x = -1; x <= 1; ++x)
    {
        float3 g = float3(x, y, z);
        float3 h = Hash33(i + g);
        float3 o = 0.5 + 0.42 * sin(phase + 6.2831853 * h);
        float3 r = g + o - f;
        float d = dot(r, r);
        if (d < d1) { d2 = d1; d1 = d; id = h.x; }
        else if (d < d2) { d2 = d; }
    }
    return float3(sqrt(d1), sqrt(d2), id);
}

// Rodrigues rotation of p about unit axis k.
float3 RotateAxis(float3 p, float3 k, float a)
{
    float c = cos(a), s = sin(a);
    return p * c + cross(k, p) * s + k * dot(k, p) * (1.0 - c);
}

float3 Hue(float h)
{
    return saturate(abs(frac(h + float3(0.0, 2.0 / 3.0, 1.0 / 3.0)) * 6.0 - 3.0) - 1.0);
}

// ---- the sky ----

float3 StarTint(float h)
{
    float3 cool = float3(1.0, 0.62, 0.38), mid = float3(1.0, 0.93, 0.84), hot = float3(0.66, 0.78, 1.0);
    return h < 0.5 ? lerp(cool, mid, h * 2.0) : lerp(mid, hot, h * 2.0 - 1.0);
}

// Hashed stars on the unit sphere: each 3D cell of the scaled sphere holds at most one star,
// kept only if its projection stays inside the cell's middle so neighbours never need testing.
float3 StarLayer(float3 dir, float scale, float density, float seed, float bright)
{
    float3 p = dir * scale;
    float3 c = floor(p);
    float3 h = Hash33(c + seed);
    float3 sp = c + 0.25 + 0.5 * Hash33(c.zxy + seed * 1.37 + 11.0);
    float3 s = normalize(sp);
    float3 q = s * scale - c;
    float inside = step(0.2, min(q.x, min(q.y, q.z))) * step(max(q.x, max(q.y, q.z)), 0.8);
    float present = step(h.x, density) * inside;
    float mag = pow(h.y, 7.0);
    float size = min(gCamFwd.w * (0.75 + 1.6 * mag), 0.18 / scale);
    float ang = length(dir - s);
    float core = exp(-ang * ang / (size * size));
    return present * core * bright * (0.12 + 3.0 * mag) * StarTint(h.z);
}

float3 Sky(float3 dir)
{
    float seed = gAxis.w;
    float3 col = StarLayer(dir, 38.0, 0.22, seed, 1.6);
    col += StarLayer(dir, 95.0, 0.16, seed + 17.0, 0.8);
    col += StarLayer(dir, 210.0, 0.12, seed + 41.0, 0.45);

    // Milky Way band with dust lanes, and faint coloured nebula wisps anywhere.
    float3 n = normalize(float3(0.25, 1.0, 0.35 + frac(seed * 0.013) - 0.5));
    float b = dot(dir, n);
    float band = exp(-b * b * 14.0);
    float neb = Fbm(dir * 3.0 + seed, 4);
    float dust = smoothstep(0.45, 0.72, Fbm(dir * 7.0 + 3.1 + seed, 4));
    float3 mw = lerp(float3(0.55, 0.52, 0.64), float3(0.85, 0.7, 0.55), neb) * band * (0.35 + neb) * (1.0 - 0.85 * dust * band);
    float w = saturate(Fbm(dir * 2.2 + 7.7 + seed, 4) * 1.7 - 0.62);
    col += (mw * 0.05 + gSky.rgb * w * w * 0.12);
    return col * gSky.w;
}

// ---- photon tracer ----
// Integrates a light ray backwards from the eye under the Schwarzschild photon equation
// a = -1.5 rs m(r) h^2 x / r^5 (h = |x cross v|), stepping proportionally to r.
// softA > 0 swaps the point mass for a soft enclosed mass m(r) = r^3 / (r^3 + softA^3) (boson star).
// Stops when the ray enters `surface` (hit), or escapes outward past 80.
struct Trace
{
    float3 pos;    // final position (surface hit point when hit)
    float3 dir;    // final direction (normalised)
    float rmin;    // closest approach to the centre
    bool hit;
};

Trace TraceRay(float3 ro, float3 rd, float rs, float surface, float softA, float ripple)
{
    Trace tr;
    float3 pos = ro;
    float3 vel = rd;
    float3 hv = cross(pos, vel);
    float h2 = dot(hv, hv);
    float rmin = 1e9;
    bool hit = false;
    int steps = (int)gMisc.x;
    float t = Time();
    [loop]
    for (int k = 0; k < steps; ++k)
    {
        float r2 = dot(pos, pos);
        float r = sqrt(r2);
        rmin = min(rmin, r);
        if (r < surface) { hit = true; break; }
        if (r > 80.0 && dot(pos, vel) > 0.0) break;
        float m = rs;
        if (softA > 0.0)
        {
            float a3 = softA * softA * softA;
            m *= r2 * r / (r2 * r + a3);
            m *= 1.0 + ripple * sin(r * 2.4 - t * 1.3) * exp(-r / (3.0 * max(softA, 0.01)));
        }
        float dt = surface > 0.0 ? clamp(0.35 * (r - surface) + 0.004, 0.004, 1.5) : clamp(0.07 * r, 0.02, 1.5);
        float3 acc = -1.5 * m * h2 * pos / (r2 * r2 * r);
        vel += acc * dt;
        pos += vel * dt;
    }
    tr.pos = pos;
    tr.dir = normalize(vel);
    tr.rmin = rmin;
    tr.hit = hit;
    return tr;
}

// Straight-ray sphere test: returns the near hit distance or -1.
float SphereHit(float3 ro, float3 rd, float radius)
{
    float b = dot(ro, rd);
    float c = dot(ro, ro) - radius * radius;
    float disc = b * b - c;
    return disc < 0.0 ? -1.0 : -b - sqrt(disc);
}

// Glow of a thin needle jet along +-axis out to `len`, seen along the straight ray (ro, rd),
// hidden beyond tHit. Knots stream outward.
float JetGlow(float3 ro, float3 rd, float3 axis, float len, float width, float tHit, float speed)
{
    float b = dot(rd, axis);
    float d = dot(rd, ro);
    float e = dot(axis, ro);
    float denom = max(1.0 - b * b, 1e-4);
    float s = clamp((e - b * d) / denom, -len, len);
    float3 pa = axis * s;
    float t = max(dot(pa - ro, rd), 0.0);
    float3 pr = ro + rd * t;
    float dist = length(pr - pa);
    float along = abs(s);
    float w = width * (0.35 + along * 0.07);
    float I = exp(-dist * dist / (w * w)) * smoothstep(1.0, 1.35, along) * (1.0 - smoothstep(len * 0.35, len, along));
    I *= 0.55 + 0.45 * sin(along * 5.0 - Time() * speed);
    if (tHit > 0.0 && t > tHit) I = 0.0;
    return I;
}

// ---- planets: a star to light them ----

float SphereHitAt(float3 ro, float3 rd, float3 c, float radius) { return SphereHit(ro - c, rd, radius); }

// The star in the sky: disk, corona glow and a wide faint halo.
float3 StarDisk(float3 rd)
{
    float3 col = 0.0;
    if (gSun.w > 0.0)
    {
        float ang = acos(clamp(dot(rd, gSun.xyz), -1.0, 1.0));
        float disk = 1.0 - smoothstep(gSun.w * 0.94, gSun.w, ang);
        float glow = exp(-max(ang - gSun.w, 0.0) / (gSun.w * 0.3 + 0.006));
        col = gSunColor.rgb * (disk * gSunColor.w + glow * 1.5 + exp(-ang * 5.0) * 0.06);
    }
    return col;
}

// Lambert with a softened terminator (atmospheres wrap light a little past 90 degrees).
float Lit(float3 n, float wrap) { return saturate((dot(n, gSun.xyz) + wrap) / (1.0 + wrap)); }

// Orthonormal frame around a planet's spin axis: x/z span the equator, y is the pole.
float3 ToLocal(float3 p, float3 axis)
{
    float3 e1 = normalize(abs(axis.y) < 0.9 ? cross(axis, float3(0, 1, 0)) : cross(axis, float3(1, 0, 0)));
    float3 e2 = cross(axis, e1);
    return float3(dot(p, e1), dot(p, axis), dot(p, e2));
}

// Atmospheric limb glow for a ray that misses a sphere of radius R at c: b = closest approach.
float3 Halo(float3 ro, float3 rd, float3 c, float R, float height, float3 tint)
{
    float3 oc = ro - c;
    float tc = -dot(oc, rd);
    float3 cp = oc + rd * max(tc, 0.0);
    float b = length(cp);
    float d = max(b - R, 0.0);
    float lit = saturate(dot(normalize(cp), gSun.xyz) * 0.8 + 0.35);
    float fwd = pow(saturate(dot(rd, gSun.xyz)), 6.0);   // back-lit rings glow
    return tint * exp(-d / height) * (lit + 2.0 * fwd) * step(0.0, tc);
}

// A small moon on a circular orbit in the spin axis's equatorial plane.
// m: x = radius (0 = none), y = orbit radius, z = angular speed, w = phase.
float3 Orbit(float4 m)
{
    float a = m.w + Time() * m.z;
    float3 ax = gAxis.xyz;
    float3 e1 = normalize(abs(ax.y) < 0.9 ? cross(ax, float3(0, 1, 0)) : cross(ax, float3(1, 0, 0)));
    float3 e2 = cross(ax, e1);
    return (e1 * cos(a) + e2 * sin(a)) * m.y;
}

// 0 where the sphere (c, r) blocks the star as seen from pos, 1 in full light (soft penumbra).
float SphereShadow(float3 pos, float3 c, float r)
{
    float3 v = c - pos;
    float t = dot(v, gSun.xyz);
    float d = length(v - gSun.xyz * t);
    return t > 0.0 ? smoothstep(r * 0.8, r * 1.1, d) : 1.0;
}

// A plain grey moon ball lit by the star (used around planets).
float3 MoonBall(float3 n, float seed)
{
    float3 p = n * 5.0 + seed;
    float a = 0.28 + 0.3 * Fbm(p, 4) + 0.12 * Fbm(p * 4.0, 3);
    return a * Lit(n, 0.0) * gSunColor.rgb;
}

float Ridge(float v) { return 1.0 - abs(v * 2.0 - 1.0); }

// Crater field on a sphere point p: bowls with raised rims. Adds height and its gradient
// (with respect to p * scale) so the caller can bend the normal and get crisp shadows.
void Craters(float3 p, float scale, float seed, float coverage, inout float h, inout float3 grad)
{
    float3 q = p * scale;
    float3 i = floor(q);
    [unroll]
    for (int z = -1; z <= 1; ++z)
    [unroll]
    for (int y = -1; y <= 1; ++y)
    [unroll]
    for (int x = -1; x <= 1; ++x)
    {
        float3 c = i + float3(x, y, z);
        float3 hs = Hash33(c + seed);
        float r = 0.16 + 0.3 * hs.y;
        float3 v = q - (c + 0.25 + 0.5 * Hash33(c.yzx + seed + 3.1));
        float lv = max(length(v), 1e-4);
        float d = lv / r;
        float on = step(1.0 - coverage, hs.z);
        float rimE = exp(-Sq((d - 1.0) / 0.22));
        float prof = (d < 1.0 ? (d * d - 1.0) * 0.55 : 0.0) + 0.22 * rimE;
        float dprof = (d < 1.0 ? 1.1 * d : 0.0) - 0.22 * rimE * 2.0 * (d - 1.0) / (0.22 * 0.22);
        h += on * prof * r;
        grad += on * dprof * (v / lv);
    }
}

// Tangent part of a gradient, applied as a bump to normal n.
float3 Bump(float3 n, float3 grad, float k) { return normalize(n - k * (grad - n * dot(grad, n))); }

#endif
