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
        // When the plugin is installed as a standalone package (dropped into a
        // project's Plugins/ directory) the native headers are bundled under
        // Source/ThirdParty/Magnaundasoni/include/.  During repository development
        // the headers live in the repo's native/include directory.  Check for the
        // bundled path first so packaged releases work out of the box.
        string BundledIncludeDir = Path.Combine(ModuleDirectory, "ThirdParty", "Magnaundasoni", "include");
        string RepoNativeDir     = Path.Combine(ModuleDirectory, "..", "..", "..", "..", "native");
        string IncludeDir = Directory.Exists(BundledIncludeDir)
            ? BundledIncludeDir
            : Path.Combine(RepoNativeDir, "include");
        PublicIncludePaths.Add(IncludeDir);

        // Platform-specific library linking.
        // In development builds the libraries live in native/build/.
        // Packaged distributions should place pre-built libraries in
        // Source/ThirdParty/Magnaundasoni/<Platform>/.
        string BundledLibDir = Path.Combine(ModuleDirectory, "ThirdParty", "Magnaundasoni");
        string DevLibDir     = Path.Combine(RepoNativeDir, "build");

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            string LibDir = Directory.Exists(Path.Combine(BundledLibDir, "Win64"))
                ? Path.Combine(BundledLibDir, "Win64")
                : DevLibDir;
            PublicAdditionalLibraries.Add(Path.Combine(LibDir, "magnaundasoni.lib"));
            RuntimeDependencies.Add(Path.Combine(LibDir, "magnaundasoni.dll"));
            PublicDelayLoadDLLs.Add("magnaundasoni.dll");
        }
        else if (Target.Platform == UnrealTargetPlatform.Linux)
        {
            string LibDir = Directory.Exists(Path.Combine(BundledLibDir, "Linux"))
                ? Path.Combine(BundledLibDir, "Linux")
                : DevLibDir;
            PublicAdditionalLibraries.Add(Path.Combine(LibDir, "libmagnaundasoni.so"));
            RuntimeDependencies.Add(Path.Combine(LibDir, "libmagnaundasoni.so"));
        }
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            string LibDir = Directory.Exists(Path.Combine(BundledLibDir, "Mac"))
                ? Path.Combine(BundledLibDir, "Mac")
                : DevLibDir;
            PublicAdditionalLibraries.Add(Path.Combine(LibDir, "libmagnaundasoni.dylib"));
            RuntimeDependencies.Add(Path.Combine(LibDir, "libmagnaundasoni.dylib"));
        }

        PublicDefinitions.Add("WITH_MAGNAUNDASONI=1");
    }
}
