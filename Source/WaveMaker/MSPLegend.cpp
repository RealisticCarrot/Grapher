// Fill out your copyright notice in the Description page of Project Settings.


#include "MSPLegend.h"

// Sets default values
AMSPLegend::AMSPLegend()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	legendPlane = CreateDefaultSubobject<UStaticMeshComponent>("LegendPlane");

	yTarget = 0.2f;

}

// Called when the game starts or when spawned
void AMSPLegend::BeginPlay()
{
	Super::BeginPlay();



	SetActorLocation(GetActorLocation() + FVector(-10.0f, 450.0f, 0.0f));


	legendPlane->SetWorldScale3D(FVector(-.1f, 1.5f, 1.0f));
	legendPlane->SetWorldRotation(rotation);


	materialInstance = UMaterialInstanceDynamic::Create(legendMaterial, this);



	legendPlane->SetMaterial(0, materialInstance);

}

// Called every frame
void AMSPLegend::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AMSPLegend::setMSPLegendScalar(FString name, float value) {

	materialInstance->SetScalarParameterValue(FName(name), value);

	if (name == "div") {

		div = value;

		UpdateLegendRange();
	}
	else if (name == "a") {
		a = value;

		UpdateLegendRange();
	}
	//GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Green, name + " updated: " + FString::SanitizeFloat(value));

}

void AMSPLegend::UpdateLegendRange() {

	float v = FMath::Sqrt(-((yTarget - 1.0f) * (10000.0f * a)) / yTarget);

	rangeMax = (20000.0f * div) + v;

	rangeMin = (20000.0f * div) - v;

	rangeMin = fmaxf(rangeMin, 0.0f);

	GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::White, FString::SanitizeFloat(rangeMax));
	GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::White, FString::SanitizeFloat(rangeMin));



	setMSPLegendScalar("rangeMax", rangeMax);
	setMSPLegendScalar("rangeMin", rangeMin);

	UpdateLabelTexts();

}
