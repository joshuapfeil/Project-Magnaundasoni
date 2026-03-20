// Copyright Project Magnaundasoni. All Rights Reserved.

#include "MagnaundasoniActorComponent.h"
#include "MagnaundasoniNativeBridge.h"
#include "MagnaundasoniRuntimeModule.h"

#include "Components/AudioComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogMagSource, Log, All);

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/** Unreal uses left-handed Z-up, centimetres.  The native engine works in metres. */
static constexpr float kCmToM = 0.01f;

/** Map EMagImportanceClass to the native uint32 importance value. */
static uint32 ToNativeImportance(EMagImportanceClass Class)
{
    switch (Class)
    {
    case EMagImportanceClass::Static:            return 0;
    case EMagImportanceClass::QuasiStatic:       return 1;
    case EMagImportanceClass::DynamicImportant:  return 2;
    case EMagImportanceClass::DynamicMinor:      return 3;
    default:                                     return 2;
    }
}

/** Copy a 3-element float[3] from an FVector, converting cm → m. */
static void CopyVec(float Out[3], const FVector& V)
{
    Out[0] = static_cast<float>(V.X * kCmToM);
    Out[1] = static_cast<float>(V.Y * kCmToM);
    Out[2] = static_cast<float>(V.Z * kCmToM);
}

/** Copy a 3-element float[3] from an FVector (direction, no unit conversion). */
static void CopyDir(float Out[3], const FVector& V)
{
    Out[0] = static_cast<float>(V.X);
    Out[1] = static_cast<float>(V.Y);
    Out[2] = static_cast<float>(V.Z);
}

// ---------------------------------------------------------------------------
// Convert a native FMagAcousticResultNative → FMagAcousticResult (UE type)
// ---------------------------------------------------------------------------
static void ConvertResult(const FMagAcousticResultNative& Src, FMagAcousticResult& Dst)
{
    // Direct component
    Dst.Direct.Delay        = Src.direct.delay;
    Dst.Direct.OcclusionLPF = Src.direct.occlusionLPF;
    Dst.Direct.Confidence   = Src.direct.confidence;
    Dst.Direct.Direction    = FVector(Src.direct.direction[0],
                                      Src.direct.direction[1],
                                      Src.direct.direction[2]);
    Dst.Direct.PerBandGain.Values.SetNum(MAG_MAX_BANDS_NATIVE);
    for (int32 b = 0; b < MAG_MAX_BANDS_NATIVE; ++b)
        Dst.Direct.PerBandGain.Values[b] = Src.direct.perBandGain[b];

    // Reflections
    // Guard against malformed native data where reflectionCount > 0 but the
    // pointer is null (e.g., native engine allocation failure).
    const uint32 ReflectionCount = (Src.reflections != nullptr) ? Src.reflectionCount : 0;
    Dst.Reflections.SetNum(ReflectionCount);
    for (uint32 i = 0; i < ReflectionCount; ++i)
    {
        const FMagReflectionTapNative& S = Src.reflections[i];
        FMagReflectionTap& D             = Dst.Reflections[i];
        D.TapID     = static_cast<int32>(S.tapID);
        D.Delay     = S.delay;
        D.Order     = static_cast<int32>(S.order);
        D.Stability = S.stability;
        D.Direction = FVector(S.direction[0], S.direction[1], S.direction[2]);
        D.PerBandEnergy.Values.SetNum(MAG_MAX_BANDS_NATIVE);
        for (int32 b = 0; b < MAG_MAX_BANDS_NATIVE; ++b)
            D.PerBandEnergy.Values[b] = S.perBandEnergy[b];
    }

    // Diffractions
    // Guard against malformed native data where diffractionCount > 0 but the
    // pointer is null (e.g., native engine allocation failure).
    const uint32 DiffractionCount = (Src.diffractions != nullptr) ? Src.diffractionCount : 0;
    Dst.Diffractions.SetNum(DiffractionCount);
    for (uint32 i = 0; i < DiffractionCount; ++i)
    {
        const FMagDiffractionTapNative& S = Src.diffractions[i];
        FMagDiffractionTap& D             = Dst.Diffractions[i];
        D.EdgeID    = static_cast<int32>(S.edgeID);
        D.Delay     = S.delay;
        D.Direction = FVector(S.direction[0], S.direction[1], S.direction[2]);
        D.PerBandAttenuation.Values.SetNum(MAG_MAX_BANDS_NATIVE);
        for (int32 b = 0; b < MAG_MAX_BANDS_NATIVE; ++b)
            D.PerBandAttenuation.Values[b] = S.perBandAttenuation[b];
    }

    // Late field
    Dst.LateField.RoomSizeEstimate      = Src.lateField.roomSizeEstimate;
    Dst.LateField.DiffuseDirectionality = Src.lateField.diffuseDirectionality;
    Dst.LateField.PerBandDecay.Values.SetNum(MAG_MAX_BANDS_NATIVE);
    Dst.LateField.RT60.Values.SetNum(MAG_MAX_BANDS_NATIVE);
    for (int32 b = 0; b < MAG_MAX_BANDS_NATIVE; ++b)
    {
        Dst.LateField.PerBandDecay.Values[b] = Src.lateField.perBandDecay[b];
        Dst.LateField.RT60.Values[b]         = Src.lateField.rt60[b];
    }
}

