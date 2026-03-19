// Copyright Project Magnaundasoni. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"
#include "MagnaundasoniModule.h"   // provides MagEngine (MagEngine_T*) from the base module

// Forward declaration – full definition lives in the Private bridge header.
struct FMagNativeBridge;

/**
 * FMagnaundasoniRuntimeModule
 *
 * Startup / shutdown for the MagnaundasoniRuntime module:
 *   - Obtains the native MagEngine handle from the base Magnaundasoni module.
 *   - Resolves all required native function pointers via FPlatformProcess::GetDllExport.
 *   - Registers a per-world tick delegate that calls mag_update() once per frame,
 *     after all Actor TickComponents have run (ensuring source/listener positions
 *     are already current before the simulation step).
 *
 * Actor components (UMagSourceComponent, UMagListenerComponent,
 * UMagGeometryComponent) query FMagnaundasoniRuntimeModule::GetBridge() to
 * access the function table.
 */
class MAGNAUNDASONIRUNTIME_API FMagnaundasoniRuntimeModule : public IModuleInterface
{
public:
    // IModuleInterface ---------------------------------------------------
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    // Accessors used by Actor Components ---------------------------------

    /** Returns the singleton instance. */
    static FMagnaundasoniRuntimeModule& Get()
    {
        return FModuleManager::LoadModuleChecked<FMagnaundasoniRuntimeModule>(
            TEXT("MagnaundasoniRuntime"));
    }

    /** Returns true if the module loaded and the native function table is valid. */
    static bool IsAvailable();

    /** Returns the native engine handle (may be null if base module failed to init). */
    static MagEngine GetNativeEngine();

    /**
     * Returns the resolved native function-pointer table.
     * Returns nullptr if the table could not be resolved (e.g. native DLL absent).
     * All component calls should guard against a null return value.
     */
    static const FMagNativeBridge* GetBridge();

private:
    /** Called once per world tick after all actors have ticked. */
    void OnWorldPostActorTick(UWorld* World, ELevelTick TickType, float DeltaSeconds);

    /** Populate the function table from the already-loaded native DLL. */
    bool ResolveFunctionPointers();

    FDelegateHandle WorldPostActorTickHandle;
};
