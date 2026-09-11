// Fill out your copyright notice in the Description page of Project Settings.


#include "Viewer.h"
#include "IMFImport.h"
#include "IMFExport.h"
#include "Misc/MessageDialog.h"

#include "MSPWindow.h"

#include "MSPTimeline.h"

#include "MSPLegend.h"


// Native file dialogs are available in packaged Windows builds.
#include "WaveMakerFileDialogs.h"
#include "GenericPlatform/GenericWindow.h"
#include "Widgets/SWindow.h"



#include <stdio.h>
#include <stdlib.h>

#include <tchar.h>

#include <string>
#include <string.h>
#include <sstream>


#include "Engine/Texture2D.h"
#include "HAL/UnrealMemory.h"

#include "Materials/MaterialInstance.h"
#include "Materials/MaterialLayersFunctions.h"


#include "MSPMarker2.h"


#include "IMFWindow.h"

#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "UnrealClient.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Async/Async.h"


// Sets default values
AViewer::AViewer()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	//mspWindow = LoadObject<AActor>(nullptr, TEXT("/Script/Engine.Blueprint'/Game/Blueprints/MSPWindowBP.MSPWindowBP'"));

	hittingTimeline = false;

	mspSizeInterp = 0.0f;
	mspBaseSize = FVector(.25f, 2.5f, 1.0f);
	mspBaseLocation = FVector(190.0f, 0.0f, -265.0f);
	mspZoomSize = FVector(1.5f, 6.5f, 1.0f);
	mspZoomLocation = FVector(190.0f, 0.0f, -200.0f);
}

// Called when the game starts or when spawned
void AViewer::BeginPlay()
{
	Super::BeginPlay();
	
	Cast<APlayerController>(GetController())->bEnableMouseOverEvents = true;



	if (APlayerController* playerController = Cast<APlayerController>(GetController())) {
		UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(playerController->GetLocalPlayer());

		//GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::White, playerController->GetName());
		FString name = playerController->GetName();
		UE_LOG(LogTemp, Warning, TEXT("Player Base Setting Up: %s"), *name);

		if (Subsystem) {
			Subsystem->ClearAllMappings();

			Subsystem->AddMappingContext(baseControls, baseControlsPriority);
		}
	}






	
}

