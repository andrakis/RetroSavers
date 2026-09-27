// Ordinary stars (red dwarf, sun-like, blue giant, red giant): Stargazer v1's domain-warped
// photosphere with granulation, spots and limb darkening, plus a streamer corona.
// Flares, CMEs, prominences and winds are particles drawn over this.
// gP0: x = surface scale, y = surface speed, z = turbulence, w = spot amount
// gP1: x = limb darkening, y = corona brightness, z = corona falloff, w = streamer amount
// gP2: x = granulation, y = surface brightness, z = pulsation amount, w = granule scale
// gC0: bright surface, gC1: dark surface, gC2: corona colour
#include "Celestial.hlsli"

float4 main(PSIn i) : SV_Target
{
    float3 ro = gCamPos.xyz;
    float3 rd = CameraRay(i.uv);
    float t = Time();
    float radius = 1.0 + gP2.z * sin(t * 0.45);
    float tc = -dot(ro, rd);
    float3 cp = ro + rd * tc;
    float b = length(cp);
    float tHit = SphereHit(ro, rd, radius);

    float3 col;
    if (tHit > 0.0)
    {
        float3 n = normalize(ro + rd * tHit);
        float3 lp = RotateAxis(n, gAxis.xyz, -gMisc.w);
        float3 p = lp * gP0.x;
        float t1 = t * 0.025 * gP0.y, t2 = t * 0.016 * gP0.y;
        float turb = gP0.z;
        float3 w1 = float3(Fbm(p + float3(t1, 0.0, 0.0), 4),
                           Fbm(p + float3(0.0, t2, 5.2), 4),
                           Fbm(p + float3(t2 * 0.5, t1 * 0.3, 3.1), 4)) * 2.2 - 1.1;
        float3 w2 = float3(Fbm(p + w1 * 1.8 * turb + float3(t2 * 0.9, t1 * 0.4, t2 * 0.6), 4),
                           Fbm(p + w1 * 1.8 * turb + float3(t1 * 0.6, -t2 * 0.7, t1 * 0.3) + 3.7, 4),
                           Fbm(p + w1 * 1.8 * turb + float3(-t2 * 0.4, t2 * 0.8, -t1 * 0.5) + 7.3, 4)) * 2.2 - 1.1;
        float nse = saturate(Fbm(p + w2 * 2.5 * turb + float3(-t1 * 0.4, t2 * 0.8, t1 * 0.6), 5));
        float bright = smoothstep(0.25, 0.75, nse);
        col = lerp(gC1.rgb * 0.25, gC0.rgb * 1.3, bright);
        float flare = pow(max(0.0, nse - 0.58) / 0.42, 2.2);
        col += flare * (gC0.rgb * 0.9 + float3(0.25, 0.15, 0.05));

        // Granulation: small boiling cells with darker lanes.
        float3 g = Voronoi(lp * gP2.w, t * 0.35 * gP0.y);
        float lane = 1.0 - smoothstep(0.0, 0.12, g.y - g.x);
        col *= 1.0 - gP2.x * (0.55 * lane + 0.25 * g.x);

        // Spots with a penumbra.
        float s = Fbm(lp * 1.9 + 11.0 + float3(t * 0.004, 0.0, 0.0), 4);
        float umbra = smoothstep(0.64, 0.69, s) * gP0.w;
        float pen = smoothstep(0.585, 0.64, s) * gP0.w;
        col *= (1.0 - 0.45 * pen) * (1.0 - 0.8 * umbra);

        float mu = saturate(dot(n, -rd));
        col *= saturate(1.0 - gP1.x * (1.0 - pow(mu, 0.5)));
        col *= gP2.y;
        // A thin bright limb halo in front of the disk edge.
        col += gC2.rgb * gP1.y * pow(1.0 - mu, 6.0) * 0.6;
    }
    else
    {
        col = Sky(rd);
        float d = max(b - radius, 0.0);
        float3 dir = normalize(cp);
        // Streamers: noise on direction only, so the structure radiates outward.
        float3 sd = RotateAxis(dir, gAxis.xyz, -gMisc.w * 0.5);
        float rays = Fbm(sd * 6.0 + float3(0.0, 0.0, t * 0.02), 4);
        float rays2 = Fbm(sd * 17.0 + 3.3, 3);
        float streak = pow(saturate(rays * 1.4 - 0.2), 2.0) * (0.6 + 0.8 * rays2);
        float falloff = exp(-d / gP1.z);
        float corona = falloff * (1.0 - gP1.w + gP1.w * streak * 2.2);
        col += gC2.rgb * gP1.y * (corona + 1.2 * exp(-d * 18.0 / radius));
    }
    return float4(col, 1.0);
}
