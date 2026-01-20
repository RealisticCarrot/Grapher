// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IMFWindow.generated.h"

struct FRow;


USTRUCT(BlueprintType)
struct FLineChain {
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite)
		TArray<FVector2D> points;

	UPROPERTY(BlueprintReadWrite)
		FColor color = FColor::White;


};

UCLASS()
class WAVEMAKER_API AIMFWindow : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AIMFWindow();

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		TArray<FRow> imfData;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		float graphScaleMax;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		float graphScaleMin;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		float startTime;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		float endTime;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		TMap<int, FLineChain> graphLines;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		TArray<FLineChain> arcTanGraph;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		TArray<int> arcTanKeys;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;


	UFUNCTION(BlueprintImplementableEvent)
		void CreateIMFGraphWidgets();

	UFUNCTION(BlueprintCallable)
		TArray<FVector2D> GetDrawPointsByColumn(int col);

	UFUNCTION(BlueprintCallable)
		void GetWindowCornersOnScreen(FVector2D &bottomLeft, FVector2D &topRight, bool scaled = true);

	UFUNCTION(BlueprintCallable)
		TArray<FLineChain> GetDrawPointsForArcTan(int colX, int colY);

};
