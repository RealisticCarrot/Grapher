#pragma once

#include "structs.generated.h"

USTRUCT()
struct FmspDataProperties {
	GENERATED_BODY()

public:
	float dataLength;
	float maxValue;
	float minValue;
	float avgValue;
	float timeStepSize;


};