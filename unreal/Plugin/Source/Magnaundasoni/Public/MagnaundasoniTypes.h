// Copyright Project Magnaundasoni. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MagnaundasoniTypes.generated.h"

#define MAG_MAX_BANDS 8

/** Quality level for acoustic simulation. */
UENUM(BlueprintType)
enum class EMagQualityLevel : uint8
{
    Low     UMETA(DisplayName = "Low"),
    Medium  UMETA(DisplayName = "Medium"),
    High    UMETA(DisplayName = "High"),
    Ultra   UMETA(DisplayName = "Ultra")
};

/** Importance classification for dynamic objects. */
UENUM(BlueprintType)
enum class EMagImportanceClass : uint8
{
    Static            UMETA(DisplayName = "Static"),
    QuasiStatic       UMETA(DisplayName = "Quasi-Static"),
    DynamicImportant  UMETA(DisplayName = "Dynamic Important"),
    DynamicMinor      UMETA(DisplayName = "Dynamic Minor")
};

/** Backend type used for ray tracing. */
UENUM(BlueprintType)
enum class EMagBackendType : uint8
{
    Auto        UMETA(DisplayName = "Auto"),
    SoftwareBVH UMETA(DisplayName = "Software BVH"),
    DXR         UMETA(DisplayName = "DXR"),
    VulkanRT    UMETA(DisplayName = "Vulkan RT")
};

/** Thickness classification for materials. */
UENUM(BlueprintType)
enum class EMagThicknessClass : uint8
{
    Thin     UMETA(DisplayName = "Thin"),
    Standard UMETA(DisplayName = "Standard"),
    Thick    UMETA(DisplayName = "Thick")
};

/** 8-band frequency array for acoustic data. */
USTRUCT(BlueprintType)
struct MAGNAUNDASONI_API FMagBandArray
{
    GENERATED_BODY()

    /** Band values for 125, 250, 500, 1k, 2k, 4k, 8k, 16k Hz. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Magnaundasoni")
    TArray<float> Values;

    FMagBandArray()
    {
        Values.SetNum(MAG_MAX_BANDS);
        for (int32 i = 0; i < MAG_MAX_BANDS; i++)
            Values[i] = 0.0f;
    }

    FMagBandArray(std::initializer_list<float> Init)
    {
        Values.SetNum(MAG_MAX_BANDS);
        int32 i = 0;
        for (float v : Init)
        {
            if (i < MAG_MAX_BANDS) Values[i++] = v;
        }
    }
};

/** Direct path component of acoustic result. */
USTRUCT(BlueprintType)
struct MAGNAUNDASONI_API FMagDirectComponent
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Magnaundasoni")
    float Delay = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Magnaundasoni")
    FVector Direction = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "Magnaundasoni")
    FMagBandArray PerBandGain;

    UPROPERTY(BlueprintReadOnly, Category = "Magnaundasoni")
    float OcclusionLPF = 22000.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Magnaundasoni")
    float Confidence = 0.0f;
};

/** Reflection tap from acoustic simulation. */
USTRUCT(BlueprintType)
struct MAGNAUNDASONI_API FMagReflectionTap
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Magnaundasoni")
    int32 TapID = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Magnaundasoni")
    float Delay = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Magnaundasoni")
    FVector Direction = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "Magnaundasoni")
    FMagBandArray PerBandEnergy;

    UPROPERTY(BlueprintReadOnly, Category = "Magnaundasoni")
    int32 Order = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Magnaundasoni")
    float Stability = 0.0f;
};

/** Diffraction tap from edge-based diffraction. */
USTRUCT(BlueprintType)
struct MAGNAUNDASONI_API FMagDiffractionTap
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Magnaundasoni")
    int32 EdgeID = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Magnaundasoni")
    float Delay = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Magnaundasoni")
    FVector Direction = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "Magnaundasoni")
    FMagBandArray PerBandAttenuation;
};

/** Late reverb field parameters. */
USTRUCT(BlueprintType)
struct MAGNAUNDASONI_API FMagLateField
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Magnaundasoni")
    FMagBandArray PerBandDecay;

    UPROPERTY(BlueprintReadOnly, Category = "Magnaundasoni")
    FMagBandArray RT60;

    UPROPERTY(BlueprintReadOnly, Category = "Magnaundasoni")
    float RoomSizeEstimate = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Magnaundasoni")
    float DiffuseDirectionality = 0.0f;
};

/** Complete acoustic result for a source-listener pair. */
USTRUCT(BlueprintType)
struct MAGNAUNDASONI_API FMagAcousticResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Magnaundasoni")
    FMagDirectComponent Direct;

    UPROPERTY(BlueprintReadOnly, Category = "Magnaundasoni")
    TArray<FMagReflectionTap> Reflections;

    UPROPERTY(BlueprintReadOnly, Category = "Magnaundasoni")
    TArray<FMagDiffractionTap> Diffractions;

    UPROPERTY(BlueprintReadOnly, Category = "Magnaundasoni")
    FMagLateField LateField;
};
