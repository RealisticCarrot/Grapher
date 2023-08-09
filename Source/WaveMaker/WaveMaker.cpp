// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaveMaker.h"
//#include "Interfaces/IPluginManager.h"



#define LOCALTEXT_NAMESPACE "FnetcdfModule"

void FnetcdfModule::StartupModule() {
#ifdef _WIN64
	concrt140Handle = FPlatformProcess::GetDllHandle(*FPaths::Combine("C:\\Users\\Braden\\Documents\\Unreal Projects\\WaveMaker\\Binaries\\Win64", "concrt140.dll"));
	hdfHandle = FPlatformProcess::GetDllHandle(*FPaths::Combine("C:\\Users\\Braden\\Documents\\Unreal Projects\\WaveMaker\\Binaries\\Win64", "hdf.dll"));
	hdf5Handle = FPlatformProcess::GetDllHandle(*FPaths::Combine("C:\\Users\\Braden\\Documents\\Unreal Projects\\WaveMaker\\Binaries\\Win64", "hdf5.dll"));
	hdf5_hlHandle = FPlatformProcess::GetDllHandle(*FPaths::Combine("C:\\Users\\Braden\\Documents\\Unreal Projects\\WaveMaker\\Binaries\\Win64", "hdf5_hl.dll"));
	hdf5_toolsHandle = FPlatformProcess::GetDllHandle(*FPaths::Combine("C:\\Users\\Braden\\Documents\\Unreal Projects\\WaveMaker\\Binaries\\Win64", "hdf5_tools.dll"));
	jpegHandle = FPlatformProcess::GetDllHandle(*FPaths::Combine("C:\\Users\\Braden\\Documents\\Unreal Projects\\WaveMaker\\Binaries\\Win64", "jpeg.dll"));
	libcurlHandle = FPlatformProcess::GetDllHandle(*FPaths::Combine("C:\\Users\\Braden\\Documents\\Unreal Projects\\WaveMaker\\Binaries\\Win64", "libcurl.dll"));
	mfhdfHandle = FPlatformProcess::GetDllHandle(*FPaths::Combine("C:\\Users\\Braden\\Documents\\Unreal Projects\\WaveMaker\\Binaries\\Win64", "mfhdf.dll"));
	//msvcp140Handle = FPlatformProcess::GetDllHandle(*FPaths::Combine("C:\\Users\\Braden\\Documents\\Unreal Projects\\WaveMaker\\Binaries\\Win64", "msvcp140.dll"));
	netcdfHandle = FPlatformProcess::GetDllHandle(*FPaths::Combine("C:\\Users\\Braden\\Documents\\Unreal Projects\\WaveMaker\\Binaries\\Win64", "netcdf.dll"));
	vcruntime140Handle = FPlatformProcess::GetDllHandle(*FPaths::Combine("C:\\Users\\Braden\\Documents\\Unreal Projects\\WaveMaker\\Binaries\\Win64", "vcruntime140.dll"));
	xdrHandle = FPlatformProcess::GetDllHandle(*FPaths::Combine("C:\\Users\\Braden\\Documents\\Unreal Projects\\WaveMaker\\Binaries\\Win64", "xdr.dll"));
	zlib1Handle = FPlatformProcess::GetDllHandle(*FPaths::Combine("C:\\Users\\Braden\\Documents\\Unreal Projects\\WaveMaker\\Binaries\\Win64", "zlib1.dll"));


#endif
}

void FnetcdfModule::ShutdownModule() {
#ifdef _WIN64
	FPlatformProcess::FreeDllHandle(concrt140Handle);
	concrt140Handle = nullptr;

	FPlatformProcess::FreeDllHandle(hdfHandle);
	hdfHandle = nullptr;

	FPlatformProcess::FreeDllHandle(hdf5Handle);
	hdf5Handle = nullptr;

	FPlatformProcess::FreeDllHandle(hdf5_hlHandle);
	hdf5_hlHandle = nullptr;

	FPlatformProcess::FreeDllHandle(hdf5_toolsHandle);
	hdf5_toolsHandle = nullptr;

	FPlatformProcess::FreeDllHandle(jpegHandle);
	jpegHandle = nullptr;

	FPlatformProcess::FreeDllHandle(libcurlHandle);
	libcurlHandle = nullptr;

	FPlatformProcess::FreeDllHandle(mfhdfHandle);
	mfhdfHandle = nullptr;

	//FPlatformProcess::FreeDllHandle(msvcp140Handle);
	//msvcp140Handle = nullptr;

	FPlatformProcess::FreeDllHandle(netcdfHandle);
	netcdfHandle = nullptr;

	FPlatformProcess::FreeDllHandle(vcruntime140Handle);
	vcruntime140Handle = nullptr;

	FPlatformProcess::FreeDllHandle(xdrHandle);
	xdrHandle = nullptr;

	FPlatformProcess::FreeDllHandle(zlib1Handle);
	zlib1Handle = nullptr;

#endif
}

#undef LOCALTEXT_NAMESPACE


IMPLEMENT_PRIMARY_GAME_MODULE( FDefaultGameModuleImpl, WaveMaker, "WaveMaker" );
