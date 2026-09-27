#include "Info.h"
#include <cmath>
#include <cstdio>

using namespace rs;

namespace {

double LogRange(Rng& rng, double lo, double hi) {
    return std::exp(rng.Range(static_cast<float>(std::log(lo)), static_cast<float>(std::log(hi))));
}

// 1234567 -> "1,234,567"
std::wstring Group(double v) {
    long long n = std::llround(v);
    std::wstring digits = std::to_wstring(n < 0 ? -n : n), out;
    for (size_t i = 0; i < digits.size(); ++i) {
        if (i > 0 && (digits.size() - i) % 3 == 0) out += L',';
        out += digits[i];
    }
    return n < 0 ? L"-" + out : out;
}

// A sensible number of digits for the magnitude: 12,400 / 245 / 12.4 / 1.44 / 0.0056.
std::wstring Nice(double v) {
    wchar_t buf[64];
    if (v >= 1000.0) {
        double step = std::pow(10.0, std::floor(std::log10(v)) - 2.0);   // three significant figures
        return Group(std::round(v / step) * step);
    }
    if (v >= 100.0) return Group(v);
    if (v >= 10.0) swprintf_s(buf, L"%.1f", v);
    else if (v >= 0.1) swprintf_s(buf, L"%.2f", v);
    else swprintf_s(buf, L"%.2g", v);
    return buf;
}

// Rough figures read better rounded: "about 2.2", "about 14", "about 130".
std::wstring About(double v) {
    wchar_t buf[32];
    if (v >= 100.0) return L"about " + Nice(v);
    swprintf_s(buf, v < 10.0 ? L"about %.1f" : L"about %.0f", v);
    return buf;
}

std::wstring Superscript(int e) {
    static const wchar_t* const kDigits = L"\x2070\x00b9\x00b2\x00b3\x2074\x2075\x2076\x2077\x2078\x2079";
    std::wstring digits = std::to_wstring(e < 0 ? -e : e), out = e < 0 ? L"\x207b" : L"";
    for (wchar_t c : digits) out += kDigits[c - L'0'];
    return out;
}

// 3.2e17 -> "3.2 x 10^17" with a real multiplication sign and superscript exponent.
std::wstring Sci(double v) {
    int e = static_cast<int>(std::floor(std::log10(v)));
    wchar_t buf[32];
    swprintf_s(buf, L"%.1f", v / std::pow(10.0, e));
    return std::wstring(buf) + L" \x00d7 10" + Superscript(e);
}

// Large counts in words: "360 million", "4.1 billion".
std::wstring Words(double v) {
    auto part = [](double x, const wchar_t* unit) {
        wchar_t buf[48];
        swprintf_s(buf, x < 10.0 ? L"%.1f %s" : L"%.0f %s", x, unit);
        return std::wstring(buf);
    };
    if (v >= 1e12) return part(v / 1e12, L"trillion");
    if (v >= 1e9) return part(v / 1e9, L"billion");
    if (v >= 1e6) return part(v / 1e6, L"million");
    return Nice(v);
}

// A made-up catalogue designation in the usual right-ascension / declination style.
std::wstring Designation(Rng& rng) {
    wchar_t buf[40];
    int dec = rng.Int(-89, 89);
    swprintf_s(buf, L"CEL J%02d%02d%c%02d%02d", rng.Int(0, 23), rng.Int(0, 59), dec < 0 ? L'-' : L'+', dec < 0 ? -dec : dec, rng.Int(0, 59));
    return buf;
}

// Harvard spectral class from surface temperature, e.g. 5772 K -> "G2".
std::wstring Spectral(double t) {
    struct Band { wchar_t letter; double hot, cool; };
    static const Band bands[] = { { L'O', 50000, 30000 }, { L'B', 30000, 10000 }, { L'A', 10000, 7500 },
                                  { L'F', 7500, 6000 }, { L'G', 6000, 5200 }, { L'K', 5200, 3700 }, { L'M', 3700, 2400 } };
    for (const Band& b : bands) {
        if (t >= b.cool || b.letter == L'M') {
            double f = (b.hot - t) / (b.hot - b.cool);
            int sub = static_cast<int>(std::floor(f * 10.0));
            sub = sub < 0 ? 0 : (sub > 9 ? 9 : sub);
            return std::wstring(1, b.letter) + std::to_wstring(sub);
        }
    }
    return L"M9";
}

// Surface temperature from luminosity and radius (solar units), Stefan-Boltzmann.
double Temperature(double lum, double radius) { return 5772.0 * std::pow(lum / (radius * radius), 0.25); }

std::wstring Kelvin(double t) { return Group(std::round(t / 10.0) * 10.0) + L" K"; }
std::wstring Distance(Rng& rng, double lo, double hi) { return Nice(LogRange(rng, lo, hi)) + L" light years"; }

// Schwarzschild horizon diameter in km for a mass in solar masses.
double HorizonKm(double mass) { return 2.0 * 2.953 * mass; }

} // namespace

