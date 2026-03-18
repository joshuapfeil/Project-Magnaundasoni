// Copyright Project Magnaundasoni. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

struct MagEngine_T;
typedef MagEngine_T* MagEngine;

/**
 * Main module for the Magnaundasoni acoustics plugin.
 * Handles native library loading and engine lifecycle.
 */
class FMagnaundasoniModule : public IModuleInterface
{
public:
    /** IModuleInterface implementation */
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    /** Get the singleton module instance. */
    static FMagnaundasoniModule& Get()
    {
        return FModuleManager::LoadModuleChecked<FMagnaundasoniModule>("Magnaundasoni");
    }

    /** Check if the module is loaded. */
    static bool IsAvailable()
    {
        return FModuleManager::Get().IsModuleLoaded("Magnaundasoni");
    }

    /** Get the native engine handle. May be null if init failed. */
    MagEngine GetNativeEngine() const { return NativeEngine; }

    /** Check if native engine is initialized. */
    bool IsEngineValid() const { return NativeEngine != nullptr; }

private:
    /** Handle to the native shared library. */
    void* NativeLibraryHandle = nullptr;

    /** Native engine instance. */
    MagEngine NativeEngine = nullptr;

    /** Load the native library for the current platform. */
    bool LoadNativeLibrary();

    /** Unload the native library. */
    void UnloadNativeLibrary();
};
