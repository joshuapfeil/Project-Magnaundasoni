// Copyright Project Magnaundasoni. All Rights Reserved.

#include "MagnaundasoniRuntimeModule.h"
#include "MagnaundasoniNativeBridge.h"
#include "MagnaundasoniGeometryComponent.h"
#include "MagnaundasoniModule.h"       // base module: FMagnaundasoniModule

#include "Components/StaticMeshComponent.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Interfaces/IPluginManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogMagnaundasoniRuntime, Log, All);

// ---------------------------------------------------------------------------
// Module-level globals (safe because UE modules are loaded on the game thread)
// ---------------------------------------------------------------------------
static FMagNativeBridge  GBridge;
static MagEngine         GNativeEngine         = nullptr;
static void*             GExtraLibHandle        = nullptr;  // extra DllHandle ref we hold
static uint32            GPrimaryListenerID     = 0;
static TSet<TWeakObjectPtr<UWorld>> GAutoRegisteredWorlds;
static TMap<TWeakObjectPtr<UWorld>, FDelegateHandle> GActorSpawnHandles;

namespace
{
void TryAutoAttachGeometryComponent(AActor* Actor)
{
    if (!Actor || !Actor->GetWorld() || !Actor->GetWorld()->IsGameWorld()) return;
    if (Actor->HasAnyFlags(RF_ClassDefaultObject)) return;
    if (Actor->FindComponentByClass<UMagGeometryComponent>()) return;

    TInlineComponentArray<UStaticMeshComponent*> MeshComponents(Actor);
    Actor->GetComponents(MeshComponents);

    UStaticMeshComponent* MeshComponent = nullptr;
    for (UStaticMeshComponent* Candidate : MeshComponents)
    {
        if (Candidate && Candidate->GetStaticMesh())
        {
            MeshComponent = Candidate;
            break;
        }
    }

    if (!MeshComponent) return;

    UMagGeometryComponent* GeometryComponent =
        NewObject<UMagGeometryComponent>(Actor, NAME_None, RF_Transient);
    if (!GeometryComponent) return;

    Actor->AddInstanceComponent(GeometryComponent);
    GeometryComponent->RegisterComponent();

    if (Actor->HasActorBegunPlay())
    {
        GeometryComponent->RegisterGeometry();
    }

    UE_LOG(LogMagnaundasoniRuntime, Verbose,
           TEXT("[%s] Auto-attached Mag Geometry component for runtime registration."),
           *Actor->GetName());
}
} // namespace

// ---------------------------------------------------------------------------
// FMagnaundasoniRuntimeModule  (statics)
// ---------------------------------------------------------------------------

bool FMagnaundasoniRuntimeModule::IsAvailable()
{
    return FModuleManager::Get().IsModuleLoaded(TEXT("MagnaundasoniRuntime"))
        && GNativeEngine != nullptr
        && GBridge.IsValid();
}

MagEngine FMagnaundasoniRuntimeModule::GetNativeEngine()
{
    return GNativeEngine;
}

const FMagNativeBridge* FMagnaundasoniRuntimeModule::GetBridge()
{
    return GBridge.IsValid() ? &GBridge : nullptr;
}

void FMagnaundasoniRuntimeModule::SetPrimaryListenerID(uint32 ListenerID)
{
    GPrimaryListenerID = ListenerID;
}

uint32 FMagnaundasoniRuntimeModule::GetPrimaryListenerID()
{
    return GPrimaryListenerID;
}

// ---------------------------------------------------------------------------
// Startup / Shutdown
// ---------------------------------------------------------------------------

void FMagnaundasoniRuntimeModule::StartupModule()
{
    // The base Magnaundasoni module must be loaded first (it owns the native
    // library and the engine instance).
    if (!FMagnaundasoniModule::IsAvailable())
    {
        UE_LOG(LogMagnaundasoniRuntime, Error,
               TEXT("MagnaundasoniRuntime: Base 'Magnaundasoni' module is not available. "
                    "Ensure it is listed earlier in the .uplugin Modules array."));
        return;
    }

    GNativeEngine = FMagnaundasoniModule::Get().GetNativeEngine();

    if (!GNativeEngine)
    {
        UE_LOG(LogMagnaundasoniRuntime, Warning,
               TEXT("MagnaundasoniRuntime: Native engine handle is null – "
                    "components will be inactive. Check that the native DLL "
                    "is present in Binaries/<Platform>/."));
    }

    if (!ResolveFunctionPointers())
    {
        UE_LOG(LogMagnaundasoniRuntime, Error,
               TEXT("MagnaundasoniRuntime: Could not resolve native function pointers. "
                    "Acoustic components will be non-functional."));
        return;
    }

    // Register a post-actor-tick callback so we call mag_update() once per
    // world frame, after all TickComponents have pushed their new positions.
    WorldPostActorTickHandle = FWorldDelegates::OnWorldPostActorTick.AddRaw(
        this, &FMagnaundasoniRuntimeModule::OnWorldPostActorTick);
    WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddRaw(
        this, &FMagnaundasoniRuntimeModule::OnWorldCleanup);
    LevelAddedToWorldHandle = FWorldDelegates::LevelAddedToWorld.AddRaw(
        this, &FMagnaundasoniRuntimeModule::OnLevelAddedToWorld);

    UE_LOG(LogMagnaundasoniRuntime, Log,
           TEXT("MagnaundasoniRuntime: Started. Function table resolved. "
                "Native engine: %s."),
           GNativeEngine ? TEXT("valid") : TEXT("null (DLL not found)"));
}

