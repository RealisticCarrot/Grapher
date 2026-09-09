// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaveMaker.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"

void FnetcdfModule::StartupModule()
{
    FDefaultGameModuleImpl::StartupModule();

#if PLATFORM_WINDOWS
    const FString CandidateDirectories[] =
    {
        FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("ThirdParty/netCDF/bin"))),
        FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Binaries/Win64"))),
        FPaths::ConvertRelativePathToFull(FPlatformProcess::BaseDir())
    };

    for (const FString& Directory : CandidateDirectories)
    {
        const FString LibraryPath = FPaths::Combine(Directory, TEXT("netcdf.dll"));
        if (!IFileManager::Get().FileExists(*LibraryPath))
        {
            continue;
        }

        // UE resolves imported DLLs using this directory for the duration of the load.
        FPlatformProcess::PushDllDirectory(*Directory);
        netcdfHandle = FPlatformProcess::GetDllHandle(*LibraryPath);
        FPlatformProcess::PopDllDirectory(*Directory);

        if (netcdfHandle)
        {
            UE_LOG(LogTemp, Log, TEXT("Loaded netCDF from %s"), *LibraryPath);
            break;
        }
    }

    if (!netcdfHandle)
    {
        UE_LOG(LogTemp, Error, TEXT("Unable to load netCDF. Check ThirdParty/netCDF/bin or the staged application binaries."));
    }
#endif
}

void FnetcdfModule::ShutdownModule()
{
#if PLATFORM_WINDOWS
    if (netcdfHandle)
    {
        FPlatformProcess::FreeDllHandle(netcdfHandle);
        netcdfHandle = nullptr;
    }
#endif

    FDefaultGameModuleImpl::ShutdownModule();
}

IMPLEMENT_PRIMARY_GAME_MODULE(FnetcdfModule, WaveMaker, "WaveMaker");
