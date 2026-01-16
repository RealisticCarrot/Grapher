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

	UPROPERTY(BlueprintReadWrite)
		TArray<FRow> imfData;

	UPROPERTY(BlueprintReadWrite)
		float graphScaleMax;

	UPROPERTY(BlueprintReadWrite)
		float graphScaleMin;

	UPROPERTY(BlueprintReadWrite)
		float startTime;

	UPROPERTY(BlueprintReadWrite)
		float endTime;

	UPROPERTY(BlueprintReadWrite)
		TMap<int, FLineChain> graphLines;

	UPROPERTY(BlueprintReadWrite)
		TArray<FLineChain> arcTanGraph;

	UPROPERTY(BlueprintReadWrite)
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

	// Auto-calculate and set the Y-axis scale based on data in the specified column
	UFUNCTION(BlueprintCallable)
		void AutoScaleYAxis(int col);

	// Get the number of data columns available
	UFUNCTION(BlueprintCallable, BlueprintPure)
		int GetColumnCount() const;

	// Get Y-axis label values (evenly spaced between graphScaleMin and graphScaleMax)
	// NumLabels: how many labels you want (e.g., 5 gives you min, 25%, 50%, 75%, max)
	UFUNCTION(BlueprintCallable, BlueprintPure)
		TArray<float> GetYAxisLabelValues(int NumLabels = 5) const;

	// Get the screen Y position for a given data value (for positioning Y-axis labels)
	UFUNCTION(BlueprintCallable, BlueprintPure)
		float GetScreenYForValue(float Value) const;

};