void FMagnaundasoniRuntimeModule::ShutdownModule()
{
    if (WorldPostActorTickHandle.IsValid())
    {
        FWorldDelegates::OnWorldPostActorTick.Remove(WorldPostActorTickHandle);
        WorldPostActorTickHandle.Reset();
    }

    if (WorldCleanupHandle.IsValid())
    {
        FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
        WorldCleanupHandle.Reset();
    }

    if (LevelAddedToWorldHandle.IsValid())
    {
        FWorldDelegates::LevelAddedToWorld.Remove(LevelAddedToWorldHandle);
        LevelAddedToWorldHandle.Reset();
    }

    for (TPair<TWeakObjectPtr<UWorld>, FDelegateHandle>& Pair : GActorSpawnHandles)
    {
        if (UWorld* World = Pair.Key.Get())
        {
            World->RemoveOnActorSpawnedHandler(Pair.Value);
        }
    }
    GActorSpawnHandles.Empty();
    GAutoRegisteredWorlds.Empty();

    GNativeEngine = nullptr;
    FMemory::Memzero(&GBridge, sizeof(GBridge));

    if (GExtraLibHandle)
    {
        FPlatformProcess::FreeDllHandle(GExtraLibHandle);
        GExtraLibHandle = nullptr;
    }

    UE_LOG(LogMagnaundasoniRuntime, Log, TEXT("MagnaundasoniRuntime: Shut down."));
}

// ---------------------------------------------------------------------------
// Per-world tick
// ---------------------------------------------------------------------------

void FMagnaundasoniRuntimeModule::OnWorldPostActorTick(
    UWorld* World, ELevelTick /*TickType*/, float DeltaSeconds)
{
    if (!GNativeEngine || !GBridge.Update) return;

    // Only advance the simulation for game worlds (not editor preview worlds,
    // PIE spectator viewports, or editor-only ticks).
    if (!World || !World->IsGameWorld()) return;

    EnsureWorldGeometryAutoRegistration(World);
    GBridge.Update(reinterpret_cast<MagEngineNative>(GNativeEngine), DeltaSeconds);
}

void FMagnaundasoniRuntimeModule::EnsureWorldGeometryAutoRegistration(UWorld* World)
{
    if (!World) return;

    const TWeakObjectPtr<UWorld> WorldKey(World);
    if (GAutoRegisteredWorlds.Contains(WorldKey)) return;

    for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
    {
        TryAutoAttachGeometryComponent(*ActorIt);
    }

    if (!GActorSpawnHandles.Contains(WorldKey))
    {
        const FDelegateHandle SpawnHandle = World->AddOnActorSpawnedHandler(
            FOnActorSpawned::FDelegate::CreateRaw(this, &FMagnaundasoniRuntimeModule::OnActorSpawned));
        GActorSpawnHandles.Add(WorldKey, SpawnHandle);
    }

    GAutoRegisteredWorlds.Add(WorldKey);
}

void FMagnaundasoniRuntimeModule::OnActorSpawned(AActor* Actor)
{
    TryAutoAttachGeometryComponent(Actor);
}

void FMagnaundasoniRuntimeModule::OnWorldCleanup(
    UWorld* World, bool /*bSessionEnded*/, bool /*bCleanupResources*/)
{
    if (!World) return;

    const TWeakObjectPtr<UWorld> WorldKey(World);
    if (FDelegateHandle* SpawnHandle = GActorSpawnHandles.Find(WorldKey))
    {
        World->RemoveOnActorSpawnedHandler(*SpawnHandle);
        GActorSpawnHandles.Remove(WorldKey);
    }

    GAutoRegisteredWorlds.Remove(WorldKey);
}

void FMagnaundasoniRuntimeModule::OnLevelAddedToWorld(ULevel* /*Level*/, UWorld* World)
{
    if (!World || !World->IsGameWorld()) return;

    GAutoRegisteredWorlds.Remove(TWeakObjectPtr<UWorld>(World));
}

// ---------------------------------------------------------------------------
// Native DLL function-pointer resolution
// ---------------------------------------------------------------------------

