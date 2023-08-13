// Fill out your copyright notice in the Description page of Project Settings.


#include "Viewer.h"

#include "MSPWindow.h"

#include "MSPTimeline.h"


//for file selection
#include "C:\\Program Files\\Epic Games\\UE_5.1\\Engine\\Source\\Developer\\DesktopPlatform\\Public\\IDesktopPlatform.h"
#include "C:\\Program Files\\Epic Games\\UE_5.1\\Engine\\Source\\Developer\\DesktopPlatform\\Public\\DesktopPlatformModule.h"



#include <stdio.h>
#include <stdlib.h>

#include <tchar.h>

#include <string>
#include <string.h>
#include <sstream>


#include "Engine/Texture2D.h"
#include "C:/Program Files/Epic Games/UE_5.1/Engine/Source/Runtime/Core/Public/HAL/UnrealMemory.h"

#include "Materials/MaterialInstance.h"
#include "Materials/MaterialLayersFunctions.h"


#include "MSPMarker2.h"


// Sets default values
AViewer::AViewer()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	//mspWindow = LoadObject<AActor>(nullptr, TEXT("/Script/Engine.Blueprint'/Game/Blueprints/MSPWindowBP.MSPWindowBP'"));

	//camera = CreateDefaultSubobject<UCameraComponent>("Camera");

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
	
	//camera->ProjectionMode = ECameraProjectionMode::Orthographic;
	//camera->OrthoWidth = 1000.0f;
	//camera->AspectRatio = 1.0f;

	//camera->SetRelativeLocation(FVector::ZeroVector);
	
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


	//camera->SetWorldLocation(GetActorLocation());
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
		
		windows.mspWindows[0]->timeLoc = mspTimeLoc;
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

	void* viewportHandle = GEngine->GameViewport->GetWindow()->GetNativeWindow()->GetOSWindowHandle();

	TArray<FString> outName;
	
	//FDesktopPlatformModule module;

	//module.StartupModule();

	IDesktopPlatform* deskPlatform = FDesktopPlatformModule::Get();//module.Get();

	deskPlatform->OpenFileDialog(viewportHandle, "Open Data File", "|:/", "file.txt", "MSP Data|*.LY;*.ly|Text Files|*.txt;*.lst", 0, outName);

	if (outName.Num() > 0) {
		FString outNameType;
		FString outNamePref;
		const FString splitter = ".";
		outName[0].Split(splitter, &outNamePref, &outNameType, ESearchCase::IgnoreCase, ESearchDir::FromEnd);

		

		if (outNameType == (FString)"ly" || outNameType == (FString)"LY") {
			//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, "MSP Data File Targeted");

			FActorSpawnParameters params;
			//params.Template = mspWindow;
			
			//for now get rid of already created msp windows, in the future multiple windows will be allowed
			if (windows.mspWindows.Num() > 0) {
				windows.mspWindows.Last()->timelineDisplay->Destroy();
				windows.mspWindows.Last()->Destroy();
				windows.mspWindows.Empty();
			}
			

			windows.mspWindows.Add(GetWorld()->SpawnActor<AMSPWindow>(mspWindowClass, GetActorLocation() + FVector(200.0f, 0.0f, 0.0f), (GetActorUpVector()).ToOrientationRotator(), params));
			windows.mspWindows.Last()->loadFileAfterConstruction(outName[0]);
			
			




		}


		if (outNameType == (FString)"txt" || outNameType == (FString)"lst") {
			TArray<FString> dataText;
			FFileHelper::LoadFileToStringArray(dataText, *outName[0]);

			for (int i = 0; i < dataText.Num(); i++) {
				FString leftStr;
				FString rightStr;

				dataText[i].Split(" ", &leftStr, &rightStr);

				//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, "Left:" + leftStr + "|Right:" + rightStr);
				
				imfData.Add(GetRow(dataText[i]));


			}

		}
		



		//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Black, outName[0]);

		/*
		for (int i = 0; i < dataText.Num(); i++) {
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Black, dataText[i]);

		}
		*/
	}
}



FRow AViewer::GetRow(FString inStr) {


	


	FRow outRow;

	FString leftStr = inStr.ConvertTabsToSpaces(3) + " ";
	FString rightStr;

	FString extractedValue;
	/*
	int n = 0;
	while (leftStr.Len() > 0 && n < 100) {

		
		leftStr.Split(" ", &extractedValue, &rightStr);

		outRow.stringData.Add(extractedValue);
		outRow.data.Add(FCString::Atof(*extractedValue));
		

		leftStr = rightStr;

		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::White, leftStr);
		UE_LOG(LogTemp, Warning, TEXT("Left String: %s"), *extractedValue);

		n++;

	}
	*/

	
	inStr.ParseIntoArray(outRow.stringData, TEXT(" "), true);

	for (FString dataS : outRow.stringData) {
		outRow.data.Add(FCString::Atof(*dataS));
		//UE_LOG(LogTemp, Warning, TEXT("Left String: %f"), FCString::Atof(*dataS));
	}


	outRow.timeMinutes = (outRow.data[2] * 60.0f) + outRow.data[3];

	return outRow;



}


float AViewer::AverageColumn(int col, FString startTime, FString endTime) {

	FString hoursStr;
	FString minutesStr;

	startTime.Split(":", &hoursStr, &minutesStr);

	float startMins = (FCString::Atof(*hoursStr) * 60.0f) + FCString::Atof(*minutesStr);



	endTime.Split(":", &hoursStr, &minutesStr);

	float endMins = (FCString::Atof(*hoursStr) * 60.0f) + FCString::Atof(*minutesStr);


	float sum = 0.0f;
	bool startAdding = false;
	int n = 0;
	if (imfData.Num() > 0) {
		for (FRow row : imfData) {
			

			if (row.timeMinutes == startMins) {

				startAdding = true;
				sum += row.data[col];
				n++;

			}
			else if (row.timeMinutes == endMins) {

				

				startAdding = false;

				

			}
			else if (startAdding) {
				sum += row.data[col];
				n++;
			}

		}
	}
	else {
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::White, "No rows");
		return 0.0f;
	}


	return sum / (float)n;

	

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
					

					mspTimeLoc = xLoc;
					

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

}


void AViewer::rightDownInput(const FInputActionValue& value) {
	
}