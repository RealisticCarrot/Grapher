#pragma once

#include "CoreMinimal.h"
#include "structs.generated.h"

USTRUCT(BlueprintType)
struct FUIPositions {
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
		FVector2D TitlePosition;

	UPROPERTY(BlueprintReadOnly)
		FVector2D LeftLabelPosition;

	UPROPERTY(BlueprintReadOnly)
		FVector2D BottomLabelPosition;

	UPROPERTY(BlueprintReadOnly)
		FVector2D LegendPosition;

	UPROPERTY(BlueprintReadOnly)
		FVector2D GraphSize;
};

USTRUCT()
struct FmspDataProperties {
	GENERATED_BODY()

public:
	float dataLength;
	float maxValue;
	float minValue;
	float avgValue;
	float timeStepSize;

	// Auto-computed color mapping defaults based on the data range
	float autoDiv = 0.3f;   // Color Center: centers color window on the data midpoint
	float autoA = 0.8f;     // Color Width: scales color spread to cover the data range

};