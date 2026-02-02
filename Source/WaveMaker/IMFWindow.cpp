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

	// Safety check: return empty if no data
	if (imfData.Num() == 0) {
		UE_LOG(LogTemp, Warning, TEXT("GetDrawPointsForArcTan: No data loaded"));
		return outData;
	}

	// Safety check: validate column indices
	int maxColumns = imfData[0].data.Num();
	if (colX < 0 || colX >= maxColumns || colY < 0 || colY >= maxColumns) {
		UE_LOG(LogTemp, Warning, TEXT("GetDrawPointsForArcTan: Invalid column indices X=%d, Y=%d (data has %d columns)"), colX, colY, maxColumns);
		return outData;
	}

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

		// Skip rows that don't have enough columns
		if (imfData[i].data.Num() <= colX || imfData[i].data.Num() <= colY) {
			continue;
		}

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


// ============================================================================
// Expression Parser for custom equations
// Supports: +, -, *, /, parentheses, Col# references
// Constants: Pi, E (Euler's number ~2.71828)
// Scientific notation: 1.5e-3, 2E+10, 3e5
// 
// Functions:
//   Trig:       Sin(x), Cos(x), Tan(x), Arctan(x), Atan(x), Atan2(y,x)
//   Math:       Sqrt(x), Abs(x), Pow(x,y), Exp(x)
//   Logarithms: Log(x), Ln(x), Log10(x), Log2(x)
//   Rounding:   Floor(x), Ceil(x), Round(x)
//   Comparison: Min(a,b), Max(a,b), Clamp(value,min,max)
// 
// Examples:
//   "Pow(Col1, 2) + Pow(Col2, 2)"     - sum of squares
//   "Atan2(Col3, Col2)"               - angle from two components
//   "Sin(Pi * Col1 / 180)"            - sine with degree conversion
//   "Log10(Abs(Col1) + 1)"            - log scale with offset
//   "Clamp(Col1, -10, 10)"            - bounded values
// ============================================================================

struct FExpressionParser
{
	const FString& Expr;
	int32 Pos;
	const TArray<float>& RowData;
	bool bHasError;
	FString ErrorMsg;
	
	FExpressionParser(const FString& InExpr, const TArray<float>& InRowData)
		: Expr(InExpr), Pos(0), RowData(InRowData), bHasError(false) {}
	
	void SkipWhitespace()
	{
		while (Pos < Expr.Len() && FChar::IsWhitespace(Expr[Pos]))
		{
			Pos++;
		}
	}
	
	bool Match(TCHAR c)
	{
		SkipWhitespace();
		if (Pos < Expr.Len() && Expr[Pos] == c)
		{
			Pos++;
			return true;
		}
		return false;
	}
	
	bool Peek(TCHAR c)
	{
		SkipWhitespace();
		return Pos < Expr.Len() && Expr[Pos] == c;
	}
	
	// Check if a value is a placeholder/invalid
	bool IsInvalidValue(float Value)
	{
		if (FMath::IsNaN(Value) || !FMath::IsFinite(Value)) return true;
		
		// Check exact placeholder values
		if (FMath::IsNearlyEqual(Value, 999.9f, 0.1f) ||
			FMath::IsNearlyEqual(Value, 999.99f, 0.01f) ||
			FMath::IsNearlyEqual(Value, 9999.9f, 0.1f) ||
			FMath::IsNearlyEqual(Value, 9999.99f, 0.01f) ||
			FMath::IsNearlyEqual(Value, 99999.9f, 0.1f) ||
			FMath::IsNearlyEqual(Value, 99999.99f, 0.01f) ||
			FMath::IsNearlyEqual(Value, -999.9f, 0.1f) ||
			FMath::IsNearlyEqual(Value, -999.99f, 0.01f) ||
			FMath::IsNearlyEqual(Value, -9999.9f, 0.1f) ||
			FMath::IsNearlyEqual(Value, -9999.99f, 0.01f))
		{
			return true;
		}
		return false;
	}
	
