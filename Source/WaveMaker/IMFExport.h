#pragma once
#include "CoreMinimal.h"
class AIMFWindow;
class UUserWidget;
namespace IMFExport
{
    bool Capture(AIMFWindow* Window, UUserWidget* Menu, float Multiplier, TArray<FColor>& Pixels, int32& Width, int32& Height, FString& Error);
}
