#ifndef MAGNAUNDASONI_SPATIAL_SPATIALCONFIG_H
#define MAGNAUNDASONI_SPATIAL_SPATIALCONFIG_H

#include "Magnaundasoni.h"

namespace magnaundasoni {

MagSpatialConfig defaultSpatialConfig();
MagSpeakerLayout defaultSpeakerLayout(MagSpeakerLayoutPreset preset);
bool isValidSpatialMode(MagSpatialMode mode);
bool isValidHRTFPreset(MagHRTFPreset preset);
bool isValidSpeakerLayout(const MagSpeakerLayout* layout);
bool isSurroundMode(MagSpatialMode mode);
MagSpeakerLayoutPreset speakerLayoutForMode(MagSpatialMode mode,
                                            MagSpeakerLayoutPreset fallback);
MagSpatialBackendInfo resolveSpatialBackend(const MagSpatialConfig& config,
                                            const MagSpeakerLayout& layout,
                                            bool hasCustomDataset);

} // namespace magnaundasoni

#endif // MAGNAUNDASONI_SPATIAL_SPATIALCONFIG_H
