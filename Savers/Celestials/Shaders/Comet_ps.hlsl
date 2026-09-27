// Comet: a dark little nucleus inside a glowing coma, a straight blue ion tail streaming
// directly away from the star and a broad, curved dust tail. The star shines in the sky.
// gP0: x = coma size, y = ion tail length, z = dust tail length, w = brightness
// gP1: xyz dust tail direction (unit), w = ion tail width
// gC0: coma, gC1: ion tail, gC2: dust tail, gC3: nucleus
#include "Celestial.hlsli"

// One-sided glowing tail from the origin along axis, widening with distance.
float Tail(float3 ro, float3 rd, float3 axis, float len, float width, float grow, out float s)
{
    float b = dot(rd, axis);
    float d = dot(rd, ro);
    float e = dot(axis, ro);
    float denom = max(1.0 - b * b, 1e-4);
    s = clamp((e - b * d) / denom, 0.0, len);
    float3 pa = axis * s;
    float t = max(dot(pa - ro, rd), 0.0);
    float dist = length(ro + rd * t - pa);
    float w = width + s * grow;
    return exp(-Sq(dist / w)) * exp(-s / (len * 0.4)) * smoothstep(0.0, 0.4, s) * (width / w * 0.7 + 0.3);
}

float4 main(PSIn i) : SV_Target
{
    float3 ro = gCamPos.xyz;
    float3 rd = CameraRay(i.uv);
    float t = Time();
    float3 away = -gSun.xyz;
    float3 col = Sky(rd) + StarDisk(rd);

    float tc = -dot(ro, rd);
    float b = length(ro + rd * max(tc, 0.0));
    float coma = exp(-b / (gP0.x * 0.35)) + 2.5 * exp(-b / 0.06);
    col += gC0.rgb * coma * gP0.w;

    float s1, s2;
    float ion = Tail(ro, rd, away, gP0.y, gP1.w, 0.02, s1);
    float streamers = 0.5 + 0.9 * Fbm(float3(s1 * 0.25 - t * 0.6, dot(rd, cross(away, float3(0, 1, 0))) * 30.0, gAxis.w), 4);
    col += gC1.rgb * ion * streamers * gP0.w * 1.2;
    float dust = Tail(ro, rd, normalize(gP1.xyz), gP0.z, 0.12, 0.09, s2);
    col += gC2.rgb * dust * (0.8 + 0.3 * Fbm(float3(s2 * 0.4, gAxis.w, 1.0), 3)) * gP0.w * 0.9;

    // The nucleus: a lumpy, very dark rock lit on one side, with a jet on the sunward face.
    float tN = SphereHit(ro, rd, 0.07);
    if (tN > 0.0)
    {
        float3 n = normalize(ro + rd * tN);
        float3 p = RotateAxis(n, gAxis.xyz, -gMisc.w);
        float3 g = float3(Fbm(p * 6.0, 3), Fbm(p * 6.0 + 5.0, 3), Fbm(p * 6.0 + 9.0, 3)) - 0.5;
        float3 nn = normalize(n + g * 1.2);
        col = gC3.rgb * (0.5 + 0.8 * Fbm(p * 10.0, 3)) * Lit(nn, 0.0) * gSunColor.rgb + gC0.rgb * 0.15;
    }
    float s3;
    col += gC0.rgb * Tail(ro, rd, gSun.xyz, 0.5, 0.02, 0.2, s3) * 0.6 * gP0.w;   // sunward jet
    return float4(col, 1.0);
}
