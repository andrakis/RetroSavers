struct PSIn
{
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

// Single triangle covering the viewport; no vertex buffer needed.
PSIn main(uint id : SV_VertexID)
{
    PSIn o;
    float2 uv = float2((id << 1) & 2, id & 2);
    o.pos = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    o.uv = uv;
    return o;
}
