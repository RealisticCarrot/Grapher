#pragma once

#include "CoreMinimal.h"

namespace WaveMakerFileDialogs
{
    bool OpenDataFile(void* ParentWindowHandle, FString& OutFilename);
    bool SaveGraphImage(void* ParentWindowHandle, FString& OutFilename);
}
