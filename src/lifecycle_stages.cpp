#include "lifecycle.h"
#include "vk_app.h"
#include <cmath>
#include <cstdio>

const float MASS_PRESETS[] = {0.1f, 0.5f, 1.0f, 3.0f, 8.0f, 15.0f, 25.0f, 40.0f, 100.0f};
const int PRESET_COUNT = 9;

const Stage STAGES[] = {
    {"STELLAR NEBULA", "NEBULA", "MILLIONS OF YEARS", "A COLD CLOUD OF GAS AND DUST STARTS TO COLLAPSE UNDER ITS OWN GRAVITY", nullptr, "EXAMPLE: ORION NEBULA",
        0, 0, 9.0f, 1.0f, 1.0f, 100.0f, 100.0f, 3.0e5f, 1.5e5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.3f, 0.0f, false},
    {"PROTOSTAR", "PROTOSTAR", "50 MILLION YEARS", "A SPINNING DISC FEEDS THE NEW STAR AND LAUNCHES TWIN JETS", nullptr, "EXAMPLE: HH 30 - A YOUNG STAR WITH TWIN JETS",
        1, 0, 10.0f, 2.8f, 1.8f, 3500.0f, 4600.0f, 70.0f, 42.0f, 0.2f, 1.0f, 0.0f, 0.0f, 1.0f, 0.2f, 0.0f, true},
    {"MAIN SEQUENCE", "SUN", "10 BILLION YEARS", "A STEADY YELLOW STAR FUSING HYDROGEN - OUR SUN IS HALFWAY THROUGH", nullptr, "EXAMPLE: THE SUN",
        2, 0, 10.0f, 0.00465f, 0.0056f, 5772.0f, 5900.0f, 0.0233f, 0.028f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.3f, 0.0f, true},
    {"RED GIANT", "RED GIANT", "1 BILLION YEARS", "THE HYDROGEN RUNS OUT AND THE SUN SWELLS TO ENGULF MERCURY AND VENUS", "EARTH'S ORBIT IS RIGHT AT THE EDGE OF THE SWOLLEN STAR", "EXAMPLE: ARCTURUS",
        3, 0, 12.0f, 0.0056f, 1.2f, 5900.0f, 3300.0f, 0.028f, 6.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.3f, 0.0f, false},
    {"HELIUM FLASH AND AGB", "AGB", "100 MILLION YEARS", "THE CORE IGNITES IN A HELIUM FLASH", "THE STAR PULSES AND SHEDS ITS OUTER LAYERS IN SHELLS", "EXAMPLE: MIRA - PULSES EVERY 11 MONTHS",
        4, 1, 12.0f, 1.2f, 2.0f, 3300.0f, 3200.0f, 6.0f, 40.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.3f, 0.0f, false},
    {"PLANETARY NEBULA", "PLAN. NEBULA", "20,000 YEARS", "THE SHED LAYERS GLOW AS A RING - A WHITE DWARF CORE IS LEFT BEHIND", nullptr, "EXAMPLE: RING NEBULA",
        5, 0, 12.0f, 4.3e-5f, 4.3e-5f, 100000.0f, 100000.0f, 1000.0f, 90000.0f, 0.0f, 1.0f, 200.0f, 18000.0f, 1.0f, 0.55f, 0.2f, true},
    {"WHITE DWARF", "WHITE DWARF", "BILLIONS OF YEARS TO COOL", "EARTH-SIZED BUT AS HEAVY AS THE SUN - IT SLOWLY COOLS AND FADES", nullptr, "EXAMPLE: SIRIUS B",
        6, 3, 13.0f, 4.3e-5f, 4.3e-5f, 100000.0f, 4000.0f, 90000.0f, 0.0004f, 0.0f, 1.0f, 18000.0f, 18000.0f, 0.45f, 0.4f, 0.0f, false},
    {"BLACK DWARF", "BLACK DWARF", "A TRILLION YEARS AND MORE", "LONGER THAN THE UNIVERSE'S AGE SO FAR - NONE EXIST YET", nullptr, "EXAMPLE: NONE YET - THE UNIVERSE IS TOO YOUNG",
        6, 4, 11.0f, 4.3e-5f, 4.3e-5f, 4000.0f, 5.0f, 0.0004f, 0.0004f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.4f, 0.0f, false},
    {"BLUE SUPERGIANT", "BLUE SG", "10 MILLION YEARS", "20 SUNS OF MASS - IT BURNS TEN THOUSAND TIMES BRIGHTER AND DIES YOUNG", nullptr, "EXAMPLE: RIGEL",
        2, 0, 10.0f, 0.04f, 0.045f, 33000.0f, 31000.0f, 0.25f, 0.28f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.3f, 0.0f, true},
    {"RED SUPERGIANT", "RED SG", "1 MILLION YEARS", "BETELGEUSE-SIZE: THIS STAR IS BIGGER THAN JUPITER'S ORBIT", nullptr, "EXAMPLE: BETELGEUSE (MAY GO SUPERNOVA IN ~100,000 YEARS)",
        3, 0, 12.0f, 0.045f, 5.6f, 31000.0f, 3500.0f, 0.28f, 30.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.3f, 0.0f, false},
    {"CORE COLLAPSE AND SUPERNOVA", "SUPERNOVA", "SECONDS TO COLLAPSE - MONTHS TO FADE", "FOR A FEW WEEKS ONE STAR OUTSHINES AN ENTIRE GALAXY", "THE BLAST SEEDS SPACE WITH IRON, CALCIUM AND OXYGEN", "EXAMPLE: SN 1987A",
        8, 2, 14.0f, 5.6f, 0.01f, 3500.0f, 60000.0f, 30.0f, 30.0f, 0.0f, 1.0f, 0.05f, 2.5e5f, 1.0f, 0.3f, 0.0f, false},
    {"NEUTRON STAR - PULSAR", "PULSAR", "A 20 KM BALL OF NEUTRONS", "A TEASPOON OF IT WEIGHS A BILLION TONS", "BEAMS SWEEP SPACE LIKE A LIGHTHOUSE - IT SPINS 30 TIMES A SECOND (SLOWED)", "EXAMPLE: CRAB PULSAR (SPINS 30 TIMES A SECOND)",
        9, 0, 14.0f, 6.7e-8f, 6.7e-8f, 1.0e6f, 1.0e6f, 1.1e6f, 8.0e-7f, 0.0f, 1.0f, 2.5e5f, 2.5e5f, 0.4f, 0.3f, -0.35f, false},
    {"BLACK HOLE", "BLACK HOLE", "ABOVE ABOUT 25 SUNS", "NOTHING STOPS THE COLLAPSE - NOT EVEN LIGHT ESCAPES", "THE GLOWING DISC IS GAS FALLING IN; ITS LIGHT BENDS OVER THE TOP", "EXAMPLE: CYGNUS X-1",
        10, 0, 12.0f, 5.9e-7f, 5.9e-7f, 1.0f, 1.0f, 1.3e-5f, 1.3e-5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.22f, -0.1f, true},
    {"RED DWARF", "RED DWARF", "TRILLIONS OF YEARS", "LIVES TRILLIONS OF YEARS - LONGER THAN THE UNIVERSE HAS EXISTED", "IT SIPS ITS HYDROGEN SO SLOWLY THAT ONLY FLARES BREAK THE CALM", "EXAMPLE: PROXIMA CENTAURI",
        2, 5, 12.0f, 0.0011f, 0.0011f, 3000.0f, 3000.0f, 0.0055f, 0.0055f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.3f, 0.0f, true},
    {"BLUE DWARF (THEORETICAL)", "BLUE DWARF", "AFTER TRILLIONS OF YEARS", "THEORY SAYS IT HEATS UP AND TURNS BLUE AS IT RUNS OUT OF FUEL", "NONE CAN EXIST YET - THE UNIVERSE IS TOO YOUNG", "EXAMPLE: NONE YET - PURELY THEORETICAL",
        2, 6, 11.0f, 0.0011f, 0.0009f, 3000.0f, 16000.0f, 0.0055f, 0.0045f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.3f, 0.0f, false},
    {"WHITE DWARF", "WHITE DWARF", "BILLIONS OF YEARS TO COOL", "THE LAST FUEL IS GONE - A DENSE EARTH-SIZED EMBER SLOWLY COOLS", nullptr, "EXAMPLE: SIRIUS B",
        6, 3, 12.0f, 4.3e-5f, 4.3e-5f, 16000.0f, 4000.0f, 0.0004f, 0.0004f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.4f, 0.0f, true},
    {"WOLF-RAYET STAR", "WOLF-RAYET", "A FEW HUNDRED THOUSAND YEARS", "THICK WINDS STRIP THE STAR TO ITS HOT BLUE-WHITE CORE", "EXPANDING WIND SHELLS GLOW AROUND IT AS A NEBULA", "EXAMPLE: WR 124",
        11, 0, 14.0f, 0.04f, 0.04f, 120000.0f, 120000.0f, 100.0f, 3.6e5f, 0.0f, 1.0f, 2.0f, 1.6e5f, 1.0f, 0.3f, 0.0f, true},
    {"MAGNETAR", "MAGNETAR", "ACTIVE FOR ABOUT 10,000 YEARS", "THE MOST MAGNETIC OBJECT KNOWN - A FIELD A TRILLION TIMES EARTH'S", "STARQUAKES CRACK ITS CRUST AND BLAST OUT GAMMA RAYS", "EXAMPLE: SGR 1806-20",
        12, 0, 14.0f, 6.7e-8f, 6.7e-8f, 1.0e6f, 1.0e6f, 1.6e-6f, 1.6e-6f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.4f, -0.35f, true},
    {"KILONOVA", "KILONOVA", "SECONDS TO MERGE - WEEKS TO FADE", "TWO NEUTRON STARS SPIRAL TOGETHER AND COLLIDE IN A BRILLIANT FLASH", "THE GOLD IN YOUR RING CAME FROM COLLIDING NEUTRON STARS", "EXAMPLE: GW170817 (SEEN IN 2017)",
        13, 0, 15.0f, 6.7e-8f, 6.7e-8f, 1.0e6f, 1.0e6f, 1.0e-5f, 1.0e-5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.4f, 0.0f, true},
};

