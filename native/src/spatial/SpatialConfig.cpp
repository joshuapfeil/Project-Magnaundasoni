#include "SpatialConfig.h"

#include <algorithm>

namespace magnaundasoni {

namespace {

void setSpeaker(float (&azimuths)[MAG_MAX_SPEAKERS],
                float (&elevations)[MAG_MAX_SPEAKERS],
                uint32_t index, float azimuthDeg, float elevationDeg = 0.0f) {
    azimuths[index] = azimuthDeg;
    elevations[index] = elevationDeg;
}

} // namespace

MagSpatialConfig defaultSpatialConfig() {
    MagSpatialConfig config{};
    config.mode = MAG_SPATIAL_AUTO;
    config.speakerLayout = MAG_SPEAKERS_STEREO;
    config.hrtfPreset = MAG_HRTF_PRESET_DEFAULT_KEMAR;
    config.maxBinauralSources = 16;
    return config;
}

MagSpeakerLayout defaultSpeakerLayout(MagSpeakerLayoutPreset preset) {
    MagSpeakerLayout layout{};
    layout.preset = preset;

    switch (preset) {
        case MAG_SPEAKERS_QUAD:
            layout.channelCount = 4;
            setSpeaker(layout.azimuthDegrees, layout.elevationDegrees, 0, -45.0f);
            setSpeaker(layout.azimuthDegrees, layout.elevationDegrees, 1, 45.0f);
            setSpeaker(layout.azimuthDegrees, layout.elevationDegrees, 2, -135.0f);
            setSpeaker(layout.azimuthDegrees, layout.elevationDegrees, 3, 135.0f);
            break;
        case MAG_SPEAKERS_51:
            layout.channelCount = 6;
            setSpeaker(layout.azimuthDegrees, layout.elevationDegrees, 0, -30.0f);
            setSpeaker(layout.azimuthDegrees, layout.elevationDegrees, 1, 30.0f);
            setSpeaker(layout.azimuthDegrees, layout.elevationDegrees, 2, 0.0f);
            setSpeaker(layout.azimuthDegrees, layout.elevationDegrees, 3, 180.0f);
            setSpeaker(layout.azimuthDegrees, layout.elevationDegrees, 4, -110.0f);
            setSpeaker(layout.azimuthDegrees, layout.elevationDegrees, 5, 110.0f);
            break;
        case MAG_SPEAKERS_71:
            layout.channelCount = 8;
            setSpeaker(layout.azimuthDegrees, layout.elevationDegrees, 0, -30.0f);
            setSpeaker(layout.azimuthDegrees, layout.elevationDegrees, 1, 30.0f);
            setSpeaker(layout.azimuthDegrees, layout.elevationDegrees, 2, 0.0f);
            setSpeaker(layout.azimuthDegrees, layout.elevationDegrees, 3, 180.0f);
            setSpeaker(layout.azimuthDegrees, layout.elevationDegrees, 4, -90.0f);
            setSpeaker(layout.azimuthDegrees, layout.elevationDegrees, 5, 90.0f);
            setSpeaker(layout.azimuthDegrees, layout.elevationDegrees, 6, -150.0f);
            setSpeaker(layout.azimuthDegrees, layout.elevationDegrees, 7, 150.0f);
            break;
        case MAG_SPEAKERS_714:
            layout.channelCount = 12;
            setSpeaker(layout.azimuthDegrees, layout.elevationDegrees, 0, -30.0f);
            setSpeaker(layout.azimuthDegrees, layout.elevationDegrees, 1, 30.0f);
            setSpeaker(layout.azimuthDegrees, layout.elevationDegrees, 2, 0.0f);
            setSpeaker(layout.azimuthDegrees, layout.elevationDegrees, 3, 180.0f);
            setSpeaker(layout.azimuthDegrees, layout.elevationDegrees, 4, -90.0f);
            setSpeaker(layout.azimuthDegrees, layout.elevationDegrees, 5, 90.0f);
            setSpeaker(layout.azimuthDegrees, layout.elevationDegrees, 6, -150.0f);
            setSpeaker(layout.azimuthDegrees, layout.elevationDegrees, 7, 150.0f);
            setSpeaker(layout.azimuthDegrees, layout.elevationDegrees, 8, -45.0f, 45.0f);
            setSpeaker(layout.azimuthDegrees, layout.elevationDegrees, 9, 45.0f, 45.0f);
            setSpeaker(layout.azimuthDegrees, layout.elevationDegrees, 10, -135.0f, 45.0f);
            setSpeaker(layout.azimuthDegrees, layout.elevationDegrees, 11, 135.0f, 45.0f);
            break;
        case MAG_SPEAKERS_STEREO:
        default:
            layout.preset = MAG_SPEAKERS_STEREO;
            layout.channelCount = 2;
            setSpeaker(layout.azimuthDegrees, layout.elevationDegrees, 0, -30.0f);
            setSpeaker(layout.azimuthDegrees, layout.elevationDegrees, 1, 30.0f);
            break;
    }

    return layout;
}

bool isValidSpatialMode(MagSpatialMode mode) {
    return mode >= MAG_SPATIAL_AUTO && mode <= MAG_SPATIAL_CORE_AUDIO;
}

bool isValidHRTFPreset(MagHRTFPreset preset) {
    return preset == MAG_HRTF_PRESET_DEFAULT_KEMAR;
}

bool isValidSpeakerLayout(const MagSpeakerLayout* layout) {
    if (!layout) return false;
    if (layout->channelCount == 0 || layout->channelCount > MAG_MAX_SPEAKERS) return false;
    return layout->preset == MAG_SPEAKERS_CUSTOM ||
           layout->channelCount == static_cast<uint32_t>(layout->preset);
}

bool isSurroundMode(MagSpatialMode mode) {
    return mode == MAG_SPATIAL_SURROUND_STEREO ||
           mode == MAG_SPATIAL_SURROUND_QUAD ||
           mode == MAG_SPATIAL_SURROUND_51 ||
           mode == MAG_SPATIAL_SURROUND_71 ||
           mode == MAG_SPATIAL_SURROUND_714;
}

MagSpeakerLayoutPreset speakerLayoutForMode(MagSpatialMode mode,
                                            MagSpeakerLayoutPreset fallback) {
    switch (mode) {
        case MAG_SPATIAL_SURROUND_STEREO: return MAG_SPEAKERS_STEREO;
        case MAG_SPATIAL_SURROUND_QUAD:   return MAG_SPEAKERS_QUAD;
        case MAG_SPATIAL_SURROUND_51:     return MAG_SPEAKERS_51;
        case MAG_SPATIAL_SURROUND_71:     return MAG_SPEAKERS_71;
        case MAG_SPATIAL_SURROUND_714:    return MAG_SPEAKERS_714;
        default:                          return fallback;
    }
}

MagSpatialBackendInfo resolveSpatialBackend(const MagSpatialConfig& config,
                                            const MagSpeakerLayout& layout,
                                            bool hasCustomDataset) {
    MagSpatialBackendInfo info{};
    info.requestedMode = config.mode;
    info.outputChannels = std::max(1u, layout.channelCount);
    info.hasCustomHRTFDataset = hasCustomDataset ? 1u : 0u;

    switch (config.mode) {
        case MAG_SPATIAL_PASSTHROUGH:
            info.type = MAG_SPATIAL_BACKEND_PASSTHROUGH;
            info.outputChannels = 2;
            break;
        case MAG_SPATIAL_BINAURAL:
            info.type = MAG_SPATIAL_BACKEND_BUILTIN_BINAURAL;
            info.outputChannels = 2;
            info.usesHeadTracking = 1;
            break;
        case MAG_SPATIAL_SURROUND_STEREO:
        case MAG_SPATIAL_SURROUND_QUAD:
        case MAG_SPATIAL_SURROUND_51:
        case MAG_SPATIAL_SURROUND_71:
        case MAG_SPATIAL_SURROUND_714:
            info.type = MAG_SPATIAL_BACKEND_BUILTIN_SURROUND;
            break;
        case MAG_SPATIAL_WINDOWS_SONIC:
            info.type = MAG_SPATIAL_BACKEND_WINDOWS_SONIC;
            info.outputChannels = 2;
            info.usesHeadTracking = 1;
            break;
        case MAG_SPATIAL_DOLBY_ATMOS:
            info.type = MAG_SPATIAL_BACKEND_DOLBY_ATMOS;
            break;
        case MAG_SPATIAL_STEAM_AUDIO:
            info.type = MAG_SPATIAL_BACKEND_STEAM_AUDIO;
            info.outputChannels = 2;
            info.usesHeadTracking = 1;
            break;
        case MAG_SPATIAL_META_XR:
            info.type = MAG_SPATIAL_BACKEND_META_XR;
            info.outputChannels = 2;
            info.usesHeadTracking = 1;
            break;
        case MAG_SPATIAL_OPENXR:
            info.type = MAG_SPATIAL_BACKEND_OPENXR;
            info.outputChannels = 2;
            info.usesHeadTracking = 1;
            break;
        case MAG_SPATIAL_CORE_AUDIO:
            info.type = MAG_SPATIAL_BACKEND_CORE_AUDIO;
            info.outputChannels = 2;
            info.usesHeadTracking = 1;
            break;
        case MAG_SPATIAL_AUTO:
        default:
            if (layout.channelCount > 2) {
                info.type = MAG_SPATIAL_BACKEND_BUILTIN_SURROUND;
            } else {
                info.type = MAG_SPATIAL_BACKEND_BUILTIN_BINAURAL;
                info.outputChannels = 2;
                info.usesHeadTracking = 1;
            }
            break;
    }

    return info;
}

} // namespace magnaundasoni
