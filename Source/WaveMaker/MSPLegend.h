// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MSPLegend.generated.h"

UCLASS()
class WAVEMAKER_API AMSPLegend : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AMSPLegend();


	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		UStaticMeshComponent* legendPlane;

	UPROPERTY(EditAnywhere)
		UMaterial* legendMaterial;

	UPROPERTY()
		UMaterialInstanceDynamic* materialInstance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		FRotator rotation;


	UPROPERTY(BlueprintReadOnly)
		float div;

	UPROPERTY(BlueprintReadOnly)
		float a;

	UPROPERTY(BlueprintReadOnly)
		float rangeMax;

	UPROPERTY(BlueprintReadOnly)
		float rangeMin;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		float yTarget;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable)
		void setMSPLegendScalar(FString name, float value);

	UFUNCTION(BlueprintCallable)
		void UpdateLegendRange();

	UFUNCTION(BlueprintImplementableEvent)
		void UpdateLabelTexts();

};
