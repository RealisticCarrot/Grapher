// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Components/StaticMeshComponent.h"

#include "structs.h"

#include "Materials/MaterialInstance.h"
#include "Materials/MaterialLayersFunctions.h"
#include "Materials/MaterialInstanceDynamic.h"

#include "MSPTimeline.generated.h"



UCLASS()
class WAVEMAKER_API AMSPTimeline : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AMSPTimeline();

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		UStaticMeshComponent* timelinePlane;

	UPROPERTY()
		float totalTime = 0.0f;

	UPROPERTY(EditAnywhere)
		UMaterial* timelineMaterial;

	UPROPERTY()
		UMaterialInstanceDynamic* materialInstance;

	UPROPERTY()
		UTexture2D* timelineDataTexture;

	UPROPERTY()
		bool textureUpdateQueued = false;

	UPROPERTY()
		FmspDataProperties mspDataProps;

	UPROPERTY()
		bool paramUpdateQueued = false;

	UPROPERTY()
		bool cursorOver = false;


protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;


	UFUNCTION(BlueprintCallable)
		void setMSPTimelineScalar(FString name, float value);

	UFUNCTION()
		void setDataTextureWhenReady(TArray<float> data);

	UFUNCTION()
		void setTextureParamsWhenReady(FmspDataProperties props);

	UFUNCTION()
		UTexture2D* CreateTextureFrom32BitFloat(TArray<float> data, int width, int height);

	UFUNCTION()
		UTexture2D* UpdateTextureFrom32BitFloat(TArray<float> data, int width, int height, UTexture2D* texture);

	UFUNCTION()
		virtual void cursorEnter(AActor* TouchedActor);

	UFUNCTION()
		virtual void cursorExit(AActor* TouchedActor);

};
