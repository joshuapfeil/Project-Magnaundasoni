// Copyright Project Magnaundasoni. All Rights Reserved.

#include "MagnaundasoniGeometryComponent.h"
#include "MagnaundasoniNativeBridge.h"
#include "MagnaundasoniRuntimeModule.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "GameFramework/Actor.h"

DEFINE_LOG_CATEGORY_STATIC(LogMagGeometry, Log, All);

static constexpr float kCmToM = 0.01f;

// ===========================================================================
// UMagGeometryComponent
// ===========================================================================

UMagGeometryComponent::UMagGeometryComponent()
{
    // Tick is only needed for dynamic geometry; disable by default and enable
    // in BeginPlay if ImportanceClass != Static.
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void UMagGeometryComponent::BeginPlay()
{
    Super::BeginPlay();

    if (bAutoRegister)
    {
        RegisterGeometry();
    }

    // Enable tick only for dynamic objects that need per-frame transform updates.
    const bool bNeedsTick =
        ImportanceClass == EMagImportanceClass::DynamicImportant ||
        ImportanceClass == EMagImportanceClass::DynamicMinor     ||
        ImportanceClass == EMagImportanceClass::QuasiStatic;

    SetComponentTickEnabled(bNeedsTick);
}

void UMagGeometryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    UnregisterGeometry();
    Super::EndPlay(EndPlayReason);
}

void UMagGeometryComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                           FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (bRegistered) PushTransformUpdate();
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

