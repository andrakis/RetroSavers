#pragma once
#include "Gfx/Mesh.h"
#include <DirectXMath.h>

// Species table and the parametric fish mesh. Every fish is built from the same recipe:
// elliptical cross-sections along the body axis (object-space X, nose at +0.5, tail joint
// at -0.5, body length 1), a fan tail, dorsal / anal / pectoral fins as two-sided quads and
// a pair of eyes. Proportions, colours, pattern and behaviour come from the species entry.
// Fish_ps reads the pattern from uv: u < 1 body, 1..1.25 tail, 1.25..2 fins, >= 2 eye.
struct FishSpecies {
    const wchar_t* name;

    // Body proportions relative to a body length of 1.
    float height, width;           // largest half-height / half-width
    float peakAt;                  // fraction along the body where it is fullest
    float peduncle;                // half-height at the tail joint as a fraction of `height`
    float tailLen, tailHeight;     // tail fin extent behind the joint and its half-height
    float forkDepth;               // 0 = rounded tail, 1 = deeply forked
    float dorsalStart, dorsalEnd, dorsalHeight;
    float analStart, analEnd, analHeight;
    float pectoralLen;
    float eyeSize;

    // Look (see Fish.hlsli).
    DirectX::XMFLOAT3 base, accent, fin, tail;
    int patternMode;               // 0 plain, 1 vertical bands, 2 horizontal band, 3 spots, 4 gradient, 5 tetra
    float p1, p2;                  // pattern parameters
    float belly;                   // belly lightening 0..1
    float sheen;                   // rim iridescence
    bool tailGradient;             // tail fin fades tail -> accent
    float specPower, specIntensity;

    // Behaviour.
    float size;                    // body length in world units
    float cruise;                  // cruising speed in body lengths per second
    float swimAmp, swimFreq;       // lateral wave amplitude (object units) and wave number
    bool schooling;
    bool bottomDweller;
    int minSchool, maxSchool;      // group size when schooling
};

constexpr int kSpeciesCount = 8;
const FishSpecies& GetSpecies(int index);

// quality 0..2 selects the tessellation (about 250 / 550 / 950 vertices).
rs::MeshData BuildFishMesh(const FishSpecies& s, int quality);
