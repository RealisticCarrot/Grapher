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

	// Also populate currentColumnSegments with properly segmented data for gap handling
	currentColumnSegments.Empty();
	FLineChain currentSegment;
	bool inValidSegment = false;

	for (int i = 0; i < imfData.Num(); i++) {

		// Check if this row has enough columns
		if (imfData[i].data.Num() <= col)
		{
			continue;
		}

		if (imfData[i].timeMinutes >= startTime && imfData[i].timeMinutes <= endTime) {
			
			float dataValue = imfData[i].data[col];
			
			// Check for invalid placeholder values (exact common placeholders, NaN, etc.)
			// Only filter EXACT placeholder values, not ranges, to avoid filtering real data
			bool isPlaceholder = FMath::IsNearlyEqual(dataValue, 999.9f, 0.1f) ||
								 FMath::IsNearlyEqual(dataValue, 999.99f, 0.01f) ||
								 FMath::IsNearlyEqual(dataValue, 9999.9f, 0.1f) ||
								 FMath::IsNearlyEqual(dataValue, 9999.99f, 0.01f) ||
								 FMath::IsNearlyEqual(dataValue, 99999.9f, 0.1f) ||
								 FMath::IsNearlyEqual(dataValue, 99999.99f, 0.01f) ||
								 FMath::IsNearlyEqual(dataValue, -999.9f, 0.1f) ||
								 FMath::IsNearlyEqual(dataValue, -999.99f, 0.01f) ||
								 FMath::IsNearlyEqual(dataValue, -9999.9f, 0.1f) ||
								 FMath::IsNearlyEqual(dataValue, -9999.99f, 0.01f);
			
			bool isInvalid = FMath::IsNaN(dataValue) || 
							 !FMath::IsFinite(dataValue) || 
							 isPlaceholder;
			
			if (isInvalid)
			{
				// End current segment if we were building one
				if (inValidSegment && currentSegment.points.Num() > 0)
				{
					currentColumnSegments.Add(currentSegment);
					currentSegment.points.Empty();
					inValidSegment = false;
				}
				continue;
			}

			// Calculate the screen position for this valid data point
			temp.X = bottomLeft.X + (((imfData[i].timeMinutes - startTime) / timeRange) * (topRight.X - bottomLeft.X));
			temp.Y = topRight.Y + (((dataValue - graphScaleMax) / scaleRange) * (bottomLeft.Y - topRight.Y));
			
			// Add to both the legacy single array and the segmented data
			outData.Add(temp);
			currentSegment.points.Add(temp);
			inValidSegment = true;
		}
	}

	// Don't forget the last segment
	if (currentSegment.points.Num() > 0)
	{
		currentColumnSegments.Add(currentSegment);
	}

	UE_LOG(LogTemp, Log, TEXT("GetDrawPointsByColumn(%d): Generated %d points in %d segments"), col, outData.Num(), currentColumnSegments.Num());

	/*
	FLineChain chain;
	chain.points = outData;
	chain.color =  (UKismetMathLibrary::HSVToRGB(FMath::FRandRange(0.0, 360.0), .5f, 0.6f)).ToFColor(true);

	graphLines.Add(chain);
	*/


	return outData;
}


TArray<FLineChain> AIMFWindow::GetDrawSegmentsByColumn(int col) {

	TArray<FLineChain> segments;

	// Safety check for empty data
	if (imfData.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("GetDrawSegmentsByColumn: No data loaded"));
		return segments;
	}
	
	// Safety check for column index
	if (col < 0 || imfData[0].data.Num() <= col)
	{
		UE_LOG(LogTemp, Warning, TEXT("GetDrawSegmentsByColumn: Invalid column %d (data has %d columns)"), col, imfData[0].data.Num());
		return segments;
	}

	FVector2D bottomLeft;
	FVector2D topRight;
	GetWindowCornersOnScreen(bottomLeft, topRight);
	
	// Prevent division by zero
	float timeRange = endTime - startTime;
	float scaleRange = graphScaleMin - graphScaleMax;
	
	if (FMath::IsNearlyZero(timeRange) || FMath::IsNearlyZero(scaleRange))
	{
		UE_LOG(LogTemp, Warning, TEXT("GetDrawSegmentsByColumn: Invalid time or scale range"));
		return segments;
	}

	FLineChain currentSegment;
	bool inValidSegment = false;

	for (int i = 0; i < imfData.Num(); i++) {

		// Check if this row has enough columns
		if (imfData[i].data.Num() <= col)
		{
			// End current segment if we were in one
			if (inValidSegment && currentSegment.points.Num() > 0)
			{
				segments.Add(currentSegment);
				currentSegment.points.Empty();
				inValidSegment = false;
			}
			continue;
		}

		// Check if within time range
		if (imfData[i].timeMinutes < startTime || imfData[i].timeMinutes > endTime)
		{
			continue;
		}

		float dataValue = imfData[i].data[col];
		
		// Check for invalid values (exact common placeholders, NaN, infinity, etc.)
		bool isPlaceholder = FMath::IsNearlyEqual(dataValue, 999.9f, 0.1f) ||
							 FMath::IsNearlyEqual(dataValue, 999.99f, 0.01f) ||
							 FMath::IsNearlyEqual(dataValue, 9999.9f, 0.1f) ||
							 FMath::IsNearlyEqual(dataValue, 9999.99f, 0.01f) ||
							 FMath::IsNearlyEqual(dataValue, 99999.9f, 0.1f) ||
							 FMath::IsNearlyEqual(dataValue, 99999.99f, 0.01f) ||
							 FMath::IsNearlyEqual(dataValue, -999.9f, 0.1f) ||
							 FMath::IsNearlyEqual(dataValue, -999.99f, 0.01f) ||
							 FMath::IsNearlyEqual(dataValue, -9999.9f, 0.1f) ||
							 FMath::IsNearlyEqual(dataValue, -9999.99f, 0.01f);
		bool isInvalidValue = FMath::IsNaN(dataValue) || 
							  !FMath::IsFinite(dataValue) || 
							  isPlaceholder;

		if (isInvalidValue)
		{
			// End current segment - this creates the break/gap
			if (inValidSegment && currentSegment.points.Num() > 0)
			{
				segments.Add(currentSegment);
				currentSegment.points.Empty();
				inValidSegment = false;
			}
			// Don't add this point - leave a gap
			continue;
		}

		// Valid data point - add to current segment
		FVector2D point;
		point.X = bottomLeft.X + (((imfData[i].timeMinutes - startTime) / timeRange) * (topRight.X - bottomLeft.X));
		point.Y = topRight.Y + (((dataValue - graphScaleMax) / scaleRange) * (bottomLeft.Y - topRight.Y));

		currentSegment.points.Add(point);
		inValidSegment = true;
	}

	// Don't forget the last segment
	if (currentSegment.points.Num() > 0)
	{
		segments.Add(currentSegment);
	}

	UE_LOG(LogTemp, Log, TEXT("GetDrawSegmentsByColumn(%d): Generated %d segments"), col, segments.Num());

	return segments;
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
