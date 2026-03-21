#include "HRTFDatabase.h"

#include <cstring>

namespace magnaundasoni {

namespace {
constexpr uint8_t kDefaultKemarDataset[] = {
    'S', 'O', 'F', 'A', '-', 'K', 'E', 'M', 'A', 'R'
};
} // namespace

HRTFDatabase::HRTFDatabase() = default;

bool HRTFDatabase::setCustomDataset(const void* data, size_t size) {
    if (!data || size == 0) return false;
    customDataset_.resize(size);
    std::memcpy(customDataset_.data(), data, size);
    return true;
}

void HRTFDatabase::setPreset(MagHRTFPreset preset) {
    preset_ = preset;
    customDataset_.clear();
}

size_t HRTFDatabase::datasetSize() const {
    return customDataset_.empty() ? sizeof(kDefaultKemarDataset) : customDataset_.size();
}

} // namespace magnaundasoni