const int STAGE_COUNT = (int)(sizeof(STAGES) / sizeof(STAGES[0]));

std::vector<int> lifeSequence(const LifeState& L) {
    std::vector<int> s;
    auto add = [&](std::initializer_list<int> l) { for (int i : l) s.push_back(i); };
    if (L.mass == 3) {
        float m = MASS_PRESETS[L.preset];
        if (m < 0.5f) add({0, 1, ST_RED_DWARF, ST_BLUE_DWARF, ST_WD_BARE, 7});
        else if (m < 8.0f) add({0, 1, 2, 3, 4, 5, 6, 7});
        else if (m < 25.0f) {
            add({8, 9, 10, 11});
            if (m >= 15.0f && m <= 20.0f) s.push_back(ST_MAGNETAR);
            s.push_back(ST_KILONOVA);
        } else if (m < 40.0f) add({8, 9, 10, 12});
        else add({8, ST_WOLF_RAYET, 10, 12});
        return s;
    }
    if (L.mass != 1) for (int i = 0; i <= 7; i++) s.push_back(i);
    if (L.mass == 1) s.push_back(0);
    if (L.mass != 0) add({8, 9, 10, 11, ST_KILONOVA, 12});
    return s;
}

float lifeStageMass(const LifeState& L, int stage) {
    if (L.mass == 3) return MASS_PRESETS[L.preset];
    if (L.mass == 0) return 1.0f;
    if (L.mass == 1) return 20.0f;
    return stage >= 8 ? 20.0f : 1.0f;
}

