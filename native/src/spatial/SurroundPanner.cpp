#include "SurroundPanner.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace magnaundasoni {

namespace {

Vec3 directionFromAngles(float azimuthDeg, float elevationDeg) {
    float azimuth = azimuthDeg * 3.14159265358979323846f / 180.0f;
    float elevation = elevationDeg * 3.14159265358979323846f / 180.0f;
    float cosElevation = std::cos(elevation);
    return {
        std::sin(azimuth) * cosElevation,
        std::sin(elevation),
        std::cos(azimuth) * cosElevation
    };
}

} // namespace

void SurroundPanner::configure(const MagSpeakerLayout& layout) {
    speakers_.clear();
    speakers_.reserve(layout.channelCount);
    for (uint32_t i = 0; i < layout.channelCount; ++i) {
        speakers_.push_back({
            directionFromAngles(layout.azimuthDegrees[i], layout.elevationDegrees[i]).normalized(),
            i
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
        if (speaker.channel >= channelCount) continue;
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
    float perChannel = gain / static_cast<float>(channelCount);
    float decorrelation = std::clamp(1.0f - directionality, 0.0f, 1.0f);
    for (uint32_t i = 0; i < channelCount; ++i) {
        channelGains[i] += perChannel * decorrelation;
    }
}

} // namespace magnaundasoni