void UMagGeometryComponent::RegisterGeometry()
{
    if (bRegistered) return;

    const FMagNativeBridge* Bridge = FMagnaundasoniRuntimeModule::GetBridge();
    MagEngine Engine = FMagnaundasoniRuntimeModule::GetNativeEngine();

    if (!Bridge || !Engine)
    {
        UE_LOG(LogMagGeometry, Verbose,
               TEXT("[%s] RegisterGeometry: Native bridge not available."),
               GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"));
        return;
    }

    // Resolve material ID (register preset once per unique name).
    ResolveMaterial();

    // Extract triangle mesh data from the first sibling StaticMeshComponent.
    TArray<float>  Vertices;
    TArray<uint32> Indices;

    if (!ExtractMeshData(Vertices, Indices))
    {
        UE_LOG(LogMagGeometry, Warning,
               TEXT("[%s] RegisterGeometry: Could not extract mesh data. "
                    "Ensure a UStaticMeshComponent exists on the same Actor."),
               GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"));
        return;
    }

    // Dynamic flag from ImportanceClass
    uint32 DynFlag = 0;
    switch (ImportanceClass)
    {
    case EMagImportanceClass::Static:            DynFlag = 0; break;
    case EMagImportanceClass::QuasiStatic:       DynFlag = 1; break;
    case EMagImportanceClass::DynamicImportant:  DynFlag = 2; break;
    case EMagImportanceClass::DynamicMinor:      DynFlag = 3; break;
    default:                                     DynFlag = 0; break;
    }

    FMagGeometryDescNative Desc = {};
    Desc.vertices    = Vertices.GetData();
    Desc.vertexCount = static_cast<uint32>(Vertices.Num() / 3);
    Desc.indices     = Indices.GetData();
    Desc.indexCount  = static_cast<uint32>(Indices.Num());
    Desc.materialID  = static_cast<MagMaterialIDNative>(MaterialID);
    Desc.dynamicFlag = DynFlag;

    MagGeometryIDNative OutID = 0;
    const MagStatusNative Status = Bridge->GeometryRegister(
        reinterpret_cast<MagEngineNative>(Engine), &Desc, &OutID);

    if (Status == 0)
    {
        GeometryID  = OutID;
        bRegistered = true;
        UE_LOG(LogMagGeometry, Verbose,
               TEXT("[%s] Registered acoustic geometry (ID=%u, verts=%u, tris=%u, mat=%u)."),
               GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"),
               GeometryID, Desc.vertexCount, Desc.indexCount / 3, MaterialID);
    }
    else
    {
        UE_LOG(LogMagGeometry, Warning,
               TEXT("[%s] mag_geometry_register failed (status=%d)."),
               GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"), Status);
    }
}

void UMagGeometryComponent::UnregisterGeometry()
{
    if (!bRegistered) return;

    const FMagNativeBridge* Bridge = FMagnaundasoniRuntimeModule::GetBridge();
    MagEngine Engine = FMagnaundasoniRuntimeModule::GetNativeEngine();

    if (Bridge && Engine && Bridge->GeometryUnregister)
    {
        Bridge->GeometryUnregister(
            reinterpret_cast<MagEngineNative>(Engine),
            static_cast<MagGeometryIDNative>(GeometryID));
    }

    GeometryID  = 0;
    bRegistered = false;
}

// ---------------------------------------------------------------------------
// Material preset resolution
// ---------------------------------------------------------------------------

void UMagGeometryComponent::ResolveMaterial()
{
    MaterialID = 0;

    const FMagNativeBridge* Bridge = FMagnaundasoniRuntimeModule::GetBridge();
    MagEngine Engine = FMagnaundasoniRuntimeModule::GetNativeEngine();
    if (!Bridge || !Engine) return;

    // Convert the FString preset name to ANSI for the C API call.
    const FString PresetName = MaterialPreset.IsEmpty() ? TEXT("Concrete") : MaterialPreset;
    // TCHAR_TO_ANSI uses a stack-allocated buffer valid for the lifetime of the expression.
    const char* AnsiPresetName = TCHAR_TO_ANSI(*PresetName);

    if (Bridge->MaterialGetPreset && Bridge->MaterialRegister)
    {
        FMagMaterialDescNative MatDesc = {};
        const MagStatusNative Status =
            Bridge->MaterialGetPreset(AnsiPresetName, &MatDesc);

        if (Status == 0)
        {
            MagMaterialIDNative OutMatID = 0;
            Bridge->MaterialRegister(
                reinterpret_cast<MagEngineNative>(Engine), &MatDesc, &OutMatID);
            MaterialID = static_cast<uint32>(OutMatID);
        }
        else
        {
            UE_LOG(LogMagGeometry, Warning,
                   TEXT("[%s] Material preset '%s' not found; using default (ID=0)."),
                   GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"), *PresetName);
        }
    }
    else
    {
        UE_LOG(LogMagGeometry, Verbose,
               TEXT("[%s] Material preset functions not available; using material ID 0."),
               GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"));
    }
}

// ---------------------------------------------------------------------------
// Mesh data extraction
// ---------------------------------------------------------------------------

bool UMagGeometryComponent::ExtractMeshData(TArray<float>& OutVertices,
                                             TArray<uint32>& OutIndices)
{
    if (!GetOwner()) return false;

    UStaticMeshComponent* MeshComp =
        GetOwner()->FindComponentByClass<UStaticMeshComponent>();
    if (!MeshComp) return false;

    UStaticMesh* Mesh = MeshComp->GetStaticMesh();
    if (!Mesh || !Mesh->GetRenderData()) return false;

    const FStaticMeshLODResources& LOD = Mesh->GetRenderData()->LODResources[0];

    // ---- Vertex positions ----
    const FStaticMeshVertexBuffers& VBs = LOD.VertexBuffers;
    const int32 NumVerts = VBs.PositionVertexBuffer.GetNumVertices();
    if (NumVerts == 0) return false;

    // World transform of the mesh component (scale baked in at registration time;
    // subsequent updates use the transform-update path).
    const FTransform WorldXform = MeshComp->GetComponentTransform();

    OutVertices.Reserve(NumVerts * 3);
    for (int32 i = 0; i < NumVerts; ++i)
    {
        // GetVertexPosition returns local-space position.
        const FVector3f LocalPos = VBs.PositionVertexBuffer.VertexPosition(i);
        // Transform to world space (still in centimetres).
        const FVector WorldPos = WorldXform.TransformPosition(
            FVector(LocalPos.X, LocalPos.Y, LocalPos.Z));
        // Convert to metres for native engine.
        OutVertices.Add(static_cast<float>(WorldPos.X * kCmToM));
        OutVertices.Add(static_cast<float>(WorldPos.Y * kCmToM));
        OutVertices.Add(static_cast<float>(WorldPos.Z * kCmToM));
    }

    // ---- Indices ----
    FIndexArrayView IndexView = LOD.IndexBuffer.GetArrayView();
    const int32 NumIndices = IndexView.Num();
    if (NumIndices == 0) return false;

    OutIndices.Reserve(NumIndices);
    for (int32 i = 0; i < NumIndices; ++i)
    {
        OutIndices.Add(static_cast<uint32>(IndexView[i]));
    }

    return OutVertices.Num() > 0 && OutIndices.Num() >= 3;
}

// ---------------------------------------------------------------------------
// Transform update (dynamic geometry)
// ---------------------------------------------------------------------------

void UMagGeometryComponent::PushTransformUpdate()
{
    const FMagNativeBridge* Bridge = FMagnaundasoniRuntimeModule::GetBridge();
    MagEngine Engine = FMagnaundasoniRuntimeModule::GetNativeEngine();
    if (!Bridge || !Engine || !Bridge->GeometryUpdateTransform) return;

    UStaticMeshComponent* MeshComp =
        GetOwner() ? GetOwner()->FindComponentByClass<UStaticMeshComponent>() : nullptr;
    if (!MeshComp) return;

    float Matrix[16];
    ToNativeMatrix(MeshComp->GetComponentTransform(), Matrix);

    Bridge->GeometryUpdateTransform(
        reinterpret_cast<MagEngineNative>(Engine),
        static_cast<MagGeometryIDNative>(GeometryID),
        Matrix);
}

// ---------------------------------------------------------------------------
// Coordinate conversion helpers
// ---------------------------------------------------------------------------

void UMagGeometryComponent::ToNativeMatrix(const FTransform& Transform, float OutMatrix[16])
{
    // Build a standard 4x4 TRS matrix from the UE FTransform.
    // Translations are converted from centimetres to metres.
    // The matrix is laid out row-major: row0={Xx Xy Xz 0}, row1={Yx Yy Yz 0},
    // row2={Zx Zy Zz 0}, row3={Tx Ty Tz 1} (translation in the last row).
    const FMatrix M = Transform.ToMatrixWithScale();

    // Columns of M (UE FMatrix is column-major internally, but accessible as M[row][col]).
    // We emit row-major so the native engine can treat each row4 as a basis vector + translation.
    for (int32 Row = 0; Row < 4; ++Row)
    {
        for (int32 Col = 0; Col < 4; ++Col)
        {
            OutMatrix[Row * 4 + Col] = static_cast<float>(M.M[Row][Col]);
        }
    }

    // Scale translation components (row 3, cols 0-2) from cm to metres.
    OutMatrix[3 * 4 + 0] *= kCmToM;
    OutMatrix[3 * 4 + 1] *= kCmToM;
    OutMatrix[3 * 4 + 2] *= kCmToM;
}