// Called every frame
void AViewer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);


	// hitting timeline is reset every frame so we know when a timeline has stopped being hit (it contrasts with timelineHasBeenHit)
	hittingTimeline = false;


	SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
	if (windows.mspWindows.Num() > 0) {
		//SetActorRotation((windows.mspWindows[0]->GetActorLocation() - GetActorLocation()).ToOrientationRotator());//FVector(1.0f, 0.0f, 0.0f).ToOrientationRotator());
	}
	else {
		SetActorRotation(FVector(0.0f, 0.0f, 0.0f).ToOrientationRotator());
	}

	GetController()->SetControlRotation(GetActorRotation());

	

	//check if the cursor is over a timeline
	FHitResult cursorHit;
	//get the cursor hit result
	Cast<APlayerController>(GetController())->GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, false, cursorHit);
	//if the cursor is over anything
	if (cursorHit.bBlockingHit) {
		//if the cursor is over a timeline
		if (cursorHit.GetActor()->ActorHasTag("Timeline")) {

			// get the timeline actor
			hitTimeline = cursorHit.GetActor();


			//set that it is hitting a timeline currently
			hittingTimeline = true;
			if (!timelineHasBeenHit) {
				timelineHasBeenHit = true;

			}

			//set the timeline to zoom mode FVector(2.0f, 4.5f, 1.0f)
			FVector scale = cursorHit.GetActor()->GetActorScale3D();
			FVector multScale = FVector::Zero() + 1.0f;
			if (scale.X < 2.0f || scale.Y < 4.5f) {
				mspSizeInterp += DeltaTime * 10.0f;
				mspSizeInterp = FMath::Clamp(mspSizeInterp, 0.0f, 1.0f);
				hitTimeline->SetActorScale3D(FMath::Lerp(mspBaseSize, mspZoomSize, mspSizeInterp));
				hitTimeline->SetActorLocation(FMath::Lerp(mspBaseLocation, mspZoomLocation, mspSizeInterp));

			}


			//hitTimeline->SetActorScale3D(scale * multScale);

		}
		//if hovering over the main msp window then show the value that we're hovering over
		

	}
	
	if (!hittingTimeline && timelineHasBeenHit) {

		if (hitTimeline) {
			//shrink to target if not at size yet (target: FVector(.25f, 2.5f, 1.0f))
			FVector scale = hitTimeline->GetActorScale3D();
			FVector multScale = FVector::Zero() + 1.0f;
			if (scale.X > .25f || scale.Y > 2.5f) {
				//multScale -= FVector(.2f, .1f, 0.0f);
				mspSizeInterp -= DeltaTime * 10.0f;
				mspSizeInterp = FMath::Clamp(mspSizeInterp, 0.0f, 1.0f);
				hitTimeline->SetActorScale3D(FMath::Lerp(mspBaseSize, mspZoomSize, mspSizeInterp));
				hitTimeline->SetActorLocation(FMath::Lerp(mspBaseLocation, mspZoomLocation, mspSizeInterp));

			}


			

			//if alreaady at target size then set timelineHasBeenHit to false
			if (scale.X <= .25f && scale.Y <= 2.5f) {
				timelineHasBeenHit = false;
			}
			else {
				//else keep shirnking
				//hitTimeline->SetActorScale3D(scale * multScale);
			}
			
		}
		else {
			timelineHasBeenHit = false;
		}
	}
	



	if (windows.mspWindows.Num() > 0) {
		
		if (bUseDirectTimeRange) {
			// Direct range mode: set main window material directly (NOT through
			// setMSPscalar, which would also zoom the timeline/legend materials).
			// Do NOT update startLoc/endLoc — the Blueprint label positioning reads those.
			windows.mspWindows[0]->bUseDirectTimeRange = true;
			if (windows.mspWindows[0]->materialInstance) {
				windows.mspWindows[0]->materialInstance->SetScalarParameterValue("start", mspStartDirect);
				windows.mspWindows[0]->materialInstance->SetScalarParameterValue("end", mspEndDirect);
			}
			// Only update the internal viewport range (for hover calculations)
			windows.mspWindows[0]->viewStartLoc = mspStartDirect;
			windows.mspWindows[0]->viewEndLoc = mspEndDirect;
		}
		else {
			// Original slider/timeline mode
			windows.mspWindows[0]->bUseDirectTimeRange = false;
			windows.mspWindows[0]->timeLoc = mspTimeLoc;

			// Forward time range from slider to MSP window
			if (mspTimeSpanUnit > 0.0f) {
				windows.mspWindows[0]->timeSpanUnit = mspTimeSpanUnit;
			}
			// Sync auto-computed value back to slider after file load
			else if (windows.mspWindows[0]->timeSpanUnit > 0.0f) {
				mspTimeSpanUnit = windows.mspWindows[0]->timeSpanUnit;
			}
		}
	}

	
}

// Called to bind functionality to input
void AViewer::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);


	if (UEnhancedInputComponent* playerEnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {

		//GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::White, "valid");

		playerEnhancedInput->BindAction(leftDownAction, ETriggerEvent::Triggered, this, &AViewer::leftDownInput);

		playerEnhancedInput->BindAction(leftClickAction, ETriggerEvent::Triggered, this, &AViewer::leftClickInput);

		playerEnhancedInput->BindAction(rightClickAction, ETriggerEvent::Triggered, this, &AViewer::rightClickInput);

	}




}