	float ParseNumber()
	{
		SkipWhitespace();
		int32 Start = Pos;
		
		// Integer part
		while (Pos < Expr.Len() && FChar::IsDigit(Expr[Pos]))
		{
			Pos++;
		}
		
		// Decimal part
		if (Pos < Expr.Len() && Expr[Pos] == '.')
		{
			Pos++;
			while (Pos < Expr.Len() && FChar::IsDigit(Expr[Pos]))
			{
				Pos++;
			}
		}
		
		// Scientific notation (e.g., 1.5e-3, 2E+10, 3e5)
		if (Pos < Expr.Len() && (Expr[Pos] == 'e' || Expr[Pos] == 'E'))
		{
			Pos++;
			// Optional sign
			if (Pos < Expr.Len() && (Expr[Pos] == '+' || Expr[Pos] == '-'))
			{
				Pos++;
			}
			// Exponent digits
			while (Pos < Expr.Len() && FChar::IsDigit(Expr[Pos]))
			{
				Pos++;
			}
		}
		
		if (Start == Pos)
		{
			bHasError = true;
			ErrorMsg = TEXT("Expected number");
			return 0.0f;
		}
		
		FString NumStr = Expr.Mid(Start, Pos - Start);
		return FCString::Atof(*NumStr);
	}
	
	// Try to parse "Col" followed by a number (case-insensitive)
	// Returns column index, or -1 if not a column reference
	int32 TryParseColRef()
	{
		SkipWhitespace();
		
		// Need at least 4 chars for "Col" + digit
		if (Pos + 3 >= Expr.Len()) return -1;
		
		// Check for "Col" prefix (case-insensitive)
		FString Prefix = Expr.Mid(Pos, 3);
		if (!Prefix.Equals(TEXT("Col"), ESearchCase::IgnoreCase))
		{
			return -1;
		}
		
		// Check that next char is a digit
		if (!FChar::IsDigit(Expr[Pos + 3]))
		{
			return -1;
		}
		
		// It's a column reference - consume "Col"
		Pos += 3;
		
		// Parse the column number
		int32 Start = Pos;
		while (Pos < Expr.Len() && FChar::IsDigit(Expr[Pos]))
		{
			Pos++;
		}
		
		FString NumStr = Expr.Mid(Start, Pos - Start);
		return FCString::Atoi(*NumStr);
	}
	
	// Try to match a function name (case-insensitive)
	// Returns the function name if matched, empty string otherwise
	FString TryMatchFunction(const FString& FuncName)
	{
		SkipWhitespace();
		int32 Len = FuncName.Len();
		
		if (Pos + Len > Expr.Len()) return FString();
		
		FString Candidate = Expr.Mid(Pos, Len);
		if (Candidate.Equals(FuncName, ESearchCase::IgnoreCase))
		{
			// Make sure it's followed by '('
			int32 CheckPos = Pos + Len;
			while (CheckPos < Expr.Len() && FChar::IsWhitespace(Expr[CheckPos]))
			{
				CheckPos++;
			}
			if (CheckPos < Expr.Len() && Expr[CheckPos] == '(')
			{
				Pos = CheckPos + 1; // Consume function name and '('
				return FuncName;
			}
		}
		return FString();
	}
	
