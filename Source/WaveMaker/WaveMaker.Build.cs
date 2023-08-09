// Copyright Epic Games, Inc. All Rights Reserved.
using System;
using System.IO;
using UnrealBuildTool;

public class WaveMaker : ModuleRules
{
	public WaveMaker(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "HeadMountedDisplay", "SlateCore", "Slate" });

		PrivateDependencyModuleNames.AddRange(new string[] {  });

        // Uncomment if you are using Slate UI
        // PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

        // Uncomment if you are using online features
        // PrivateDependencyModuleNames.Add("OnlineSubsystem");

        // To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true



        // windows schtuff

        // Add any macros that need to be set
        PublicDefinitions.Add("WITH_NetCDFLib=1");

        // Add any include paths for the plugin
        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "C:\\Program Files\\netCDF 4.9.2\\include"));

        // Add any import libraries or static libraries
        PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "C:\\Program Files\\netCDF 4.9.2\\lib", "hdf.lib"));
        PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "C:\\Program Files\\netCDF 4.9.2\\lib", "hdf5.lib"));
        PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "C:\\Program Files\\netCDF 4.9.2\\lib", "hdf5_hl.lib"));
        PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "C:\\Program Files\\netCDF 4.9.2\\lib", "hdf5_tools.lib"));
        PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "C:\\Program Files\\netCDF 4.9.2\\lib", "jpeg.lib"));
        PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "C:\\Program Files\\netCDF 4.9.2\\lib", "libcurl_imp.lib"));
        PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "C:\\Program Files\\netCDF 4.9.2\\lib", "libhdf.lib"));
        PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "C:\\Program Files\\netCDF 4.9.2\\lib", "libhdf5.lib"));
        PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "C:\\Program Files\\netCDF 4.9.2\\lib", "libhdf5_hl.lib"));
        PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "C:\\Program Files\\netCDF 4.9.2\\lib", "libhdf5_tools.lib"));
        PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "C:\\Program Files\\netCDF 4.9.2\\lib", "libmfhdf.lib"));
        PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "C:\\Program Files\\netCDF 4.9.2\\lib", "libxdr.lib"));
        PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "C:\\Program Files\\netCDF 4.9.2\\lib", "mfhdf.lib"));
        PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "C:\\Program Files\\netCDF 4.9.2\\lib", "netcdf.lib"));
        PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "C:\\Program Files\\netCDF 4.9.2\\lib", "xdr.lib"));
        PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "C:\\Program Files\\netCDF 4.9.2\\lib", "zlib.lib"));
        PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "C:\\Program Files\\netCDF 4.9.2\\lib", "zlibstatic.lib"));


        RuntimeDependencies.Add(Path.Combine(ModuleDirectory, "C:\\Users\\Braden\\Documents\\Unreal Projects\\WaveMaker\\Binaries\\Win64", "concrt140.dll"));
        PublicDelayLoadDLLs.Add("concrt140.dll");

        RuntimeDependencies.Add(Path.Combine(ModuleDirectory, "C:\\Users\\Braden\\Documents\\Unreal Projects\\WaveMaker\\Binaries\\Win64", "hdf.dll"));
        PublicDelayLoadDLLs.Add("hdf.dll");

        RuntimeDependencies.Add(Path.Combine(ModuleDirectory, "C:\\Users\\Braden\\Documents\\Unreal Projects\\WaveMaker\\Binaries\\Win64", "hdf5.dll"));
        PublicDelayLoadDLLs.Add("hdf5.dll");

        RuntimeDependencies.Add(Path.Combine(ModuleDirectory, "C:\\Users\\Braden\\Documents\\Unreal Projects\\WaveMaker\\Binaries\\Win64", "hdf5_hl.dll"));
        PublicDelayLoadDLLs.Add("hdf5_hl.dll");

        RuntimeDependencies.Add(Path.Combine(ModuleDirectory, "C:\\Users\\Braden\\Documents\\Unreal Projects\\WaveMaker\\Binaries\\Win64", "hdf5_tools.dll"));
        PublicDelayLoadDLLs.Add("hdf5_tools.dll");

        RuntimeDependencies.Add(Path.Combine(ModuleDirectory, "C:\\Users\\Braden\\Documents\\Unreal Projects\\WaveMaker\\Binaries\\Win64", "jpeg.dll"));
        PublicDelayLoadDLLs.Add("jpeg.dll");

        RuntimeDependencies.Add(Path.Combine(ModuleDirectory, "C:\\Users\\Braden\\Documents\\Unreal Projects\\WaveMaker\\Binaries\\Win64", "libcurl.dll"));
        PublicDelayLoadDLLs.Add("libcurl.dll");

        RuntimeDependencies.Add(Path.Combine(ModuleDirectory, "C:\\Users\\Braden\\Documents\\Unreal Projects\\WaveMaker\\Binaries\\Win64", "mfhdf.dll"));
        PublicDelayLoadDLLs.Add("mfhdf.dll");

        //RuntimeDependencies.Add(Path.Combine(ModuleDirectory, "C:\\Users\\Braden\\Documents\\Unreal Projects\\WaveMaker\\Binaries\\Win64", "msvcp140.dll"));
        //PublicDelayLoadDLLs.Add("msvcp140.dll");

        RuntimeDependencies.Add(Path.Combine(ModuleDirectory, "C:\\Users\\Braden\\Documents\\Unreal Projects\\WaveMaker\\Binaries\\Win64", "netcdf.dll"));
        PublicDelayLoadDLLs.Add("netcdf.dll");

        RuntimeDependencies.Add(Path.Combine(ModuleDirectory, "C:\\Users\\Braden\\Documents\\Unreal Projects\\WaveMaker\\Binaries\\Win64", "vcruntime140.dll"));
        PublicDelayLoadDLLs.Add("vcruntime140.dll");

        RuntimeDependencies.Add(Path.Combine(ModuleDirectory, "C:\\Users\\Braden\\Documents\\Unreal Projects\\WaveMaker\\Binaries\\Win64", "xdr.dll"));
        PublicDelayLoadDLLs.Add("xdr.dll");

        RuntimeDependencies.Add(Path.Combine(ModuleDirectory, "C:\\Users\\Braden\\Documents\\Unreal Projects\\WaveMaker\\Binaries\\Win64", "zlib1.dll"));
        PublicDelayLoadDLLs.Add("zlib1.dll");
    }
}