std::string lifeMassText(float m) {
    char b[32];
    if (m < 1.0f) std::snprintf(b, sizeof(b), "%.1f", m);
    else std::snprintf(b, sizeof(b), "%.0f", m);
    return b;
}

std::string lifeLifetimeText(float m) {
    if (m < 0.5f) return "TRILLIONS OF YEARS";
    double y = 1.0e10 * std::pow((double)m, -2.5);
    const char* unit = "YEARS";
    double v = y;
    if (y >= 1.0e12) { v = y / 1.0e12; unit = "TRILLION YEARS"; }
    else if (y >= 1.0e9) { v = y / 1.0e9; unit = "BILLION YEARS"; }
    else if (y >= 1.0e6) { v = y / 1.0e6; unit = "MILLION YEARS"; }
    else if (y >= 1.0e3) { v = y / 1.0e3; unit = "THOUSAND YEARS"; }
    char b[64];
    if (v < 10.0) std::snprintf(b, sizeof(b), "%.1f %s", v, unit);
    else std::snprintf(b, sizeof(b), "%.0f %s", v, unit);
    return b;
}

StageText lifeStageText(int stage, float m) {
    const Stage& s = STAGES[stage];
    StageText t{s.name, s.span, s.fact, s.fact2 ? s.fact2 : "", s.ex ? s.ex : ""};
    std::string ms = lifeMassText(m);
    if (stage == 2) {
        t.span = lifeLifetimeText(m);
        if (std::fabs(m - 1.0f) > 0.01f) t.fact = ms + " SUNS OF MASS - A STEADY STAR FUSING HYDROGEN; BIGGER STARS BURN FASTER";
        if (m > 2.0f) t.ex = "EXAMPLE: SIRIUS A";
    } else if (stage == 3 && std::fabs(m - 1.0f) > 0.01f) {
        t.fact = "THE HYDROGEN RUNS OUT AND THE STAR SWELLS INTO A RED GIANT";
        t.fact2 = "ANY INNER PLANETS ARE SWALLOWED";
    } else if (stage == 8) {
        t.span = lifeLifetimeText(m);
        if (std::fabs(m - 20.0f) > 0.01f) t.fact = ms + " SUNS OF MASS - IT BURNS FAR BRIGHTER THAN THE SUN AND DIES YOUNG";
    } else if (stage == 10 && m >= 40.0f) {
        t.name = "DIRECT COLLAPSE AND SUPERNOVA";
        t.fact = "THE STRIPPED CORE COLLAPSES AT ONCE IN A VIOLENT BLAST";
    }
    return t;
}

