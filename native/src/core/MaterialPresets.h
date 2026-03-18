/**
 * @file MaterialPresets.h
 * @brief Acoustically realistic built-in material presets.
 */

#ifndef MAGNAUNDASONI_CORE_MATERIAL_PRESETS_H
#define MAGNAUNDASONI_CORE_MATERIAL_PRESETS_H

#include "Magnaundasoni.h"

namespace magnaundasoni {

/**
 * Look up a named preset and populate @p outDesc.
 * @return true if the preset was found.
 *
 * Supported names (case-insensitive):
 *   General, Metal, Wood, Fabric, Rock, Dirt, Grass, Carpet,
 *   Glass, Concrete, Plaster, Water
 */
bool getMaterialPreset(const char* name, MagMaterialDesc& outDesc);

} // namespace magnaundasoni

#endif // MAGNAUNDASONI_CORE_MATERIAL_PRESETS_H
