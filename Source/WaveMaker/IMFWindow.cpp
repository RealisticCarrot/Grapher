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

	// Auto-calculate startTime and endTime based on actual data
	if (imfData.Num() > 0)
	{
		// Find min and max time values in the data
		float minTime = imfData[0].timeMinutes;
		float maxTime = imfData[0].timeMinutes;
		
		for (int i = 1; i < imfData.Num(); i++)
		{
			if (imfData[i].timeMinutes < minTime)
			{
				minTime = imfData[i].timeMinutes;
			}
			if (imfData[i].timeMinutes > maxTime)
			{
				maxTime = imfData[i].timeMinutes;
			}
		}
		
		startTime = minTime;
		endTime = maxTime;
		
		UE_LOG(LogTemp, Log, TEXT("IMF Data loaded: %d rows, time range: %.1f - %.1f minutes"), imfData.Num(), startTime, endTime);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("IMF Data is empty!"));
	}

	CreateIMFGraphWidgets();
}

// Called every frame
void AIMFWindow::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}


TArray<FVector2D> AIMFWindow::GetDrawPointsByColumn(int col) {

	TArray<FVector2D> outData;

	// Safety check for empty data
	if (imfData.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("GetDrawPointsByColumn: No data loaded"));
		return outData;
	}
	
	// Safety check for column index
	if (col < 0 || imfData[0].data.Num() <= col)
	{
		UE_LOG(LogTemp, Warning, TEXT("GetDrawPointsByColumn: Invalid column %d (data has %d columns)"), col, imfData[0].data.Num());
		return outData;
	}

	FVector2D temp;

	FVector2D bottomLeft;
	FVector2D topRight;

	GetWindowCornersOnScreen(bottomLeft, topRight);
	
	// Prevent division by zero
	float timeRange = endTime - startTime;
	float scaleRange = graphScaleMin - graphScaleMax;
	
	if (FMath::IsNearlyZero(timeRange) || FMath::IsNearlyZero(scaleRange))
	{
		UE_LOG(LogTemp, Warning, TEXT("GetDrawPointsByColumn: Invalid time or scale range"));
		return outData;
	}


	for (int i = 0; i < imfData.Num(); i++) {

		// Check if this row has enough columns
		if (imfData[i].data.Num() <= col)
		{
			continue;
		}

		if (imfData[i].timeMinutes >= startTime && imfData[i].timeMinutes <= endTime) {
			
			float dataValue = imfData[i].data[col];
			
			// Skip invalid placeholder values
			if (dataValue > 90000.0f || dataValue < -90000.0f)
			{
				continue;
			}

			temp.X = bottomLeft.X + (((imfData[i].timeMinutes - startTime) / timeRange) * (topRight.X - bottomLeft.X));
			temp.Y = topRight.Y + (((dataValue - graphScaleMax) / scaleRange) * (bottomLeft.Y - topRight.Y));

			
			
			outData.Add(temp);

		}


	}

	UE_LOG(LogTemp, Log, TEXT("GetDrawPointsByColumn(%d): Generated %d points"), col, outData.Num());

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


void AIMFWindow::AutoScaleYAxis(int col)
{
	if (imfData.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("AutoScaleYAxis: No data loaded"));
		return;
	}
	
	// Check if column index is valid
	if (imfData[0].data.Num() <= col || col < 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("AutoScaleYAxis: Invalid column index %d (data has %d columns)"), col, imfData[0].data.Num());
		return;
	}
	
	float minVal = imfData[0].data[col];
	float maxVal = imfData[0].data[col];
	
	for (int i = 1; i < imfData.Num(); i++)
	{
		if (imfData[i].data.Num() > col)
		{
			float val = imfData[i].data[col];
			
			// Skip invalid values (like 99999.9 which is often used as a placeholder)
			if (val > 90000.0f || val < -90000.0f)
			{
				continue;
			}
			
			if (val < minVal) minVal = val;
			if (val > maxVal) maxVal = val;
		}
	}
	
	// Add 10% padding to the scale
	float range = maxVal - minVal;
	float padding = range * 0.1f;
	
	// Handle case where all values are the same
	if (range < 0.001f)
	{
		padding = 1.0f;
	}
	
	graphScaleMin = minVal - padding;
	graphScaleMax = maxVal + padding;
	
	UE_LOG(LogTemp, Log, TEXT("AutoScaleYAxis for column %d: min=%.2f, max=%.2f"), col, graphScaleMin, graphScaleMax);
}


int AIMFWindow::GetColumnCount() const
{
	if (imfData.Num() > 0 && imfData[0].data.Num() > 0)
	{
		return imfData[0].data.Num();
	}
	return 0;
}


TArray<float> AIMFWindow::GetYAxisLabelValues(int NumLabels) const
{
	TArray<float> LabelValues;
	
	if (NumLabels < 2)
	{
		NumLabels = 2; // At least min and max
	}
	
	float range = graphScaleMax - graphScaleMin;
	float step = range / (NumLabels - 1);
	
	for (int i = 0; i < NumLabels; i++)
	{
		float value = graphScaleMin + (step * i);
		LabelValues.Add(value);
	}
	
	return LabelValues;
}


float AIMFWindow::GetScreenYForValue(float Value) const
{
	FVector2D bottomLeft;
	FVector2D topRight;
	
	// Need to cast away const for GetWindowCornersOnScreen (it doesn't modify state but isn't marked const)
	const_cast<AIMFWindow*>(this)->GetWindowCornersOnScreen(bottomLeft, topRight);
	
	float scaleRange = graphScaleMin - graphScaleMax;
	
	if (FMath::IsNearlyZero(scaleRange))
	{
		return (topRight.Y + bottomLeft.Y) / 2.0f; // Return center if invalid
	}
	
	float screenY = topRight.Y + (((Value - graphScaleMax) / scaleRange) * (bottomLeft.Y - topRight.Y));
	
	return screenY;
}
