// White hole: a blinding core that only ever gives. An inverted accretion disk whose spiral
// arms stream outward, and spherical shock fronts expanding forever (seen as bright rings).
// gP0: x = core brightness, y = glow brightness, z = disk brightness, w = disk outer radius
// gP1: x = shock rate (per s), y = shock max radius, z = shock brightness, w = flow speed
// gC0: core colour, gC1: inner disk colour, gC2: outer disk colour, gC3: shock colour
#include "Celestial.hlsli"

float ShellPath(float b, float R, float w)
{
    float outer = sqrt(max((R + w) * (R + w) - b * b, 0.0));
    float inner = sqrt(max(R * R - b * b, 0.0));
    return (outer - inner) / w;
}

float4 main(PSIn i) : SV_Target
{
    float3 ro = gCamPos.xyz;
    float3 rd = CameraRay(i.uv);
    float t = Time();
    float tc = -dot(ro, rd);
    float b = length(ro + rd * tc);
    float tHit = SphereHit(ro, rd, 1.0);
    bool coreHit = tHit > 0.0;
    float pulse = 1.0 + 0.08 * sin(t * 0.9) + 0.05 * sin(t * 0.37 + 1.0);

    float3 col = coreHit ? 0.0 : Sky(rd);

    // Core and its glare.
    if (coreHit)
    {
        float mu = saturate(-dot(normalize(ro + rd * tHit), rd));
        col += gC0.rgb * gP0.x * pulse * (0.75 + 0.25 * mu);
    }
    else
    {
        float d = b - 1.0;
        col += gC0.rgb * gP0.y * pulse * (1.6 * exp(-d * 1.6) + 0.25 / (1.0 + d * d));
    }

    // Inverted disk in the y = 0 plane.
    if (abs(rd.y) > 1e-4)
    {
        float td = -ro.y / rd.y;
        if (td > 0.0 && (!coreHit || td < tHit))
        {
            float3 hp = ro + rd * td;
            float r = length(hp.xz);
            float rout = gP0.w;
            if (r > 1.05 && r < rout)
            {
                float phi = atan2(hp.z, hp.x);
                float lr = log(r);
                float flow = t * gP1.w;
                // Log-polar coordinates flowing outward: the pattern rides out along the arms.
                float3 q = float3(cos(phi) * 1.8, sin(phi) * 1.8, lr * 3.2 - flow);
                float streams = Fbm(q * 1.4 + gAxis.w, 5);
                float arms = pow(0.5 + 0.5 * cos(2.0 * phi - 4.5 * lr + flow * 0.6), 2.0);
                float dens = saturate((0.35 + arms) * (streams * 1.5 - 0.25));
                float fall = pow(r, -1.35) * smoothstep(1.05, 1.6, r) * (1.0 - smoothstep(rout * 0.55, rout, r));
                float3 c = lerp(gC1.rgb, gC2.rgb, saturate((r - 1.0) / (rout * 0.6)));
                // Shock ripples running out through the disk.
                float ripple = 0.0;
                [unroll]
                for (int k = 0; k < 4; ++k)
                {
                    float ph = frac(t * gP1.x + k * 0.25);
                    float R = 1.2 + ph * gP1.y;
                    ripple += exp(-Sq((r - R) / (0.25 + ph * 0.6))) * (1.0 - ph) * (1.0 - ph);
                }
                col += c * gP0.z * fall * (dens * 3.0 + 0.15) * (1.0 + 1.8 * ripple);
            }
        }
    }

    // Spherical shock fronts: thin shells, brightest at their limb.
    [unroll]
    for (int k = 0; k < 4; ++k)
    {
        float ph = frac(t * gP1.x + k * 0.25);
        float R = 1.2 + ph * gP1.y;
        float w = 0.08 + ph * 0.5;
        float fade = (1.0 - ph) * (1.0 - ph) * smoothstep(0.0, 0.06, ph);
        col += gC3.rgb * gP1.z * min(ShellPath(b, R, w), 6.0) * fade * 0.12;
    }
    return float4(col, 1.0);
}
