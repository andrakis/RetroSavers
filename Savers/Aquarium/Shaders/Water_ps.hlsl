// Back wall of the tank, drawn fullscreen before the scene. Each pixel's view ray is
// intersected with the wall plane so the gradient runs by world height: the wall colour at
// floor level equals the fog colour, and the fogged floor meets it without a seam. Slow, soft
// bands of light wander through the "volume" and the surface glows.

cbuffer Water : register(b0)
{
    float4 gTop;      // rgb near the surface
    float4 gBottom;   // rgb at the floor (= fog colour)
    float4 gParams;   // x = time, y = aspect, z = band strength, w = surface glow
    float4 gEye;      // xyz eye, w = wall plane z
    float4 gForward;  // xyz view direction
    float4 gRight;    // xyz camera right * tan(fov/2) * aspect
    float4 gUp;       // xyz camera up * tan(fov/2)
    float4 gHeights;  // x = floor level, y = surface level
};

struct PSIn
{
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

float4 main(PSIn i) : SV_Target
{
    float2 uv = i.uv;
    float t = gParams.x;

    // Height on the wall plane along this pixel's ray (rays that miss the plane look up).
    float3 dir = gForward.xyz + gRight.xyz * (uv.x * 2.0 - 1.0) + gUp.xyz * (1.0 - uv.y * 2.0);
    float dz = max(dir.z, 1e-3);
    float hit = gEye.y + dir.y * (gEye.w - gEye.z) / dz;
    float k = smoothstep(gHeights.x, gHeights.y, hit);
    float3 c = lerp(gBottom.rgb, gTop.rgb, k);

    // Two slow sine bands drifting sideways; brighter towards the surface.
    float x = uv.x * gParams.y;
    float b = 0.5 + 0.5 * sin(x * 5.0 + t * 0.25 + 0.8 * sin(uv.y * 3.0 - t * 0.15));
    b *= 0.5 + 0.5 * sin(x * 2.3 - t * 0.17 + uv.y * 1.5);
    b = b * b * k;
    c += gTop.rgb * b * gParams.z;

    // Light pooling just under the surface.
    c += gTop.rgb * 0.35 * pow(k, 4.0) * gParams.w;
    return float4(c, 1.0);
}
