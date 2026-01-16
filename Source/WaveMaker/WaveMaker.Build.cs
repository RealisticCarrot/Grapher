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
            "HeadMountedDisplay",
            "SlateCore",
            "Slate",
            "UMG"
        });

        PrivateDependencyModuleNames.AddRange(new string[] { });

        //
        // netCDF (Windows) setup - PROJECT LOCAL (packaging-safe)
        //
        // Put these folders in your project root (same level as WaveMaker.uproject):
        //   ThirdParty/netCDF/include
        //   ThirdParty/netCDF/lib
        //   ThirdParty/netCDF/bin
        //
        // Copy from:
        //   C:\Program Files\netCDF 4.9.2\include -> ThirdParty\netCDF\include
        //   C:\Program Files\netCDF 4.9.2\lib     -> ThirdParty\netCDF\lib
        //   C:\Program Files\netCDF 4.9.2\bin     -> ThirdParty\netCDF\bin
        //

        string ProjectDir = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", ".."));
        string NetCDFRoot = Path.Combine(ProjectDir, "ThirdParty", "netCDF");
        string NetCDFInclude = Path.Combine(NetCDFRoot, "include");
        string NetCDFLib = Path.Combine(NetCDFRoot, "lib");
        string NetCDFBin = Path.Combine(NetCDFRoot, "bin");

        PublicDefinitions.Add("WITH_NetCDFLib=1");
        PublicIncludePaths.Add(NetCDFInclude);

        // Link libraries
        PublicAdditionalLibraries.AddRange(new string[]
        {
            Path.Combine(NetCDFLib, "netcdf.lib"),
            Path.Combine(NetCDFLib, "hdf.lib"),
            Path.Combine(NetCDFLib, "hdf5.lib"),
            Path.Combine(NetCDFLib, "hdf5_hl.lib"),
            Path.Combine(NetCDFLib, "hdf5_tools.lib"),
            Path.Combine(NetCDFLib, "jpeg.lib"),
            Path.Combine(NetCDFLib, "libcurl_imp.lib"),
            Path.Combine(NetCDFLib, "libhdf.lib"),
            Path.Combine(NetCDFLib, "libhdf5.lib"),
            Path.Combine(NetCDFLib, "libhdf5_hl.lib"),
            Path.Combine(NetCDFLib, "libhdf5_tools.lib"),
            Path.Combine(NetCDFLib, "libmfhdf.lib"),
            Path.Combine(NetCDFLib, "libxdr.lib"),
            Path.Combine(NetCDFLib, "mfhdf.lib"),
            Path.Combine(NetCDFLib, "xdr.lib"),
            Path.Combine(NetCDFLib, "zlib.lib"),
            Path.Combine(NetCDFLib, "zlibstatic.lib"),
        });

        // Delay-load DLLs
        PublicDelayLoadDLLs.AddRange(new string[]
        {
            "netcdf.dll",
            "hdf.dll",
            "hdf5.dll",
            "hdf5_hl.dll",
            "hdf5_tools.dll",
            "jpeg.dll",
            "libcurl.dll",
            "mfhdf.dll",
            "xdr.dll",
            "zlib1.dll",
        });

        // Stage DLLs from project-local ThirdParty folder (packaging-safe)
        AddRuntimeDependencyIfExists(NetCDFBin, "netcdf.dll");
        AddRuntimeDependencyIfExists(NetCDFBin, "hdf.dll");
        AddRuntimeDependencyIfExists(NetCDFBin, "hdf5.dll");
        AddRuntimeDependencyIfExists(NetCDFBin, "hdf5_hl.dll");
        AddRuntimeDependencyIfExists(NetCDFBin, "hdf5_tools.dll");
        AddRuntimeDependencyIfExists(NetCDFBin, "jpeg.dll");
        AddRuntimeDependencyIfExists(NetCDFBin, "libcurl.dll");
        AddRuntimeDependencyIfExists(NetCDFBin, "mfhdf.dll");
        AddRuntimeDependencyIfExists(NetCDFBin, "xdr.dll");
        AddRuntimeDependencyIfExists(NetCDFBin, "zlib1.dll");
    }

    private void AddRuntimeDependencyIfExists(string directory, string fileName)
    {
        string fullPath = Path.Combine(directory, fileName);
        if (File.Exists(fullPath))
        {
            // For UE 5.1, this is enough to stage the file for packaging as well.
            RuntimeDependencies.Add(fullPath);
        }
    }
}
