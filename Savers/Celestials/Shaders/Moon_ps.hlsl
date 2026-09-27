// Moons, with their giant parent planet looming behind:
//   style 0 cratered (bump-shaded crater field, dark maria), 1 icy (Europa: reddish lineae),
//   style 2 volcanic (Io: sulfur colours, dark vents glowing at night), 3 hazy (Titan: orange smog).
// gP0: x = style, y = crater relief, z = feature scale, w = parent ring opacity
// gP1: xyz parent centre, w = parent radius
// gP2: x = parent ring inner, y = parent ring outer (0 = none), z = parent band count, w = unused
// gC0/gC1: moon colours, gC2: feature colour, gC3: haze / atmosphere, gC4/gC5: parent bands
#include "Celestial.hlsli"

static const float3 kParentAxis = float3(0.08, 1.0, 0.05);

float3 MoonSurface(float3 n, float3 rd, float3 pos)
{
    float t = Time();
    float seed = gAxis.w;
    float3 p = RotateAxis(n, gAxis.xyz, -gMisc.w);
    int style = (int)(gP0.x + 0.5);
    float3 albedo;
    float3 emit = 0.0;
    float3 nn = n;
    float wrap = 0.0;
    if (style == 0)
    {
        float h = 0.0;
        float3 g1 = 0.0, g2 = 0.0, g3 = 0.0;
        Craters(p, 2.6, seed, 0.75, h, g1);
        Craters(p, 7.0, seed + 11.0, 0.85, h, g2);
        Craters(p, 17.0, seed + 23.0, 0.9, h, g3);
        nn = Bump(n, g1 * 0.2 + g2 * 0.12 + g3 * 0.07, gP0.y);
        float maria = smoothstep(0.52, 0.6, Fbm(p * 1.5 + seed, 5));
        albedo = lerp(gC0.rgb, gC1.rgb, maria) * (0.8 + 0.35 * Fbm(p * 14.0, 3));
    }
    else if (style == 1)
    {
        float h = 0.0;
        float3 g = 0.0;
        Craters(p, 12.0, seed, 0.25, h, g);
        nn = Bump(n, g * 0.05, 1.0);
        float l1 = pow(saturate(Ridge(Noise3(p * 3.1 + seed))), 30.0);
        float l2 = pow(saturate(Ridge(Noise3(p * 6.7 + seed + 4.0))), 36.0);
        float l3 = pow(saturate(Ridge(Noise3(p * 1.5 + seed + 9.0))), 44.0);
        float lines = saturate(l1 + 0.7 * l2 + 1.3 * l3);
        float chaos = smoothstep(0.56, 0.7, Fbm(p * 2.2 + seed + 2.0, 5));
        albedo = lerp(gC0.rgb, gC1.rgb, chaos * 0.85);
        albedo = lerp(albedo, gC2.rgb, lines * 0.85);
    }
    else if (style == 2)
    {
        float3 v = Voronoi(p * gP0.z + seed, 0.0);
        float active = step(0.55, v.z);
        float vent = (1.0 - smoothstep(0.05, 0.15, v.x)) * active;
        float ring = exp(-Sq((v.x - 0.27) / 0.09)) * active;
        float3 base = lerp(gC0.rgb, gC1.rgb, Fbm(p * 4.0 + seed, 5));
        base = lerp(base, float3(0.95, 0.94, 0.86), smoothstep(0.62, 0.75, Fbm(p * 7.0 + 3.0, 4)) * 0.6);
        albedo = lerp(base, gC2.rgb, ring * 0.75);
        albedo = lerp(albedo, float3(0.06, 0.05, 0.04), vent);
        emit = float3(1.0, 0.33, 0.05) * vent * (0.6 + 0.7 * Noise3(p * 30.0 + t * 0.5)) * 2.2;
    }
    else
    {
        float3 lp = ToLocal(p, gAxis.xyz);
        float band = Fbm(float3(lp.x * 1.5, lp.y * 6.0, lp.z * 1.5) + seed, 4);
        albedo = gC0.rgb * (0.7 + 0.6 * band);
        wrap = 0.3;
    }
    float lit = Lit(nn, wrap) * (style == 0 ? saturate(dot(n, gSun.xyz) * 4.0 + 0.2) : 1.0);
    float shade = SphereShadow(pos, gP1.xyz, gP1.w);
    float night = 1.0 - Lit(n, 0.1);
    float3 col = albedo * lit * shade * gSunColor.rgb + emit * (0.25 + 0.75 * night);
    float mu = saturate(dot(n, -rd));
    if (style == 3) col += gC3.rgb * pow(1.0 - mu, 2.0) * (Lit(n, 0.45) + 0.15) * 0.7;
    return col;
}

