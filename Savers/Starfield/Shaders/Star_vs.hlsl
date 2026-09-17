cbuffer Viewport : register(b0)
{
    float4 gViewport; // x = width, y = height (pixels)
};

// Per-instance stream (slot 0): screen position in pixels, size in pixels, brightness.
struct VSIn
{
    float2 pos        : POSITION;
    float  size       : TEXCOORD0;
    float  brightness : TEXCOORD1;
    uint   id         : SV_VertexID;
};

struct PSIn
{
    float4 pos   : SV_Position;
    float  bright : COLOR0;
};

PSIn main(VSIn i)
{
    // Two triangles per instance: 0,1,2  2,1,3 -> corners of a square.
    static const float2 corners[6] = { float2(0, 0), float2(1, 0), float2(0, 1), float2(0, 1), float2(1, 0), float2(1, 1) };
    float2 c = corners[i.id];
    float2 p = i.pos + (c - 0.5) * i.size;
    PSIn o;
    o.pos = float4(p.x / gViewport.x * 2.0 - 1.0, 1.0 - p.y / gViewport.y * 2.0, 0.5, 1.0);
    o.bright = i.brightness;
    return o;
}
