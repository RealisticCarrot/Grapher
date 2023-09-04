// Fill out your copyright notice in the Description page of Project Settings.


#include "IMFWindow.h"

#include "Viewer.h"

#include "Kismet/GameplayStatics.h"

#include "Kismet/KismetMathLibrary.h"


#include "Blueprint/WidgetLayoutLibrary.h"



// Sets default values
AIMFWindow::AIMFWindow()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	graphScaleMax = 10.0f;
	graphScaleMin = -10.0f;

	startTime = 0.0f;
	endTime = 60.0f;

}

// Called when the game starts or when spawned
void AIMFWindow::BeginPlay()
{
	Super::BeginPlay();
	

	imfData = Cast<AViewer>(GetWorld()->GetFirstPlayerController()->GetPawn())->imfData;

	CreateIMFGraphWidgets();
}

// Called every frame
void AIMFWindow::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}


TArray<FVector2D> AIMFWindow::GetDrawPointsByColumn(int col) {

	TArray<FVector2D> outData;

	FVector2D temp;

	FVector2D bottomLeft;
	FVector2D topRight;

	GetWindowCornersOnScreen(bottomLeft, topRight);


	for (int i = 0; i < imfData.Num(); i++) {


		if (imfData[i].timeMinutes >= startTime && imfData[i].timeMinutes <= endTime) {

			temp.X = bottomLeft.X + (((imfData[i].timeMinutes - startTime) / (endTime - startTime)) * (topRight.X - bottomLeft.X));
			temp.Y = topRight.Y + (((imfData[i].data[col] - graphScaleMax) / (graphScaleMin - graphScaleMax)) * (bottomLeft.Y - topRight.Y));

			
			
			outData.Add(temp);

		}


	}


	/*
	FLineChain chain;
	chain.points = outData;
	chain.color =  (UKismetMathLibrary::HSVToRGB(FMath::FRandRange(0.0, 360.0), .5f, 0.6f)).ToFColor(true);

	graphLines.Add(chain);
	*/


	return outData;
}



TArray<FLineChain> AIMFWindow::GetDrawPointsForArcTan(int colX, int colY) {

	TArray<FLineChain> outData;

	FVector2D temp;

	FVector2D bottomLeft;
	FVector2D topRight;

	
	GetWindowCornersOnScreen(bottomLeft, topRight);

	FVector2D screenDif = bottomLeft - topRight;


	float ang;

	FLineChain currentLineChain;

	float Yoff = 0;

	FVector2D prev = (topRight + bottomLeft) / 2.0f;
	float dif;



	for (int i = 0; i < imfData.Num(); i++) {

		

		if (imfData[i].timeMinutes >= startTime && imfData[i].timeMinutes <= endTime) {

			ang = FMath::Atan2(imfData[i].data[colY], imfData[i].data[colX]);


			temp.X = bottomLeft.X + (((imfData[i].timeMinutes - startTime) / (endTime - startTime)) * (topRight.X - bottomLeft.X));
			temp.Y = Yoff + topRight.Y + (((ang - graphScaleMax) / (graphScaleMin - graphScaleMax)) * (bottomLeft.Y - topRight.Y));


			dif = temp.Y - prev.Y;

			

			if (fabsf(dif) > screenDif.Y * 0.5f) {


				if (FMath::Sign(dif) > 0.0f) {
					currentLineChain.points.Add(FVector2D((prev.X + temp.X) / 2.0f, topRight.Y));
					outData.Add(currentLineChain);
					currentLineChain.points.Empty();

					//Yoff makes it so that when it jumps around the graph it loops to the other side
					//Yoff += FMath::Sign(dif) * screenDif.Y;

					currentLineChain.points.Add(FVector2D((prev.X + temp.X) / 2.0f, bottomLeft.Y));
				}
				else {
					currentLineChain.points.Add(FVector2D((prev.X + temp.X) / 2.0f, bottomLeft.Y));
					outData.Add(currentLineChain);
					currentLineChain.points.Empty();

					//Yoff makes it so that when it jumps around the graph it loops to the other side
					//Yoff += FMath::Sign(dif) * screenDif.Y;

					currentLineChain.points.Add(FVector2D((prev.X + temp.X) / 2.0f, topRight.Y));
				}
				
				
				
				


			}
			


			currentLineChain.points.Add(temp);

			prev = temp;

			

		}


	}

	outData.Add(currentLineChain);

	return outData;
}





void AIMFWindow::GetWindowCornersOnScreen(FVector2D &bottomLeft, FVector2D &topRight, bool scaled) {

	FVector origin;
	FVector extent;

	GetActorBounds(true, origin, extent);

	float scale = UWidgetLayoutLibrary::GetViewportScale(GetWorld());
	


	UGameplayStatics::ProjectWorldToScreen(GetWorld()->GetFirstPlayerController(), origin - extent, bottomLeft);
	UGameplayStatics::ProjectWorldToScreen(GetWorld()->GetFirstPlayerController(), origin + extent, topRight);
	
	if (scaled) {
		bottomLeft /= scale;
		topRight /= scale;
	}
	

}