float3 Parent(float3 pos, float3 rd)
{
    float3 n = normalize(pos - gP1.xyz);
    float3 lp = ToLocal(n, normalize(kParentAxis));
    float turb = Fbm(float3(lp.x * 2.5, lp.y * 16.0, lp.z * 2.5) + gAxis.w + 40.0, 5);
    float band = sin((lp.y + (turb - 0.5) * 0.1) * gP2.z * 3.14159);
    float3 col = lerp(gC5.rgb, gC4.rgb, 0.5 + 0.5 * band) * (0.85 + 0.3 * Fbm(float3(lp.x * 5.0, lp.y * 40.0, lp.z * 5.0), 3));
    float mu = saturate(dot(n, -rd));
    float lit = Lit(n, 0.05) * SphereShadow(pos, 0.0, 1.0);   // the moon's own shadow can cross it
    return col * lit * gSunColor.rgb * (0.55 + 0.45 * pow(mu, 0.35)) + gC4.rgb * pow(1.0 - mu, 3.0) * Lit(n, 0.3) * 0.3;
}

float4 main(PSIn i) : SV_Target
{
    float3 ro = gCamPos.xyz;
    float3 rd = CameraRay(i.uv);
    float tM = SphereHit(ro, rd, 1.0);
    float tP = SphereHitAt(ro, rd, gP1.xyz, gP1.w);
    float tMin = 1e9;
    float3 col;
    if (tM > 0.0)
    {
        float3 pos = ro + rd * tM;
        col = MoonSurface(normalize(pos), rd, pos);
        tMin = tM;
    }
    else if (tP > 0.0)
    {
        col = Parent(ro + rd * tP, rd);
        tMin = tP;
    }
    else
    {
        col = Sky(rd) + StarDisk(rd) + Halo(ro, rd, gP1.xyz, gP1.w, gP1.w * 0.02, gC4.rgb) * 0.3;
    }
    if (tM <= 0.0 && gP0.x > 2.5) col += Halo(ro, rd, 0.0, 1.0, 0.07, gC3.rgb) * 0.9;

    // The parent's rings.
    if (gP2.y > 0.0)
    {
        float3 ax = normalize(kParentAxis);
        float denom = dot(rd, ax);
        if (abs(denom) > 1e-5)
        {
            float tr = dot(gP1.xyz - ro, ax) / denom;
            float3 hp = ro + rd * tr;
            float r = length(hp - gP1.xyz) / gP1.w;
            if (tr > 0.0 && tr < tMin && r > gP2.x && r < gP2.y)
            {
                float x = (r - gP2.x) / (gP2.y - gP2.x);
                float dens = (0.55 + 0.45 * Noise3(float3(r * 30.0, gAxis.w, 0.0))) * (1.0 - 0.9 * exp(-Sq((x - 0.63) / 0.03)));
                dens *= smoothstep(0.0, 0.05, x) * (1.0 - smoothstep(0.93, 1.0, x));
                float inShadow = SphereHit(hp - gP1.xyz, gSun.xyz, gP1.w) > 0.0 ? 0.05 : 1.0;
                col = lerp(col, gC4.rgb * 1.1 * gSunColor.rgb * inShadow * (0.4 + 0.6 * abs(dot(gSun.xyz, ax))), saturate(dens * gP0.w));
            }
        }
    }
    return float4(col, 1.0);
}
