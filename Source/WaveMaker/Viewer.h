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



USTRUCT(BlueprintType)
struct FWindowManager
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite)
		TArray<AMSPWindow*> mspWindows;

};

USTRUCT(BlueprintType)
struct FRow
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintReadWrite)
		float timeMinutes;

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

	//UPROPERTY()
	//	UCameraComponent* camera;

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


	UPROPERTY(EditAnywhere)
		TSubclassOf<AMSPWindow> mspWindowClass;


	UPROPERTY(EditAnywhere)
		TSubclassOf<AMSPMarker2> mspMarkerClass;

	UPROPERTY(BlueprintReadWrite)
		TArray<AMSPMarker2*> mspMarkers;
	




	// IMF STUFF
	
	UPROPERTY(BlueprintReadWrite)
		TArray<FRow> imfData;










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
	
	UFUNCTION(BlueprintCallable)
		FWindowManager getWindows();


	UFUNCTION(BlueprintCallable)
		FRow GetRow(FString inStr);

	UFUNCTION(BlueprintCallable)
		float AverageColumn(int col, FString startTime, FString endTime);

	
	

};