float lifeEpoch(int stage, float u) {
    switch (stage) {
        case 0: case 1: return 0.0f;
        case 2: case 8: case 13: case 14: case 15: return 1.0f;
        case 3: return 1.6f + 0.8f * u;
        case 4: return 2.4f + 0.3f * u;
        case 5: case 6: case 7: return 2.7f;
        case 9: return 2.0f + 1.6f * u;
        case 10: return 4.0f + 2.0f * u;
        case 16: return 2.5f + 1.1f * u;
        case 18: return 6.0f + u;
        default: return 6.0f;
    }
}

float elementBirth(int z) {
    if (z <= 3) return 0.0f;
    if (z <= 5) return 5.0f;
    if (z <= 8) return 2.0f;
    if (z <= 20) return 3.0f;
    if (z <= 24) return 3.6f;
    if (z <= 28) return 4.0f;
    if (z == 43 || z == 61 || z >= 95) return 99.0f;
    if (z <= 47) return 5.0f;
    if (z <= 55) return 5.4f;
    return 6.0f;
}

int elementCell(int row, int col) {
    auto in = [&](int a, int b) { return col >= a && col <= b; };
    switch (row) {
        case 0: return col == 0 ? 1 : col == 17 ? 2 : 0;
        case 1: return in(0, 1) ? 3 + col : in(12, 17) ? col - 7 : 0;
        case 2: return in(0, 1) ? 11 + col : in(12, 17) ? col + 1 : 0;
        case 3: return 19 + col;
        case 4: return 37 + col;
        case 5: return in(0, 1) ? 55 + col : in(3, 17) ? 69 + col : 0;
        case 6: return in(0, 1) ? 87 + col : in(3, 17) ? 101 + col : 0;
        default: {
            static const int f[5] = {60, 63, 90, 92, 94};
            return in(3, 7) ? f[col - 3] : 0;
        }
    }
}

const char* elementSymbol(int z) {
    static const char* S[] = {"", "H", "He", "Li", "Be", "B", "C", "N", "O", "F", "Ne", "Na", "Mg", "Al", "Si", "P", "S", "Cl", "Ar", "K", "Ca",
        "Sc", "Ti", "V", "Cr", "Mn", "Fe", "Co", "Ni", "Cu", "Zn", "Ga", "Ge", "As", "Se", "Br", "Kr", "Rb", "Sr", "Y", "Zr",
        "Nb", "Mo", "Tc", "Ru", "Rh", "Pd", "Ag", "Cd", "In", "Sn", "Sb", "Te", "I", "Xe", "Cs", "Ba", "La", "Ce", "Pr", "Nd",
        "Pm", "Sm", "Eu", "Gd", "Tb", "Dy", "Ho", "Er", "Tm", "Yb", "Lu", "Hf", "Ta", "W", "Re", "Os", "Ir", "Pt", "Au", "Hg",
        "Tl", "Pb", "Bi", "Po", "At", "Rn", "Fr", "Ra", "Ac", "Th", "Pa", "U", "Np", "Pu", "Am", "Cm", "Bk", "Cf", "Es", "Fm",
        "Md", "No", "Lr", "Rf", "Db", "Sg", "Bh", "Hs", "Mt", "Ds", "Rg", "Cn", "Nh", "Fl", "Mc", "Lv", "Ts", "Og"};
    return z >= 1 && z <= 118 ? S[z] : "";
}

const char* lifeEpochNote(float e) {
    if (e < 0.9f) return "H AND HE: THE BIG BANG";
    if (e < 1.7f) return "HE: FUSED IN STAR CORES";
    if (e < 3.0f) return "C N O: MADE IN GIANTS";
    if (e < 3.9f) return "NE TO CA: MASSIVE CORES";
    if (e < 4.6f) return "FE AND NI: CORE COLLAPSE";
    if (e < 5.9f) return "SUPERNOVA: HEAVIER ATOMS";
    if (e < 6.01f) return "SUPERNOVA: HEAVY ATOMS";
    return "KILONOVA: GOLD PLATINUM URANIUM";
}
