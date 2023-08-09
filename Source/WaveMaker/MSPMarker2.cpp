// Fill out your copyright notice in the Description page of Project Settings.


#include "MSPMarker2.h"

// Sets default values
AMSPMarker2::AMSPMarker2()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	Tags.Add("MSPMarker");

}

// Called when the game starts or when spawned
void AMSPMarker2::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AMSPMarker2::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

