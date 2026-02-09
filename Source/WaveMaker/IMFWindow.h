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

	// Column offset where actual data starts (4 for LST format, 6 for TXT format)
	// This is automatically set when loading the file
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		int dataColumnOffset = 4;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		TMap<int, FLineChain> graphLines;

	// Stores line segments with gaps for invalid data (9999.99, etc.)
	// Each call to GetDrawPointsByColumn also populates this array
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		TArray<FLineChain> currentColumnSegments;

	// Storage for custom equation graphs - maps key to equation string
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		TMap<int, FString> equationGraphs;

	// Stored line chains for equation graphs (for rendering)
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		TMap<int, FLineChain> equationGraphLines;

	// Vertical marker lines (stores time positions in minutes)
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		TArray<float> markerTimes;

	// Tracks which columns are currently being displayed (for mouse hover detection)
	// Call AddDisplayedColumn/RemoveDisplayedColumn when toggling columns
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		TArray<int> displayedColumns;

	// Whether to show grid lines on the graph
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		bool bShowGridLines = true;

	// Number of horizontal grid divisions (default 9, matching Y-axis tick marks)
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		int gridHorizontalDivisions = 9;

	// Number of vertical grid divisions (default 12)
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		int gridVerticalDivisions = 12;

	// Grid line color (faint gray by default)
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		FColor gridLineColor = FColor(180, 180, 180, 255);

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
	// Syntax: Col1, Col2, etc. for column references (Col1 = first column, Col0 = error)
	// For LST files: data starts at Col5. For TXT files: data starts at Col7.
	// Operators: +, -, *, /, ^ (power); parentheses for grouping
	// Constants: Pi, E (Euler's number)
	// Trig: Sin(x), Cos(x), Tan(x), Arctan(x), Atan(x), Atan2(y,x)
	// Math: Sqrt(x), Abs(x), Pow(x,y), Exp(x), Log(x), Ln(x), Log10(x), Log2(x)
	// Rounding: Floor(x), Ceil(x), Round(x)
	// Comparison: Min(a,b), Max(a,b), Clamp(value,min,max)
	// Supports scientific notation: 1.5e-3, 2E+10
	// Examples: "Col5^2 + Col6^2", "Atan2(Col6, Col5)", "Sin(Pi * Col5)"
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

	// Validates an equation string and returns true if it's valid
	// Also returns an error message if invalid
	UFUNCTION(BlueprintCallable)
		bool IsValidEquation(const FString& equation, FString& outErrorMessage);

	// Registers an equation graph for hover detection
	// Call this after GetDrawPointsForEquation if you want hover to detect the equation
	// key: A unique identifier for this equation (you manage this)
	// equation: The equation string (for display)
	// chains: The line chains returned by GetDrawPointsForEquation
	UFUNCTION(BlueprintCallable)
		void RegisterEquationForHover(int key, const FString& equation, const TArray<FLineChain>& chains);

	// Unregisters an equation graph from hover detection
	UFUNCTION(BlueprintCallable)
		void UnregisterEquationFromHover(int key);

	// Clears ALL equation registrations (for debugging/reset)
	UFUNCTION(BlueprintCallable)
		void ClearAllEquationRegistrations();

	// Debug: prints all currently registered equations to log
	UFUNCTION(BlueprintCallable)
		void DebugPrintEquationRegistrations();

	// Evaluates an equation for a single row of data
	// Returns the result, or 0 if evaluation failed
	// bSuccess is set to true if evaluation succeeded, false otherwise
	// outErrorMessage contains error description if evaluation failed
	UFUNCTION(BlueprintCallable)
		static float EvaluateEquationForRow(const FString& equation, const TArray<float>& rowData, 
			bool& bSuccess, FString& outErrorMessage);

	// Converts minutes to time string. Formats:
	// - DD:HH:MM if days > 0 (e.g., 1563 -> "01:02:03" meaning Day 1, Hour 2, Min 3)
	// - HH:MM if days = 0 (e.g., 123 -> "02:03" meaning Hour 2, Min 3)
	UFUNCTION(BlueprintCallable, BlueprintPure)
		static FString MinutesToTimeString(float minutes);

	// Gets the graph coordinates (time and value) at the current mouse position
	// Returns true if mouse is over the graph, false otherwise
	// outTime: The time value (in minutes) at the mouse X position
	// outValue: The Y-axis value at the mouse Y position
	// outTimeString: The time formatted as DD:HH:MM or HH:MM string
	UFUNCTION(BlueprintCallable)
		bool GetValueAtMousePosition(float& outTime, float& outValue, FString& outTimeString);

	// Gets detailed info about the closest graph line at mouse position
	// Returns true if mouse is over the graph and a nearby line was found
	// outTime: The time value (in minutes) at the mouse X position
	// outTimeString: The time formatted as DD:HH:MM or HH:MM
	// outGraphValue: The actual Y value from the closest graph line at this time
	// outGraphName: Name of the graph ("Col5", "Col6", or equation like "Col5^2")
	// outGraphKey: The key/column number of the closest graph (-1 if none found)
	// outIsEquation: True if closest graph is a custom equation, false if column
	// outColor: The color of the closest graph line
	UFUNCTION(BlueprintCallable)
		bool GetClosestGraphAtMouse(float& outTime, FString& outTimeString, float& outGraphValue, 
			FString& outGraphName, int& outGraphKey, bool& outIsEquation, FColor& outColor);

	// Converts a time string to minutes. Accepts formats:
	// - "DD:HH:MM" format (e.g., "01:02:03" -> Day 1, Hour 2, Min 3 -> 1563 minutes)
	// - "HH:MM" format (e.g., "02:03" -> Hour 2, Min 3 -> 123 minutes)
	// - Plain number (e.g., "80" -> 80 minutes)
	// Returns true if parsing succeeded, false otherwise
	// outErrorMessage contains the error description if parsing failed
	UFUNCTION(BlueprintCallable, BlueprintPure)
		static bool TimeStringToMinutes(const FString& timeString, float& outMinutes, FString& outErrorMessage);

	// === Marker System ===

	// Adds a vertical marker line at the specified time (from mouse position)
	// Returns true if marker was added, false if mouse is not over graph
	UFUNCTION(BlueprintCallable)
		bool AddMarkerAtMousePosition();

	// Removes the nearest vertical marker line to the current mouse position
	// Returns true if a marker was removed, false if no markers nearby or mouse not over graph
	UFUNCTION(BlueprintCallable)
		bool RemoveMarkerAtMousePosition();

	// Adds a marker at a specific time value
	UFUNCTION(BlueprintCallable)
		void AddMarkerAtTime(float timeMinutes);

	// Removes a marker at a specific time value (with tolerance)
	// Returns true if marker was found and removed
	UFUNCTION(BlueprintCallable)
		bool RemoveMarkerAtTime(float timeMinutes, float toleranceMinutes = 5.0f);

	// Clears all markers
	UFUNCTION(BlueprintCallable)
		void ClearAllMarkers();

	// Rebuilds vertical marker lines in graphLines so they are rendered by the Blueprint painter
	// Uses keys -10000, -10001, etc. to avoid conflicts with column/equation graph keys
	// Called automatically when markers are added/removed
	UFUNCTION(BlueprintCallable)
		void RefreshMarkerGraphLines();

	// Adds faint grid lines (horizontal and vertical) to graphLines
	// Uses keys -20000, -20001, etc. to avoid conflicts
	// Call this after changing axis ranges or toggling grid visibility
	UFUNCTION(BlueprintCallable)
		void RefreshGridLines();

	// Gets screen X positions for all markers (for rendering)
	// Returns array of screen X coordinates for markers within visible range
	UFUNCTION(BlueprintCallable)
		TArray<float> GetMarkerScreenPositions();

	// Gets marker info as formatted strings (for display)
	UFUNCTION(BlueprintCallable)
		TArray<FString> GetMarkerTimeStrings();

	// === Displayed Column Tracking (for mouse hover) ===

	// Call this when a column is selected/displayed
	UFUNCTION(BlueprintCallable)
		void AddDisplayedColumn(int col);

	// Call this when a column is deselected/hidden
	UFUNCTION(BlueprintCallable)
		void RemoveDisplayedColumn(int col);

	// Clears all displayed columns tracking
	UFUNCTION(BlueprintCallable)
		void ClearDisplayedColumns();

};
