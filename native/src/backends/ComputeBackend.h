#ifndef MAGNAUNDASONI_BACKENDS_COMPUTE_BACKEND_H
#define MAGNAUNDASONI_BACKENDS_COMPUTE_BACKEND_H

#include "core/BVH.h"
#include "core/Types.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace magnaundasoni {

class ComputeBackend {
public:
    virtual ~ComputeBackend() = default;

    virtual bool available() const = 0;
    virtual bool attachExternalD3D11Device(void* device, void* deviceContext) = 0;
    virtual bool usingExternalD3D11Device() const = 0;
    virtual bool syncScene(const BVH& bvh) = 0;
    virtual bool traceClosestBatch(const std::vector<Ray>& rays,
                                   std::vector<HitResult>& outHits) = 0;
    virtual bool traceAnyBatch(const std::vector<Ray>& rays,
                               std::vector<uint8_t>& outHits) = 0;
};

std::unique_ptr<ComputeBackend> createComputeBackend();

} // namespace magnaundasoni

#endif // MAGNAUNDASONI_BACKENDS_COMPUTE_BACKEND_H
