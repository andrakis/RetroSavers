// Boson star: no surface and no horizon, only a soft ball of mass that light passes straight
// through. The sky is bent into a ring and a small inverted image, and a colourless
// "anti-light" halo drains colour and brightness from whatever sits behind it.
// gP0: x = core radius, y = mass (rs), z = halo strength, w = ripple strength
// gC0: halo fringe tint (very pale)
#include "Celestial.hlsli"

float4 main(PSIn i) : SV_Target
{
    float3 rd = CameraRay(i.uv);
    float a = gP0.x;
    Trace tr = TraceRay(gCamPos.xyz, rd, gP0.y, 0.0, a, gP0.w);
    float3 col = Sky(tr.dir);
    float t = Time();

    // Where the ray passed: the closest-approach point tells how deep into the star it went.
    float depth = tr.rmin / a;
    float inner = exp(-depth * depth * 0.6);
    float ring = exp(-Sq((depth - 1.7) / 0.9));

    // Anti-light: desaturate and darken, most strongly in a ring just outside the core.
    float lum = dot(col, float3(0.3, 0.59, 0.11));
    col = lerp(col, lum.xxx, saturate(0.85 * inner + 0.5 * ring));
    col *= 1.0 - gP0.z * (0.55 * ring + 0.25 * inner);

    // Faint shimmer: slow, colourless, with the barest iridescent fringe.
    float3 d = tr.dir;
    float sh = Fbm(d * 7.0 + float3(0.0, t * 0.09, t * 0.05), 4);
    float fringe = exp(-Sq((depth - 2.6) / 0.35));
    float3 irid = lerp(float3(0.6, 0.6, 0.62), Hue(frac(sh * 1.5 + t * 0.02)), 0.18);
    col += gP0.z * (0.010 * inner + 0.022 * ring * sh + 0.030 * fringe * sh) * irid * gC0.rgb;
    return float4(col, 1.0);
}
