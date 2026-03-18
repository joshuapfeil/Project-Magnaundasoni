// Copyright Project Magnaundasoni. All Rights Reserved.

#include "MagnaundasoniModule.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/IPluginManager.h"

// Forward declarations of C ABI functions (loaded dynamically)
extern "C"
{
    typedef int32 MagStatus;
    typedef struct MagEngine_T* MagEngineHandle;

    struct MagEngineConfig
    {
        int32 Quality;
        int32 PreferredBackend;
        uint32 MaxSources;
        uint32 MaxReflectionOrder;
        uint32 MaxDiffractionDepth;
        uint32 RaysPerSource;
        uint32 ThreadCount;
        float WorldChunkSize;
        uint32 EffectiveBandCount;
    };

    typedef MagStatus (*mag_engine_create_fn)(const MagEngineConfig*, MagEngineHandle*);
    typedef MagStatus (*mag_engine_destroy_fn)(MagEngineHandle);
}

// Function pointers loaded from DLL
static mag_engine_create_fn  pfn_mag_engine_create  = nullptr;
static mag_engine_destroy_fn pfn_mag_engine_destroy = nullptr;

void FMagnaundasoniModule::StartupModule()
{
    if (!LoadNativeLibrary())
    {
        UE_LOG(LogTemp, Error, TEXT("Magnaundasoni: Failed to load native library"));
        return;
    }

    // Resolve function pointers
    pfn_mag_engine_create = (mag_engine_create_fn)
        FPlatformProcess::GetDllExport(NativeLibraryHandle, TEXT("mag_engine_create"));
    pfn_mag_engine_destroy = (mag_engine_destroy_fn)
        FPlatformProcess::GetDllExport(NativeLibraryHandle, TEXT("mag_engine_destroy"));

    if (!pfn_mag_engine_create || !pfn_mag_engine_destroy)
    {
        UE_LOG(LogTemp, Error, TEXT("Magnaundasoni: Failed to resolve native API symbols"));
        return;
    }

    // Create engine with default High quality config
    MagEngineConfig Config = {};
    Config.Quality = 2;            // High
    Config.PreferredBackend = 0;   // Auto
    Config.MaxSources = 64;
    Config.MaxReflectionOrder = 3;
    Config.MaxDiffractionDepth = 2;
    Config.RaysPerSource = 512;
    Config.ThreadCount = 4;
    Config.WorldChunkSize = 50.0f;
    Config.EffectiveBandCount = 8;

    MagStatus Result = pfn_mag_engine_create(&Config, &NativeEngine);
    if (Result != 0)
    {
        UE_LOG(LogTemp, Error, TEXT("Magnaundasoni: mag_engine_create failed with status %d"), Result);
        NativeEngine = nullptr;
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("Magnaundasoni: Native engine initialized successfully"));
    }
}

void FMagnaundasoniModule::ShutdownModule()
{
    if (NativeEngine && pfn_mag_engine_destroy)
    {
        pfn_mag_engine_destroy(NativeEngine);
        NativeEngine = nullptr;
        UE_LOG(LogTemp, Log, TEXT("Magnaundasoni: Native engine shut down"));
    }

    UnloadNativeLibrary();
}

bool FMagnaundasoniModule::LoadNativeLibrary()
{
    FString LibraryPath;
    FString PluginDir = IPluginManager::Get().FindPlugin(TEXT("Magnaundasoni"))->GetBaseDir();

#if PLATFORM_WINDOWS
    LibraryPath = FPaths::Combine(*PluginDir, TEXT("Binaries/Win64/magnaundasoni.dll"));
#elif PLATFORM_LINUX
    LibraryPath = FPaths::Combine(*PluginDir, TEXT("Binaries/Linux/libmagnaundasoni.so"));
#elif PLATFORM_MAC
    LibraryPath = FPaths::Combine(*PluginDir, TEXT("Binaries/Mac/libmagnaundasoni.dylib"));
#else
    UE_LOG(LogTemp, Error, TEXT("Magnaundasoni: Unsupported platform"));
    return false;
#endif

    if (!FPaths::FileExists(LibraryPath))
    {
        // Try alternative paths (build output)
        FString AltPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Plugins/Magnaundasoni/ThirdParty"));
#if PLATFORM_WINDOWS
        AltPath = FPaths::Combine(AltPath, TEXT("magnaundasoni.dll"));
#elif PLATFORM_LINUX
        AltPath = FPaths::Combine(AltPath, TEXT("libmagnaundasoni.so"));
#elif PLATFORM_MAC
        AltPath = FPaths::Combine(AltPath, TEXT("libmagnaundasoni.dylib"));
#endif
        if (FPaths::FileExists(AltPath))
        {
            LibraryPath = AltPath;
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Magnaundasoni: Native library not found at %s or %s"), *LibraryPath, *AltPath);
            return false;
        }
    }

    NativeLibraryHandle = FPlatformProcess::GetDllHandle(*LibraryPath);
    if (!NativeLibraryHandle)
    {
        UE_LOG(LogTemp, Error, TEXT("Magnaundasoni: Failed to load DLL from %s"), *LibraryPath);
        return false;
    }

    UE_LOG(LogTemp, Log, TEXT("Magnaundasoni: Loaded native library from %s"), *LibraryPath);
    return true;
}

void FMagnaundasoniModule::UnloadNativeLibrary()
{
    if (NativeLibraryHandle)
    {
        FPlatformProcess::FreeDllHandle(NativeLibraryHandle);
        NativeLibraryHandle = nullptr;
    }
}

IMPLEMENT_MODULE(FMagnaundasoniModule, Magnaundasoni)
