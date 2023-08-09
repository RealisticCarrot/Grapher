// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"


class FnetcdfModule : public IModuleInterface {
public: 
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

#ifdef _WIN64
	void* concrt140Handle;
	void* hdfHandle;
	void* hdf5Handle;
	void* hdf5_hlHandle;
	void* hdf5_toolsHandle;
	void* jpegHandle;
	void* libcurlHandle;
	void* mfhdfHandle;
	void* msvcp140Handle;
	void* netcdfHandle;
	void* vcruntime140Handle;
	void* xdrHandle;
	void* zlib1Handle;
	
#endif
};