	float ParsePrimary()
	{
		SkipWhitespace();
		
		// Try function calls first
		// Atan2(y, x) - two argument arctangent
		if (!TryMatchFunction(TEXT("Atan2")).IsEmpty())
		{
			float Arg1 = ParseExpression();
			if (!Match(','))
			{
				bHasError = true;
				ErrorMsg = TEXT("Atan2 requires two arguments: Atan2(y, x)");
				return 0.0f;
			}
			float Arg2 = ParseExpression();
			if (!Match(')'))
			{
				bHasError = true;
				ErrorMsg = TEXT("Expected closing parenthesis ')' after Atan2");
			}
			return FMath::Atan2(Arg1, Arg2);
		}
		
		// Arctan(x) or Atan(x) - single argument arctangent
		if (!TryMatchFunction(TEXT("Arctan")).IsEmpty() || !TryMatchFunction(TEXT("Atan")).IsEmpty())
		{
			float Arg = ParseExpression();
			if (!Match(')'))
			{
				bHasError = true;
				ErrorMsg = TEXT("Expected closing parenthesis ')' after Arctan");
			}
			return FMath::Atan(Arg);
		}
		
		// Sqrt(x) - square root
		if (!TryMatchFunction(TEXT("Sqrt")).IsEmpty())
		{
			float Arg = ParseExpression();
			if (!Match(')'))
			{
				bHasError = true;
				ErrorMsg = TEXT("Expected closing parenthesis ')' after Sqrt");
			}
			if (Arg < 0.0f)
			{
				bHasError = true;
				ErrorMsg = TEXT("Square root of negative number");
				return 0.0f;
			}
			return FMath::Sqrt(Arg);
		}
		
		// Abs(x) - absolute value
		if (!TryMatchFunction(TEXT("Abs")).IsEmpty())
		{
			float Arg = ParseExpression();
			if (!Match(')'))
			{
				bHasError = true;
				ErrorMsg = TEXT("Expected closing parenthesis ')' after Abs");
			}
			return FMath::Abs(Arg);
		}
		
		// Sin(x) - sine
		if (!TryMatchFunction(TEXT("Sin")).IsEmpty())
		{
			float Arg = ParseExpression();
			if (!Match(')'))
			{
				bHasError = true;
				ErrorMsg = TEXT("Expected closing parenthesis ')' after Sin");
			}
			return FMath::Sin(Arg);
		}
		
		// Cos(x) - cosine
		if (!TryMatchFunction(TEXT("Cos")).IsEmpty())
		{
			float Arg = ParseExpression();
			if (!Match(')'))
			{
				bHasError = true;
				ErrorMsg = TEXT("Expected closing parenthesis ')' after Cos");
			}
			return FMath::Cos(Arg);
		}
		
		// Tan(x) - tangent
		if (!TryMatchFunction(TEXT("Tan")).IsEmpty())
		{
			float Arg = ParseExpression();
			if (!Match(')'))
			{
				bHasError = true;
				ErrorMsg = TEXT("Expected closing parenthesis ')' after Tan");
			}
			return FMath::Tan(Arg);
		}
		
		// Pow(x, y) - power function (x^y)
		if (!TryMatchFunction(TEXT("Pow")).IsEmpty())
		{
			float Base = ParseExpression();
			if (!Match(','))
			{
				bHasError = true;
				ErrorMsg = TEXT("Pow requires two arguments: Pow(base, exponent)");
				return 0.0f;
			}
			float Exponent = ParseExpression();
			if (!Match(')'))
			{
				bHasError = true;
				ErrorMsg = TEXT("Expected closing parenthesis ')' after Pow");
			}
			return FMath::Pow(Base, Exponent);
		}
		
		// Exp(x) - e^x
		if (!TryMatchFunction(TEXT("Exp")).IsEmpty())
		{
			float Arg = ParseExpression();
			if (!Match(')'))
			{
				bHasError = true;
				ErrorMsg = TEXT("Expected closing parenthesis ')' after Exp");
			}
			return FMath::Exp(Arg);
		}
		
		// Log(x) - natural logarithm (ln)
		if (!TryMatchFunction(TEXT("Log")).IsEmpty() || !TryMatchFunction(TEXT("Ln")).IsEmpty())
		{
			float Arg = ParseExpression();
			if (!Match(')'))
			{
				bHasError = true;
				ErrorMsg = TEXT("Expected closing parenthesis ')' after Log/Ln");
			}
			if (Arg <= 0.0f)
			{
				bHasError = true;
				ErrorMsg = TEXT("Logarithm of non-positive number");
				return 0.0f;
			}
			return FMath::Loge(Arg);
		}
		
		// Log10(x) - base-10 logarithm
		if (!TryMatchFunction(TEXT("Log10")).IsEmpty())
		{
			float Arg = ParseExpression();
			if (!Match(')'))
			{
				bHasError = true;
				ErrorMsg = TEXT("Expected closing parenthesis ')' after Log10");
			}
			if (Arg <= 0.0f)
			{
				bHasError = true;
				ErrorMsg = TEXT("Logarithm of non-positive number");
				return 0.0f;
			}
			return FMath::LogX(10.0f, Arg);
		}
		
		// Log2(x) - base-2 logarithm
		if (!TryMatchFunction(TEXT("Log2")).IsEmpty())
		{
			float Arg = ParseExpression();
			if (!Match(')'))
			{
				bHasError = true;
				ErrorMsg = TEXT("Expected closing parenthesis ')' after Log2");
			}
			if (Arg <= 0.0f)
			{
				bHasError = true;
				ErrorMsg = TEXT("Logarithm of non-positive number");
				return 0.0f;
			}
			return FMath::Log2(Arg);
		}
		
		// Floor(x) - round down
		if (!TryMatchFunction(TEXT("Floor")).IsEmpty())
		{
			float Arg = ParseExpression();
			if (!Match(')'))
			{
				bHasError = true;
				ErrorMsg = TEXT("Expected closing parenthesis ')' after Floor");
			}
			return FMath::FloorToFloat(Arg);
		}
		
		// Ceil(x) - round up
		if (!TryMatchFunction(TEXT("Ceil")).IsEmpty())
		{
			float Arg = ParseExpression();
			if (!Match(')'))
			{
				bHasError = true;
				ErrorMsg = TEXT("Expected closing parenthesis ')' after Ceil");
			}
			return FMath::CeilToFloat(Arg);
		}
		
		// Round(x) - round to nearest
		if (!TryMatchFunction(TEXT("Round")).IsEmpty())
		{
			float Arg = ParseExpression();
			if (!Match(')'))
			{
				bHasError = true;
				ErrorMsg = TEXT("Expected closing parenthesis ')' after Round");
			}
			return FMath::RoundToFloat(Arg);
		}
		
		// Min(x, y) - minimum of two values
		if (!TryMatchFunction(TEXT("Min")).IsEmpty())
		{
			float Arg1 = ParseExpression();
			if (!Match(','))
			{
				bHasError = true;
				ErrorMsg = TEXT("Min requires two arguments: Min(a, b)");
				return 0.0f;
			}
			float Arg2 = ParseExpression();
			if (!Match(')'))
			{
				bHasError = true;
				ErrorMsg = TEXT("Expected closing parenthesis ')' after Min");
			}
			return FMath::Min(Arg1, Arg2);
		}
		
		// Max(x, y) - maximum of two values
		if (!TryMatchFunction(TEXT("Max")).IsEmpty())
		{
			float Arg1 = ParseExpression();
			if (!Match(','))
			{
				bHasError = true;
				ErrorMsg = TEXT("Max requires two arguments: Max(a, b)");
				return 0.0f;
			}
			float Arg2 = ParseExpression();
			if (!Match(')'))
			{
				bHasError = true;
				ErrorMsg = TEXT("Expected closing parenthesis ')' after Max");
			}
			return FMath::Max(Arg1, Arg2);
		}
		
		// Clamp(x, min, max) - clamp value to range
		if (!TryMatchFunction(TEXT("Clamp")).IsEmpty())
		{
			float Value = ParseExpression();
			if (!Match(','))
			{
				bHasError = true;
				ErrorMsg = TEXT("Clamp requires three arguments: Clamp(value, min, max)");
				return 0.0f;
			}
			float MinVal = ParseExpression();
			if (!Match(','))
			{
				bHasError = true;
				ErrorMsg = TEXT("Clamp requires three arguments: Clamp(value, min, max)");
				return 0.0f;
			}
			float MaxVal = ParseExpression();
			if (!Match(')'))
			{
				bHasError = true;
				ErrorMsg = TEXT("Expected closing parenthesis ')' after Clamp");
			}
			return FMath::Clamp(Value, MinVal, MaxVal);
		}
		
		// Parenthesized expression
		if (Match('('))
		{
			float Result = ParseExpression();
			if (!Match(')'))
			{
				bHasError = true;
				ErrorMsg = TEXT("Expected closing parenthesis ')'");
			}
			return Result;
		}
		
		// Try column reference first
		int32 SavedPos = Pos;
		int32 ColNum = TryParseColRef();
		if (ColNum >= 0)
		{
			if (ColNum >= RowData.Num())
			{
				bHasError = true;
				ErrorMsg = FString::Printf(TEXT("Column %d out of range (max: %d)"), ColNum, RowData.Num() - 1);
				return 0.0f;
			}
			
			float Value = RowData[ColNum];
			if (IsInvalidValue(Value))
			{
				bHasError = true;
				ErrorMsg = FString::Printf(TEXT("Column %d has invalid/placeholder value"), ColNum);
				return 0.0f;
			}
			return Value;
		}
		Pos = SavedPos; // Reset if not a column ref
		
		// Try mathematical constants (case-insensitive)
		SkipWhitespace();
		if (Pos + 2 <= Expr.Len())
		{
			FString TwoChars = Expr.Mid(Pos, 2);
			if (TwoChars.Equals(TEXT("Pi"), ESearchCase::IgnoreCase))
			{
				// Make sure it's not part of a longer identifier
				if (Pos + 2 >= Expr.Len() || !FChar::IsAlnum(Expr[Pos + 2]))
				{
					Pos += 2;
					return PI;
				}
			}
		}
		if (Pos + 1 <= Expr.Len())
		{
			TCHAR c = Expr[Pos];
			if (c == 'E' || c == 'e')
			{
				// Make sure it's not part of a longer identifier or scientific notation
				if (Pos + 1 >= Expr.Len() || (!FChar::IsAlnum(Expr[Pos + 1]) && Expr[Pos + 1] != '+' && Expr[Pos + 1] != '-'))
				{
					Pos += 1;
					return UE_EULERS_NUMBER; // e ≈ 2.71828
				}
			}
		}
		
		// Parse number
		return ParseNumber();
	}
	
