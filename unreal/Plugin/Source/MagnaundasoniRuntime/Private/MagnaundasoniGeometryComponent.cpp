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

        // Push the initial world transform so the native engine positions the
        // geometry correctly from the very first simulation tick.  Vertices were
        // registered in local space, so this is always required.
        PushTransformUpdate();
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
    // FTCHARToUTF8 holds the converted buffer for the duration of this scope,
    // so the pointer remains valid through both MaterialGetPreset and MaterialRegister.
    const FString PresetName = MaterialPreset.IsEmpty() ? TEXT("Concrete") : MaterialPreset;
    FTCHARToUTF8 AnsiConverter(*PresetName);
    const char* AnsiPresetName = AnsiConverter.Get();

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

    OutVertices.Reserve(NumVerts * 3);
    for (int32 i = 0; i < NumVerts; ++i)
    {
        // Export vertices in local space (cm → m).
        // The native engine applies the component world transform separately
        // via mag_geometry_update_transform; baking world space here would
        // double-apply the transform for dynamic geometry.
        const FVector3f LocalPosCm = VBs.PositionVertexBuffer.VertexPosition(i);
        const FVector3f LocalPosM  = LocalPosCm * kCmToM;
        OutVertices.Add(LocalPosM.X);
        OutVertices.Add(LocalPosM.Y);
        OutVertices.Add(LocalPosM.Z);
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
    // The native engine reads the matrix with Mat4x4::fromColumnMajor, which
    // expects the flat 16-element array in column-major order:
    //   OutMatrix[Col * 4 + Row] = m[Row][Col]
    //
    // The native transformPoint convention (column-vector multiplication) requires
    // translation to be in column 3 (m[0..2][3]).
    //
    // UE's FMatrix uses row-vector convention with translation in row 3
    // (M.M[3][0..2]).  To produce the equivalent column-vector matrix we need
    // the transpose of the UE matrix, which moves translation into column 3.
    //
    // Combined: OutMatrix[Col * 4 + Row] = M.M[Col][Row]   (transpose via index swap)
    //
    // After transposing, translation lives at indices 12, 13, 14 (Col=3, Row=0-2),
    // and we scale those from centimetres to metres.

    const FMatrix M = Transform.ToMatrixWithScale();

    for (int32 Row = 0; Row < 4; ++Row)
    {
        for (int32 Col = 0; Col < 4; ++Col)
        {
            OutMatrix[Col * 4 + Row] = static_cast<float>(M.M[Col][Row]);
        }
    }

    // Scale translation components (col 3, rows 0-2) from cm to metres.
    // Flat indices: Col=3 → base = 3*4 = 12; rows 0, 1, 2 → indices 12, 13, 14.
    OutMatrix[12] *= kCmToM;  // Tx (col=3, row=0)
    OutMatrix[13] *= kCmToM;  // Ty (col=3, row=1)
    OutMatrix[14] *= kCmToM;  // Tz (col=3, row=2)
}
