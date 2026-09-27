// Pulsar: a small, blazing neutron star whose tilted magnetic axis sweeps two beams around
// like a lighthouse, inside a glowing equatorial ring of wind, jets along the spin axis and
// faint supernova-remnant filaments. The beams never point straight at the camera.
// gP0: x = beam length, y = beam width, z = beam brightness, w = torus radius
// gP1: x = torus brightness, y = remnant brightness, z = jet brightness, w = unused
// gP2: xyz current magnetic (beam) axis
// gC0: star, gC1: beams, gC2: torus, gC3: remnant filaments
#include "Celestial.hlsli"

// A widening searchlight cone along +-axis, hidden behind the star past tHit.
float Beam(float3 ro, float3 rd, float3 axis, float len, float width, float tHit)
{
    float b = dot(rd, axis);
    float d = dot(rd, ro);
    float e = dot(axis, ro);
    float denom = max(1.0 - b * b, 1e-4);
    float s = clamp((e - b * d) / denom, -len, len);
    float3 pa = axis * s;
    float t = max(dot(pa - ro, rd), 0.0);
    float dist = length(ro + rd * t - pa);
    float along = abs(s);
    float w = width * (0.25 + along * 0.16);
    float I = exp(-Sq(dist / w)) * smoothstep(1.0, 1.6, along) * exp(-along / (len * 0.45));
    return (tHit > 0.0 && t > tHit) ? 0.0 : I;
}

float4 main(PSIn i) : SV_Target
{
    float3 ro = gCamPos.xyz;
    float3 rd = CameraRay(i.uv);
    float t = Time();
    float3 mag = normalize(gP2.xyz);
    float tHit = SphereHit(ro, rd, 1.0);
    float tc = -dot(ro, rd);
    float b = length(ro + rd * tc);

    float3 col;
    if (tHit > 0.0)
    {
        float3 n = normalize(ro + rd * tHit);
        float spots = exp(-Sq((1.0 - abs(dot(n, mag))) / 0.08));   // hot polar caps
        float gran = Fbm(RotateAxis(n, gAxis.xyz, -gMisc.w) * 9.0 + t * 0.3, 3);
        col = gC0.rgb * (1.6 + 0.6 * gran) + gC1.rgb * spots * 5.0;
    }
    else
    {
        // Faint remnant filaments behind everything.
        float3 fd = rd * 2.2 + gAxis.w;
        float fil = pow(saturate(Ridge(Fbm(fd, 5))), 10.0);
        float fil2 = pow(saturate(Ridge(Fbm(fd * 1.9 + 7.0, 4))), 14.0);
        col = Sky(rd) + (gC3.rgb * fil + gC1.rgb * 0.5 * fil2) * gP1.y;
        col += gC0.rgb * (0.6 * exp(-(b - 1.0) * 5.0) + 0.03 * exp(-(b - 1.0) * 0.8));
    }

    // Equatorial wind torus, seen where the ray crosses the equatorial plane.
    float denom = dot(rd, gAxis.xyz);
    if (abs(denom) > 1e-4)
    {
        float tp = -dot(ro, gAxis.xyz) / denom;
        float3 hp = ro + rd * tp;
        float r = length(hp);
        if (tp > 0.0 && (tHit < 0.0 || tp < tHit))
        {
            float wisps = Fbm(float3(hp * 0.6) + float3(0.0, t * 0.05, 0.0) + gAxis.w, 5);
            float ring = exp(-Sq((r - gP0.w) / (gP0.w * 0.28))) * (0.35 + 1.1 * wisps);
            ring += exp(-Sq((r - gP0.w * 0.45) / (gP0.w * 0.1))) * 0.35 * wisps;
            col += gC2.rgb * ring * gP1.x;
        }
    }
    // Polar jets along the spin axis, and the lighthouse beams.
    col += gC2.rgb * JetGlow(ro, rd, gAxis.xyz, gP0.w * 1.6, 0.35, tHit, 3.0) * gP1.z;
    col += gC1.rgb * Beam(ro, rd, mag, gP0.x, gP0.y, tHit) * gP0.z;
    return float4(col, 1.0);
}