	float ParseUnary()
	{
		SkipWhitespace();
		
		if (Match('-'))
		{
			return -ParseUnary();
		}
		if (Match('+'))
		{
			return ParseUnary();
		}
		
		return ParsePrimary();
	}
	
	float ParseTerm()
	{
		float Left = ParseUnary();
		
		while (!bHasError)
		{
			SkipWhitespace();
			if (Match('*'))
			{
				Left *= ParseUnary();
			}
			else if (Match('/'))
			{
				float Right = ParseUnary();
				if (FMath::IsNearlyZero(Right))
				{
					bHasError = true;
					ErrorMsg = TEXT("Division by zero");
					return 0.0f;
				}
				Left /= Right;
			}
			else
			{
				break;
			}
		}
		
		return Left;
	}
	
	float ParseExpression()
	{
		float Left = ParseTerm();
		
		while (!bHasError)
		{
			SkipWhitespace();
			if (Match('+'))
			{
				Left += ParseTerm();
			}
			else if (Match('-'))
			{
				Left -= ParseTerm();
			}
			else
			{
				break;
			}
		}
		
		return Left;
	}
	
	float Evaluate()
	{
		float Result = ParseExpression();
		SkipWhitespace();
		
		if (!bHasError && Pos < Expr.Len())
		{
			bHasError = true;
			ErrorMsg = FString::Printf(TEXT("Unexpected character '%c' at position %d"), Expr[Pos], Pos);
		}
		
		return Result;
	}
};