void AViewer::loadFile() {

	// Safety check for GEngine and viewport
	if (!GEngine || !GEngine->GameViewport)
	{
		UE_LOG(LogTemp, Error, TEXT("LoadFile: GEngine or GameViewport is null"));
		return;
	}
	
	TSharedPtr<SWindow> Window = GEngine->GameViewport->GetWindow();
	if (!Window.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("LoadFile: Window is not valid"));
		return;
	}
	
	TSharedPtr<FGenericWindow> NativeWindow = Window->GetNativeWindow();
	if (!NativeWindow.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("LoadFile: NativeWindow is not valid"));
		return;
	}

	void* viewportHandle = NativeWindow->GetOSWindowHandle();

	FString SelectedFilename;
	if (!WaveMakerFileDialogs::OpenDataFile(viewportHandle, SelectedFilename))
	{
		return;
	}

	TArray<FString> outName;
	outName.Add(MoveTemp(SelectedFilename));

	if (outName.Num() > 0) {
		FString outNameType;
		FString outNamePref;
		const FString splitter = ".";
		outName[0].Split(splitter, &outNamePref, &outNameType, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		
		// Convert extension to lowercase for easier comparison
		outNameType = outNameType.ToLower();
		
		// ========== CHECK FOR SUPPORTED FORMAT FIRST ==========
		// Only proceed if format is supported, otherwise show error and return
		bool isSupportedFormat = (outNameType == "ly" || outNameType == "txt" || outNameType == "lst");
		
		if (!isSupportedFormat) {
			// Unsupported file format - show message and return WITHOUT resetting anything
			UE_LOG(LogTemp, Warning, TEXT("LoadFile: Unsupported file format: .%s"), *outNameType);
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Orange, FString::Printf(TEXT("Unsupported file format: .%s\nSupported formats: .ly, .lst, .txt"), *outNameType));
			return;
		}
		// ======================================================
		
        if (outNameType == "txt" || outNameType == "lst")
        {
            FString Error;
            if (!LoadIMFFile(outName[0], true, Error) && !Error.IsEmpty()) FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Error));
            return;
        }
        ResetLoadedWindows();

		if (outNameType == "ly") {
			//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, "MSP Data File Targeted");

			FActorSpawnParameters params;
			//params.Template = mspWindow;
			
			// Check if MSP window class is set
			if (!mspWindowClass)
			{
				UE_LOG(LogTemp, Error, TEXT("LoadFile: MSP Window Class is not set in Blueprint"));
				GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("MSP Window Class is not configured"));
				return;
			}

			AMSPWindow* newWindow = GetWorld()->SpawnActor<AMSPWindow>(mspWindowClass, GetActorLocation() + FVector(200.0f, 0.0f, 0.0f), (GetActorUpVector()).ToOrientationRotator(), params);
			if (newWindow)
			{
				windows.mspWindows.Add(newWindow);
				mspWindow = newWindow;
				newWindow->loadFileAfterConstruction(outName[0]);
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("LoadFile: Failed to spawn MSP window"));
				GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("Failed to create MSP window"));
			}
			
			




		}



	}
}

void AViewer::ResetLoadedWindows()
{
		// ========== RESET ALL WINDOWS BEFORE LOADING NEW FILE ==========
		// Call Blueprint event FIRST so it can clear any cached references
		OnBeforeReset();
		
		// IMPORTANT: Clear arrays FIRST before destroying, so Blueprint sees empty arrays
		// and doesn't try to access destroyed actors
		
		// Store references to destroy, then clear the public-facing arrays immediately
		AIMFWindow* imfWindowToDestroy = imfWindow;
		imfWindow = nullptr;
		
		TArray<AMSPWindow*> mspWindowsToDestroy = windows.mspWindows;
		windows.mspWindows.Empty();
		mspWindow = nullptr;
		
		TArray<AMSPMarker2*> markersToDestroy = mspMarkers;
		mspMarkers.Empty();
		
		// Clear IMF data
		imfData.Empty();
		
		// Now safely destroy the stored actors (arrays are already cleared)
		if (imfWindowToDestroy != nullptr && IsValid(imfWindowToDestroy)) {
			imfWindowToDestroy->Destroy();
		}
		
		for (AMSPWindow* mspWin : mspWindowsToDestroy) {
			if (mspWin && IsValid(mspWin)) {
				// Destroy the timeline display
				if (mspWin->timelineDisplay && IsValid(mspWin->timelineDisplay)) {
					mspWin->timelineDisplay->Destroy();
				}
				// Destroy the legend display (Y-axis labels)
				if (mspWin->legendDisplay && IsValid(mspWin->legendDisplay)) {
					mspWin->legendDisplay->Destroy();
				}
				mspWin->Destroy();
			}
		}
		
		for (AMSPMarker2* marker : markersToDestroy) {
			if (marker && IsValid(marker)) {
				marker->DestroySelfAndWidgets();
			}
		}
		// ================================================================

}

