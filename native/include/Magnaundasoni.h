/**
 * @file Magnaundasoni.h
 * @brief Public C ABI for the Magnaundasoni real-time acoustics engine.
 *
 * This is the ONLY header external consumers need to include.
 * All types and functions are C-compatible so the library can be loaded
 * dynamically from any language (C#, Rust, Python, etc.).
 */

#ifndef MAGNAUNDASONI_H
#define MAGNAUNDASONI_H

#include <stdint.h>

/* ------------------------------------------------------------------ */
/* Export / import macros                                              */
/* ------------------------------------------------------------------ */
#if defined(_WIN32) || defined(__CYGWIN__)
    #ifdef MAGNAUNDASONI_EXPORTS
        #define MAG_API __declspec(dllexport)
    #else
        #define MAG_API __declspec(dllimport)
    #endif
#elif defined(__GNUC__) || defined(__clang__)
    #ifdef MAGNAUNDASONI_EXPORTS
        #define MAG_API __attribute__((visibility("default")))
    #else
        #define MAG_API
    #endif
#else
    #define MAG_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Status codes                                                       */
/* ------------------------------------------------------------------ */
typedef enum {
    MAG_OK              =  0,
    MAG_ERROR           = -1,
    MAG_INVALID_PARAM   = -2,
    MAG_OUT_OF_MEMORY   = -3,
    MAG_NOT_INITIALIZED = -4
} MagStatus;

/* ------------------------------------------------------------------ */
/* Opaque handles                                                     */
/* ------------------------------------------------------------------ */
typedef struct MagEngine_T* MagEngine;
typedef uint32_t MagSourceID;
typedef uint32_t MagListenerID;
typedef uint32_t MagGeometryID;
typedef uint32_t MagMaterialID;

/* ------------------------------------------------------------------ */
/* Quality / backend enums                                            */
/* ------------------------------------------------------------------ */
typedef enum {
    MAG_QUALITY_LOW    = 0,
    MAG_QUALITY_MEDIUM = 1,
    MAG_QUALITY_HIGH   = 2,
    MAG_QUALITY_ULTRA  = 3
} MagQualityLevel;

typedef enum {
    MAG_BACKEND_AUTO         = 0,
    MAG_BACKEND_SOFTWARE_BVH = 1,
    MAG_BACKEND_DXR          = 2,
    MAG_BACKEND_VULKAN_RT    = 3
} MagBackendType;

/* ------------------------------------------------------------------ */
/* Constants                                                          */
/* ------------------------------------------------------------------ */
#define MAG_MAX_BANDS 8

/* ------------------------------------------------------------------ */
/* Configuration                                                      */
/* ------------------------------------------------------------------ */
typedef struct {
    MagQualityLevel quality;
    MagBackendType  preferredBackend;
    uint32_t        maxSources;
    uint32_t        maxReflectionOrder;
    uint32_t        maxDiffractionDepth;
    uint32_t        raysPerSource;
    uint32_t        threadCount;
    float           worldChunkSize;
    uint32_t        effectiveBandCount;  /* 4, 6, or 8 */
} MagEngineConfig;

/* ------------------------------------------------------------------ */
/* Geometry / mesh descriptor                                         */
/* ------------------------------------------------------------------ */
typedef struct {
    const float*    vertices;      /* x,y,z interleaved                */
    uint32_t        vertexCount;
    const uint32_t* indices;
    uint32_t        indexCount;
    MagMaterialID   materialID;
    uint32_t        dynamicFlag;   /* 0=static 1=quasi 2=dyn-imp 3=dyn-minor */
} MagGeometryDesc;

/* ------------------------------------------------------------------ */
/* Material descriptor (8-band)                                       */
/* ------------------------------------------------------------------ */
typedef struct {
    float       absorption[MAG_MAX_BANDS];
    float       transmission[MAG_MAX_BANDS];
    float       scattering[MAG_MAX_BANDS];
    float       roughness;
    uint32_t    thicknessClass;    /* 0=thin, 1=standard, 2=thick     */
    float       leakageHint;
    const char* categoryTag;
} MagMaterialDesc;

/* ------------------------------------------------------------------ */
/* Source descriptor                                                   */
/* ------------------------------------------------------------------ */
typedef struct {
    float    position[3];
    float    direction[3];   /* forward direction for directivity     */
    float    radius;         /* source radius for near-field          */
    uint32_t importance;     /* 0=low 1=medium 2=high 3=critical     */
} MagSourceDesc;

/* ------------------------------------------------------------------ */
/* Listener descriptor                                                */
/* ------------------------------------------------------------------ */
typedef struct {
    float position[3];
    float forward[3];
    float up[3];
} MagListenerDesc;