// ===========================================================================
// UMagSourceComponent
// ===========================================================================

UMagSourceComponent::UMagSourceComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    // Pre-physics tick: push position before the simulation step runs.
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void UMagSourceComponent::BeginPlay()
{
    Super::BeginPlay();

    // Cache sibling AudioComponent (first match wins).
    if (bAutoApplyToAudioComponent && GetOwner())
    {
        CachedAudioComponent = GetOwner()->FindComponentByClass<UAudioComponent>();
        if (!CachedAudioComponent.IsValid())
        {
            UE_LOG(LogMagSource, Verbose,
                   TEXT("[%s] UMagSourceComponent: No sibling UAudioComponent found; "
                        "bAutoApplyToAudioComponent has no effect."),
                   *GetOwner()->GetName());
        }
    }

    if (bActive)
    {
        RegisterSource();
    }
}

void UMagSourceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    UnregisterSource();
    Super::EndPlay(EndPlayReason);
}

// ---------------------------------------------------------------------------
// Tick
// ---------------------------------------------------------------------------

void UMagSourceComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                         FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bActive || !bRegistered) return;

    PushSourceTransform();

    // After the module's global mag_update has run (post-tick), query the result.
    // We read from the *last* frame here – a one-frame latency that is imperceptible.
    QueryAndApplyAcousticResult();
}

// ---------------------------------------------------------------------------
// Register / Unregister
// ---------------------------------------------------------------------------

