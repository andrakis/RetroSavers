// Planetary nebula: the fluorescing shell a dying star has blown off, raymarched as emission.
// Teal oxygen inside, red hydrogen/nitrogen at the rim, dark cometary knots, optional bipolar
// lobes and a second inner shell, and the tiny white-hot star at the centre.
// gP0: x = shell radius, y = shell thickness (fraction), z = bipolar lobes, w = density
// gP1: x = inner shell ratio (0 = none), y = knots, z = brightness, w = march steps
// gC0: inner (oxygen), gC1: outer (hydrogen), gC2: knot rims, gC3: central star
#include "Celestial.hlsli"

float4 main(PSIn i) : SV_Target
{
    float3 ro = gCamPos.xyz;
    float3 rd = CameraRay(i.uv);
    float seed = gAxis.w;
    float R = gP0.x;
    float bound = R * (1.0 + gP0.z * 1.6) * 1.35;
    float3 emission = 0.0;
    float trans = 1.0;

    float b0 = dot(ro, rd);
    float disc = b0 * b0 - (dot(ro, ro) - bound * bound);
    if (disc > 0.0)
    {
        float t0 = max(-b0 - sqrt(disc), 0.0), t1 = -b0 + sqrt(disc);
        int steps = (int)gP1.w;
        float dt = (t1 - t0) / steps;
        float jitter = frac(sin(dot(i.pos.xy, float2(12.9898, 78.233))) * 43758.5453);
        [loop]
        for (int k = 0; k < steps; ++k)
        {
            float3 pos = ro + rd * (t0 + (k + jitter) * dt);
            float r = length(pos);
            float3 dir = pos / max(r, 1e-4);
            float ca = dot(dir, gAxis.xyz);
            float ca2 = ca * ca;
            // Bipolar: lobes along the axis, pinched at the waist.
            float shellR = R * (1.0 + gP0.z * ca2 * ca2 * 2.0) * (1.0 - gP0.z * 0.35 * (1.0 - ca2));
            shellR *= 1.0 + 0.14 * (Fbm(dir * 2.5 + seed, 3) - 0.5);
            float x = r / shellR;
            float shell = exp(-Sq((x - 1.0) / gP0.y));
            float fil = Fbm(pos * (2.2 / R) + seed, 5);
            float dens = shell * (0.3 + 1.4 * fil * fil) + smoothstep(1.0, 0.2, x) * 0.12 * fil;
            if (gP1.x > 0.0) dens += exp(-Sq((x - gP1.x) / (gP0.y * 0.8))) * 0.6 * (0.4 + fil);
            float knots = smoothstep(0.64, 0.8, Fbm(pos * (6.0 / R) + seed + 3.0, 4)) * shell * gP1.y;
            float3 c = lerp(gC0.rgb, gC1.rgb, smoothstep(0.78, 1.05, x));
            emission += trans * (c * dens + gC2.rgb * knots * 0.6) * dt * gP0.w;
            trans *= exp(-knots * dt * 3.0);
        }
    }
    float tc = -dot(ro, rd);
    float b = length(ro + rd * tc);
    float3 star = gC3.rgb * (40.0 * exp(-Sq(b / 0.035)) + 0.8 * exp(-b / 0.25));
    float3 col = Sky(rd) * trans + emission * gP1.z + star * trans;
    return float4(col, 1.0);
}
