// Copyright Project Magnaundasoni. All Rights Reserved.

#pragma once

/**
 * MagnaundasoniNativeBridge.h
 *
 * Internal (Private) header shared by all .cpp files in the MagnaundasoniRuntime
 * module.  Defines:
 *   - C ABI structs that mirror Magnaundasoni.h (without importing that header
 *     so we avoid pulling in <stdint.h> / extern "C" blocks in UE translation units).
 *   - Function pointer typedefs for every native symbol we use.
 *   - FMagNativeBridge struct that collects all pointers in one place.
 *
 * DO NOT include from Public headers – this is a Private implementation detail.
 */

#include "CoreMinimal.h"

// ---------------------------------------------------------------------------
// C ABI type mirrors  (must match Magnaundasoni.h exactly)
// ---------------------------------------------------------------------------

typedef int32  MagStatusNative;
typedef void*  MagEngineNative;   // same layout as MagEngine_T*

typedef uint32 MagSourceIDNative;
typedef uint32 MagListenerIDNative;
typedef uint32 MagGeometryIDNative;
typedef uint32 MagMaterialIDNative;

#define MAG_MAX_BANDS_NATIVE 8

struct FMagSourceDescNative
{
    float    position[3];
    float    direction[3];
    float    radius;
    uint32   importance;
};

struct FMagListenerDescNative
{
    float position[3];
    float forward[3];
    float up[3];
};

struct FMagGeometryDescNative
{
    const float*    vertices;
    uint32          vertexCount;
    const uint32*   indices;
    uint32          indexCount;
    MagMaterialIDNative materialID;
    uint32          dynamicFlag;   // 0=static 1=quasi 2=dyn-important 3=dyn-minor
};

struct FMagMaterialDescNative
{
    float       absorption[MAG_MAX_BANDS_NATIVE];
    float       transmission[MAG_MAX_BANDS_NATIVE];
    float       scattering[MAG_MAX_BANDS_NATIVE];
    float       roughness;
    uint32      thicknessClass;  // 0=thin 1=standard 2=thick
    float       leakageHint;
    const char* categoryTag;
};

struct FMagDirectComponentNative
{
    float delay;
    float direction[3];
    float perBandGain[MAG_MAX_BANDS_NATIVE];
    float occlusionLPF;
    float confidence;
};

struct FMagReflectionTapNative
{
    uint32 tapID;
    float  delay;
    float  direction[3];
    float  perBandEnergy[MAG_MAX_BANDS_NATIVE];
    uint32 order;
    float  stability;
};

struct FMagDiffractionTapNative
{
    uint32 edgeID;
    float  delay;
    float  direction[3];
    float  perBandAttenuation[MAG_MAX_BANDS_NATIVE];
};

struct FMagLateFieldNative
{
    float perBandDecay[MAG_MAX_BANDS_NATIVE];
    float rt60[MAG_MAX_BANDS_NATIVE];
    float roomSizeEstimate;
    float diffuseDirectionality;
};

struct FMagAcousticResultNative
{
    FMagDirectComponentNative direct;
    FMagReflectionTapNative*  reflections;
    uint32                    reflectionCount;
    FMagDiffractionTapNative* diffractions;
    uint32                    diffractionCount;
    FMagLateFieldNative       lateField;
};

// ---------------------------------------------------------------------------
// Function pointer typedefs
// ---------------------------------------------------------------------------

typedef MagStatusNative (*PFN_mag_source_register)(
    MagEngineNative engine, const FMagSourceDescNative* desc, MagSourceIDNative* outID);

typedef MagStatusNative (*PFN_mag_source_unregister)(
    MagEngineNative engine, MagSourceIDNative id);

typedef MagStatusNative (*PFN_mag_source_update)(
    MagEngineNative engine, MagSourceIDNative id, const FMagSourceDescNative* desc);

typedef MagStatusNative (*PFN_mag_listener_register)(
    MagEngineNative engine, const FMagListenerDescNative* desc, MagListenerIDNative* outID);

typedef MagStatusNative (*PFN_mag_listener_unregister)(
    MagEngineNative engine, MagListenerIDNative id);

typedef MagStatusNative (*PFN_mag_listener_update)(
    MagEngineNative engine, MagListenerIDNative id, const FMagListenerDescNative* desc);

typedef MagStatusNative (*PFN_mag_geometry_register)(
    MagEngineNative engine, const FMagGeometryDescNative* desc, MagGeometryIDNative* outID);

typedef MagStatusNative (*PFN_mag_geometry_unregister)(
    MagEngineNative engine, MagGeometryIDNative id);

typedef MagStatusNative (*PFN_mag_geometry_update_transform)(
    MagEngineNative engine, MagGeometryIDNative id, const float* transform4x4);

typedef MagStatusNative (*PFN_mag_update)(
    MagEngineNative engine, float deltaTime);

typedef MagStatusNative (*PFN_mag_get_acoustic_result)(
    MagEngineNative engine, MagSourceIDNative srcID,
    MagListenerIDNative listenerID, FMagAcousticResultNative* outResult);

typedef MagStatusNative (*PFN_mag_material_get_preset)(
    const char* presetName, FMagMaterialDescNative* outDesc);

typedef MagStatusNative (*PFN_mag_material_register)(
    MagEngineNative engine, const FMagMaterialDescNative* desc, MagMaterialIDNative* outID);

// ---------------------------------------------------------------------------
// FMagNativeBridge
// ---------------------------------------------------------------------------

/**
 * FMagNativeBridge
 *
 * Collects all resolved native function pointers.
 * Populated once in FMagnaundasoniRuntimeModule::StartupModule via
 * FPlatformProcess::GetDllExport.
 */
struct FMagNativeBridge
{
    PFN_mag_source_register           SourceRegister           = nullptr;
    PFN_mag_source_unregister         SourceUnregister         = nullptr;
    PFN_mag_source_update             SourceUpdate             = nullptr;

    PFN_mag_listener_register         ListenerRegister         = nullptr;
    PFN_mag_listener_unregister       ListenerUnregister       = nullptr;
    PFN_mag_listener_update           ListenerUpdate           = nullptr;

    PFN_mag_geometry_register         GeometryRegister         = nullptr;
    PFN_mag_geometry_unregister       GeometryUnregister       = nullptr;
    PFN_mag_geometry_update_transform GeometryUpdateTransform  = nullptr;

    PFN_mag_update                    Update                   = nullptr;
    PFN_mag_get_acoustic_result       GetAcousticResult        = nullptr;

    PFN_mag_material_get_preset       MaterialGetPreset        = nullptr;
    PFN_mag_material_register         MaterialRegister         = nullptr;

    /** Returns true if the mandatory function pointers are all resolved. */
    bool IsValid() const
    {
        return SourceRegister   && SourceUnregister   && SourceUpdate
            && ListenerRegister && ListenerUnregister && ListenerUpdate
            && GeometryRegister && GeometryUnregister && GeometryUpdateTransform
            && Update           && GetAcousticResult
            && MaterialGetPreset && MaterialRegister;
    }
};
