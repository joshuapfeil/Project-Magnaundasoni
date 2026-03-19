// Copyright Project Magnaundasoni. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "MagnaundasoniTypes.h"
#include "MagnaundasoniActorComponent.generated.h"

// Forward declarations
class UAudioComponent;

// ---------------------------------------------------------------------------
// UMagSourceComponent
// ---------------------------------------------------------------------------

/**
 * UMagSourceComponent
 *
 * Attach to any Actor to register it as an acoustic sound source with the
 * Magnaundasoni simulation.
 *
 * Lifecycle
 * ---------
 *   BeginPlay  → calls mag_source_register, optionally finds sibling UAudioComponent
 *   TickComponent → calls mag_source_update with current world position/orientation
 *                   (happens before the module-level mag_update call)
 *   EndPlay    → calls mag_source_unregister
 *
 * Blueprint workflow
 * ------------------
 *   1. Add this component alongside an AudioComponent on your emitter Actor.
 *   2. Set ImportanceClass and RadiusCm in the Details panel.
 *   3. Enable bAutoApplyToAudioComponent so occlusion/reverb are applied each frame.
 *   4. Read LastAcousticResult from Blueprint for custom DSP processing.
 */
UCLASS(ClassGroup = "Magnaundasoni",
       meta = (BlueprintSpawnableComponent),
       DisplayName = "Mag Source")
class MAGNAUNDASONIRUNTIME_API UMagSourceComponent : public USceneComponent
{
    GENERATED_BODY()

public:
    UMagSourceComponent();

    // UActorComponent interface ------------------------------------------
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    // -----------------------------------------------------------------------
    // Designer-facing properties
    // -----------------------------------------------------------------------

    /**
     * Importance level used by the ray-budget allocation algorithm.
     * Higher importance sources receive more simulation rays per tick.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Magnaundasoni|Source")
    EMagImportanceClass ImportanceClass = EMagImportanceClass::DynamicImportant;

    /**
     * Near-field source radius in Unreal units (centimetres).
     * Converted to metres (÷ 100) before passing to the native engine.
     * Affects near-field amplitude divergence.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Magnaundasoni|Source",
              meta = (ClampMin = "1.0", UIMin = "1.0"))
    float RadiusCm = 50.0f;

    /**
     * When true the component will automatically locate a sibling UAudioComponent
     * on BeginPlay and apply acoustic result parameters (volume, LPF, reverb send)
     * to it each tick.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Magnaundasoni|Source")
    bool bAutoApplyToAudioComponent = true;

    /** Whether this source participates in the acoustic simulation. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Magnaundasoni|Source")
    bool bActive = true;

    // -----------------------------------------------------------------------
    // Read-only results (updated each tick)
    // -----------------------------------------------------------------------

    /**
     * Most recent acoustic result for this source relative to the primary listener.
     * Populated each tick after mag_update completes.
     */
    UPROPERTY(BlueprintReadOnly, Category = "Magnaundasoni|Source")
    FMagAcousticResult LastAcousticResult;

    // -----------------------------------------------------------------------
    // Blueprint callable helpers
    // -----------------------------------------------------------------------

    /** Returns the native source ID assigned by the engine (0 if not registered). */
    UFUNCTION(BlueprintPure, Category = "Magnaundasoni")
    int32 GetNativeSourceID() const { return static_cast<int32>(SourceID); }

    /**
     * Manually trigger a result query and apply it to the sibling AudioComponent.
     * Normally called automatically when bAutoApplyToAudioComponent is true.
     */
    UFUNCTION(BlueprintCallable, Category = "Magnaundasoni")
    void QueryAndApplyAcousticResult();

private:
    uint32 SourceID    = 0;
    bool   bRegistered = false;

    /** Sibling AudioComponent resolved once on BeginPlay. */
    TWeakObjectPtr<UAudioComponent> CachedAudioComponent;

    void RegisterSource();
    void UnregisterSource();
    void PushSourceTransform();
    void ApplyResultToAudio(UAudioComponent* Audio);
};

// ---------------------------------------------------------------------------
// UMagListenerComponent
// ---------------------------------------------------------------------------

/**
 * UMagListenerComponent
 *
 * Attach to the player camera or character root to register an acoustic
 * listener with the Magnaundasoni simulation.
 *
 * Typically only one primary listener is active at a time (bIsPrimaryListener = true).
 * Secondary listeners (false) are useful for spectator cameras or binaural
 * ambisonic mixing from a fixed perspective.
 *
 * The component updates the listener position/orientation every tick so that
 * acoustic results returned from mag_get_acoustic_result are correct relative
 * to the camera/character.
 */
UCLASS(ClassGroup = "Magnaundasoni",
       meta = (BlueprintSpawnableComponent),
       DisplayName = "Mag Listener")
class MAGNAUNDASONIRUNTIME_API UMagListenerComponent : public USceneComponent
{
    GENERATED_BODY()

public:
    UMagListenerComponent();

    // UActorComponent interface ------------------------------------------
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    // -----------------------------------------------------------------------
    // Properties
    // -----------------------------------------------------------------------

    /**
     * When true, this listener is used as the reference for per-source acoustic
     * result queries.  Only one primary listener should be active in a world at
     * a time; later registrations override the previous primary.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Magnaundasoni|Listener")
    bool bIsPrimaryListener = true;

    /** Returns the native listener ID (0 if not registered). */
    UFUNCTION(BlueprintPure, Category = "Magnaundasoni")
    int32 GetNativeListenerID() const { return static_cast<int32>(ListenerID); }

private:
    uint32 ListenerID  = 0;
    bool   bRegistered = false;

    void RegisterListener();
    void UnregisterListener();
    void PushListenerTransform();
};
