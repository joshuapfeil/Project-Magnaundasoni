// Copyright Project Magnaundasoni. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MagnaundasoniTypes.h"
#include "MagnaundasoniGeometryComponent.generated.h"

// ---------------------------------------------------------------------------
// UMagGeometryComponent
// ---------------------------------------------------------------------------

/**
 * UMagGeometryComponent
 *
 * Attach to any Actor that has at least one UStaticMeshComponent to register
 * its collision geometry with the Magnaundasoni acoustic simulation.
 *
 * Lifecycle
 * ---------
 *   BeginPlay  → extracts vertices/indices from the first sibling
 *                UStaticMeshComponent (LOD 0 render mesh), registers an
 *                acoustic material preset, then calls mag_geometry_register.
 *   TickComponent → for ImportanceClass != Static, calls
 *                   mag_geometry_update_transform with the actor's current
 *                   world transform (dynamic occluders such as doors).
 *   EndPlay    → calls mag_geometry_unregister.
 *
 * Material presets
 * ----------------
 * Set MaterialPreset to one of the built-in names:
 *   "Concrete", "Wood", "Metal", "Carpet", "Glass", "Brick",
 *   "Fabric", "Rock", "Dirt", "Grass", "General"
 * or leave it blank to use the default (Concrete).
 *
 * Dynamic objects (doors, lifts, vehicles)
 * -----------------------------------------
 * Set ImportanceClass to DynamicImportant or DynamicMinor.  The component
 * will push transform updates every tick so the native BVH stays accurate.
 * For static world geometry leave ImportanceClass = Static (no per-tick cost).
 *
 * Blueprint workflow
 * ------------------
 *   1. Add this component to a wall, floor, ceiling, or door actor.
 *   2. Set MaterialPreset and ImportanceClass.
 *   3. For large static worlds, leave bAutoRegister = true.
 *   4. Call RegisterGeometry / UnregisterGeometry from Blueprint when you
 *      need to swap geometry at runtime (e.g., destructible walls).
 */
UCLASS(ClassGroup = "Magnaundasoni",
       meta = (BlueprintSpawnableComponent),
       DisplayName = "Mag Geometry")
class MAGNAUNDASONIRUNTIME_API UMagGeometryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UMagGeometryComponent();

    // UActorComponent interface ------------------------------------------
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    // -----------------------------------------------------------------------
    // Designer-facing properties
    // -----------------------------------------------------------------------

    /**
     * Acoustic material preset name.
     * Must match one of the preset names registered in the native engine.
     * Defaults to "Concrete".
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Magnaundasoni|Geometry")
    FString MaterialPreset = TEXT("Concrete");

    /**
     * Dynamic classification.
     * - Static: geometry never moves; the native BVH is built once.
     * - QuasiStatic: geometry moves rarely (e.g., large elevators); BVH rebuilt
     *   on transform changes above a threshold.
     * - DynamicImportant: geometry moves every frame (doors, vehicles); full
     *   transform update pushed each tick.
     * - DynamicMinor: small dynamic occluders; update at reduced frequency.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Magnaundasoni|Geometry")
    EMagImportanceClass ImportanceClass = EMagImportanceClass::Static;

    /**
     * When true (default), geometry is automatically extracted and registered
     * on BeginPlay.  Set to false if you need to defer registration until a
     * later point (e.g., after procedural mesh generation).
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Magnaundasoni|Geometry")
    bool bAutoRegister = true;

    // -----------------------------------------------------------------------
    // Blueprint callable helpers
    // -----------------------------------------------------------------------

    /** Manually register geometry with the acoustic engine. */
    UFUNCTION(BlueprintCallable, Category = "Magnaundasoni")
    void RegisterGeometry();

    /** Manually unregister geometry from the acoustic engine. */
    UFUNCTION(BlueprintCallable, Category = "Magnaundasoni")
    void UnregisterGeometry();

    /** Returns the native geometry ID (0 if not registered). */
    UFUNCTION(BlueprintPure, Category = "Magnaundasoni")
    int32 GetNativeGeometryID() const { return static_cast<int32>(GeometryID); }

private:
    uint32 GeometryID  = 0;
    uint32 MaterialID  = 0;
    bool   bRegistered = false;

    /**
     * Extract triangle soup from the first sibling UStaticMeshComponent (LOD 0).
     * Fills OutVertices (x,y,z interleaved, in metres) and OutIndices (triplets).
     * Returns false if no suitable mesh could be found.
     */
    bool ExtractMeshData(TArray<float>& OutVertices, TArray<uint32>& OutIndices);

    /**
     * Register (or look up) the material preset in the native engine and store
     * the resulting MaterialID.
     */
    void ResolveMaterial();

    /** Push the actor's current world transform to the native engine. */
    void PushTransformUpdate();

    /**
     * Convert a UE FTransform (centimetre translations) to a flat row-major
     * 4×4 float matrix with translations expressed in metres, as expected by
     * mag_geometry_update_transform.
     */
    static void ToNativeMatrix(const FTransform& Transform, float OutMatrix[16]);
};
