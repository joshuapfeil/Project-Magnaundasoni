// Copyright Project Magnaundasoni. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class Magnaundasoni : ModuleRules
{
    public Magnaundasoni(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "AudioMixer",
            "AudioExtensions"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Slate",
            "SlateCore"
        });

        // Native library paths
        string NativeDir = Path.Combine(ModuleDirectory, "..", "..", "..", "..", "native");
        string IncludeDir = Path.Combine(NativeDir, "include");
        PublicIncludePaths.Add(IncludeDir);

        // Platform-specific library linking
        string LibDir = Path.Combine(NativeDir, "build");

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PublicAdditionalLibraries.Add(Path.Combine(LibDir, "magnaundasoni.lib"));
            RuntimeDependencies.Add(Path.Combine(LibDir, "magnaundasoni.dll"));
            PublicDelayLoadDLLs.Add("magnaundasoni.dll");
        }
        else if (Target.Platform == UnrealTargetPlatform.Linux)
        {
            PublicAdditionalLibraries.Add(Path.Combine(LibDir, "libmagnaundasoni.so"));
            RuntimeDependencies.Add(Path.Combine(LibDir, "libmagnaundasoni.so"));
        }
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            PublicAdditionalLibraries.Add(Path.Combine(LibDir, "libmagnaundasoni.dylib"));
            RuntimeDependencies.Add(Path.Combine(LibDir, "libmagnaundasoni.dylib"));
        }

        PublicDefinitions.Add("WITH_MAGNAUNDASONI=1");
    }
}
