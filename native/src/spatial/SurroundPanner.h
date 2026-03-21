#ifndef MAGNAUNDASONI_SPATIAL_SURROUNDPANNER_H
#define MAGNAUNDASONI_SPATIAL_SURROUNDPANNER_H

#include "../core/Types.h"
#include "Magnaundasoni.h"

#include <vector>

namespace magnaundasoni {

class SurroundPanner {
public:
    void configure(const MagSpeakerLayout& layout);
    void pan(const Vec3& direction, float gain, float* channelGains,
             uint32_t channelCount) const;
    void diffuse(float gain, float* channelGains, uint32_t channelCount,
                 float directionality) const;

private:
    struct Speaker {
        Vec3 direction;
        uint32_t channel = 0;
    };

    std::vector<Speaker> speakers_;
};

} // namespace magnaundasoni

#endif // MAGNAUNDASONI_SPATIAL_SURROUNDPANNER_H
