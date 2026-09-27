// Gas giants (Jupiter-like, ringed, ice giant, hot Jupiter): zonal cloud bands sheared by jets,
// a great storm, optional rings (the planet shadows the rings and the rings stripe the planet),
// small moons that cast shadows as they transit, and a glowing night side for hot Jupiters.
// gP0: x = band count, y = turbulence, z = storm size (0 = none), w = jet speed
// gP1: x = ring inner radius, y = ring outer radius (0 = no rings), z = ring opacity, w = night glow
// gP2: x = storm latitude, y = storm longitude, z = atmosphere rim strength, w = unused
// gP4, gP5: moons (radius, orbit radius, angular speed, phase); radius 0 = none
// gC0: light bands, gC1: dark bands, gC2: storm, gC3: atmosphere tint, gC4: ring tint, gC5: night glow
#include "Celestial.hlsli"

float RingDensity(float r)
{
    float x = saturate((r - gP1.x) / (gP1.y - gP1.x));
    float d = 0.55 + 0.45 * Noise3(float3(r * 22.0, gAxis.w, 0.0));
    d *= 0.6 + 0.4 * Noise3(float3(r * 75.0, gAxis.w + 5.0, 0.0));
    d *= 1.0 - 0.93 * exp(-Sq((x - 0.63) / 0.022));   // a Cassini-like division
    d *= 1.0 - 0.6 * exp(-Sq((x - 0.88) / 0.01));
    d *= smoothstep(0.0, 0.04, x) * (1.0 - smoothstep(0.94, 1.0, x));
    d *= lerp(0.35, 1.0, smoothstep(0.12, 0.3, x));    // faint inner ring
    return saturate(d);
}

// How much starlight reaches pos through the rings.
float RingShadow(float3 pos)
{
    float s = 1.0;
    float denom = dot(gSun.xyz, gAxis.xyz);
    if (gP1.y > 0.0 && abs(denom) > 1e-4)
    {
        float t = -dot(pos, gAxis.xyz) / denom;
        float r = length(pos + gSun.xyz * t);
        if (t > 0.0 && r > gP1.x && r < gP1.y) s = 1.0 - RingDensity(r) * gP1.z * 0.85;
    }
    return s;
}

float3 Planet(float3 n, float3 rd, float3 pos, float3 m1, float3 m2)
{
    float t = Time();
    float3 lp = ToLocal(RotateAxis(n, gAxis.xyz, -gMisc.w), gAxis.xyz);
    float lat = lp.y;
    // Zonal jets: each latitude drifts at its own rate, shearing the clouds into bands.
    float shear = t * gP0.w * sin(lat * gP0.x * 1.7 + gAxis.w);
    float ca = cos(shear), sa = sin(shear);
    float3 q = float3(lp.x * ca - lp.z * sa, lat, lp.x * sa + lp.z * ca);
    float turb = Fbm(float3(q.x * 2.5, q.y * 16.0, q.z * 2.5) + gAxis.w, 5);
    float warp = Fbm(q * 3.0 + float3(0.0, 0.0, t * 0.01), 4);
    float band = sin((lat + (turb - 0.5) * 0.12 * gP0.y + (warp - 0.5) * 0.06 * gP0.y) * gP0.x * 3.14159);
    float3 col = lerp(gC1.rgb, gC0.rgb, 0.5 + 0.5 * band);
    col *= 0.82 + 0.36 * Fbm(float3(q.x * 5.0, q.y * 44.0, q.z * 5.0) + 3.0, 4);

    // The great storm: a swirling oval riding its band.
    if (gP0.z > 0.0)
    {
        float lon = atan2(q.z, q.x);
        float dlon = lon - gP2.y;
        dlon -= 6.2831853 * round(dlon / 6.2831853);
        float2 e = float2(dlon * sqrt(saturate(1.0 - lat * lat)) / (gP0.z * 1.8), (lat - gP2.x) / gP0.z);
        float d = length(e);
        float ang = atan2(e.y, e.x) + (1.0 - d) * 3.0 - t * 0.25;
        float swirl = Fbm(float3(cos(ang) * d * 3.0, sin(ang) * d * 3.0, gAxis.w), 4);
        float storm = 1.0 - smoothstep(0.75, 1.05, d + (swirl - 0.5) * 0.2);
        col = lerp(col, gC2.rgb * (0.75 + 0.5 * swirl), storm * 0.9);
    }
    float ovals = smoothstep(0.74, 0.8, Fbm(float3(q.x * 6.0, q.y * 22.0, q.z * 6.0) + 11.0, 3));
    col = lerp(col, float3(0.95, 0.93, 0.9), ovals * 0.45 * saturate(gP0.y));

    float mu = saturate(dot(n, -rd));
    float lit = Lit(n, 0.06);
    float shade = RingShadow(pos);
    if (gP4.x > 0.0) shade *= SphereShadow(pos, m1, gP4.x);
    if (gP5.x > 0.0) shade *= SphereShadow(pos, m2, gP5.x);
    float3 outCol = col * gSunColor.rgb * lit * shade * (0.5 + 0.5 * pow(mu, 0.35));
    outCol += gC3.rgb * pow(1.0 - mu, 3.0) * gP2.z * (Lit(n, 0.3) + 0.03);
    float hotspots = smoothstep(0.45, 0.75, Fbm(q * 4.0 + float3(t * 0.02, 0.0, 0.0), 4));
    outCol += gC5.rgb * gP1.w * pow(1.0 - Lit(n, 0.2), 2.0) * (0.12 + 0.9 * hotspots * hotspots) * (0.6 + 0.4 * band);
    return outCol;
}

