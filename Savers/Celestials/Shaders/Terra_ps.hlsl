// Earth-like world: oceans with a sun glint, continents with deserts, mountains and ice caps,
// drifting clouds, city lights on the night side and a thin blue atmosphere. One moon.
// gP0: x = sea level, y = cloud cover, z = city lights, w = cloud drift speed
// gP1: x = ice cap latitude, y = atmosphere strength, z = continent scale, w = unused
// gP4: moon (radius, orbit radius, angular speed, phase)
// gC0: deep ocean, gC1: shallow water, gC2: vegetation, gC3: atmosphere, gC4: desert, gC5: city lights
#include "Celestial.hlsli"

float3 Surface(float3 n, float3 rd, float3 pos, float3 moon)
{
    float t = Time();
    float3 lp = ToLocal(RotateAxis(n, gAxis.xyz, -gMisc.w), gAxis.xyz);
    float3 wq = lp * gP1.z + gAxis.w;
    float h = Fbm(wq + (Fbm(wq * 1.7 + 4.0, 3) - 0.5) * 0.6, 6);
    float land = smoothstep(gP0.x, gP0.x + 0.008, h);
    float elev = saturate((h - gP0.x) / (1.0 - gP0.x) * 3.0);
    float lat = abs(lp.y);

    float dry = smoothstep(0.45, 0.62, Fbm(lp * 3.0 + 13.0, 3)) * (1.0 - smoothstep(0.35, 0.6, lat));
    float3 ground = lerp(gC2.rgb, gC4.rgb, dry) * (0.8 + 0.4 * Fbm(lp * 24.0, 3));
    ground = lerp(ground, float3(0.42, 0.4, 0.38), smoothstep(0.45, 0.8, elev));
    float3 water = lerp(gC0.rgb, gC1.rgb, smoothstep(gP0.x - 0.06, gP0.x, h));
    float3 albedo = lerp(water, ground, land);
    float ice = smoothstep(gP1.x, gP1.x + 0.04, lat + (Fbm(lp * 5.0, 3) - 0.5) * 0.18);
    albedo = lerp(albedo, float3(0.9, 0.93, 0.97), ice);

    // Clouds drift a little faster than the ground and curl into storm systems.
    float3 cq = RotateAxis(lp, float3(0, 1, 0), t * gP0.w);
    float3 cw = float3(Fbm(cq * 2.0 + 1.0, 3), Fbm(cq * 2.0 + 7.0, 3), Fbm(cq * 2.0 + 3.0, 3)) - 0.5;
    float cloud = Fbm(float3(cq.x * 3.0, cq.y * 5.0, cq.z * 3.0) + cw * 1.6 + 20.0, 5);
    float clouds = smoothstep(1.0 - gP0.y - 0.05, 1.0 - gP0.y + 0.2, cloud);

    float lit = Lit(n, 0.04);
    float shade = gP4.x > 0.0 ? SphereShadow(pos, moon, gP4.x) : 1.0;
    float wet = (1.0 - land) * (1.0 - ice) * (1.0 - clouds);
    float glint = pow(saturate(dot(reflect(rd, n), gSun.xyz)), 90.0) * 2.5 * wet;
    float3 day = lerp(albedo, float3(0.95, 0.96, 0.98), clouds);
    float3 col = (day * lit + glint * step(0.0, dot(n, gSun.xyz))) * gSunColor.rgb * shade;

    // City lights where the night falls on land.
    float night = 1.0 - smoothstep(-0.05, 0.12, dot(n, gSun.xyz));
    float cities = step(0.78, Noise3(lp * 140.0)) * smoothstep(0.45, 0.65, Fbm(lp * 9.0 + 5.0, 3)) + step(0.9, Noise3(lp * 260.0)) * 0.5;
    col += gC5.rgb * cities * land * (1.0 - ice) * night * gP0.z * (1.0 - 0.75 * clouds);

    // Atmosphere: blue limb, warm towards the terminator.
    float mu = saturate(dot(n, -rd));
    float term = exp(-Sq(dot(n, gSun.xyz) / 0.18));
    float3 air = lerp(gC3.rgb, float3(1.0, 0.45, 0.2), term * 0.6);
    col += air * pow(1.0 - mu, 2.5) * Lit(n, 0.35) * gP1.y;
    col = lerp(col, gC3.rgb * Lit(n, 0.2), 0.08 * gP1.y);
    return col;
}

float4 main(PSIn i) : SV_Target
{
    float3 ro = gCamPos.xyz;
    float3 rd = CameraRay(i.uv);
    float3 moon = Orbit(gP4);
    float tP = SphereHit(ro, rd, 1.0);
    float tM = gP4.x > 0.0 ? SphereHitAt(ro, rd, moon, gP4.x) : -1.0;
    float3 col;
    if (tM > 0.0 && (tP < 0.0 || tM < tP))
    {
        float3 pos = ro + rd * tM;
        col = MoonBall(normalize(pos - moon), 3.0) * SphereShadow(pos, 0.0, 1.0);
    }
    else if (tP > 0.0)
    {
        float3 pos = ro + rd * tP;
        col = Surface(normalize(pos), rd, pos, moon);
    }
    else
    {
        col = Sky(rd) + StarDisk(rd) + Halo(ro, rd, 0.0, 1.0, 0.03, gC3.rgb) * gP1.y;
    }
    return float4(col, 1.0);
}