bool FMagnaundasoniRuntimeModule::ResolveFunctionPointers()
{
    // Attempt to get a handle to the already-loaded native library.
    // FPlatformProcess::GetDllHandle() with just the library stem re-uses an
    // existing load rather than loading a second copy.  We keep this extra ref
    // so we can safely call GetDllExport later even if the base module has
    // freed its own ref during an unusual shutdown order.
    const TCHAR* LibStem =
#if PLATFORM_WINDOWS
        TEXT("magnaundasoni");
#elif PLATFORM_LINUX
        TEXT("libmagnaundasoni.so");
#elif PLATFORM_MAC
        TEXT("libmagnaundasoni.dylib");
#else
        TEXT("magnaundasoni");
#endif

    void* Lib = FPlatformProcess::GetDllHandle(LibStem);

    if (!Lib)
    {
        // Library not yet in the process image – fall back to the plugin
        // binaries directory, which is how the base module loads it.
        TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("Magnaundasoni"));
        if (Plugin.IsValid())
        {
            FString PluginDir = Plugin->GetBaseDir();
            FString LibPath;
#if PLATFORM_WINDOWS
            LibPath = FPaths::Combine(PluginDir, TEXT("Binaries/Win64/magnaundasoni.dll"));
#elif PLATFORM_LINUX
            LibPath = FPaths::Combine(PluginDir, TEXT("Binaries/Linux/libmagnaundasoni.so"));
#elif PLATFORM_MAC
            LibPath = FPaths::Combine(PluginDir, TEXT("Binaries/Mac/libmagnaundasoni.dylib"));
#endif
            Lib = FPlatformProcess::GetDllHandle(*LibPath);
        }
    }

    if (!Lib)
    {
        UE_LOG(LogMagnaundasoniRuntime, Warning,
               TEXT("ResolveFunctionPointers: Could not obtain native library handle. "
                    "Ensure the native DLL is built and placed in the plugin Binaries folder."));
        return false;
    }

    GExtraLibHandle = Lib;

    // Resolve each symbol.  Non-critical symbols (MaterialGetPreset,
    // MaterialRegister) log a warning but do not fail the whole table.
#define MAG_RESOLVE_REQUIRED(FieldName, SymbolName, PfnType)                           \
    GBridge.FieldName = reinterpret_cast<PfnType>(                                     \
        FPlatformProcess::GetDllExport(Lib, TEXT(SymbolName)));                        \
    if (!GBridge.FieldName)                                                            \
    {                                                                                  \
        UE_LOG(LogMagnaundasoniRuntime, Error,                                         \
               TEXT("ResolveFunctionPointers: Required symbol '%s' not found."),       \
               TEXT(SymbolName));                                                       \
    }

#define MAG_RESOLVE_OPTIONAL(FieldName, SymbolName, PfnType)                          \
    GBridge.FieldName = reinterpret_cast<PfnType>(                                    \
        FPlatformProcess::GetDllExport(Lib, TEXT(SymbolName)));                       \
    if (!GBridge.FieldName)                                                           \
    {                                                                                 \
        UE_LOG(LogMagnaundasoniRuntime, Warning,                                      \
               TEXT("ResolveFunctionPointers: Optional symbol '%s' not found."),      \
               TEXT(SymbolName));                                                     \
    }

    MAG_RESOLVE_REQUIRED(SourceRegister,           "mag_source_register",            PFN_mag_source_register)
    MAG_RESOLVE_REQUIRED(SourceUnregister,         "mag_source_unregister",          PFN_mag_source_unregister)
    MAG_RESOLVE_REQUIRED(SourceUpdate,             "mag_source_update",              PFN_mag_source_update)
    MAG_RESOLVE_REQUIRED(ListenerRegister,         "mag_listener_register",          PFN_mag_listener_register)
    MAG_RESOLVE_REQUIRED(ListenerUnregister,       "mag_listener_unregister",        PFN_mag_listener_unregister)
    MAG_RESOLVE_REQUIRED(ListenerUpdate,           "mag_listener_update",            PFN_mag_listener_update)
    MAG_RESOLVE_REQUIRED(GeometryRegister,         "mag_geometry_register",          PFN_mag_geometry_register)
    MAG_RESOLVE_REQUIRED(GeometryUnregister,       "mag_geometry_unregister",        PFN_mag_geometry_unregister)
    MAG_RESOLVE_REQUIRED(GeometryUpdateTransform,  "mag_geometry_update_transform",  PFN_mag_geometry_update_transform)
    MAG_RESOLVE_REQUIRED(Update,                   "mag_update",                     PFN_mag_update)
    MAG_RESOLVE_REQUIRED(GetAcousticResult,        "mag_get_acoustic_result",        PFN_mag_get_acoustic_result)
    MAG_RESOLVE_OPTIONAL(MaterialGetPreset,        "mag_material_get_preset",        PFN_mag_material_get_preset)
    MAG_RESOLVE_OPTIONAL(MaterialRegister,         "mag_material_register",          PFN_mag_material_register)

#undef MAG_RESOLVE_REQUIRED
#undef MAG_RESOLVE_OPTIONAL

    if (!GBridge.IsValid())
    {
        UE_LOG(LogMagnaundasoniRuntime, Error,
               TEXT("ResolveFunctionPointers: One or more required symbols could not "
                    "be resolved. The native DLL may be an incompatible version."));
        FPlatformProcess::FreeDllHandle(Lib);
        GExtraLibHandle = nullptr;
        return false;
    }

    UE_LOG(LogMagnaundasoniRuntime, Log,
           TEXT("ResolveFunctionPointers: All required symbols resolved successfully."));
    return true;
}

IMPLEMENT_MODULE(FMagnaundasoniRuntimeModule, MagnaundasoniRuntime)
