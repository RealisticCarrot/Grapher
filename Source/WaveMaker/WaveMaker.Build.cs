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
            "UMG",
            "HTTP",  // Added for HTTP/curl support
            "ImageCore",  // For FImageUtils
            "ImageWrapper"  // For proper PNG export with alpha
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

        // Only add netCDF library dependencies if the lib folder exists and has files
        if (Directory.Exists(NetCDFLib))
        {
            // Link libraries (only add if they exist)
            AddLibraryIfExists(NetCDFLib, "netcdf.lib");
            AddLibraryIfExists(NetCDFLib, "hdf.lib");
            AddLibraryIfExists(NetCDFLib, "hdf5.lib");
            AddLibraryIfExists(NetCDFLib, "hdf5_hl.lib");
            AddLibraryIfExists(NetCDFLib, "hdf5_tools.lib");
            AddLibraryIfExists(NetCDFLib, "jpeg.lib");
            AddLibraryIfExists(NetCDFLib, "libcurl_imp.lib");
            AddLibraryIfExists(NetCDFLib, "libhdf.lib");
            AddLibraryIfExists(NetCDFLib, "libhdf5.lib");
            AddLibraryIfExists(NetCDFLib, "libhdf5_hl.lib");
            AddLibraryIfExists(NetCDFLib, "libhdf5_tools.lib");
            AddLibraryIfExists(NetCDFLib, "libmfhdf.lib");
            AddLibraryIfExists(NetCDFLib, "libxdr.lib");
            AddLibraryIfExists(NetCDFLib, "mfhdf.lib");
            AddLibraryIfExists(NetCDFLib, "xdr.lib");
            AddLibraryIfExists(NetCDFLib, "zlib.lib");
            AddLibraryIfExists(NetCDFLib, "zlibstatic.lib");
        }

        // Delay-load DLLs (netCDF related)
        PublicDelayLoadDLLs.AddRange(new string[]
        {
            "netcdf.dll",
            "hdf.dll",
            "hdf5.dll",
            "hdf5_hl.dll",
            "hdf5_tools.dll",
            "jpeg.dll",
            "mfhdf.dll",
            "xdr.dll",
            "zlib1.dll",
        });

        // Stage DLLs from project-local ThirdParty folder to the executable directory (packaging-safe)
        if (Directory.Exists(NetCDFBin))
        {
            // Copy DLLs to the same folder as the executable using $(BinaryOutputDir)
            AddRuntimeDependencyToBinaries(NetCDFBin, "netcdf.dll");
            AddRuntimeDependencyToBinaries(NetCDFBin, "hdf.dll");
            AddRuntimeDependencyToBinaries(NetCDFBin, "hdf5.dll");
            AddRuntimeDependencyToBinaries(NetCDFBin, "hdf5_hl.dll");
            AddRuntimeDependencyToBinaries(NetCDFBin, "hdf5_tools.dll");
            AddRuntimeDependencyToBinaries(NetCDFBin, "jpeg.dll");
            AddRuntimeDependencyToBinaries(NetCDFBin, "libcurl.dll");
            AddRuntimeDependencyToBinaries(NetCDFBin, "mfhdf.dll");
            AddRuntimeDependencyToBinaries(NetCDFBin, "xdr.dll");
            AddRuntimeDependencyToBinaries(NetCDFBin, "zlib1.dll");
        }
    }

    private void AddLibraryIfExists(string directory, string fileName)
    {
        string fullPath = Path.Combine(directory, fileName);
        if (File.Exists(fullPath))
        {
            PublicAdditionalLibraries.Add(fullPath);
        }
    }

    // Copies a DLL to the same folder as the executable (BinaryOutputDir)
    private void AddRuntimeDependencyToBinaries(string sourceDirectory, string fileName)
    {
        string sourcePath = Path.Combine(sourceDirectory, fileName);
        if (File.Exists(sourcePath))
        {
            // $(BinaryOutputDir) is the folder where the .exe is placed
            RuntimeDependencies.Add("$(BinaryOutputDir)/" + fileName, sourcePath);
        }
    }
}
