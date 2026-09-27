#pragma once
#include "CelestialsSaver.h"
#include "Util/Rng.h"
#include <string>
#include <utility>
#include <vector>

// The on-screen card for the object being shown: what it is, and a few estimated figures.
// Figures are rolled per visit inside plausible ranges and kept physically consistent
// (radius / luminosity / temperature follow from mass, horizon size from mass, ...).
struct InfoCard {
    std::wstring title;       // "Black hole"
    std::wstring subtitle;    // class + a made-up catalogue designation
    std::wstring body;        // a few sentences
    std::vector<std::pair<std::wstring, std::wstring>> stats;   // label, value
};

InfoCard MakeInfo(Kind kind, rs::Rng& rng, bool disk);
