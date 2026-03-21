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
        PrecompileForTargets = PrecompileTargetsType.Any;

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
        // Prefer the plugin-local ThirdParty layout (used when the plugin is installed
        // in an external UE project); fall back to the repo-relative native/include
        // path so the module compiles during in-repo development.
        string PluginIncludeDir = Path.Combine(ModuleDirectory, "..", "..", "ThirdParty", "Magnaundasoni", "include");
        string NativeDir = Path.Combine(ModuleDirectory, "..", "..", "..", "..", "native");
        string RepoIncludeDir = Path.Combine(NativeDir, "include");

        if (Directory.Exists(PluginIncludeDir))
        {
            PublicIncludePaths.Add(PluginIncludeDir);
        }
        else if (Directory.Exists(RepoIncludeDir))
        {
            PublicIncludePaths.Add(RepoIncludeDir);
        }
    }
}