/* ------------------------------------------------------------------ */
/* Output structures – canonical acoustic output contract             */
/* ------------------------------------------------------------------ */
typedef struct {
    float delay;
    float direction[3];
    float perBandGain[MAG_MAX_BANDS];
    float occlusionLPF;   /* low-pass cutoff Hz, 0 = no filter       */
    float confidence;
} MagDirectComponent;

typedef struct {
    uint32_t tapID;
    float    delay;
    float    direction[3];
    float    perBandEnergy[MAG_MAX_BANDS];
    uint32_t order;
    float    stability;
} MagReflectionTap;

typedef struct {
    uint32_t edgeID;
    float    delay;
    float    direction[3];
    float    perBandAttenuation[MAG_MAX_BANDS];
} MagDiffractionTap;

typedef struct {
    float perBandDecay[MAG_MAX_BANDS];
    float rt60[MAG_MAX_BANDS];
    float roomSizeEstimate;
    float diffuseDirectionality;
} MagLateField;

typedef struct {
    MagDirectComponent  direct;
    MagReflectionTap*   reflections;
    uint32_t            reflectionCount;
    MagDiffractionTap*  diffractions;
    uint32_t            diffractionCount;
    MagLateField        lateField;
} MagAcousticResult;

typedef struct {
    MagQualityLevel activeQuality;
    MagBackendType  backendUsed;
    double          timestamp;
    uint32_t        activeSourceCount;
    float           cpuTimeMs;
} MagGlobalState;

/* ------------------------------------------------------------------ */
/* Engine lifecycle                                                    */
/* ------------------------------------------------------------------ */
MAG_API MagStatus mag_engine_create(const MagEngineConfig* config,
                                    MagEngine* outEngine);
MAG_API MagStatus mag_engine_destroy(MagEngine engine);

/* ------------------------------------------------------------------ */
/* Material management                                                */
/* ------------------------------------------------------------------ */
MAG_API MagStatus mag_material_register(MagEngine engine,
                                        const MagMaterialDesc* desc,
                                        MagMaterialID* outID);
MAG_API MagStatus mag_material_get_preset(const char* presetName,
                                          MagMaterialDesc* outDesc);

/* ------------------------------------------------------------------ */
/* Geometry management                                                */
/* ------------------------------------------------------------------ */
MAG_API MagStatus mag_geometry_register(MagEngine engine,
                                        const MagGeometryDesc* desc,
                                        MagGeometryID* outID);
MAG_API MagStatus mag_geometry_unregister(MagEngine engine,
                                          MagGeometryID id);
MAG_API MagStatus mag_geometry_update_transform(MagEngine engine,
                                                MagGeometryID id,
                                                const float* transform4x4);

/* ------------------------------------------------------------------ */
/* Source management                                                  */
/* ------------------------------------------------------------------ */
MAG_API MagStatus mag_source_register(MagEngine engine,
                                      const MagSourceDesc* desc,
                                      MagSourceID* outID);
MAG_API MagStatus mag_source_unregister(MagEngine engine, MagSourceID id);
MAG_API MagStatus mag_source_update(MagEngine engine, MagSourceID id,
                                    const MagSourceDesc* desc);

/* ------------------------------------------------------------------ */
/* Listener management                                                */
/* ------------------------------------------------------------------ */
MAG_API MagStatus mag_listener_register(MagEngine engine,
                                        const MagListenerDesc* desc,
                                        MagListenerID* outID);
MAG_API MagStatus mag_listener_unregister(MagEngine engine,
                                          MagListenerID id);
MAG_API MagStatus mag_listener_update(MagEngine engine, MagListenerID id,
                                      const MagListenerDesc* desc);

/* ------------------------------------------------------------------ */
/* Simulation                                                         */
/* ------------------------------------------------------------------ */
MAG_API MagStatus mag_update(MagEngine engine, float deltaTime);

/* ------------------------------------------------------------------ */
/* Query results                                                      */
/* ------------------------------------------------------------------ */
MAG_API MagStatus mag_get_acoustic_result(MagEngine engine,
                                          MagSourceID sourceID,
                                          MagListenerID listenerID,
                                          MagAcousticResult* outResult);
MAG_API MagStatus mag_get_global_state(MagEngine engine,
                                       MagGlobalState* outState);

/* ------------------------------------------------------------------ */
/* Quality control                                                    */
/* ------------------------------------------------------------------ */
MAG_API MagStatus mag_set_quality(MagEngine engine, MagQualityLevel level);

/* ------------------------------------------------------------------ */
/* Debug helpers                                                      */
/* ------------------------------------------------------------------ */
MAG_API MagStatus mag_debug_get_ray_count(MagEngine engine,
                                          uint32_t* outCount);
MAG_API MagStatus mag_debug_get_active_edges(MagEngine engine,
                                             uint32_t* outCount);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* MAGNAUNDASONI_H */
