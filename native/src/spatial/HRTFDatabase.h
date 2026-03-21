#ifndef MAGNAUNDASONI_SPATIAL_HRTFDATABASE_H
#define MAGNAUNDASONI_SPATIAL_HRTFDATABASE_H

#include "Magnaundasoni.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace magnaundasoni {

class HRTFDatabase {
public:
    HRTFDatabase();

    bool setCustomDataset(const void* data, size_t size);
    void setPreset(MagHRTFPreset preset);

    MagHRTFPreset currentPreset() const { return preset_; }
    bool hasCustomDataset() const { return !customDataset_.empty(); }
    size_t datasetSize() const;

private:
    MagHRTFPreset      preset_ = MAG_HRTF_PRESET_DEFAULT_KEMAR;
    std::vector<uint8_t> customDataset_;
};

} // namespace magnaundasoni

#endif // MAGNAUNDASONI_SPATIAL_HRTFDATABASE_H