void AViewer::ExportGraphAsImage()
{
    if (bExportInProgress) return;
	if (!imfWindow && windows.mspWindows.Num() == 0)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Orange, TEXT("Export Graph: No graph loaded (load a data file first)"));
		}
		return;
	}

	if (!GEngine || !GEngine->GameViewport)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("Export Graph: Viewport not available"));
		}
		return;
	}

	TSharedPtr<SWindow> Window = GEngine->GameViewport->GetWindow();
	if (!Window.IsValid())
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("Export Graph: Window not valid"));
		}
		return;
	}

	TSharedPtr<FGenericWindow> NativeWindow = Window->GetNativeWindow();
	if (!NativeWindow.IsValid())
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("Export Graph: Native window not valid"));
		}
		return;
	}

	void* ViewportHandle = NativeWindow->GetOSWindowHandle();
	FString SaveFilename;
	if (!WaveMakerFileDialogs::SaveGraphImage(ViewportHandle, SaveFilename))
	{
		return;
	}

    if (imfWindow)
    {
        TArray<FColor> Pixels; int32 Width = 0, Height = 0; FString Error;
        imfWindow->EnsurePlotCurrent();
        if (!IMFExport::Capture(imfWindow, menuWidget, exportResolutionMultiplier, Pixels, Width, Height, Error))
        { FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Error)); return; }
        bExportInProgress = true;
        WriteGraphPNG(MoveTemp(Pixels), Width, Height, SaveFilename);
        return;
    }
    bExportInProgress = true;
    PendingSavePath = MoveTemp(SaveFilename);

	UGameViewportClient* GameViewport = GEngine->GameViewport;
	if (ScreenshotDelegateHandle.IsValid())
	{
		GameViewport->OnScreenshotCaptured().Remove(ScreenshotDelegateHandle);
		ScreenshotDelegateHandle.Reset();
	}
	ScreenshotDelegateHandle = GameViewport->OnScreenshotCaptured().AddUObject(this, &AViewer::OnScreenshotCaptured);

	// Let Blueprint hide any widgets that shouldn't appear in the export
    bRestoreScreenshotUI = true;
	OnBeforeExportScreenshot();

	// Delay the screenshot by 2 frames so widget visibility changes take effect
	GetWorldTimerManager().SetTimer(ExportTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		FScreenshotRequest::RequestScreenshot(true);
	}), 0.05f, false);
}

