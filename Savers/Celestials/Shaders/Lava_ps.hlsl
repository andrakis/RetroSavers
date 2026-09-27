// Lava world: a rocky planet hugging its star. A dark crust split into plates floats on
// glowing seas of molten rock; the star fills a huge part of the sky.
// gP0: x = crust plate scale, y = lava amount, z = lava glow, w = flow speed
// gC0: lava, gC1: crust, gC2: hot haze
#include "Celestial.hlsli"

float4 main(PSIn i) : SV_Target
{
    float3 ro = gCamPos.xyz;
    float3 rd = CameraRay(i.uv);
    float t = Time();
    float tP = SphereHit(ro, rd, 1.0);
    float3 col;
    if (tP > 0.0)
    {
        float3 n = normalize(ro + rd * tP);
        float3 lp = RotateAxis(n, gAxis.xyz, -gMisc.w);
        float sp = gP0.w;
        float3 v = Voronoi(lp * gP0.x + (Fbm(lp * 3.0, 3) - 0.5) * 0.8, t * 0.04 * sp);
        float cracks = (1.0 - smoothstep(0.0, 0.035, v.y - v.x)) * 0.7;
        float seas = smoothstep(1.0 - gP0.y, 1.0 - gP0.y + 0.08, Fbm(lp * 2.0 + gAxis.w + float3(0.0, t * 0.006 * sp, 0.0), 5));
        float flow = Fbm(lp * 9.0 + float3(t * 0.05 * sp, -t * 0.07 * sp, 0.0), 4);
        float lava = saturate(max(cracks * (0.5 + flow), seas * (0.6 + 0.7 * flow)));

        float3 crust = gC1.rgb * (0.5 + 0.9 * Fbm(lp * 22.0, 3)) * (0.7 + 0.6 * v.z);
        float lit = Lit(n, 0.0);
        float3 hot = gC0.rgb * (0.6 + 0.9 * flow);
        hot = lerp(hot, float3(1.0, 0.75, 0.35), smoothstep(0.65, 0.9, flow) * seas * 0.7);
        col = crust * lit * gSunColor.rgb * (1.0 - lava) + hot * lava * gP0.z;
        float mu = saturate(dot(n, -rd));
        col += gC2.rgb * pow(1.0 - mu, 3.0) * (0.5 + Lit(n, 0.3));
    }
    else
    {
        col = Sky(rd) + StarDisk(rd) + Halo(ro, rd, 0.0, 1.0, 0.05, gC2.rgb) * 0.8;
    }
    return float4(col, 1.0);
}
