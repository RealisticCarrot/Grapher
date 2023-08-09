// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MSPMarker2.generated.h"

UCLASS()
class WAVEMAKER_API AMSPMarker2 : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AMSPMarker2();

	UPROPERTY(BlueprintReadWrite)
		AMSPMarker2* Partner;

	UPROPERTY(BlueprintReadWrite)
		int Index;

	

	UFUNCTION(BlueprintImplementableEvent)
		void DestroySelfAndWidgets();



protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
