// Shared cbuffer layouts and helpers. Matrices are uploaded transposed from DirectXMath
// so that mul(vector, matrix) follows the row-vector convention used on the CPU.

#ifndef RS_COMMON_HLSLI
#define RS_COMMON_HLSLI

cbuffer PerFrame : register(b0)
{
    float4x4 gViewProj;
    float4 gEyePos;      // xyz world-space eye
    float4 gLightDir;    // xyz unit direction *towards* the light
    float4 gLightColor;  // rgb diffuse/spec light colour
    float4 gAmbient;     // rgb ambient
    float4 gFogColor;    // rgb fog, a > 0.5 enables fog
    float4 gFogParams;   // x = start, y = end
};

cbuffer PerObject : register(b1)
{
    float4x4 gWorld;
    float4 gColor;       // material base colour (rgba)
    float4 gMaterial;    // x = spec power, y = spec intensity, z = use texture, w = uv scale
    float4 gLightMap;    // x > 0.5 enables the projected light map (t1), y = world XZ -> uv scale, z = strength
};

struct PhongPSIn
{
    float4 pos   : SV_Position;
    float3 wpos  : TEXCOORD0;
    float3 nrm   : TEXCOORD1;
    float2 uv    : TEXCOORD2;
    float4 color : COLOR0;
};

#endif
