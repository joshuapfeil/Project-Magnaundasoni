// Copyright Project Magnaundasoni. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

/// <summary>
/// Build rules for the MagnaundasoniRuntime module.
///
/// This module provides the Actor Component classes (UMagSourceComponent,
/// UMagListenerComponent, UMagGeometryComponent) that game code and Blueprints
/// use to integrate the Magnaundasoni acoustic simulation into their actors.
///
/// It depends on the base Magnaundasoni module, which owns the native library
/// lifecycle (DLL load, engine create/destroy).
/// </summary>
public class MagnaundasoniRuntime : ModuleRules
{
    public MagnaundasoniRuntime(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "AudioMixer",
            "AudioExtensions",
            // Base module owns native library lifecycle; Runtime piggy-backs on it.
            "Magnaundasoni"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Slate",
            "SlateCore",
            "RenderCore",   // for FStaticMeshLODResources
            "RHI"
        });

        // Native C header for type definitions (no static link – loaded dynamically).
        string NativeDir = Path.Combine(ModuleDirectory, "..", "..", "..", "..", "native");
        string IncludeDir = Path.Combine(NativeDir, "include");
        PublicIncludePaths.Add(IncludeDir);
    }
}