void AViewer::OnScreenshotCaptured(int32 SizeX, int32 SizeY, const TArray<FColor>& Colors)
{
	UGameViewportClient* GameViewport = GEngine ? GEngine->GameViewport : nullptr;
	if (GameViewport && ScreenshotDelegateHandle.IsValid())
	{
		GameViewport->OnScreenshotCaptured().Remove(ScreenshotDelegateHandle);
		ScreenshotDelegateHandle.Reset();
	}

	if (PendingSavePath.IsEmpty())
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("Export Graph: No save path"));
		}
		OnAfterExportScreenshot();
        bExportInProgress = false;
		return;
	}

	FVector2D BottomLeft = FVector2D::ZeroVector;
	FVector2D TopRight = FVector2D::ZeroVector;
	bool bGotBounds = false;
	bool bIsIMF = false;
	AActor* GraphActor = nullptr;

	// Try IMF graph first
	if (IsValid(imfWindow))
	{
		GraphActor = imfWindow;
		bIsIMF = true;
	}
	// Otherwise try MSP window
	else if (windows.mspWindows.Num() > 0 && windows.mspWindows[0] && IsValid(windows.mspWindows[0]))
	{
		GraphActor = windows.mspWindows[0];
	}

	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (GraphActor && PC)
	{
		FVector BoundOrigin = FVector::ZeroVector;
		FVector BoundExtents = FVector::ZeroVector;
		GraphActor->GetActorBounds(true, BoundOrigin, BoundExtents);

		const bool bBottomLeftProjected = PC->ProjectWorldLocationToScreen(BoundOrigin - BoundExtents, BottomLeft);
		const bool bTopRightProjected = PC->ProjectWorldLocationToScreen(BoundOrigin + BoundExtents, TopRight);
		bGotBounds = bBottomLeftProjected && bTopRightProjected && !BottomLeft.ContainsNaN() && !TopRight.ContainsNaN();
	}

	if (!bGotBounds)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("Export Graph: Could not determine graph bounds"));
		}
		PendingSavePath.Empty();
		OnAfterExportScreenshot();
        bExportInProgress = false;
		return;
	}

	// Scale projected bounds from viewport space to image space
	// (needed when using high-res screenshot multiplier)
	FVector2D ViewportSize;
	GEngine->GameViewport->GetViewportSize(ViewportSize);
	float ImageScaleX = (float)SizeX / FMath::Max(ViewportSize.X, 1.0f);
	float ImageScaleY = (float)SizeY / FMath::Max(ViewportSize.Y, 1.0f);
	BottomLeft.X *= ImageScaleX;
	BottomLeft.Y *= ImageScaleY;
	TopRight.X *= ImageScaleX;
	TopRight.Y *= ImageScaleY;

	// Scale padding based on actual image resolution (designed for 1920x1080)
	float ScaleX = (float)SizeX / 1920.0f;
	float ScaleY = (float)SizeY / 1080.0f;

	float PaddingLeft, PaddingBottom, PaddingTop, PaddingRight;
	if (bIsIMF)
	{
		PaddingLeft = 160.0f * ScaleX;
		PaddingBottom = 100.0f * ScaleY;
		PaddingTop = 120.0f * ScaleY;
		PaddingRight = 200.0f * ScaleX;
	}
	else
	{
		// MSP padding
		PaddingLeft = 130.0f * ScaleX;
		PaddingBottom = 120.0f * ScaleY;
		PaddingTop = 110.0f * ScaleY;
		PaddingRight = 200.0f * ScaleX;
	}

	float MinX = FMath::Min(BottomLeft.X, TopRight.X);
	float MaxX = FMath::Max(BottomLeft.X, TopRight.X);
	float MinY = FMath::Min(BottomLeft.Y, TopRight.Y);
	float MaxY = FMath::Max(BottomLeft.Y, TopRight.Y);

	int32 CropLeft = FMath::Clamp(FMath::RoundToInt(MinX - PaddingLeft), 0, SizeX - 1);
	int32 CropRight = FMath::Clamp(FMath::RoundToInt(MaxX + PaddingRight), 0, SizeX);
	int32 CropTop = FMath::Clamp(FMath::RoundToInt(MinY - PaddingTop), 0, SizeY - 1);
	int32 CropBottom = FMath::Clamp(FMath::RoundToInt(MaxY + PaddingBottom), 0, SizeY);

	if (CropRight <= CropLeft || CropBottom <= CropTop)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("Export Graph: Invalid crop region"));
		}
		PendingSavePath.Empty();
		OnAfterExportScreenshot();
        bExportInProgress = false;
		return;
	}

	const int32 CropWidth = CropRight - CropLeft;
	const int32 CropHeight = CropBottom - CropTop;

	// Fast row-based memcpy instead of per-pixel copy
	TArray<FColor> CroppedPixels;
	CroppedPixels.SetNumUninitialized(CropWidth * CropHeight);

	for (int32 Row = 0; Row < CropHeight; Row++)
	{
		FMemory::Memcpy(
			&CroppedPixels[Row * CropWidth],
			&Colors[(CropTop + Row) * SizeX + CropLeft],
			CropWidth * sizeof(FColor)
		);
	}

	// Move heavy PNG encode + file write to a background thread so
	// the game thread (and framerate) is not blocked
	FString SavePath = PendingSavePath;
	PendingSavePath.Empty();

    WriteGraphPNG(MoveTemp(CroppedPixels), CropWidth, CropHeight, SavePath);
}

FRow AViewer::GetRow(FString inStr)
{
    FRow Row; IMFImport::Tokenize(inStr, Row.stringData);
    for (const FString& Token : Row.stringData)
    {
        double V; Row.data.Add(IMFImport::ParseNumber(Token, V) && FMath::IsFinite(float(V)) ? float(V) : NAN);
    }
    if (Row.data.Num() >= 4 && FMath::IsFinite(Row.data[2]) && FMath::IsFinite(Row.data[3])) Row.timeMinutes = double(Row.data[2]) * 60 + Row.data[3];
    return Row;
}
float AViewer::AverageColumn(const FString& equation, const FString& startTime, const FString& endTime, FString& outErrorMessage)
{
    return AverageWithMode(equation, startTime, endTime, AIMFWindow::IsClockExpression(equation), outErrorMessage);
}


