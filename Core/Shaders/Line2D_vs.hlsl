cbuffer Viewport : register(b0)
{
    float4 gViewport; // x = width, y = height (pixels)
};

struct VSIn
{
    float3 pos   : POSITION;
    float4 color : COLOR0;
};

struct PSIn
{
    float4 pos   : SV_Position;
    float4 color : COLOR0;
};

PSIn main(VSIn i)
{
    PSIn o;
    // Pixel centre convention: add 0.5 so integer coordinates land on pixel centres.
    float x = (i.pos.x + 0.5) / gViewport.x * 2.0 - 1.0;
    float y = 1.0 - (i.pos.y + 0.5) / gViewport.y * 2.0;
    o.pos = float4(x, y, 0.5, 1.0);
    o.color = i.color;
    return o;
}
