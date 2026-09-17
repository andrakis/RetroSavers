cbuffer Viewport : register(b0)
{
    float4 gViewport; // x = width, y = height (pixels)
};

// Per-instance stream (slot 0): centre in pixels, size in pixels, uv rect, colour, rotation.
struct VSIn
{
    float2 pos      : POSITION;
    float2 size     : TEXCOORD0;
    float4 uvRect   : TEXCOORD1;   // u0, v0, u1, v1
    float4 color    : COLOR0;
    float  rotation : TEXCOORD2;   // radians, clockwise on screen
    uint   id       : SV_VertexID;
};

struct PSIn
{
    float4 pos   : SV_Position;
    float2 uv    : TEXCOORD0;
    float4 color : COLOR0;
};

PSIn main(VSIn i)
{
    // Two triangles per instance: 0,1,2  2,1,3 -> corners of a unit square.
    static const float2 corners[6] = { float2(0, 0), float2(1, 0), float2(0, 1), float2(0, 1), float2(1, 0), float2(1, 1) };
    float2 c = corners[i.id];
    float2 local = (c - 0.5) * i.size;
    float s, co;
    sincos(i.rotation, s, co);
    float2 p = i.pos + float2(local.x * co - local.y * s, local.x * s + local.y * co);
    PSIn o;
    o.pos = float4(p.x / gViewport.x * 2.0 - 1.0, 1.0 - p.y / gViewport.y * 2.0, 0.5, 1.0);
    o.uv = float2(lerp(i.uvRect.x, i.uvRect.z, c.x), lerp(i.uvRect.y, i.uvRect.w, c.y));
    o.color = i.color;
    return o;
}