float AViewer::TimeStringToNormalized(const FString& timeStr, bool& bSuccess, FString& outError)
{
	bSuccess = false;
	outError = TEXT("");

	if (windows.mspWindows.Num() == 0 || !IsValid(windows.mspWindows[0]))
	{
		outError = TEXT("No MSP window loaded");
		return 0.0f;
	}

	AMSPWindow* mspWin = windows.mspWindows[0];
	float timeDiff = mspWin->endTime - mspWin->startTime;
	if (FMath::IsNearlyZero(timeDiff))
	{
		outError = TEXT("Data has zero time range");
		return 0.0f;
	}

	FString trimmed = timeStr.TrimStartAndEnd();
	if (trimmed.IsEmpty())
	{
		outError = TEXT("Empty time string");
		return 0.0f;
	}

	// Parse DD:HH:MM or HH:MM or plain minutes
	TArray<FString> parts;
	trimmed.ParseIntoArray(parts, TEXT(":"), true);

	float totalSeconds = 0.0f;

	if (parts.Num() == 3)
	{
		// DD:HH:MM
		float days = FCString::Atof(*parts[0]);
		float hours = FCString::Atof(*parts[1]);
		float minutes = FCString::Atof(*parts[2]);
		totalSeconds = days * 86400.0f + hours * 3600.0f + minutes * 60.0f;
	}
	else if (parts.Num() == 2)
	{
		// HH:MM
		float hours = FCString::Atof(*parts[0]);
		float minutes = FCString::Atof(*parts[1]);
		totalSeconds = hours * 3600.0f + minutes * 60.0f;
	}
	else if (parts.Num() == 1)
	{
		// Plain minutes
		float minutes = FCString::Atof(*parts[0]);
		totalSeconds = minutes * 60.0f;
	}
	else
	{
		outError = TEXT("Invalid format. Use DD:HH:MM, HH:MM, or minutes");
		return 0.0f;
	}

	float normalized = totalSeconds / timeDiff;
	bSuccess = true;
	return FMath::Clamp(normalized, 0.0f, 1.0f);
}

FString AViewer::NormalizedToTimeString(float normalizedValue)
{
	if (windows.mspWindows.Num() == 0 || !IsValid(windows.mspWindows[0]))
	{
		return TEXT("--:--:--");
	}

	AMSPWindow* mspWin = windows.mspWindows[0];
	float timeDiff = mspWin->endTime - mspWin->startTime;
	float totalSeconds = normalizedValue * timeDiff;

	int32 totalSecondsInt = FMath::RoundToInt(totalSeconds);
	int32 days = totalSecondsInt / 86400;
	int32 remaining = totalSecondsInt % 86400;
	int32 hours = remaining / 3600;
	remaining = remaining % 3600;
	int32 minutes = remaining / 60;

	return FString::Printf(TEXT("%d:%02d:%02d"), days, hours, minutes);
}

FString AViewer::GetDataDurationString()
{
	if (windows.mspWindows.Num() == 0 || !IsValid(windows.mspWindows[0]))
	{
		return TEXT("No data loaded");
	}

	AMSPWindow* mspWin = windows.mspWindows[0];
	float timeDiff = mspWin->endTime - mspWin->startTime;

	int32 totalSecondsInt = FMath::RoundToInt(timeDiff);
	int32 days = totalSecondsInt / 86400;
	int32 remaining = totalSecondsInt % 86400;
	int32 hours = remaining / 3600;
	remaining = remaining % 3600;
	int32 minutes = remaining / 60;

	return FString::Printf(TEXT("%d:%02d:%02d"), days, hours, minutes);
}

FWindowManager AViewer::getWindows() {
	return windows;
}


