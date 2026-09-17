cbuffer Billboard : register(b0)
{
    float4x4 gViewProj;
    float4 gCenter;  // xyz world centre
    float4 gRight;   // xyz = camera right * half width
    float4 gUp;      // xyz = camera up * half height
    float4 gColor;
    float4 gUvRect;  // u0, v0, u1, v1
};

struct PSIn
{
    float4 pos   : SV_Position;
    float2 uv    : TEXCOORD0;
    float4 color : COLOR0;
};

PSIn main(uint id : SV_VertexID)
{
    // Triangle strip: 0=(-1,+1) 1=(+1,+1) 2=(-1,-1) 3=(+1,-1)
    float2 c = float2((id & 1) ? 1.0 : -1.0, (id & 2) ? -1.0 : 1.0);
    float3 wp = gCenter.xyz + gRight.xyz * c.x + gUp.xyz * c.y;
    PSIn o;
    o.pos = mul(float4(wp, 1.0), gViewProj);
    o.uv = float2(c.x > 0.0 ? gUvRect.z : gUvRect.x, c.y > 0.0 ? gUvRect.y : gUvRect.w);
    o.color = gColor;
    return o;
}
