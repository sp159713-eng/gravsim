#include "nightsky.h"

namespace {

const char* const ORION[] = {"MEISSA", "BETELGEUSE", "ALNITAK", "ALNILAM", "MINTAKA", "BELLATRIX", "MEISSA", nullptr, "ALNITAK", "SAIPH", nullptr, "MINTAKA", "RIGEL", nullptr, nullptr};
const char* const UMA[] = {"DUBHE", "MERAK", "PHECDA", "MEGREZ", "DUBHE", nullptr, "MEGREZ", "ALIOTH", "MIZAR", "ALKAID", nullptr, nullptr};
const char* const UMI[] = {"POLARIS", "YILDUN", "EPSILON UMI", "ZETA UMI", "ETA UMI", "PHERKAD", "KOCHAB", "ZETA UMI", nullptr, nullptr};
const char* const CAS[] = {"CAPH", "SCHEDAR", "NAVI", "RUCHBAH", "SEGIN", nullptr, nullptr};
const char* const SCO[] = {"GRAFFIAS", "DSCHUBBA", "PI SCO", nullptr, "DSCHUBBA", "SIGMA SCO", "ANTARES", "TAU SCO", "EPSILON SCO", "MU1 SCO", "ZETA2 SCO", "ETA SCO", "SARGAS", "IOTA1 SCO", "KAPPA SCO", "SHAULA", "LESATH", nullptr, nullptr};
const char* const LEO[] = {"REGULUS", "ETA LEO", "ALGIEBA", "ADHAFERA", "RASALAS", "ALGENUBI", nullptr, "ALGIEBA", "ZOSMA", "DENEBOLA", "CHERTAN", "ZOSMA", nullptr, nullptr};
const char* const CYG[] = {"DENEB", "SADR", "ALBIREO", nullptr, "DELTA CYG", "SADR", "GIENAH CYG", nullptr, nullptr};
const char* const CRU[] = {"ACRUX", "GACRUX", nullptr, "MIMOSA", "DELTA CRU", nullptr, nullptr};
const char* const TAU[] = {"ZETA TAU", "ALDEBARAN", "THETA2 TAU", "GAMMA TAU", "DELTA1 TAU", "EPSILON TAU", "ELNATH", nullptr, "GAMMA TAU", "LAMBDA TAU", nullptr, nullptr};
const char* const GEM[] = {"CASTOR", "MEBSUTA", "TEJAT", "PROPUS", nullptr, "MEBSUTA", "MEKBUDA", "ALHENA", nullptr, "POLLUX", "WASAT", "MEKBUDA", nullptr, nullptr};
const char* const CMA[] = {"MIRZAM", "SIRIUS", "WEZEN", "ADHARA", "FURUD", "MIRZAM", nullptr, "WEZEN", "ALUDRA", nullptr, nullptr};
const char* const LYR[] = {"VEGA", "ZETA1 LYR", "DELTA2 LYR", "SULAFAT", "SHELIAK", "ZETA1 LYR", nullptr, nullptr};
const char* const AQL[] = {"TARAZED", "ALTAIR", "ALSHAIN", "THETA AQL", nullptr, "ALTAIR", "DELTA AQL", "LAMBDA AQL", nullptr, "DELTA AQL", "ZETA AQL", nullptr, nullptr};
const char* const SGR[] = {"ALNASL", "KAUS AUSTRALIS", "KAUS MEDIA", "KAUS BOREALIS", "PHI SGR", "TAU SGR", "ASCELLA", "KAUS AUSTRALIS", nullptr, "PHI SGR", "NUNKI", "TAU SGR", nullptr, nullptr};
const char* const PEG[] = {"MARKAB", "SCHEAT", "ALPHERATZ", "ALGENIB", "MARKAB", nullptr, "MARKAB", "HOMAM", "BIHAM", "ENIF", nullptr, "SCHEAT", "MATAR", nullptr, nullptr};
const char* const AND[] = {"ALPHERATZ", "DELTA AND", "MIRACH", "ALMACH", nullptr, nullptr};
const char* const PER[] = {"ETA PER", "GAMMA PER", "MIRFAK", "DELTA PER", "EPSILON PER", "ZETA PER", nullptr, "MIRFAK", "ALGOL", nullptr, nullptr};
const char* const BOO[] = {"ARCTURUS", "IZAR", "DELTA BOO", "NEKKAR", "SEGINUS", "RHO BOO", "ARCTURUS", "MUPHRID", nullptr, nullptr};
const char* const VIR[] = {"ZAVIJAVA", "PORRIMA", "SPICA", "HEZE", "SYRMA", nullptr, "PORRIMA", "MINELAUVA", "VINDEMIATRIX", nullptr, "PORRIMA", "HEZE", nullptr, nullptr};
const char* const CEN[] = {"ALPHA CENTAURI", "HADAR", "EPSILON CEN", "ZETA CEN", "MU CEN", nullptr, "EPSILON CEN", "DELTA CEN", "GAMMA CEN", "IOTA CEN", "MENKENT", nullptr, nullptr};

}

const NsConst NS_CONSTS[] = {
    {"ORION", ORION}, {"URSA MAJOR", UMA}, {"URSA MINOR", UMI}, {"CASSIOPEIA", CAS}, {"SCORPIUS", SCO},
    {"LEO", LEO}, {"CYGNUS", CYG}, {"CRUX", CRU}, {"TAURUS", TAU}, {"GEMINI", GEM},
    {"CANIS MAJOR", CMA}, {"LYRA", LYR}, {"AQUILA", AQL}, {"SAGITTARIUS", SGR}, {"PEGASUS", PEG},
    {"ANDROMEDA", AND}, {"PERSEUS", PER}, {"BOOTES", BOO}, {"VIRGO", VIR}, {"CENTAURUS", CEN},
};

const int NS_CONSTS_N = sizeof(NS_CONSTS) / sizeof(NS_CONSTS[0]);