void UMagSourceComponent::RegisterSource()
{
    const FMagNativeBridge* Bridge = FMagnaundasoniRuntimeModule::GetBridge();
    MagEngine Engine = FMagnaundasoniRuntimeModule::GetNativeEngine();

    if (!Bridge || !Engine)
    {
        UE_LOG(LogMagSource, Verbose,
               TEXT("[%s] RegisterSource: Native bridge not available."),
               GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"));
        return;
    }

    FMagSourceDescNative Desc = {};
    CopyVec(Desc.position, GetComponentLocation());
    CopyDir(Desc.direction, GetForwardVector());
    Desc.radius     = RadiusCm * kCmToM;
    Desc.importance = ToNativeImportance(ImportanceClass);

    MagSourceIDNative OutID = 0;
    const MagStatusNative Status = Bridge->SourceRegister(
        reinterpret_cast<MagEngineNative>(Engine), &Desc, &OutID);

    if (Status == 0)
    {
        SourceID    = OutID;
        bRegistered = true;
        UE_LOG(LogMagSource, Verbose,
               TEXT("[%s] Registered acoustic source (ID=%u)."),
               GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"), SourceID);
    }
    else
    {
        UE_LOG(LogMagSource, Warning,
               TEXT("[%s] mag_source_register failed (status=%d)."),
               GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"), Status);
    }
}

void UMagSourceComponent::UnregisterSource()
{
    if (!bRegistered) return;

    const FMagNativeBridge* Bridge = FMagnaundasoniRuntimeModule::GetBridge();
    MagEngine Engine = FMagnaundasoniRuntimeModule::GetNativeEngine();

    if (Bridge && Engine && Bridge->SourceUnregister)
    {
        Bridge->SourceUnregister(
            reinterpret_cast<MagEngineNative>(Engine),
            static_cast<MagSourceIDNative>(SourceID));
    }

    SourceID    = 0;
    bRegistered = false;
}

// ---------------------------------------------------------------------------
// Per-frame helpers
// ---------------------------------------------------------------------------

void UMagSourceComponent::PushSourceTransform()
{
    const FMagNativeBridge* Bridge = FMagnaundasoniRuntimeModule::GetBridge();
    MagEngine Engine = FMagnaundasoniRuntimeModule::GetNativeEngine();
    if (!Bridge || !Engine || !Bridge->SourceUpdate) return;

    FMagSourceDescNative Desc = {};
    CopyVec(Desc.position, GetComponentLocation());
    CopyDir(Desc.direction, GetForwardVector());
    Desc.radius     = RadiusCm * kCmToM;
    Desc.importance = ToNativeImportance(ImportanceClass);

    Bridge->SourceUpdate(
        reinterpret_cast<MagEngineNative>(Engine),
        static_cast<MagSourceIDNative>(SourceID), &Desc);
}

void UMagSourceComponent::QueryAndApplyAcousticResult()
{
    if (!bRegistered) return;

    const FMagNativeBridge* Bridge = FMagnaundasoniRuntimeModule::GetBridge();
    MagEngine Engine = FMagnaundasoniRuntimeModule::GetNativeEngine();
    if (!Bridge || !Engine || !Bridge->GetAcousticResult) return;

    // Query against the actual primary listener ID tracked by the runtime module.
    // If no listener is registered yet, skip the query (returns 0 sentinel).
    const MagListenerIDNative PrimaryListenerID =
        static_cast<MagListenerIDNative>(FMagnaundasoniRuntimeModule::GetPrimaryListenerID());
    if (PrimaryListenerID == 0) return;

    FMagAcousticResultNative NativeResult = {};
    const MagStatusNative Status = Bridge->GetAcousticResult(
        reinterpret_cast<MagEngineNative>(Engine),
        static_cast<MagSourceIDNative>(SourceID),
        PrimaryListenerID,
        &NativeResult);

    if (Status != 0) return;

    ConvertResult(NativeResult, LastAcousticResult);

    if (bAutoApplyToAudioComponent)
    {
        UAudioComponent* Audio = CachedAudioComponent.Get();
        if (Audio)
        {
            ApplyResultToAudio(Audio);
        }
    }
}

void UMagSourceComponent::ApplyResultToAudio(UAudioComponent* Audio)
{
    if (!Audio) return;

    // --- Volume (direct path, average across bands for simplicity) ---
    float AvgGain = 0.0f;
    const TArray<float>& Gains = LastAcousticResult.Direct.PerBandGain.Values;
    for (float g : Gains) AvgGain += g;
    if (Gains.Num() > 0) AvgGain /= static_cast<float>(Gains.Num());
    Audio->SetVolumeMultiplier(FMath::Clamp(AvgGain, 0.0f, 2.0f));

    // --- Occlusion low-pass filter ---
    // UAudioComponent does not have a direct SetLowPassFilter API; developers
    // should wire up an Audio Modulation or Sound Cue LPF node and drive it
    // from LastAcousticResult.Direct.OcclusionLPF (in Hz).
    // We log a Verbose message so Blueprint users know the value is available.
    UE_LOG(LogMagSource, VeryVerbose,
           TEXT("[%s] OcclusionLPF=%.0f Hz  AvgGain=%.3f"),
           GetOwner() ? *GetOwner()->GetName() : TEXT("?"),
           LastAcousticResult.Direct.OcclusionLPF, AvgGain);
}

// ===========================================================================
// UMagListenerComponent
// ===========================================================================

UMagListenerComponent::UMagListenerComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UMagListenerComponent::BeginPlay()
{
    Super::BeginPlay();
    RegisterListener();
}

void UMagListenerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    UnregisterListener();
    Super::EndPlay(EndPlayReason);
}

void UMagListenerComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                           FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (bRegistered) PushListenerTransform();
}

void UMagListenerComponent::RegisterListener()
{
    const FMagNativeBridge* Bridge = FMagnaundasoniRuntimeModule::GetBridge();
    MagEngine Engine = FMagnaundasoniRuntimeModule::GetNativeEngine();

    if (!Bridge || !Engine)
    {
        UE_LOG(LogMagSource, Verbose,
               TEXT("[%s] RegisterListener: Native bridge not available."),
               GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"));
        return;
    }

    FMagListenerDescNative Desc = {};
    CopyVec(Desc.position, GetComponentLocation());
    CopyDir(Desc.forward,  GetForwardVector());
    CopyDir(Desc.up,       GetUpVector());

    MagListenerIDNative OutID = 0;
    const MagStatusNative Status = Bridge->ListenerRegister(
        reinterpret_cast<MagEngineNative>(Engine), &Desc, &OutID);

    if (Status == 0)
    {
        ListenerID  = OutID;
        bRegistered = true;
        UE_LOG(LogMagSource, Verbose,
               TEXT("[%s] Registered acoustic listener (ID=%u, primary=%s)."),
               GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"),
               ListenerID, bIsPrimaryListener ? TEXT("yes") : TEXT("no"));

        // Notify the runtime module so that UMagSourceComponent queries against
        // the correct listener ID rather than a hard-coded constant.
        if (bIsPrimaryListener)
        {
            FMagnaundasoniRuntimeModule::SetPrimaryListenerID(ListenerID);
        }
    }
    else
    {
        UE_LOG(LogMagSource, Warning,
               TEXT("[%s] mag_listener_register failed (status=%d)."),
               GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"), Status);
    }
}

void UMagListenerComponent::UnregisterListener()
{
    if (!bRegistered) return;

    // If we were the primary listener, clear the module's tracked ID.
    if (bIsPrimaryListener &&
        FMagnaundasoniRuntimeModule::GetPrimaryListenerID() == ListenerID)
    {
        FMagnaundasoniRuntimeModule::SetPrimaryListenerID(0);
    }

    const FMagNativeBridge* Bridge = FMagnaundasoniRuntimeModule::GetBridge();
    MagEngine Engine = FMagnaundasoniRuntimeModule::GetNativeEngine();

    if (Bridge && Engine && Bridge->ListenerUnregister)
    {
        Bridge->ListenerUnregister(
            reinterpret_cast<MagEngineNative>(Engine),
            static_cast<MagListenerIDNative>(ListenerID));
    }

    ListenerID  = 0;
    bRegistered = false;
}

void UMagListenerComponent::PushListenerTransform()
{
    const FMagNativeBridge* Bridge = FMagnaundasoniRuntimeModule::GetBridge();
    MagEngine Engine = FMagnaundasoniRuntimeModule::GetNativeEngine();
    if (!Bridge || !Engine || !Bridge->ListenerUpdate) return;

    FMagListenerDescNative Desc = {};
    CopyVec(Desc.position, GetComponentLocation());
    CopyDir(Desc.forward,  GetForwardVector());
    CopyDir(Desc.up,       GetUpVector());

    Bridge->ListenerUpdate(
        reinterpret_cast<MagEngineNative>(Engine),
        static_cast<MagListenerIDNative>(ListenerID), &Desc);
}