InfoCard MakeInfo(Kind kind, Rng& rng, bool disk) {
    InfoCard c;
    std::wstring id = Designation(rng);
    switch (kind) {
    case Kind::BlackHole: {
        double m = LogRange(rng, 5.0, 60.0);
        double d = HorizonKm(m);
        c.title = L"Black hole";
        c.subtitle = std::wstring(m > 30.0 ? L"Heavy stellar-mass" : L"Stellar-mass") + L" black hole, not rotating  \x00b7  " + id;
        c.body = L"Gravity here is so strong that nothing, not even light, can climb back out. The black disc is the hole's "
                 L"shadow; the thin ring around it is light that circled the hole before escaping towards you. ";
        c.body += disk
            ? L"Its accretion disk is bent over the top and under the bottom by the warped space, and the side swinging "
              L"towards you glows brighter because the gas orbits at a large fraction of the speed of light."
            : L"With no disk to light it, it shows itself only as a lens: stars behind it are smeared into arcs, and "
              L"stars near its edge appear twice.";
        c.stats = { { L"Mass", Nice(m) + L" solar masses" },
                    { L"Event horizon", Nice(d) + L" km across" },
                    { L"Photon ring", Nice(d * 1.5) + L" km across" } };
        if (disk) c.stats.push_back({ L"Inner disk", L"about " + Words(1e7 * std::pow(m / 10.0, -0.25)) + L" K" });
        c.stats.push_back({ L"Distance", Distance(rng, 3000.0, 40000.0) });
        break;
    }
    case Kind::Boson: {
        double m = LogRange(rng, 0.5, 8.0);
        c.title = L"Boson star";
        c.subtitle = L"Hypothetical dark-matter star  \x00b7  " + id;
        c.body = L"A hypothetical star built not from atoms but from a single quantum field of ultralight particles called "
                 L"bosons, one of the candidates for dark matter. It has no surface and gives off no light; photons pass "
                 L"straight through it. The only sign that it is there is the way its mass bends the stars behind it into "
                 L"rings, and the dust it slings about.";
        c.stats = { { L"Mass", Nice(m) + L" solar masses" },
                    { L"Radius", About(m * rng.Range(9.0f, 16.0f)) + L" km, with no surface" },
                    { L"Light emitted", L"none" },
                    { L"Status", L"hypothetical, never observed" },
                    { L"Distance", Distance(rng, 500.0, 20000.0) } };
        break;
    }
    case Kind::WhiteHole: {
        double m = LogRange(rng, 8.0, 40.0);
        c.title = L"White hole";
        c.subtitle = L"Time-reversed black hole  \x00b7  " + id;
        c.body = L"The time-reverse of a black hole: nothing can ever fall in, and everything inside is flung out. "
                 L"General relativity allows white holes, but nobody knows how one could form, and none has been seen. "
                 L"Here matter and light pour from the core without end, and shock fronts ripple out through an accretion "
                 L"disk that runs backwards.";
        c.stats = { { L"Mass", Nice(m) + L" solar masses" },
                    { L"Horizon", Nice(HorizonKm(m)) + L" km across" },
                    { L"Outflow", Nice(rng.Range(0.3f, 0.9f)) + L" \x00d7 the speed of light" },
                    { L"Status", L"theoretical" },
                    { L"Distance", Distance(rng, 5000.0, 60000.0) } };
        break;
    }
    case Kind::Tzo: {
        double m = rng.Range(12.0f, 25.0f), core = rng.Range(1.3f, 2.0f);
        double r = rng.Range(900.0f, 1500.0f), t = rng.Range(3300.0f, 3800.0f);
        double lum = r * r * std::pow(t / 5772.0, 4.0);
        c.title = L"Thorne\x2013\x017Bytkow object";
        c.subtitle = L"Red supergiant with a neutron-star core  \x00b7  " + id;
        c.body = L"A red supergiant that has swallowed a neutron star. The dense remnant sank to the centre and now powers "
                 L"the star from within; nuclear reactions at its surface should leave unusual traces, such as rubidium "
                 L"and molybdenum, in the star's atmosphere. Proposed by Kip Thorne and Anna \x017Bytkow in 1977; a few "
                 L"candidates are known, but none is confirmed.";
        c.stats = { { L"Mass", Nice(m) + L" solar masses" },
                    { L"Neutron-star core", Nice(core) + L" solar masses" },
                    { L"Radius", Group(r) + L" \x00d7 the Sun" },
                    { L"Surface", Kelvin(t) },
                    { L"Luminosity", Words(lum) + L" \x00d7 the Sun" },
                    { L"Distance", Distance(rng, 8000.0, 200000.0) } };
        break;
    }
    case Kind::Strange: {
        double m = rng.Range(1.2f, 2.0f), r = rng.Range(8.0f, 11.0f), period = LogRange(rng, 1.5, 30.0);
        double density = m * 1.989e30 / (4.0 / 3.0 * 3.14159265 * std::pow(r * 1000.0, 3.0));
        c.title = L"Strange star";
        c.subtitle = L"Hypothetical quark star  \x00b7  " + id;
        c.body = L"A hypothetical star made of strange quark matter, packed denser than an atomic nucleus. If quark matter "
                 L"is the true ground state of matter, some neutron stars could collapse into stars like this. Its fast "
                 L"spin and strong magnetic field drive thin jets from its poles. The colours are artistic licence.";
        c.stats = { { L"Mass", Nice(m) + L" solar masses" },
                    { L"Radius", Nice(r) + L" km" },
                    { L"Density", Sci(density) + L" kg/m\x00b3" },
                    { L"Spin", Nice(1000.0 / period) + L" turns a second" },
                    { L"Surface", L"about " + Words(LogRange(rng, 1e6, 5e6)) + L" K" },
                    { L"Status", L"hypothetical" } };
        break;
    }
    case Kind::Ember: {
        double m = rng.Range(1.2f, 2.0f), r = rng.Range(10.0f, 13.0f);
        double g = 6.674e-11 * m * 1.989e30 / std::pow(r * 1000.0, 2.0) / 9.81;
        c.title = L"Cold neutron star";
        c.subtitle = L"Isolated neutron star, long cooled  \x00b7  " + id;
        c.body = L"A neutron star that has wandered the galaxy alone for billions of years, its heat almost gone. What is "
                 L"left is a city-sized ball of neutrons under a thin, cracked iron crust, glowing a dull red. It still "
                 L"bends starlight near its edge, and its gravity would still tear apart anything that came close.";
        c.stats = { { L"Mass", Nice(m) + L" solar masses" },
                    { L"Radius", Nice(r) + L" km" },
                    { L"Surface", Kelvin(rng.Range(1200.0f, 3000.0f)) },
                    { L"Surface gravity", Words(g) + L" \x00d7 Earth's" },
                    { L"Age", About(rng.Range(8.0f, 12.0f)) + L" billion years" },
                    { L"Speed through the galaxy", Group(rng.Range(100.0f, 600.0f)) + L" km/s" } };
        break;
    }
    case Kind::RedDwarf: {
        double m = rng.Range(0.1f, 0.45f);
        double r = std::pow(m, 0.9), lum = 0.23 * std::pow(m, 2.3), t = Temperature(lum, r);
        c.title = L"Red dwarf";
        c.subtitle = L"Main-sequence star, class " + Spectral(t) + L" V  \x00b7  " + id;
        c.body = L"The most common kind of star in the galaxy: small, cool and dim, burning its hydrogen so slowly that it "
                 L"will outlive every other star here. Red dwarfs are violent for their size, with frequent flares that "
                 L"can outshine the whole star for minutes. Proxima Centauri, the nearest star to the Sun, is one.";
        c.stats = { { L"Mass", Nice(m) + L" solar masses" },
                    { L"Radius", Nice(r) + L" \x00d7 the Sun" },
                    { L"Surface", Kelvin(t) },
                    { L"Luminosity", Nice(lum) + L" \x00d7 the Sun" },
                    { L"Lifespan", L"trillions of years" },
                    { L"Distance", Distance(rng, 10.0, 400.0) } };
        break;
    }
    case Kind::SunLike: {
        double m = rng.Range(0.85f, 1.15f);
        double r = std::pow(m, 0.8), lum = std::pow(m, 4.0), t = Temperature(lum, r);
        c.title = L"Sun-like star";
        c.subtitle = L"Main-sequence star, class " + Spectral(t) + L" V  \x00b7  " + id;
        c.body = L"A yellow main-sequence star much like our Sun, fusing hydrogen into helium in its core. Its boiling "
                 L"surface is marked by dark sunspots where magnetic fields choke the flow of heat. Loops of glowing plasma "
                 L"arch above its edge, and every so often a flare or a coronal mass ejection throws material into space.";
        c.stats = { { L"Mass", Nice(m) + L" solar masses" },
                    { L"Radius", Nice(r) + L" \x00d7 the Sun" },
                    { L"Surface", Kelvin(t) },
                    { L"Luminosity", Nice(lum) + L" \x00d7 the Sun" },
                    { L"Lifespan", About(10.0 * m / lum) + L" billion years" },
                    { L"Distance", Distance(rng, 10.0, 1000.0) } };
        break;
    }
    case Kind::BlueGiant: {
        double m = rng.Range(12.0f, 40.0f);
        double r = 2.0 * std::pow(m, 0.6), lum = 1.4 * std::pow(m, 3.5), t = Temperature(lum, r);
        c.title = L"Blue giant";
        c.subtitle = L"Giant star, class " + Spectral(t) + L" III  \x00b7  " + id;
        c.body = L"A massive young star that burns ferociously hot and fast. It is tens of thousands of times brighter "
                 L"than the Sun, and its fierce stellar wind strips material from its surface. It will live only a few "
                 L"million years before ending in a supernova.";
        c.stats = { { L"Mass", Nice(m) + L" solar masses" },
                    { L"Radius", Nice(r) + L" \x00d7 the Sun" },
                    { L"Surface", Kelvin(t) },
                    { L"Luminosity", Words(lum) + L" \x00d7 the Sun" },
                    { L"Lifespan", About(1e4 * m / lum) + L" million years" },
                    { L"Distance", Distance(rng, 300.0, 5000.0) } };
        break;
    }
    case Kind::GasGiant: {
        double m = LogRange(rng, 0.3, 3.0), r = rng.Range(0.95f, 1.15f) * 71492.0;
        c.title = L"Gas giant";
        c.subtitle = L"Jovian planet  \x00b7  " + id + L" b";
        c.body = L"A giant world of hydrogen and helium with no solid surface to stand on. Its clouds are dragged into "
                 L"bands by powerful jet streams, and storms bigger than Earth can churn for centuries. Its moons cast "
                 L"small black shadows as they cross its face.";
        c.stats = { { L"Mass", Nice(m) + L" \x00d7 Jupiter" },
                    { L"Radius", Nice(r) + L" km" },
                    { L"Day length", Nice(rng.Range(9.0f, 11.5f)) + L" hours" },
                    { L"Cloud tops", Kelvin(rng.Range(110.0f, 165.0f)) },
                    { L"Known moons", Group(rng.Int(20, 95)) },
                    { L"Orbit", Nice(rng.Range(3.0f, 10.0f)) + L" AU from its star" } };
        break;
    }
    case Kind::RingedGiant: {
        double me = rng.Range(60.0f, 150.0f), r = rng.Range(54000.0f, 62000.0f);
        double density = me * 5.972e24 / (4.0 / 3.0 * 3.14159265 * std::pow(r * 1e3, 3.0)) / 1000.0;
        c.title = L"Ringed giant";
        c.subtitle = L"Gas giant with a ring system  \x00b7  " + id + L" c";
        c.body = L"A gas giant wrapped in rings of ice and rock, hundreds of thousands of kilometres wide but mostly only "
                 L"tens of metres thick. The planet casts its shadow across the rings, and the rings cast thin stripes of "
                 L"shadow back onto its clouds. The gaps are swept clear by small moons.";
        c.stats = { { L"Mass", Nice(me) + L" \x00d7 Earth" },
                    { L"Ring span", Nice(r * 4.6) + L" km" },
                    { L"Ring thickness", L"about " + Group(rng.Int(10, 100)) + L" m" },
                    { L"Density", Nice(density) + L" g/cm\x00b3, less than water" },
                    { L"Orbit", Nice(rng.Range(8.0f, 12.0f)) + L" AU from its star" } };
        break;
    }
    case Kind::IceGiant: {
        double me = rng.Range(12.0f, 20.0f);
        c.title = L"Ice giant";
        c.subtitle = L"Neptune-class planet  \x00b7  " + id + L" d";
        c.body = L"A cold world of water, ammonia and methane ices beneath a hydrogen atmosphere; the methane gives it its "
                 L"blue. Despite getting little sunlight, it has the fastest winds of any planet in its system, and dark "
                 L"storms come and go within a few years.";
        c.stats = { { L"Mass", Nice(me) + L" \x00d7 Earth" },
                    { L"Radius", Nice(rng.Range(3.6f, 4.1f)) + L" \x00d7 Earth" },
                    { L"Winds", L"up to " + Group(std::round(rng.Range(1500.0f, 2100.0f) / 10.0f) * 10.0) + L" km/h" },
                    { L"Cloud tops", Kelvin(rng.Range(55.0f, 75.0f)) },
                    { L"Orbit", Nice(rng.Range(19.0f, 35.0f)) + L" AU from its star" } };
        break;
    }
    case Kind::HotJupiter: {
        c.title = L"Hot Jupiter";
        c.subtitle = L"Tidally locked gas giant  \x00b7  " + id + L" b";
        c.body = L"A gas giant orbiting so close to its star that a year lasts a few days. One side always faces the star "
                 L"and glows at well over a thousand degrees, while fierce winds carry the heat round to the night side. "
                 L"The star is boiling its atmosphere away into a faint trailing tail.";
        c.stats = { { L"Mass", Nice(LogRange(rng, 0.5, 2.0)) + L" \x00d7 Jupiter" },
                    { L"Radius", Nice(rng.Range(1.2f, 1.8f)) + L" \x00d7 Jupiter (puffed up by heat)" },
                    { L"Year", Nice(rng.Range(1.5f, 4.0f)) + L" days" },
                    { L"Day side", Kelvin(rng.Range(1400.0f, 2500.0f)) },
                    { L"Atmosphere lost", L"about " + Words(LogRange(rng, 5000.0, 50000.0)) + L" tonnes a second" } };
        break;
    }
    case Kind::EarthLike: {
        double m = rng.Range(0.8f, 1.5f);
        c.title = L"Earth-like world";
        c.subtitle = L"Temperate rocky planet  \x00b7  " + id + L" e";
        c.body = L"A rocky world at just the right distance from its star for liquid water. Oceans cover most of it, "
                 L"weather systems swirl through a thin blue atmosphere, and on the night side lights glitter along the "
                 L"coasts. Whether anyone is home is another question.";
        c.stats = { { L"Mass", Nice(m) + L" \x00d7 Earth" },
                    { L"Radius", Nice(std::pow(m, 0.27)) + L" \x00d7 Earth" },
                    { L"Oceans", Group(rng.Int(55, 85)) + L"% of the surface" },
                    { L"Average temperature", Group(rng.Int(5, 25)) + L" \x00b0" L"C" },
                    { L"Day", Nice(rng.Range(18.0f, 40.0f)) + L" hours" },
                    { L"Year", Group(rng.Int(250, 500)) + L" days" } };
        break;
    }
    case Kind::LavaWorld: {
        double m = rng.Range(2.0f, 8.0f);
        c.title = L"Lava world";
        c.subtitle = L"Molten super-Earth  \x00b7  " + id + L" b";
        c.body = L"A rocky planet so close to its star that its surface has melted. Seas of lava glow through a thin, "
                 L"cracking crust, and the star fills a huge part of the sky. Worlds like 55 Cancri e may even have "
                 L"skies of vaporised rock.";
        c.stats = { { L"Mass", Nice(m) + L" \x00d7 Earth" },
                    { L"Radius", Nice(std::pow(m, 0.27)) + L" \x00d7 Earth" },
                    { L"Surface", Kelvin(rng.Range(1500.0f, 2700.0f)) },
                    { L"Year", Nice(rng.Range(11.0f, 40.0f)) + L" hours" },
                    { L"Distance from star", Nice(rng.Range(0.015f, 0.03f)) + L" AU" } };
        break;
    }
    case Kind::CrateredMoon: {
        double r = rng.Range(800.0f, 2000.0f);
        c.title = L"Cratered moon";
        c.subtitle = L"Airless rocky moon  \x00b7  " + id + L" I";
        c.body = L"An airless rocky moon, battered by billions of years of impacts. With no weather to wear them away, "
                 L"every crater stays, from basins hundreds of kilometres across down to pits a few metres wide. The dark "
                 L"plains are ancient lava that flooded the biggest basins.";
        c.stats = { { L"Radius", Group(std::round(r / 10.0) * 10.0) + L" km" },
                    { L"Surface gravity", Nice(r / 1737.0 * 0.165) + L" \x00d7 Earth's" },
                    { L"Surface", L"-170 to +120 \x00b0" L"C, day to night" },
                    { L"Orbit", Nice(rng.Range(3.0f, 28.0f)) + L" days round its planet" } };
        break;
    }
    case Kind::IcyMoon: {
        c.title = L"Icy moon";
        c.subtitle = L"Ocean moon  \x00b7  " + id + L" II";
        c.body = L"A moon sheathed in ice, criss-crossed by long reddish cracks where the shell has split and refrozen. "
                 L"Beneath the ice lies a global ocean of liquid water, kept warm by the tidal squeezing of its giant "
                 L"planet: one of the best places to look for life beyond Earth.";
        c.stats = { { L"Radius", Group(std::round(rng.Range(1400.0f, 1700.0f) / 10.0f) * 10.0) + L" km" },
                    { L"Ice shell", Group(rng.Int(15, 25)) + L" km thick" },
                    { L"Ocean", Group(rng.Int(60, 150)) + L" km deep" },
                    { L"Surface", Kelvin(rng.Range(95.0f, 125.0f)) },
                    { L"Orbit", Nice(rng.Range(3.0f, 4.0f)) + L" days round its planet" } };
        break;
    }
    case Kind::VolcanicMoon: {
        c.title = L"Volcanic moon";
        c.subtitle = L"Io-type moon  \x00b7  " + id + L" I";
        c.body = L"The most volcanic kind of world known: tidal flexing by its giant planet melts its interior and feeds "
                 L"hundreds of active volcanoes. Sulfur paints the surface yellow, orange and white, and umbrella-shaped "
                 L"plumes throw material hundreds of kilometres up into space.";
        c.stats = { { L"Radius", Group(std::round(rng.Range(1700.0f, 1900.0f) / 10.0f) * 10.0) + L" km" },
                    { L"Active volcanoes", Group(rng.Int(300, 450)) },
                    { L"Plume height", L"up to " + Group(std::round(rng.Range(250.0f, 500.0f) / 10.0f) * 10.0) + L" km" },
                    { L"Lava", Kelvin(rng.Range(1500.0f, 1900.0f)) },
                    { L"Orbit", Nice(rng.Range(1.5f, 2.0f)) + L" days round its planet" } };
        break;
    }
    case Kind::HazyMoon: {
        c.title = L"Hazy moon";
        c.subtitle = L"Titan-type moon  \x00b7  " + id + L" VI";
        c.body = L"A moon with a thicker atmosphere than Earth's, wrapped in an orange smog made from methane broken "
                 L"apart by sunlight. Beneath the haze it rains methane onto a frozen surface of rivers, lakes and seas "
                 L"of liquid hydrocarbons.";
        c.stats = { { L"Radius", Group(std::round(rng.Range(2400.0f, 2700.0f) / 10.0f) * 10.0) + L" km" },
                    { L"Surface pressure", Nice(rng.Range(1.3f, 1.6f)) + L" \x00d7 Earth's" },
                    { L"Surface", Kelvin(rng.Range(90.0f, 95.0f)) },
                    { L"Lakes and seas", L"liquid methane and ethane" },
                    { L"Orbit", Nice(rng.Range(14.0f, 17.0f)) + L" days round its planet" } };
        break;
    }
    case Kind::Pulsar: {
        double period = LogRange(rng, 0.03, 1.0);
        c.title = L"Pulsar";
        c.subtitle = L"Rotating neutron star  \x00b7  PSR " + id.substr(4);
        c.body = L"A rapidly spinning neutron star whose magnetic poles fire beams of radiation. The magnetic axis is "
                 L"tilted from the spin axis, so the beams sweep round like a lighthouse, and from far away they arrive "
                 L"as pulses as regular as an atomic clock. Its wind of particles lights up a ring around it. The spin "
                 L"here is slowed right down.";
        c.stats = { { L"Mass", Nice(rng.Range(1.3f, 2.0f)) + L" solar masses" },
                    { L"Radius", Nice(rng.Range(10.0f, 13.0f)) + L" km" },
                    { L"Spin", Nice(1.0 / period) + L" turns a second" },
                    { L"Magnetic field", Sci(LogRange(rng, 1e7, 1e9)) + L" tesla" },
                    { L"Age", L"about " + Words(LogRange(rng, 1e3, 1e7)) + L" years" } };
        break;
    }
    case Kind::PlanetaryNebula: {
        c.title = L"Planetary nebula";
        c.subtitle = L"Shell of a dying star  \x00b7  PN " + id.substr(4);
        c.body = L"The glowing shell of gas shed by a dying Sun-like star. Its exposed core, now a tiny white dwarf hotter "
                 L"than 100,000 K, floods the gas with ultraviolet light and makes it fluoresce: oxygen glows teal, "
                 L"hydrogen and nitrogen glow red. It will fade within a few tens of thousands of years.";
        c.stats = { { L"Diameter", Nice(rng.Range(0.5f, 3.0f)) + L" light years" },
                    { L"Age", Nice(std::round(rng.Range(5000.0f, 25000.0f) / 100.0f) * 100.0) + L" years" },
                    { L"Expanding at", Group(rng.Int(20, 40)) + L" km/s" },
                    { L"Central star", Kelvin(LogRange(rng, 60000.0, 200000.0)) },
                    { L"Distance", Distance(rng, 700.0, 8000.0) } };
        break;
    }
    case Kind::MassTransfer: {
        c.title = L"Mass-transfer binary";
        c.subtitle = L"Cataclysmic variable  \x00b7  " + id + L" AB";
        c.body = L"Two stars in such a tight orbit that one is feeding the other. The swollen star has overfilled its "
                 L"gravitational boundary, and gas spills from its tip towards a white-dwarf companion, spiralling into a "
                 L"hot accretion disk. Enough stolen hydrogen can ignite in a nova, and sometimes in a supernova.";
        c.stats = { { L"Donor star", Nice(rng.Range(0.5f, 1.5f)) + L" solar masses" },
                    { L"White dwarf", Nice(rng.Range(0.6f, 1.3f)) + L" solar masses" },
                    { L"Orbit", Nice(rng.Range(3.0f, 20.0f)) + L" hours" },
                    { L"Transfer rate", Sci(LogRange(rng, 1e-10, 1e-8)) + L" solar masses a year" },
                    { L"Inner disk", Kelvin(std::round(LogRange(rng, 10000.0, 50000.0) / 100.0) * 100.0) } };
        break;
    }
    case Kind::Comet: {
        c.title = L"Comet";
        c.subtitle = L"Long-period comet  \x00b7  C/" + std::to_wstring(rng.Int(1990, 2026)) + L" " + std::wstring(1, static_cast<wchar_t>(L'A' + rng.Int(0, 24))) + std::to_wstring(rng.Int(1, 9));
        c.body = L"A mountain of ice and dust falling past its star. Sunlight turns its ices straight into gas, puffing up "
                 L"a vast glowing coma around a nucleus only a few kilometres wide. Light pushes the dust into a broad, "
                 L"curved tail, while the stellar wind drags ionised gas into a straight blue one; both point away from "
                 L"the star.";
        c.stats = { { L"Nucleus", Nice(rng.Range(2.0f, 30.0f)) + L" km across" },
                    { L"Coma", Nice(std::round(LogRange(rng, 50000.0, 500000.0) / 1000.0) * 1000.0) + L" km across" },
                    { L"Tail", Words(LogRange(rng, 1e7, 1e8)) + L" km long" },
                    { L"Speed", Group(rng.Int(30, 70)) + L" km/s" },
                    { L"Returns every", Words(LogRange(rng, 200.0, 50000.0)) + L" years" } };
        break;
    }
    case Kind::RedGiant:
    default: {
        double m = rng.Range(0.8f, 3.0f), r = LogRange(rng, 20.0, 120.0), t = rng.Range(3600.0f, 4500.0f);
        double lum = r * r * std::pow(t / 5772.0, 4.0);
        c.title = L"Red giant";
        c.subtitle = L"Evolved giant star, class " + Spectral(t) + L" III  \x00b7  " + id;
        c.body = L"An ageing star that has run out of hydrogen in its core and swollen to many times its former size. "
                 L"Its cool, deep orange surface is churned by enormous convection cells, and it slowly sheds its outer "
                 L"layers into space. The Sun will become a red giant in about five billion years.";
        c.stats = { { L"Mass", Nice(m) + L" solar masses" },
                    { L"Radius", Nice(r) + L" \x00d7 the Sun" },
                    { L"Surface", Kelvin(t) },
                    { L"Luminosity", Nice(lum) + L" \x00d7 the Sun" },
                    { L"Distance", Distance(rng, 50.0, 3000.0) } };
        break;
    }
    }
    return c;
}