TArray<FLineChain> AIMFWindow::GetDrawPointsForEquation(const FString& equation)
{
	TArray<FLineChain> outData;
	
	// Safety check: return empty if no data
	if (imfData.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("GetDrawPointsForEquation: No data loaded"));
		return outData;
	}
	
	// Safety check: empty equation
	if (equation.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("GetDrawPointsForEquation: Empty equation"));
		return outData;
	}
	
	FVector2D bottomLeft;
	FVector2D topRight;
	GetWindowCornersOnScreen(bottomLeft, topRight);
	
	float timeRange = endTime - startTime;
	float scaleRange = graphScaleMin - graphScaleMax;
	
	if (FMath::IsNearlyZero(timeRange) || FMath::IsNearlyZero(scaleRange))
	{
		UE_LOG(LogTemp, Warning, TEXT("GetDrawPointsForEquation: Invalid time or scale range"));
		return outData;
	}
	
	FLineChain currentLineChain;
	bool bFirstError = true;
	
	for (int i = 0; i < imfData.Num(); i++)
	{
		// Skip if outside time range
		if (imfData[i].timeMinutes < startTime || imfData[i].timeMinutes > endTime)
		{
			continue;
		}
		
		// Evaluate the equation for this row
		FExpressionParser Parser(equation, imfData[i].data);
		float result = Parser.Evaluate();
		
		// Check for parsing errors or invalid results
		if (Parser.bHasError || FMath::IsNaN(result) || !FMath::IsFinite(result))
		{
			// Log the first error for debugging
			if (bFirstError && Parser.bHasError)
			{
				UE_LOG(LogTemp, Warning, TEXT("GetDrawPointsForEquation: %s"), *Parser.ErrorMsg);
				bFirstError = false;
			}
			
			// Break the line at invalid points
			if (currentLineChain.points.Num() > 0)
			{
				outData.Add(currentLineChain);
				currentLineChain.points.Empty();
			}
			continue;
		}
		
		// Calculate screen position
		FVector2D point;
		point.X = bottomLeft.X + (((imfData[i].timeMinutes - startTime) / timeRange) * (topRight.X - bottomLeft.X));
		point.Y = topRight.Y + (((result - graphScaleMax) / scaleRange) * (bottomLeft.Y - topRight.Y));
		
		currentLineChain.points.Add(point);
	}
	
	// Add final segment
	if (currentLineChain.points.Num() > 0)
	{
		outData.Add(currentLineChain);
	}
	
	// Log detailed info about generated segments
	int totalPoints = 0;
	for (int i = 0; i < outData.Num(); i++)
	{
		totalPoints += outData[i].points.Num();
		UE_LOG(LogTemp, Log, TEXT("GetDrawPointsForEquation('%s'): Segment %d has %d points"), *equation, i, outData[i].points.Num());
	}
	UE_LOG(LogTemp, Log, TEXT("GetDrawPointsForEquation('%s'): Generated %d segments with %d total points"), *equation, outData.Num(), totalPoints);
	
	return outData;
}