void AViewer::leftClickInput(const FInputActionValue& value) {

	//GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Red, "Left Click");

	
	if (windows.mspWindows.Num() > 0) {

		//GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Orange, "More Than 0 MSP Windows");

		FHitResult hit;

		if (GetLocalViewingPlayerController()->GetHitResultUnderCursor(ECC_Visibility, true, hit)) {


			//GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Yellow, "Something Hit");

			if (hit.GetActor()->ActorHasTag("MSPWindow")) {

				//GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Green, "MSP Window Hit");


				FActorSpawnParameters params;

				mspMarkers.Add(GetWorld()->SpawnActor<AMSPMarker2>(mspMarkerClass, hit.Location - FVector(0.0f, 1.0f, 0.0f), FRotator::ZeroRotator));
				
				int lastInd = mspMarkers.Num() - 1;

				mspMarkers[lastInd]->Index = lastInd;

				if (mspMarkers[lastInd]->Index % 2 == 1) {
					mspMarkers[mspMarkers.Num() - 2]->Partner = mspMarkers[lastInd];

					//I might not set the partner for this marker because I want only the first marker to draw a line
					// so I'll only draw when partner is set
					//mspMarkers.Last()->Partner = mspMarkers[mspMarkers.Num() - 2];

				}
				

				//DrawDebugSphere(GetWorld(), hit.Location, 10.0f, 16, FColor::Green, true);


			}

		}


	}

	// IMF graph vertical marker - only active for .txt/.lst files (imfWindow is nullptr for .ly files)
	if (imfWindow)
	{
		imfWindow->AddMarkerAtMousePosition();
	}
}


void AViewer::leftDownInput(const FInputActionValue& value) {
	//GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Blue, "MouseDown");


	if (windows.mspWindows.Num() > 0) {

		FHitResult hit;

		
		if (GetLocalViewingPlayerController()->GetHitResultUnderCursor(ECC_Visibility, true, hit)) {
			
			//GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::White, hit.GetActor()->GetName());

			
			if (hit.GetActor()->ActorHasTag("Timeline")) {
				FVector2D uv;
				if (UGameplayStatics::FindCollisionUV(hit, 0, uv)) {
					//GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Blue, "UV Successful");

				}
				else {
					//GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Yellow, "UV Unsuccessful");

					FVector2D mouseLoc;
					GetLocalViewingPlayerController()->GetMousePosition(mouseLoc.X, mouseLoc.Y);


					FVector boundOrigin;
					FVector boundExtents;

					windows.mspWindows[0]->timelineDisplay->GetActorBounds(true, boundOrigin, boundExtents);

					FVector2D topRight;
					GetLocalViewingPlayerController()->ProjectWorldLocationToScreen(boundOrigin + boundExtents, topRight);
					FVector2D bottomLeft;
					GetLocalViewingPlayerController()->ProjectWorldLocationToScreen(boundOrigin - boundExtents, bottomLeft);

					//DrawDebugBox(GetWorld(), boundOrigin, boundExtents, FColor::Yellow);
					//DrawDebugSphere(GetWorld(), boundOrigin + boundExtents, 50.0f, 32, FColor::Green);

					float xLoc = (mouseLoc.X - bottomLeft.X) / (topRight.X - bottomLeft.X);
					

					if (bUseDirectTimeRange) {
						// Re-center the zoomed window on the clicked position
						float span = mspEndDirect - mspStartDirect;
						mspStartDirect = FMath::Clamp(xLoc - span / 2.0f, 0.0f, 1.0f - span);
						mspEndDirect = mspStartDirect + span;
					}
					else {
						mspTimeLoc = xLoc;
					}
					

				}
			}
			

			
		}

	}
}


void AViewer::rightClickInput(const FInputActionValue& value) {
	
	TArray<AActor*> markerActors;
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), "MSPMarker", markerActors);

	for (AActor* marker : markerActors) {
		Cast<AMSPMarker2>(marker)->DestroySelfAndWidgets();


		mspMarkers.Empty();
	}

	// IMF graph vertical marker removal - only active for .txt/.lst files (imfWindow is nullptr for .ly files)
	if (imfWindow)
	{
		imfWindow->RemoveMarkerAtMousePosition();
	}
}


void AViewer::rightDownInput(const FInputActionValue& value) {
	
}
