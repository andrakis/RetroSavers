#include "Common.hlsli"
#include "Lighting.hlsli"
#include "Fish.hlsli"

// Procedural fish colouring by uv: u < 1 is the body (u along the body, v = 0 belly .. 1 back),
// 1 <= u < 1.25 the tail fin, 1.25 <= u < 2 the other fins, u >= 2 the eye (v = 0 at the pupil).

float Hash(float2 p)
{
    return frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453);
}

float3 BodyColor(float u, float v)
{
    float3 base = gBase.rgb;
    int mode = (int)gPattern.x;
    if (mode == 1)
    {
        // Vertical bands: gPattern.y bands with half-width gPattern.z and a dark edge.
        float n = gPattern.y;
        float d = abs(frac(u * n) - 0.5) / n;
        float w = gPattern.z;
        float band = 1.0 - smoothstep(w, w + 0.012, d);
        float edge = (1.0 - smoothstep(w + 0.012, w + 0.035, d)) - band;
        float inBody = smoothstep(0.03, 0.08, u) * (1.0 - smoothstep(0.93, 0.98, u));
        base = lerp(base, gAccent.rgb, band * inBody);
        base = lerp(base, float3(0.03, 0.03, 0.04), edge * inBody * 0.85);
    }
    else if (mode == 2)
    {
        // Horizontal band between v = gPattern.y and gPattern.z, plus a dark tail joint.
        float band = smoothstep(gPattern.y - 0.03, gPattern.y + 0.03, v) * (1.0 - smoothstep(gPattern.z - 0.03, gPattern.z + 0.03, v));
        band *= smoothstep(0.08, 0.16, u) * (1.0 - smoothstep(0.8, 0.86, u));
        float joint = smoothstep(0.86, 0.9, u);
        base = lerp(base, gAccent.rgb, max(band, joint));
    }
    else if (mode == 3)
    {
        // Spots: one per cell of a gPattern.y-wide grid, radius gPattern.z of a cell.
        float2 c = float2(u, v) * float2(gPattern.y, gPattern.y * 0.5);
        float2 ic = floor(c), fc = frac(c);
        float h = Hash(ic);
        float2 o = float2(Hash(ic + 7.1), Hash(ic + 3.3)) * 0.5 + 0.25;
        float d = length(fc - o);
        float spot = (1.0 - smoothstep(gPattern.z * 0.7, gPattern.z, d)) * step(0.3, h);
        base = lerp(base, gAccent.rgb, spot);
    }
    else if (mode == 4)
    {
        // Gradient: accent belly to base back.
        base = lerp(gAccent.rgb, base, smoothstep(0.2, 0.7, v));
    }
    else if (mode == 5)
    {
        // Neon tetra: iridescent stripe along the upper flank, red lower rear half.
        float stripe = smoothstep(0.5, 0.56, v) * (1.0 - smoothstep(0.74, 0.8, v)) * smoothstep(0.05, 0.12, u);
        float red = (1.0 - smoothstep(0.48, 0.54, v)) * smoothstep(0.35, 0.5, u);
        base = lerp(base, gTail.rgb, red);
        base = lerp(base, gAccent.rgb, stripe);
    }
    // Belly lightening.
    base = lerp(base, base * 0.5 + 0.5, smoothstep(0.45, 0.12, v) * gPattern.w);
    return base;
}

float4 main(PhongPSIn i, bool front : SV_IsFrontFace) : SV_Target
{
    float3 n = normalize(i.nrm);
    if (!front) n = -n;                                  // fins are single quads drawn two-sided
    float u = i.uv.x, v = i.uv.y;
    float3 base;
    if (u >= 2.0)
    {
        float3 iris = lerp(float3(0.9, 0.78, 0.35), gBase.rgb, 0.25);
        base = v < 0.32 ? float3(0.02, 0.02, 0.02) : iris * (0.7 + 0.3 * smoothstep(0.32, 0.45, v));
    }
    else if (u >= 1.0)
    {
        float3 fin = u < 1.25 ? gTail.rgb : gFin.rgb;
        if (u < 1.25 && gPattern2.y > 0.5) fin = lerp(gTail.rgb, gAccent.rgb, smoothstep(0.1, 0.9, v));
        fin *= 0.92 + 0.08 * sin(v * 60.0);              // faint fin rays
        base = fin;
    }
    else
    {
        base = BodyColor(u, v);
    }

    float3 c = ShadePhong(base, n, i.wpos, gPattern2.z, gPattern2.w);
    float3 V = normalize(gEyePos.xyz - i.wpos);
    float rim = pow(1.0 - saturate(dot(n, V)), 3.0) * gPattern2.x;
    c += gLightColor.rgb * rim * lerp(base, float3(1.0, 1.0, 1.0), 0.5);
    return float4(ApplyFog(c, i.wpos), 1.0);
}
