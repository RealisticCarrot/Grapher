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

	// Stores line segments with gaps for invalid data (9999.99, etc.)
	// Each call to GetDrawPointsByColumn also populates this array
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		TArray<FLineChain> currentColumnSegments;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		TArray<FLineChain> arcTanGraph;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		TArray<int> arcTanKeys;

	// Storage for custom equation graphs - maps key to equation string
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		TMap<int, FString> equationGraphs;

	// Stored line chains for equation graphs (for rendering)
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		TMap<int, FLineChain> equationGraphLines;

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

	// Returns line segments with breaks at invalid values (9999.99, NaN, etc.)
	// Use this instead of GetDrawPointsByColumn to handle data gaps properly
	UFUNCTION(BlueprintCallable)
		TArray<FLineChain> GetDrawSegmentsByColumn(int col);

	UFUNCTION(BlueprintCallable)
		void GetWindowCornersOnScreen(FVector2D &bottomLeft, FVector2D &topRight, bool scaled = true);

	UFUNCTION(BlueprintCallable)
		TArray<FLineChain> GetDrawPointsForArcTan(int colX, int colY);

	// Evaluates a custom equation and plots the result vs time
	// Syntax: Col1, Col2, etc. for column references; +, -, *, / for operations; parentheses for grouping
	// Constants: Pi, E (Euler's number)
	// Trig: Sin(x), Cos(x), Tan(x), Arctan(x), Atan(x), Atan2(y,x)
	// Math: Sqrt(x), Abs(x), Pow(x,y), Exp(x), Log(x), Ln(x), Log10(x), Log2(x)
	// Rounding: Floor(x), Ceil(x), Round(x)
	// Comparison: Min(a,b), Max(a,b), Clamp(value,min,max)
	// Supports scientific notation: 1.5e-3, 2E+10
	// Examples: "Pow(Col1, 2) + Pow(Col2, 2)", "Atan2(Col3, Col2)", "Sin(Pi * Col1)"
	UFUNCTION(BlueprintCallable)
		TArray<FLineChain> GetDrawPointsForEquation(const FString& equation);

	// Adds an equation graph and stores it for later refresh
	// Returns the key assigned to this graph
	UFUNCTION(BlueprintCallable)
		int AddEquationGraph(const FString& equation);

	// Removes an equation graph by key
	UFUNCTION(BlueprintCallable)
		void RemoveEquationGraph(int key);

	// Recalculates all stored equation graphs (call when axis range changes)
	UFUNCTION(BlueprintCallable)
		void RefreshEquationGraphs();

	// Gets the current line chains for a specific equation graph
	UFUNCTION(BlueprintCallable)
		TArray<FLineChain> GetEquationGraphLines(int key);

	// Converts minutes to HH:MM format string (e.g., 310 -> "05:10")
	UFUNCTION(BlueprintCallable, BlueprintPure)
		static FString MinutesToTimeString(float minutes);

};
