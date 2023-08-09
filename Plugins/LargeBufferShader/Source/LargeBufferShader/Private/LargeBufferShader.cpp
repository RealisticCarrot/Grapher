// Copyright Epic Games, Inc. All Rights Reserved.

#include "LargeBufferShader.h"

#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "RHI.h"
#include "GlobalShader.h"
#include "RHICommandList.h"
#include "RenderGraphBuilder.h"
#include "Runtime/Core/Public/Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"


#define LOCTEXT_NAMESPACE "FLargeBufferShaderModule"

void FLargeBufferShaderModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module


	FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("LargeBufferShader"))->GetBaseDir(), TEXT("Shaders/Private"));
	AddShaderSourceDirectoryMapping(TEXT("/LargeBufferShaderShaders"), PluginShaderDir);

}

void FLargeBufferShaderModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FLargeBufferShaderModule, LargeBufferShader)