// Strange star: a neutron star of exotic quark matter. Fast boiling cells posterised into
// harsh toxic bands, needle jets along the magnetic axis, a sickly halo and a rainbow limb.
// gP0: x = rs (lensing), y = boil speed, z = cell scale, w = jet length
// gP1: x = jet width, y = jet brightness, z = halo strength, w = surface brightness
// gC0..gC3: the four bands (dark to bright)
#include "Celestial.hlsli"

float3 Band(float v)
{
    // Hard steps, not a smooth blackbody ramp.
    float s = saturate(v) * 3.999;
    float k = floor(s);
    float f = smoothstep(0.88, 1.0, frac(s));
    float3 a = k < 1.0 ? gC0.rgb : (k < 2.0 ? gC1.rgb : (k < 3.0 ? gC2.rgb : gC3.rgb));
    float3 b = k < 1.0 ? gC1.rgb : (k < 2.0 ? gC2.rgb : gC3.rgb);
    return lerp(a, b, f);
}

float4 main(PSIn i) : SV_Target
{
    float3 ro = gCamPos.xyz;
    float3 rd = CameraRay(i.uv);
    Trace tr = TraceRay(ro, rd, gP0.x, 1.0, 0.0, 0.0);
    float t = Time();
    float3 col;
    if (tr.hit)
    {
        float3 n = normalize(tr.pos);
        float3 p = RotateAxis(n, gAxis.xyz, -gMisc.w);
        float sp = gP0.y;
        float3 v1 = Voronoi(p * gP0.z, t * 2.1 * sp);
        float3 v2 = Voronoi(p * gP0.z * 2.3 + 3.0, t * 3.3 * sp);
        float churn = Fbm(p * 7.0 + float3(t * 0.5 * sp, 0.0, -t * 0.4 * sp), 3);
        float v = 1.0 - (v1.x * 0.55 + v2.x * 0.35) + (churn - 0.5) * 0.6;
        float3 c = Band(v * 0.95 + 0.05 * sin(t * 0.7));
        float seam = 1.0 - smoothstep(0.0, 0.06, v1.y - v1.x);
        float mu = saturate(dot(n, -tr.dir));
        col = c * gP1.w * (0.8 + 0.5 * pow(1.0 - mu, 2.0)) + gC3.rgb * seam * gP1.w * 1.4;
    }
    else
    {
        col = Sky(tr.dir);
        float d = max(tr.rmin - 1.0, 0.0);
        col += gC2.rgb * gP1.z * (0.5 * exp(-d * 5.0) + 0.12 * exp(-d * 1.2));
        // Rainbow fringe hugging the limb, drifting round.
        float3 cp = ro + rd * max(-dot(ro, rd), 0.0);
        float ang = atan2(dot(cp, gCamUp.xyz), dot(cp, gCamRight.xyz));
        col += Hue(frac(ang / 6.2831853 + d * 3.0 - t * 0.05)) * exp(-d * 14.0) * 0.7 * gP1.z;
    }

    // Jets (straight rays: the lensing here is weak), hidden behind the star.
    float3 axis = gAxis.xyz;
    float tHit = tr.hit ? SphereHit(ro, rd, 1.0) : -1.0;
    float j = JetGlow(ro, rd, axis, gP0.w, gP1.x, tHit, 16.0);
    float jc = JetGlow(ro, rd, axis, gP0.w, gP1.x * 0.3, tHit, 16.0);
    col += (gC2.rgb * 0.6 + gC1.rgb * 0.4) * j * gP1.y + float3(0.9, 1.0, 0.8) * jc * gP1.y * 1.5;
    return float4(col, 1.0);
}
