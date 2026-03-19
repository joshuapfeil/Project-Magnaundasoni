/**
 * @file MaterialPresets.cpp
 * @brief Hardcoded acoustically realistic material presets.
 *
 * Frequency bands: 63 Hz, 125 Hz, 250 Hz, 500 Hz, 1 kHz, 2 kHz, 4 kHz, 8 kHz
 * Values sourced from standard architectural acoustics references
 * (ISO 354 / Cox & D'Antonio / Kuttruff).
 */

#include "core/MaterialPresets.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>
#include <unordered_map>

namespace magnaundasoni {

namespace {

struct PresetData {
    float absorption[MAG_MAX_BANDS];
    float transmission[MAG_MAX_BANDS];
    float scattering[MAG_MAX_BANDS];
    float roughness;
    uint32_t thicknessClass;
    float leakageHint;
    const char* categoryTag;
};

// clang-format off
static const std::unordered_map<std::string, PresetData> kPresets = {
    {"general", {
        {0.10f, 0.12f, 0.14f, 0.16f, 0.18f, 0.20f, 0.22f, 0.24f},  // absorption
        {0.05f, 0.04f, 0.03f, 0.02f, 0.02f, 0.01f, 0.01f, 0.01f},  // transmission
        {0.10f, 0.12f, 0.15f, 0.18f, 0.22f, 0.25f, 0.28f, 0.30f},  // scattering
        0.5f, 1, 0.05f, "general"
    }},
    {"metal", {
        {0.01f, 0.01f, 0.02f, 0.02f, 0.03f, 0.04f, 0.05f, 0.05f},  // very low absorption
        {0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f},  // essentially opaque
        {0.10f, 0.10f, 0.12f, 0.15f, 0.18f, 0.22f, 0.28f, 0.30f},  // moderate scattering
        0.2f, 1, 0.00f, "metal"
    }},
    {"wood", {
        {0.15f, 0.11f, 0.10f, 0.07f, 0.06f, 0.07f, 0.08f, 0.09f},  // moderate, panel absorption at low freq
        {0.04f, 0.03f, 0.02f, 0.02f, 0.01f, 0.01f, 0.01f, 0.01f},  // low transmission
        {0.10f, 0.12f, 0.14f, 0.18f, 0.22f, 0.28f, 0.32f, 0.35f},  // scattering rises with freq
        0.4f, 1, 0.02f, "wood"
    }},
    {"fabric", {
        {0.03f, 0.04f, 0.11f, 0.17f, 0.24f, 0.35f, 0.45f, 0.50f},  // high absorption at HF
        {0.15f, 0.12f, 0.10f, 0.08f, 0.06f, 0.04f, 0.03f, 0.02f},  // moderate transmission
        {0.10f, 0.15f, 0.20f, 0.30f, 0.40f, 0.50f, 0.55f, 0.60f},  // very diffuse
        0.8f, 0, 0.10f, "fabric"
    }},
    {"rock", {
        {0.02f, 0.02f, 0.03f, 0.04f, 0.05f, 0.05f, 0.05f, 0.05f},  // very hard
        {0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f},
        {0.12f, 0.14f, 0.16f, 0.20f, 0.24f, 0.28f, 0.32f, 0.35f},
        0.6f, 2, 0.00f, "rock"
    }},
    {"dirt", {
        {0.15f, 0.25f, 0.40f, 0.55f, 0.60f, 0.60f, 0.60f, 0.60f},  // very absorptive porous
        {0.02f, 0.02f, 0.01f, 0.01f, 0.01f, 0.00f, 0.00f, 0.00f},
        {0.20f, 0.30f, 0.40f, 0.50f, 0.55f, 0.60f, 0.60f, 0.60f},
        0.9f, 2, 0.01f, "dirt"
    }},
    {"grass", {
        {0.11f, 0.26f, 0.60f, 0.69f, 0.92f, 0.99f, 0.99f, 0.99f},  // very absorptive at HF
        {0.10f, 0.08f, 0.05f, 0.03f, 0.02f, 0.01f, 0.01f, 0.01f},
        {0.30f, 0.40f, 0.50f, 0.60f, 0.70f, 0.80f, 0.85f, 0.90f},
        0.95f, 0, 0.05f, "grass"
    }},
    {"carpet", {
        {0.05f, 0.10f, 0.20f, 0.35f, 0.50f, 0.65f, 0.60f, 0.55f},  // peak at 4kHz
        {0.10f, 0.08f, 0.06f, 0.04f, 0.03f, 0.02f, 0.02f, 0.01f},
        {0.10f, 0.15f, 0.25f, 0.40f, 0.55f, 0.65f, 0.70f, 0.75f},
        0.85f, 0, 0.08f, "carpet"
    }},
    {"glass", {
        {0.35f, 0.25f, 0.18f, 0.12f, 0.07f, 0.05f, 0.05f, 0.05f},  // panel resonance at LF
        {0.08f, 0.06f, 0.04f, 0.03f, 0.02f, 0.02f, 0.01f, 0.01f},
        {0.05f, 0.05f, 0.06f, 0.08f, 0.10f, 0.12f, 0.14f, 0.15f},
        0.1f, 0, 0.03f, "glass"
    }},
    {"concrete", {
        {0.01f, 0.01f, 0.02f, 0.02f, 0.02f, 0.03f, 0.04f, 0.04f},
        {0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f},
        {0.10f, 0.11f, 0.12f, 0.14f, 0.16f, 0.20f, 0.24f, 0.28f},
        0.3f, 2, 0.00f, "concrete"
    }},
    {"plaster", {
        {0.01f, 0.02f, 0.02f, 0.03f, 0.04f, 0.05f, 0.05f, 0.05f},
        {0.01f, 0.01f, 0.01f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f},
        {0.10f, 0.12f, 0.14f, 0.16f, 0.20f, 0.24f, 0.28f, 0.30f},
        0.35f, 1, 0.01f, "plaster"
    }},
    {"water", {
        {0.01f, 0.01f, 0.01f, 0.02f, 0.02f, 0.03f, 0.03f, 0.04f},
        {0.90f, 0.85f, 0.80f, 0.70f, 0.60f, 0.50f, 0.40f, 0.30f},  // high transmission
        {0.05f, 0.06f, 0.08f, 0.10f, 0.12f, 0.15f, 0.18f, 0.20f},
        0.05f, 2, 0.50f, "water"
    }},
};
// clang-format on

std::string toLower(const char* s) {
    std::string out;
    while (*s) {
        out += static_cast<char>(std::tolower(static_cast<unsigned char>(*s)));
        ++s;
    }
    return out;
}

} // anonymous namespace

bool getMaterialPreset(const char* name, MagMaterialDesc& outDesc) {
    if (!name) return false;

    auto it = kPresets.find(toLower(name));
    if (it == kPresets.end()) return false;

    const PresetData& p = it->second;
    std::memcpy(outDesc.absorption,  p.absorption,  sizeof(p.absorption));
    std::memcpy(outDesc.transmission, p.transmission, sizeof(p.transmission));
    std::memcpy(outDesc.scattering,  p.scattering,  sizeof(p.scattering));
    outDesc.roughness      = p.roughness;
    outDesc.thicknessClass = p.thicknessClass;
    outDesc.leakageHint    = p.leakageHint;
    outDesc.categoryTag    = p.categoryTag;
    return true;
}

} // namespace magnaundasoni
