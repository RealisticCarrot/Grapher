// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"


class FnetcdfModule : public FDefaultGameModuleImpl {
public: 
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	bool IsAvailable() const { return netcdfHandle != nullptr; }

private:
	void* netcdfHandle = nullptr;
};
