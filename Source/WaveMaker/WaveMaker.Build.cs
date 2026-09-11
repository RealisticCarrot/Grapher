// Copyright Epic Games, Inc. All Rights Reserved.
using System.IO;
using UnrealBuildTool;

public class WaveMaker : ModuleRules
{
    public WaveMaker(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "EnhancedInput",
            "SlateCore",
            "Slate",
            "UMG",
            "HTTP",
            "ImageCore",
            "ImageWrapper",
            "Json",
            "RenderCore",
            "RHI"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "ApplicationCore"
        });

        if (Target.Platform != UnrealTargetPlatform.Win64)
        {
            throw new BuildException("WaveMaker's bundled netCDF library currently supports Win64 only.");
        }

        PublicSystemLibraries.Add("comdlg32.lib");

        string ProjectDir = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", ".."));
        string NetCDFRoot = Path.Combine(ProjectDir, "ThirdParty", "netCDF");
        string NetCDFInclude = Path.Combine(NetCDFRoot, "include");
        string NetCDFLibrary = Path.Combine(NetCDFRoot, "lib", "netcdf.lib");
        string NetCDFBin = Path.Combine(NetCDFRoot, "bin");

        RequireFile(Path.Combine(NetCDFInclude, "netcdf.h"));
        RequireFile(NetCDFLibrary);

        PublicDefinitions.Add("WITH_NetCDFLib=1");
        PublicDefinitions.Add("DLL_NETCDF=1");
        PublicSystemIncludePaths.Add(NetCDFInclude);
        // netcdf.lib imports the C API; its transitive dependencies are DLLs.
        PublicAdditionalLibraries.Add(NetCDFLibrary);
        PublicDelayLoadDLLs.Add("netcdf.dll");

        // Stage the bundled netCDF 4.9.2 dependency chain beside the module/executable.
        // The current Visual C++ runtime is provided by Unreal's prerequisites.
        foreach (string Dll in new string[]
        {
            "netcdf.dll",
            "hdf5_hl.dll",
            "hdf5.dll",
            "libcurl.dll",
            "zlib1.dll"
        })
        {
            string SourcePath = Path.Combine(NetCDFBin, Dll);
            RequireFile(SourcePath);
            RuntimeDependencies.Add("$(BinaryOutputDir)/" + Dll, SourcePath, StagedFileType.NonUFS);
        }
    }

    private static void RequireFile(string FilePath)
    {
        if (!File.Exists(FilePath))
        {
            throw new BuildException("Required project-local netCDF dependency is missing: {0}", FilePath);
        }
    }
}
