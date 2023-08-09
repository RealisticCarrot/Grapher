#pragma once

#include "CoreMinimal.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "Kismet/BlueprintAsyncActionBase.h"

#include "MSPComputeShader.generated.h"

struct LARGEBUFFERSHADER_API FMSPComputeShaderDispatchParams
{
	int X;
	int Y;
	int Z;

	int Input[2];
	int Output;

	FMSPComputeShaderDispatchParams(int x, int y, int z)
		: X(x), Y(y), Z(z) {}

};


class LARGEBUFFERSHADER_API FMSPComputeShaderInterface {
public:
	static void DispatchRenderThread(
		FRHICommandListImmediate& RHICmdList,
		FMSPComputeShaderDispatchParams Params,
		TFunction<void(int OutputVal)> AsyncCallback
	);

	static void DispatchGameThread(
		FMSPComputeShaderDispatchParams Params,
		TFunction<void(int OutputVal)> AsyncCallback
	) {
		ENQUEUE_RENDER_COMMAND(SceneDrawCompletion)(
			[Params, AsyncCallback](FRHICommandListImmediate& RHICmdList) {
				DispatchRenderThread(RHICmdList, Params, AsyncCallback);
			});
	}


	static void Dispatch(
		FMSPComputeShaderDispatchParams Params,
		TFunction<void(int OutputVal)> AsyncCallback
	) {
		if (IsInRenderingThread()) {
			DispatchRenderThread(GetImmediateCommandList_ForRenderCommand(), Params, AsyncCallback);
		}
		else {
			DispatchGameThread(Params, AsyncCallback);
		}
	}

};



DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMSPComputeShaderLibrary_AsyncExecutionCompleted, const int, Value);


UCLASS()
class LARGEBUFFERSHADER_API UMSPComputeShaderLibrary_AsyncExecution : public UBlueprintAsyncActionBase {
	GENERATED_BODY()

public:
	virtual void Activate() override {

		FMSPComputeShaderDispatchParams Params(1, 1, 1);
		Params.Input[0] = Arg1;
		Params.Input[1] = Arg2;

		FMSPComputeShaderInterface::Dispatch(Params, [this](int OutputVal) {
			this->Completed.Broadcast(OutputVal);
		});
	}

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", Category = "ComputeShader", WorldContext = "WorldContextObject"))
	static UMSPComputeShaderLibrary_AsyncExecution* ExecuteBaseComputeShader(UObject* WorldContextObject, int Arg1, int Arg2) {
		
		UMSPComputeShaderLibrary_AsyncExecution* Action = NewObject<UMSPComputeShaderLibrary_AsyncExecution>();
		Action->Arg1 = Arg1;
		Action->Arg2 = Arg2;
		Action->RegisterWithGameInstance(WorldContextObject);

		return Action;
	}
	

	UPROPERTY(BlueprintAssignable)
		FOnMSPComputeShaderLibrary_AsyncExecutionCompleted Completed;

	int Arg1;
	int Arg2;

};