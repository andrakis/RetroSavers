// Schwarzschild black hole (rs = 1) with a thin Keplerian accretion disk in the y = 0 plane.
// Every pixel's photon is marched back through the curved space; each crossing of the disk
// plane adds (and is partly blocked by) disk emission, so the far side of the disk is lensed
// over and under the shadow, and the photon ring and Einstein ring appear by themselves.
// gP0: x = disk inner radius, y = outer radius, z = disk brightness, w = swirl rate
// gC0: hot disk colour, gC1: warm disk colour, gC2: cool (outer) disk colour
#include "Celestial.hlsli"

float3 DiskTint(float t)
{
    return t < 0.5 ? lerp(gC2.rgb, gC1.rgb, t * 2.0) : lerp(gC1.rgb, gC0.rgb, t * 2.0 - 1.0);
}

float TempProfile(float r, float rin)
{
    return pow(max(r, 1e-3), -0.75) * pow(saturate(1.0 - sqrt(rin / r)), 0.25);
}

float4 DiskSample(float3 hp, float3 rayDir)
{
    float rin = gP0.x, rout = gP0.y;
    float r = length(hp.xz);
    float x = saturate((r - rin) / (rout - rin));

    // Differential (Keplerian) rotation: inner rings lap the outer ones, winding the clumps into streaks.
    float omega = gP0.w * pow(r, -1.5);
    float a = -omega * Time();
    float ca = cos(a), sa = sin(a);
    float2 q = float2(hp.x * ca - hp.z * sa, hp.x * sa + hp.z * ca);
    float clumps = Fbm(float3(q * 0.9, r * 0.35 + gAxis.w), 5);
    float rings = Noise3(float3(r * 4.0, gAxis.w, 0.5)) * 0.6 + Noise3(float3(r * 11.0, gAxis.w + 3.0, 0.5)) * 0.4;
    float dens = saturate(clumps * 1.6 - 0.35) * (0.45 + 0.9 * rings);
    float edge = smoothstep(0.0, 0.05, x) * (1.0 - smoothstep(0.45, 1.0, x));

    float temp = TempProfile(r, rin) / TempProfile(1.3611 * rin, rin);

    // Relativistic beaming: gas orbits counter-clockwise seen from +y.
    float3 vdir = normalize(float3(-hp.z, 0.0, hp.x));
    float v = sqrt(0.5 / r);
    float gamma = rsqrt(1.0 - v * v);
    float cosT = dot(vdir, -rayDir);
    float D = 1.0 / (gamma * (1.0 - v * cosT));
    float g = D * sqrt(saturate(1.0 - 1.0 / r));
    float beam = pow(max(g, 0.0), 2.6);

    float3 col = DiskTint(saturate(temp * g * 0.95)) * (temp * temp * 1.6 + 0.05) * beam * gP0.z;
    float alpha = saturate(dens * edge * 1.5);
    return float4(col * alpha * (0.6 + 0.8 * dens), alpha);
}

float4 main(PSIn i) : SV_Target
{
    float3 rd = CameraRay(i.uv);
    float3 pos = gCamPos.xyz;
    float3 vel = rd;
    float3 hv = cross(pos, vel);
    float h2 = dot(hv, hv);
    float3 col = 0.0;
    float trans = 1.0;
    bool captured = false;
    bool disk = gMisc.y > 0.5;
    int steps = (int)gMisc.x;

    [loop]
    for (int k = 0; k < steps; ++k)
    {
        float r2 = dot(pos, pos);
        float r = sqrt(r2);
        if (r < 1.0) { captured = true; break; }
        if (r > 80.0 && dot(pos, vel) > 0.0) break;
        if (trans < 0.02) break;
        float dt = clamp(0.06 * r * (r - 0.85), 0.012, 1.6);
        float3 acc = -1.5 * h2 * pos / (r2 * r2 * r);
        vel += acc * dt;
        float3 np = pos + vel * dt;
        if (disk && pos.y * np.y < 0.0)
        {
            float f = pos.y / (pos.y - np.y);
            float3 hp = lerp(pos, np, f);
            float rr = length(hp.xz);
            if (rr > gP0.x && rr < gP0.y)
            {
                float4 d = DiskSample(hp, normalize(vel));
                col += trans * d.rgb;
                trans *= 1.0 - d.a;
            }
        }
        pos = np;
    }
    if (!captured) col += trans * Sky(normalize(vel));
    return float4(col, 1.0);
}
