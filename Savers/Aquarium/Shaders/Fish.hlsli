// Per-fish constants shared by Fish_vs and Fish_ps (slot b2 in both stages).

#ifndef RS_FISH_HLSLI
#define RS_FISH_HLSLI

cbuffer Fish : register(b2)
{
    float4 gAnim;       // x = swim phase, y = amplitude, z = wave number, w = unused
    float4 gAnim2;      // x = turn bend, y = tail flick, z = head amplitude fraction, w = unused
    float4 gBase;       // body colour
    float4 gAccent;     // pattern colour
    float4 gFin;        // dorsal / anal / pectoral fin colour
    float4 gTail;       // tail fin colour (also the red of the tetra)
    float4 gPattern;    // x = mode, y = p1, z = p2, w = belly lightening
    float4 gPattern2;   // x = sheen, y = tail gradient, z = spec power, w = spec intensity
};

#endif
