// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"



#include "Camera/CameraComponent.h"

#include "InputAction.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInput/Public/EnhancedInputComponent.h"


#include "Kismet/GameplayStatics.h"

#include "Engine/EngineTypes.h"



#include "Viewer.generated.h"


class AMSPWindow;

class AMSPMarker2;


class AIMFWindow;



USTRUCT(BlueprintType)
struct FWindowManager
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite)
		TArray<AMSPWindow*> mspWindows;

};

// Enum for different data file formats
UENUM(BlueprintType)
enum class EDataFileFormat : uint8
{
	LST,  // Year DayOfYear Hour Minute [data...] - data starts at column 4
	TXT   // Year Month Day Hour Minute Second [data...] - data starts at column 6
};

USTRUCT(BlueprintType)
struct FRow
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintReadWrite)
		float timeMinutes = 0.0f;

	UPROPERTY(BlueprintReadWrite)
		TArray<FString> stringData;

	UPROPERTY(BlueprintReadWrite)
		TArray<float> data;
};



UCLASS()
class WAVEMAKER_API AViewer : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	AViewer();


	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		AActor* mspWindow;

	UPROPERTY(BlueprintReadWrite)
		FWindowManager windows;

	UPROPERTY()
		bool hittingTimeline;

	UPROPERTY()
		bool timelineHasBeenHit;

	UPROPERTY()
		AActor* hitTimeline;

	UPROPERTY()
		FVector mspZoomSize;
	UPROPERTY()
		FVector mspZoomLocation;

	UPROPERTY()
		FVector mspBaseSize;
	UPROPERTY()
		FVector mspBaseLocation;

	UPROPERTY()
		float mspSizeInterp;

	UPROPERTY(BlueprintReadWrite)
		float rangeBarValue;

	
	UPROPERTY(BlueprintReadWrite)
		float mspTimeLoc;

	UPROPERTY(BlueprintReadWrite)
		float mspTimeSpanUnit;

	// Direct time range control — normalized [0,1] values set from Blueprint text boxes
	UPROPERTY(BlueprintReadWrite)
		float mspStartDirect = 0.0f;

	UPROPERTY(BlueprintReadWrite)
		float mspEndDirect = 1.0f;

	// When true, mspStartDirect/mspEndDirect override the timeLoc/timeSpanUnit system
	UPROPERTY(BlueprintReadWrite)
		bool bUseDirectTimeRange = false;


	UPROPERTY(EditAnywhere)
		TSubclassOf<AMSPWindow> mspWindowClass;


	UPROPERTY(EditAnywhere)
		TSubclassOf<AMSPMarker2> mspMarkerClass;

	UPROPERTY(BlueprintReadWrite)
		TArray<AMSPMarker2*> mspMarkers;
	




	// IMF STUFF
	
	UPROPERTY(BlueprintReadWrite)
		TArray<FRow> imfData;

	// Current data file format (LST or TXT)
	UPROPERTY(BlueprintReadWrite)
		EDataFileFormat currentFileFormat = EDataFileFormat::LST;

	// Column offset where actual data starts (4 for LST, 6 for TXT)
	UPROPERTY(BlueprintReadWrite)
		int dataColumnOffset = 4;


	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		TSubclassOf<AIMFWindow> imfWindowClass;

	UPROPERTY(BlueprintReadWrite)
		AIMFWindow* imfWindow;

	





	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Controls")
		UInputAction* leftClickAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Controls")
		UInputAction* leftDownAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Controls")
		UInputAction* rightClickAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Controls")
		UInputAction* rightDownAction;



	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control Mappings")
		UInputMappingContext* baseControls;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control Mappings")
		int32 baseControlsPriority = 0;



	UFUNCTION()
		void leftClickInput(const FInputActionValue& value);

	UFUNCTION()
		void leftDownInput(const FInputActionValue& value);

	UFUNCTION()
		void rightClickInput(const FInputActionValue& value);

	UFUNCTION()
		void rightDownInput(const FInputActionValue& value);




protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;


	UFUNCTION(BlueprintCallable)
		void loadFile();

	// Captures the IMF graph area (with axis labels and markers), crops to graph region, and saves as PNG via Save File Dialog
	UFUNCTION(BlueprintCallable)
		void ExportGraphAsImage();
	
	UFUNCTION(BlueprintCallable)
		FWindowManager getWindows();


	UFUNCTION(BlueprintCallable)
		FRow GetRow(FString inStr);

	// Averages the result of an equation over a time range
	// equation: Can be a simple column reference like "Col4" or a full equation like "Col4+Col3/2"
	// startTime/endTime: Time strings in DD:HH:MM, HH:MM, or plain minutes format
	// Returns the average value, or 0 if no valid data points
	// outErrorMessage: Contains error description if calculation failed
	UFUNCTION(BlueprintCallable)
		float AverageColumn(const FString& equation, const FString& startTime, const FString& endTime, FString& outErrorMessage);


	// Converts a time string (DD:HH:MM, HH:MM, or plain minutes) to a normalized [0,1]
	// position based on the currently loaded MSP data's time range.
	// The time is relative to the start of the dataset.
	UFUNCTION(BlueprintCallable, Category = "MSP Time")
		float TimeStringToNormalized(const FString& timeStr, bool& bSuccess, FString& outError);

	// Converts a normalized [0,1] position back to a DD:HH:MM string
	// relative to the start of the dataset.
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MSP Time")
		FString NormalizedToTimeString(float normalizedValue);

	// Returns the total duration of the loaded MSP data as a DD:HH:MM string
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MSP Time")
		FString GetDataDurationString();

	UFUNCTION(BlueprintImplementableEvent)
		void CreateIMFGraphWidgets();
	
	// Called BEFORE resetting/destroying windows when loading a new file
	// Implement this in Blueprint to clear any cached window references
	UFUNCTION(BlueprintImplementableEvent)
		void OnBeforeReset();

	// Resolution multiplier for graph export (1.0 = viewport res, 2.0 = 2x, 4.0 = 4x)
	// Higher values produce sharper exports but take longer to render
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Export")
		float exportResolutionMultiplier = 1.0f;

	// Called just before the export screenshot is taken
	// Use this to hide any widgets you don't want in the exported image (e.g. graph manager, color wheel)
	UFUNCTION(BlueprintImplementableEvent)
		void OnBeforeExportScreenshot();

	// Called after the export screenshot has been saved
	// Use this to restore any widgets you hid in OnBeforeExportScreenshot
	UFUNCTION(BlueprintImplementableEvent)
		void OnAfterExportScreenshot();

private:
	// One-shot callback when viewport screenshot is captured (used by ExportGraphAsImage)
	void OnScreenshotCaptured(int32 SizeX, int32 SizeY, const TArray<FColor>& Colors);

	// Path chosen in Save File Dialog; used in OnScreenshotCaptured
	FString PendingSavePath;

	// So we can unbind the screenshot callback after one use
	FDelegateHandle ScreenshotDelegateHandle;

};
