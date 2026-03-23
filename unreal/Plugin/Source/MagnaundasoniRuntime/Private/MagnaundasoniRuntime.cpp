// Copyright Project Magnaundasoni. All Rights Reserved.

#include "MagnaundasoniRuntimeModule.h"
#include "MagnaundasoniNativeBridge.h"
#include "MagnaundasoniGeometryComponent.h"
#include "MagnaundasoniModule.h"       // base module: FMagnaundasoniModule

#include "Components/StaticMeshComponent.h"
#include "Components/AudioComponent.h"
#include "HAL/PlatformProcess.h"
#include "Engine/StaticMesh.h"
#include "Misc/Paths.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/IPluginManager.h"
#include "Sound/SoundWaveProcedural.h"
#include "StaticMeshResources.h"

#include "MagnaundasoniActorComponent.h"

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
static TMap<TWeakObjectPtr<UWorld>, uint32> GAutoListenerIDs;

struct FMagWorldAudioRendererState
{
    TWeakObjectPtr<AActor> Actor;
    TWeakObjectPtr<UAudioComponent> AudioComponent;
    TWeakObjectPtr<USoundWaveProcedural> ProceduralSound;
    TArray<float> FloatBuffer;
    TArray<uint8> PCMBuffer;
    uint32 SampleRate = 48000;
    uint32 NumChannels = 2;
    uint32 FramesPerChunk = 1024;
};

static TMap<TWeakObjectPtr<UWorld>, FMagWorldAudioRendererState> GWorldAudioRenderers;

