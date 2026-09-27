// Mass-transfer binary: a swollen star pulled into a teardrop by its white-dwarf companion
// (at the origin), which wears a hot accretion disk with a bright spot where the stream lands.
// The stream itself is particles.
// gP0: xyz donor centre, w = donor radius
// gP1: x = disk inner, y = disk outer, z = disk brightness, w = tidal stretch
// gC0: donor bright, gC1: donor dark, gC2: disk hot, gC3: disk cool, gC4: white dwarf
#include "Celestial.hlsli"

float DonorDist(float3 p)
{
    float3 v = p - gP0.xyz;
    float r = length(v);
    float toward = dot(v / max(r, 1e-4), normalize(-gP0.xyz));
    return r - gP0.w * (1.0 + gP1.w * pow(saturate(toward), 6.0));
}

float4 main(PSIn i) : SV_Target
{
    float3 ro = gCamPos.xyz;
    float3 rd = CameraRay(i.uv);
    float t = Time();
    float3 D = gP0.xyz;

    // March the teardrop inside its bounding sphere.
    float tD = -1.0;
    float bound = gP0.w * (1.0 + gP1.w) * 1.05;
    float tb = SphereHitAt(ro, rd, D, bound);
    if (tb > 0.0 || length(ro - D) < bound)
    {
        float tt = max(tb, 0.0);
        [loop]
        for (int k = 0; k < 48; ++k)
        {
            float d = DonorDist(ro + rd * tt);
            if (d < 0.002) { tD = tt; break; }
            tt += d * 0.7;
            if (tt > tb + bound * 2.2) break;
        }
    }

    float3 col = Sky(rd);
    float tNear = 1e9;
    if (tD > 0.0)
    {
        float3 pos = ro + rd * tD;
        float e = 0.01;
        float3 n = normalize(float3(DonorDist(pos + float3(e, 0, 0)) - DonorDist(pos - float3(e, 0, 0)),
                                    DonorDist(pos + float3(0, e, 0)) - DonorDist(pos - float3(0, e, 0)),
                                    DonorDist(pos + float3(0, 0, e)) - DonorDist(pos - float3(0, 0, e))));
        float3 lp = (pos - D) / gP0.w;
        float nse = Fbm(lp * 3.0 + float3(t * 0.04, 0.0, t * 0.03) + gAxis.w, 5);
        float gdark = smoothstep(0.2, 1.0, dot(normalize(pos - D), normalize(-D)));   // cooler at the tip
        float3 c = lerp(gC1.rgb, gC0.rgb, smoothstep(0.3, 0.7, nse)) * (1.0 - 0.45 * gdark);
        float mu = saturate(dot(n, -rd));
        col = c * (0.35 + 0.65 * pow(mu, 0.5)) * 1.3;
        tNear = tD;
    }
    else
    {
        float3 oc = ro - D;
        float b = length(oc + rd * max(-dot(oc, rd), 0.0));
        col += gC0.rgb * 0.6 * exp(-(b - gP0.w) / (gP0.w * 0.15)) * step(gP0.w, b);
    }

    // Accretion disk in the orbital (y = 0) plane.
    if (abs(rd.y) > 1e-5)
    {
        float tp = -ro.y / rd.y;
        float3 hp = ro + rd * tp;
        float r = length(hp.xz);
        if (tp > 0.0 && tp < tNear && r > gP1.x && r < gP1.y)
        {
            float x = (r - gP1.x) / (gP1.y - gP1.x);
            float a = -t * 0.9 * pow(r, -1.5);
            float2 q = float2(hp.x * cos(a) - hp.z * sin(a), hp.x * sin(a) + hp.z * cos(a));
            float dens = saturate(Fbm(float3(q * 2.2, r + gAxis.w), 4) * 1.5 - 0.2);
            float edge = smoothstep(0.0, 0.08, x) * (1.0 - smoothstep(0.7, 1.0, x));
            float heat = pow(saturate(gP1.x / r), 0.75);
            // Hot spot where the stream strikes the rim, trailing the donor's direction.
            float dAng = atan2(hp.z, hp.x) - atan2(D.z, D.x) + 0.55;
            dAng -= 6.2831853 * round(dAng / 6.2831853);
            float spot = exp(-Sq(dAng / 0.35) - Sq((x - 0.85) / 0.12));
            float3 c = lerp(gC3.rgb, gC2.rgb, heat) * (heat * 4.0 + 0.3) + gC2.rgb * spot * 2.5;
            float alpha = saturate(dens * edge * 1.3 + spot * 0.5);
            col = lerp(col, c * (0.5 + dens), alpha);
            tNear = min(tNear, tp);
        }
    }

    // The white dwarf: a pinpoint with a glow, unless something is in front of it.
    float tc = -dot(ro, rd);
    float b = length(ro + rd * tc);
    if (tc < tNear) col += gC4.rgb * (30.0 * exp(-Sq(b / 0.06)) + 0.5 * exp(-b / 0.3));
    return float4(col, 1.0);
}
