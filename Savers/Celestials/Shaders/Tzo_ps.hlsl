// Thorne-Zytkow object: a red supergiant that swallowed a neutron star. A semi-transparent,
// slowly boiling envelope; where it thins, a blazing blue-white core shows through, and the
// envelope is stained with cold blue patches that should not be there.
// gP0: x = core radius, y = core brightness, z = scattered core light, w = plume scale
// gP1: x = envelope brightness, y = anomaly amount, z = haze strength, w = heartbeat period (s)
// gC0: plume colour, gC1: lane colour, gC2: anomaly colour, gC3: core colour
#include "Celestial.hlsli"

float Heartbeat(float t, float period)
{
    float u = frac(t / period);
    return exp(-Sq((u - 0.10) / 0.05)) + 0.6 * exp(-Sq((u - 0.24) / 0.06));
}

float4 main(PSIn i) : SV_Target
{
    float3 ro = gCamPos.xyz;
    float3 rd = CameraRay(i.uv);
    float t = Time();
    float tc = -dot(ro, rd);
    float b = length(ro + rd * tc);
    float beat = 1.0 + 0.35 * Heartbeat(t, gP1.w);
    float coreR = gP0.x;
    float coreLight = gP0.y * beat * (exp(-pow(b / coreR, 2.0)) + 0.08 * exp(-b / (coreR * 2.0)));
    float scatter = gP0.z * beat * exp(-b * b * 9.0);

    float3 col;
    float tHit = SphereHit(ro, rd, 1.0);
    if (tHit > 0.0)
    {
        float3 n = normalize(ro + rd * tHit);
        float3 p = RotateAxis(n, gAxis.xyz, -gMisc.w);
        float sc = gP0.w;
        // Convective plumes: huge slow cells warped by fbm, bright centres, dark lanes.
        float3 w = float3(Fbm(p * 1.6 + t * 0.020, 4), Fbm(p * 1.6 + 5.2 - t * 0.017, 4), Fbm(p * 1.6 + 9.1, 4)) - 0.5;
        float3 v = Voronoi(p * sc + w * 1.4, t * 0.11);
        float plume = 1.0 - smoothstep(0.0, 0.9, v.x);
        float lanes = smoothstep(0.0, 0.45, v.y - v.x);
        float detail = Fbm(p * 7.0 + w * 2.0 + t * 0.03, 4);
        float cellHeat = 0.55 + 0.9 * frac(v.z * 7.13 + t * 0.01);
        float3 surf = lerp(gC1.rgb, gC0.rgb, saturate(plume * lanes * cellHeat)) * (0.5 + 0.9 * detail);

        // Colour anomalies where the neutron core disturbs the envelope.
        float anomaly = smoothstep(0.66, 0.78, Fbm(p * 1.9 + w + float3(0.0, t * 0.025, 0.0), 5)) * gP1.y;
        surf = lerp(surf, gC2.rgb * (0.7 + 0.8 * detail), anomaly * 0.8);

        float mu = saturate(dot(n, -rd));
        surf *= 0.3 + 0.7 * pow(mu, 0.55);

        // The envelope is thinner in places: let the core through there.
        float thin = smoothstep(0.38, 0.82, Fbm(p * 2.3 - float3(t * 0.012, 0.0, 0.0), 4));
        float see = lerp(0.10, 0.85, thin);
        col = surf * gP1.x * (1.0 - 0.5 * see) + gC3.rgb * (coreLight * see + scatter * (0.4 + see));
    }
    else
    {
        col = Sky(rd);
        float d = b - 1.0;
        col += gC0.rgb * gP1.z * (0.45 * exp(-d * 5.0) + 0.08 * exp(-d * 1.1));
    }
    return float4(col, 1.0);
}