namespace
{
static constexpr float kCmToM = 0.01f;

void CopyVec(float Out[3], const FVector& V)
{
    Out[0] = static_cast<float>(V.X * kCmToM);
    Out[1] = static_cast<float>(V.Y * kCmToM);
    Out[2] = static_cast<float>(V.Z * kCmToM);
}

void CopyDir(float Out[3], const FVector& V)
{
    const FVector Normalized = V.GetSafeNormal();
    Out[0] = static_cast<float>(Normalized.X);
    Out[1] = static_cast<float>(Normalized.Y);
    Out[2] = static_cast<float>(Normalized.Z);
}

UStaticMeshComponent* FindUsableStaticMeshComponent(const AActor* Actor)
{
    if (!Actor) return nullptr;

    TInlineComponentArray<UStaticMeshComponent*> MeshComponentsArray;
    Actor->GetComponents<UStaticMeshComponent>(MeshComponentsArray);

    for (UStaticMeshComponent* Candidate : MeshComponentsArray)
    {
        if (!Candidate) continue;

        UStaticMesh* Mesh = Candidate->GetStaticMesh();
        if (!Mesh) continue;

        const FStaticMeshRenderData* RenderData = Mesh->GetRenderData();
        if (!RenderData || RenderData->LODResources.Num() == 0) continue;

        return Candidate;
    }

    return nullptr;
}

bool HasExplicitMagSourceComponent(const AActor* Actor)
{
    if (!Actor) return false;

    TInlineComponentArray<UMagSourceComponent*> SourceComponents;
    Actor->GetComponents<UMagSourceComponent>(SourceComponents);

    for (UMagSourceComponent* SourceComponent : SourceComponents)
    {
        if (SourceComponent && !SourceComponent->HasAnyFlags(RF_Transient))
        {
            return true;
        }
    }

    return false;
}

bool HasMagSourceForAudioComponent(const AActor* Actor, const UAudioComponent* AudioComponent)
{
    if (!Actor || !AudioComponent) return false;

    TInlineComponentArray<UMagSourceComponent*> SourceComponents;
    Actor->GetComponents<UMagSourceComponent>(SourceComponents);

    for (UMagSourceComponent* SourceComponent : SourceComponents)
    {
        if (SourceComponent && SourceComponent->GetAttachParent() == AudioComponent)
        {
            return true;
        }
    }

    return false;
}

void TryAutoAttachGeometryComponent(AActor* Actor)
{
    if (!GNativeEngine || !GBridge.IsValid()) return;

    UWorld* World = Actor ? Actor->GetWorld() : nullptr;
    if (!World || !World->IsGameWorld()) return;
    if (Actor->HasAnyFlags(RF_ClassDefaultObject)) return;
    if (Actor->FindComponentByClass<UMagGeometryComponent>()) return;

    UStaticMeshComponent* MeshComponent = FindUsableStaticMeshComponent(Actor);
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

void TryAutoAttachSourceComponents(AActor* Actor)
{
    if (!GNativeEngine || !GBridge.IsValid()) return;

    UWorld* World = Actor ? Actor->GetWorld() : nullptr;
    if (!World || !World->IsGameWorld()) return;
    if (!Actor || Actor->HasAnyFlags(RF_ClassDefaultObject)) return;
    if (HasExplicitMagSourceComponent(Actor)) return;

    TInlineComponentArray<UAudioComponent*> AudioComponents;
    Actor->GetComponents<UAudioComponent>(AudioComponents);

    for (UAudioComponent* AudioComponent : AudioComponents)
    {
        if (!AudioComponent) continue;
        if (HasMagSourceForAudioComponent(Actor, AudioComponent)) continue;

        UMagSourceComponent* SourceComponent =
            NewObject<UMagSourceComponent>(Actor, NAME_None, RF_Transient);
        if (!SourceComponent) continue;

        SourceComponent->SetupAttachment(AudioComponent);
        Actor->AddInstanceComponent(SourceComponent);
        SourceComponent->RegisterComponent();
        SourceComponent->InitializeAutoAttachment(AudioComponent);

        UE_LOG(LogMagnaundasoniRuntime, Verbose,
               TEXT("[%s] Auto-attached Mag Source component to '%s'."),
               *Actor->GetName(), *AudioComponent->GetName());
    }
}

void UnregisterAutoListener(UWorld* World)
{
    if (!World) return;

    const TWeakObjectPtr<UWorld> WorldKey(World);
    const uint32* ListenerID = GAutoListenerIDs.Find(WorldKey);
    if (!ListenerID || *ListenerID == 0) return;

    if (GBridge.ListenerUnregister && GNativeEngine)
    {
        GBridge.ListenerUnregister(
            reinterpret_cast<MagEngineNative>(GNativeEngine),
            static_cast<MagListenerIDNative>(*ListenerID));
    }

    if (GPrimaryListenerID == *ListenerID)
    {
        GPrimaryListenerID = 0;
    }

    GAutoListenerIDs.Remove(WorldKey);
}

void DestroyWorldAudioRenderer(UWorld* World)
{
    if (!World) return;

    const TWeakObjectPtr<UWorld> WorldKey(World);
    if (FMagWorldAudioRendererState* State = GWorldAudioRenderers.Find(WorldKey))
    {
        if (UAudioComponent* AudioComponent = State->AudioComponent.Get())
        {
            AudioComponent->Stop();
            AudioComponent->DestroyComponent();
        }

        if (AActor* Actor = State->Actor.Get())
        {
            Actor->Destroy();
        }

        GWorldAudioRenderers.Remove(WorldKey);
    }
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

    for (const TPair<TWeakObjectPtr<UWorld>, FDelegateHandle>& Pair : GActorSpawnHandles)
    {
        if (UWorld* World = Pair.Key.Get())
        {
            World->RemoveOnActorSpawnedHandler(Pair.Value);
        }
    }
    GActorSpawnHandles.Empty();
    GAutoRegisteredWorlds.Empty();
    GAutoListenerIDs.Empty();
    GWorldAudioRenderers.Empty();

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

    EnsureWorldAutoRegistration(World);
    UpdateAutoListener(World);
    GBridge.Update(reinterpret_cast<MagEngineNative>(GNativeEngine), DeltaSeconds);
    EnsureWorldAudioRenderer(World);
    PumpWorldAudioRenderer(World);
}

void FMagnaundasoniRuntimeModule::EnsureWorldAutoRegistration(UWorld* World)
{
    if (!World) return;

    const TWeakObjectPtr<UWorld> WorldKey(World);
    if (GAutoRegisteredWorlds.Contains(WorldKey)) return;

    for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
    {
        TryAutoAttachGeometryComponent(*ActorIt);
        TryAutoAttachSourceComponents(*ActorIt);
    }

    if (!GActorSpawnHandles.Contains(WorldKey))
    {
        const FDelegateHandle SpawnHandle = World->AddOnActorSpawnedHandler(
            FOnActorSpawned::FDelegate::CreateRaw(this, &FMagnaundasoniRuntimeModule::OnActorSpawned));
        GActorSpawnHandles.Add(WorldKey, SpawnHandle);
    }

    GAutoRegisteredWorlds.Add(WorldKey);
}

void FMagnaundasoniRuntimeModule::UpdateAutoListener(UWorld* World)
{
    if (!World || !GNativeEngine || !GBridge.ListenerRegister || !GBridge.ListenerUpdate) return;

    const TWeakObjectPtr<UWorld> WorldKey(World);
    const uint32 ExistingAutoListenerID = GAutoListenerIDs.FindRef(WorldKey);

    if (ExistingAutoListenerID != 0
        && GPrimaryListenerID != 0
        && GPrimaryListenerID != ExistingAutoListenerID)
    {
        UnregisterAutoListener(World);
        return;
    }

    APlayerController* PlayerController = World->GetFirstPlayerController();
    if (!PlayerController)
    {
        UnregisterAutoListener(World);
        return;
    }

    FVector ListenerLocation;
    FVector ListenerFront;
    FVector ListenerRight;
    PlayerController->GetAudioListenerPosition(ListenerLocation, ListenerFront, ListenerRight);

    FVector ListenerUp = FVector::CrossProduct(ListenerFront, ListenerRight).GetSafeNormal();
    if (ListenerUp.IsNearlyZero())
    {
        ListenerUp = FVector::UpVector;
    }

    FMagListenerDescNative Desc = {};
    CopyVec(Desc.position, ListenerLocation);
    CopyDir(Desc.forward, ListenerFront);
    CopyDir(Desc.up, ListenerUp);

    uint32 AutoListenerID = ExistingAutoListenerID;
    if (AutoListenerID == 0)
    {
        MagListenerIDNative OutID = 0;
        const MagStatusNative Status = GBridge.ListenerRegister(
            reinterpret_cast<MagEngineNative>(GNativeEngine), &Desc, &OutID);
        if (Status != 0) return;

        AutoListenerID = static_cast<uint32>(OutID);
        GAutoListenerIDs.Add(WorldKey, AutoListenerID);

        UE_LOG(LogMagnaundasoniRuntime, Verbose,
               TEXT("[%s] Auto-registered primary listener from the active audio listener (ID=%u)."),
               *World->GetName(), AutoListenerID);
    }
    else
    {
        GBridge.ListenerUpdate(
            reinterpret_cast<MagEngineNative>(GNativeEngine),
            static_cast<MagListenerIDNative>(AutoListenerID), &Desc);
    }

    if (GPrimaryListenerID == 0 || GPrimaryListenerID == AutoListenerID)
    {
        GPrimaryListenerID = AutoListenerID;
    }
}

void FMagnaundasoniRuntimeModule::EnsureWorldAudioRenderer(UWorld* World)
{
    if (!World || !World->IsGameWorld()) return;

    const TWeakObjectPtr<UWorld> WorldKey(World);
    FMagWorldAudioRendererState* ExistingState = GWorldAudioRenderers.Find(WorldKey);
    if (ExistingState && ExistingState->AudioComponent.IsValid() && ExistingState->ProceduralSound.IsValid())
    {
        return;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.ObjectFlags |= RF_Transient;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AActor* AudioActor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SpawnParams);
    if (!AudioActor) return;

    AudioActor->SetActorHiddenInGame(true);

    UAudioComponent* AudioComponent = NewObject<UAudioComponent>(AudioActor, NAME_None, RF_Transient);
    USoundWaveProcedural* ProceduralSound = NewObject<USoundWaveProcedural>(AudioComponent, NAME_None, RF_Transient);
    if (!AudioComponent || !ProceduralSound)
    {
        if (AudioActor)
        {
            AudioActor->Destroy();
        }
        return;
    }

    ProceduralSound->SetSampleRate(48000);
    ProceduralSound->NumChannels = 2;
    ProceduralSound->Duration = INDEFINITELY_LOOPING_DURATION;
    ProceduralSound->SoundGroup = SOUNDGROUP_Default;

    AudioActor->AddInstanceComponent(AudioComponent);
    AudioActor->SetRootComponent(AudioComponent);
    AudioComponent->bAutoActivate = false;
    AudioComponent->bAllowSpatialization = false;
    AudioComponent->bIsUISound = true;
    AudioComponent->SetSound(ProceduralSound);
    AudioComponent->RegisterComponent();
    AudioComponent->Play();

    FMagWorldAudioRendererState& State = GWorldAudioRenderers.FindOrAdd(WorldKey);
    State.Actor = AudioActor;
    State.AudioComponent = AudioComponent;
    State.ProceduralSound = ProceduralSound;
}

void FMagnaundasoniRuntimeModule::PumpWorldAudioRenderer(UWorld* World)
{
    if (!World || !GNativeEngine || !GBridge.RenderAudio) return;

    const uint32 ListenerID = GetPrimaryListenerID();
    if (ListenerID == 0) return;

    const TWeakObjectPtr<UWorld> WorldKey(World);
    FMagWorldAudioRendererState* State = GWorldAudioRenderers.Find(WorldKey);
    if (!State || !State->AudioComponent.IsValid() || !State->ProceduralSound.IsValid()) return;

    State->FloatBuffer.SetNumZeroed(static_cast<int32>(State->FramesPerChunk * State->NumChannels));
    const MagStatusNative Status = GBridge.RenderAudio(
        reinterpret_cast<MagEngineNative>(GNativeEngine),
        static_cast<MagListenerIDNative>(ListenerID),
        State->FloatBuffer.GetData(),
        State->FramesPerChunk,
        State->NumChannels,
        State->SampleRate);
    if (Status != 0) return;

    State->PCMBuffer.SetNumUninitialized(State->FloatBuffer.Num() * static_cast<int32>(sizeof(int16)));
    int16* PCMOut = reinterpret_cast<int16*>(State->PCMBuffer.GetData());
    for (int32 SampleIndex = 0; SampleIndex < State->FloatBuffer.Num(); ++SampleIndex)
    {
        PCMOut[SampleIndex] = static_cast<int16>(FMath::Clamp(State->FloatBuffer[SampleIndex], -1.0f, 1.0f) * 32767.0f);
    }

    State->ProceduralSound->QueueAudio(State->PCMBuffer.GetData(), State->PCMBuffer.Num());
    if (!State->AudioComponent->IsPlaying())
    {
        State->AudioComponent->Play();
    }
}

void FMagnaundasoniRuntimeModule::OnActorSpawned(AActor* Actor)
{
    TryAutoAttachGeometryComponent(Actor);
    TryAutoAttachSourceComponents(Actor);
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

    UnregisterAutoListener(World);
    DestroyWorldAudioRenderer(World);
    GAutoRegisteredWorlds.Remove(WorldKey);
}

void FMagnaundasoniRuntimeModule::OnLevelAddedToWorld(ULevel* Level, UWorld* World)
{
    if (!Level || !World || !World->IsGameWorld()) return;

    for (AActor* Actor : Level->Actors)
    {
        if (!Actor) continue;
        TryAutoAttachGeometryComponent(Actor);
        TryAutoAttachSourceComponents(Actor);
    }
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
    MAG_RESOLVE_REQUIRED(SubmitSourceAudio,        "mag_submit_source_audio",        PFN_mag_submit_source_audio)
    MAG_RESOLVE_REQUIRED(RenderAudio,              "mag_render_audio",               PFN_mag_render_audio)
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
