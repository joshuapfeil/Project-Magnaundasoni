#include "SurroundPanner.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace magnaundasoni {

namespace {

constexpr float kPi = 3.14159265358979323846f;

Vec3 directionFromAngles(float azimuthDeg, float elevationDeg) {
    float azimuth = azimuthDeg * kPi / 180.0f;
    float elevation = elevationDeg * kPi / 180.0f;
    float cosElevation = std::cos(elevation);
    return {
        std::sin(azimuth) * cosElevation,
        std::sin(elevation),
        std::cos(azimuth) * cosElevation
    };
}

bool isLFEChannel(const MagSpeakerLayout& layout, uint32_t index) {
    return (layout.preset == MAG_SPEAKERS_51 ||
            layout.preset == MAG_SPEAKERS_71 ||
            layout.preset == MAG_SPEAKERS_714) &&
           index == 3;
}

} // namespace

void SurroundPanner::configure(const MagSpeakerLayout& layout) {
    speakers_.clear();
    speakers_.reserve(layout.channelCount);
    for (uint32_t i = 0; i < layout.channelCount; ++i) {
        speakers_.push_back({
            directionFromAngles(layout.azimuthDegrees[i], layout.elevationDegrees[i]).normalized(),
            i,
            !isLFEChannel(layout, i)
        });
    }
}

void SurroundPanner::pan(const Vec3& direction, float gain, float* channelGains,
                         uint32_t channelCount) const {
    if (!channelGains || channelCount == 0) return;
    std::memset(channelGains, 0, channelCount * sizeof(float));
    if (speakers_.empty()) return;

    Vec3 dir = direction.normalized();
    float total = 0.0f;
    uint32_t bestIndex = 0;
    float bestWeight = -1.0f;

    for (const auto& speaker : speakers_) {
        if (speaker.channel >= channelCount || !speaker.participatesInSpatialPanning) continue;
        float weight = std::max(0.0f, dir.dot(speaker.direction));
        weight *= weight;
        channelGains[speaker.channel] = weight;
        total += weight;
        if (weight > bestWeight) {
            bestWeight = weight;
            bestIndex = speaker.channel;
        }
    }

    if (total <= 1e-6f) {
        channelGains[bestIndex] = gain;
        return;
    }

    float scale = gain / total;
    for (uint32_t i = 0; i < channelCount; ++i) {
        channelGains[i] *= scale;
    }
}

void SurroundPanner::diffuse(float gain, float* channelGains, uint32_t channelCount,
                             float directionality) const {
    if (!channelGains || channelCount == 0) return;
    uint32_t diffuseChannels = 0;
    for (const auto& speaker : speakers_) {
        if (speaker.channel < channelCount && speaker.participatesInSpatialPanning) {
            ++diffuseChannels;
        }
    }
    bool diffuseAllChannels = diffuseChannels == 0 || speakers_.empty();
    if (diffuseAllChannels) {
        diffuseChannels = channelCount;
    }

    float perChannel = gain / static_cast<float>(diffuseChannels);
    float decorrelation = std::clamp(1.0f - directionality, 0.0f, 1.0f);
    if (diffuseAllChannels) {
        for (uint32_t i = 0; i < channelCount; ++i) {
            channelGains[i] += perChannel * decorrelation;
        }
        return;
    }

    for (const auto& speaker : speakers_) {
        if (speaker.channel < channelCount && speaker.participatesInSpatialPanning) {
            channelGains[speaker.channel] += perChannel * decorrelation;
        }
    }
}

} // namespace magnaundasoni