float4 main(PSIn i) : SV_Target
{
    float3 ro = gCamPos.xyz;
    float3 rd = CameraRay(i.uv);
    float3 m1 = Orbit(gP4), m2 = Orbit(gP5);
    float tP = SphereHit(ro, rd, 1.0);
    float t1 = gP4.x > 0.0 ? SphereHitAt(ro, rd, m1, gP4.x) : -1.0;
    float t2 = gP5.x > 0.0 ? SphereHitAt(ro, rd, m2, gP5.x) : -1.0;
    float tMin = 1e9;
    int which = 0;
    if (tP > 0.0) { tMin = tP; which = 1; }
    if (t1 > 0.0 && t1 < tMin) { tMin = t1; which = 2; }
    if (t2 > 0.0 && t2 < tMin) { tMin = t2; which = 3; }

    float3 col;
    if (which == 1)
    {
        float3 pos = ro + rd * tP;
        col = Planet(normalize(pos), rd, pos, m1, m2);
    }
    else if (which >= 2)
    {
        float3 c = which == 2 ? m1 : m2;
        float3 pos = ro + rd * tMin;
        float3 n = normalize(pos - c);
        col = MoonBall(n, which * 17.0) * SphereShadow(pos, 0.0, 1.0);
    }
    else
    {
        col = Sky(rd) + StarDisk(rd) + Halo(ro, rd, 0.0, 1.0, 0.035, gC3.rgb) * gP2.z * 0.6;
    }

    // Rings, over whatever lies behind them.
    if (gP1.y > 0.0)
    {
        float denom = dot(rd, gAxis.xyz);
        if (abs(denom) > 1e-5)
        {
            float tr = -dot(ro, gAxis.xyz) / denom;
            float3 hp = ro + rd * tr;
            float r = length(hp);
            if (tr > 0.0 && tr < tMin && r > gP1.x && r < gP1.y)
            {
                float dens = RingDensity(r);
                float alpha = saturate(dens * gP1.z);
                bool sunSide = dot(gSun.xyz, gAxis.xyz) * dot(-rd, gAxis.xyz) > 0.0;
                float light = sunSide ? 0.45 + 0.55 * abs(dot(gSun.xyz, gAxis.xyz)) : 0.3 * (1.0 - dens);
                float inShadow = SphereHit(hp, gSun.xyz, 1.0) > 0.0 ? 0.04 : 1.0;
                float3 rc = gC4.rgb * (0.7 + 0.5 * Noise3(float3(r * 40.0, gAxis.w, 2.0))) * gSunColor.rgb * light * inShadow;
                col = lerp(col, rc, alpha);
            }
        }
    }
    return float4(col, 1.0);
}