FString AIMFWindow::MinutesToTimeString(float minutes) {
	// Handle negative values
	if (minutes < 0) {
		return FString::Printf(TEXT("-%s"), *MinutesToTimeString(-minutes));
	}
	
	int totalMinutes = FMath::RoundToInt(minutes);
	int hours = totalMinutes / 60;
	int mins = totalMinutes % 60;
	
	// Format as HH:MM (e.g., 05:10)
	return FString::Printf(TEXT("%02d:%02d"), hours, mins);
}


// Counter for generating unique equation graph keys
static int EquationGraphKeyCounter = 0;

int AIMFWindow::AddEquationGraph(const FString& equation)
{
	int key = EquationGraphKeyCounter++;
	
	// Store the equation string
	equationGraphs.Add(key, equation);
	
	// Calculate and store the line chains
	TArray<FLineChain> chains = GetDrawPointsForEquation(equation);
	
	// Combine all chains into one FLineChain for storage (preserving segments)
	// Or store the first chain - you may want to adjust this based on your rendering needs
	if (chains.Num() > 0)
	{
		// For now, store all points in a single chain (you might want to store all segments)
		FLineChain combined;
		for (const FLineChain& chain : chains)
		{
			for (const FVector2D& point : chain.points)
			{
				combined.points.Add(point);
			}
		}
		equationGraphLines.Add(key, combined);
		
		// Also add each line chain segment to graphLines for immediate rendering
		// Use key * 1000 + chainIndex to match Blueprint's "Add Equation Lines" key scheme
		for (int i = 0; i < chains.Num(); i++)
		{
			int graphLineKey = key * 1000 + i;
			graphLines.Add(graphLineKey, chains[i]);
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("AddEquationGraph: Added equation '%s' with key %d, added %d segments to graphLines"), *equation, key, chains.Num());
	UE_LOG(LogTemp, Log, TEXT("AddEquationGraph: graphLines now has %d entries"), graphLines.Num());
	
	return key;
}

void AIMFWindow::RemoveEquationGraph(int key)
{
	if (equationGraphs.Contains(key))
	{
		equationGraphs.Remove(key);
		equationGraphLines.Remove(key);
		
		// Also remove the corresponding graphLines entries
		// Keys are in the range [key * 1000, key * 1000 + 999]
		int baseKey = key * 1000;
		TArray<int> keysToRemove;
		for (auto& pair : graphLines)
		{
			if (pair.Key >= baseKey && pair.Key < baseKey + 1000)
			{
				keysToRemove.Add(pair.Key);
			}
		}
		for (int k : keysToRemove)
		{
			graphLines.Remove(k);
		}
		
		UE_LOG(LogTemp, Log, TEXT("RemoveEquationGraph: Removed graph with key %d (%d graphLine entries)"), key, keysToRemove.Num());
	}
}

void AIMFWindow::RefreshEquationGraphs()
{
	UE_LOG(LogTemp, Log, TEXT("RefreshEquationGraphs: Refreshing %d equation graphs"), equationGraphs.Num());
	
	// Recalculate all stored equation graphs
	for (auto& pair : equationGraphs)
	{
		int key = pair.Key;
		const FString& equation = pair.Value;
		
		// First, remove old graphLines entries for this equation
		int baseKey = key * 1000;
		TArray<int> keysToRemove;
		for (auto& glPair : graphLines)
		{
			if (glPair.Key >= baseKey && glPair.Key < baseKey + 1000)
			{
				keysToRemove.Add(glPair.Key);
			}
		}
		for (int k : keysToRemove)
		{
			graphLines.Remove(k);
		}
		
		TArray<FLineChain> chains = GetDrawPointsForEquation(equation);
		
		if (chains.Num() > 0)
		{
			FLineChain combined;
			for (const FLineChain& chain : chains)
			{
				for (const FVector2D& point : chain.points)
				{
					combined.points.Add(point);
				}
			}
			equationGraphLines.Add(key, combined);
			
			// Also update graphLines for rendering
			for (int i = 0; i < chains.Num(); i++)
			{
				int graphLineKey = key * 1000 + i;
				graphLines.Add(graphLineKey, chains[i]);
			}
		}
		else
		{
			// Clear the lines if no data in range
			equationGraphLines.Remove(key);
		}
	}
}

TArray<FLineChain> AIMFWindow::GetEquationGraphLines(int key)
{
	TArray<FLineChain> result;
	
	// Re-evaluate the equation to get current line chains with proper segmentation
	if (equationGraphs.Contains(key))
	{
		result = GetDrawPointsForEquation(equationGraphs[key]);
	}
	
	return result;
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

void AIMFWindow::DebugLogGraphLines()
{
	UE_LOG(LogTemp, Log, TEXT("=== DEBUG: graphLines state ==="));
	UE_LOG(LogTemp, Log, TEXT("graphLines has %d entries:"), graphLines.Num());
	
	for (auto& pair : graphLines)
	{
		UE_LOG(LogTemp, Log, TEXT("  Key %d: %d points, Color: R=%d G=%d B=%d"), 
			pair.Key, 
			pair.Value.points.Num(),
			pair.Value.color.R,
			pair.Value.color.G,
			pair.Value.color.B);
	}
	
	UE_LOG(LogTemp, Log, TEXT("equationGraphs has %d entries:"), equationGraphs.Num());
	for (auto& pair : equationGraphs)
	{
		UE_LOG(LogTemp, Log, TEXT("  Key %d: '%s'"), pair.Key, *pair.Value);
	}
	
	UE_LOG(LogTemp, Log, TEXT("=== END DEBUG ==="));
}

int AIMFWindow::AddLineChainsToGraphLines(const TArray<FLineChain>& chains, int baseKey)
{
	int addedCount = 0;
	
	for (int i = 0; i < chains.Num(); i++)
	{
		int graphLineKey = baseKey + i;
		graphLines.Add(graphLineKey, chains[i]);
		addedCount++;
		
		UE_LOG(LogTemp, Log, TEXT("AddLineChainsToGraphLines: Added key %d with %d points"), 
			graphLineKey, chains[i].points.Num());
	}
	
	UE_LOG(LogTemp, Log, TEXT("AddLineChainsToGraphLines: Added %d chains starting at key %d, graphLines now has %d entries"), 
		addedCount, baseKey, graphLines.Num());
	
	return addedCount;
}
