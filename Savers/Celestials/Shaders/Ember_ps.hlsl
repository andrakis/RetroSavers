// Rogue neutron star gone cold: a dark cracked crust with dull red seams, a faint infrared
// halo and weak lensing of the stars around its limb.
// gP0: x = rs (lensing), y = crack glow, z = halo strength, w = crack scale
// gC0: crack colour, gC1: crust colour, gC2: halo colour
#include "Celestial.hlsli"

float4 main(PSIn i) : SV_Target
{
    float3 rd = CameraRay(i.uv);
    Trace tr = TraceRay(gCamPos.xyz, rd, gP0.x, 1.0, 0.0, 0.0);
    float t = Time();
    float3 col;
    if (tr.hit)
    {
        float3 n = normalize(tr.pos);
        float3 p = RotateAxis(n, gAxis.xyz, -gMisc.w);
        float3 big = Voronoi(p * gP0.w, 0.0);
        float3 small = Voronoi(p * gP0.w * 2.7 + 5.0, 0.0);
        float crack = 1.0 - smoothstep(0.0, 0.07, big.y - big.x);
        float fine = 1.0 - smoothstep(0.0, 0.04, small.y - small.x);
        // Heat seeps unevenly along the seams and slowly breathes.
        float heat = Noise3(p * 5.0 + float3(0.0, t * 0.07, 0.0));
        heat = smoothstep(0.25, 0.85, heat) * (0.75 + 0.25 * sin(t * 0.4 + big.z * 20.0));
        float3 glow = gC0.rgb * gP0.y * (crack * (0.35 + heat) + fine * 0.3 * heat);

        // Crust plates: dark, grainy, each a slightly different temperature.
        float grain = Fbm(p * 18.0, 3);
        float3 crust = gC1.rgb * (0.4 + 0.9 * grain) * (0.6 + 0.8 * big.z);
        float mu = saturate(dot(n, -tr.dir));
        float rim = pow(1.0 - mu, 3.0);
        col = crust + glow + gC2.rgb * rim * 0.15;
    }
    else
    {
        col = Sky(tr.dir);
        float d = max(tr.rmin - 1.0, 0.0);
        col += gC2.rgb * gP0.z * (0.18 * exp(-d * 3.0) + 0.05 * exp(-d * 0.8));
    }
    return float4(col, 1.0);
}
