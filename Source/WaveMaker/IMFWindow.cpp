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
	
	// Clear any leftover state from previous sessions
	equationGraphs.Empty();
	equationGraphLines.Empty();
	displayedColumns.Empty();
	
	AViewer* viewer = Cast<AViewer>(GetWorld()->GetFirstPlayerController()->GetPawn());
	imfData = viewer->imfData;
	dataColumnOffset = viewer->dataColumnOffset;

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
// Supports: +, -, *, /, ^(power), parentheses, Col# references
// Constants: Pi, E (Euler's number ~2.71828)
// Scientific notation: 1.5e-3, 2E+10, 3e5
// 
// Column References (Col1 = first column, Col0 = error):
//   LST files: Col1=Year, Col2=DayOfYear, Col3=Hour, Col4=Min, Col5+=Data
//   TXT files: Col1=Year, Col2=Month, Col3=Day, Col4=Hour, Col5=Min, Col6=Sec, Col7+=Data
// 
// Functions:
//   Trig:       Sin(x), Cos(x), Tan(x), Arctan(x), Atan(x), Atan2(y,x)
//   Math:       Sqrt(x), Abs(x), Pow(x,y), Exp(x)
//   Logarithms: Log(x), Ln(x), Log10(x), Log2(x)
//   Rounding:   Floor(x), Ceil(x), Round(x)
//   Comparison: Min(a,b), Max(a,b), Clamp(value,min,max)
// 
// Examples:
//   "Col5^2 + Col6^2"                 - sum of squares for LST data (using ^ operator)
//   "Pow(Col5, 2) + Pow(Col6, 2)"     - sum of squares (using Pow function)
//   "Atan2(Col6, Col5)"               - angle from two components
//   "Sin(Pi * Col5 / 180)"            - sine with degree conversion
//   "Log10(Abs(Col5) + 1)"            - log scale with offset
//   "Clamp(Col5, -10, 10)"            - bounded values
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
	// Col1 = raw index 0 (Year), Col2 = raw index 1, etc.
	// Col0 returns an error
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
		int32 UserColNum = FCString::Atoi(*NumStr);
		
		// Col0 is not allowed - columns start at 1
		if (UserColNum < 1)
		{
			bHasError = true;
			ErrorMsg = TEXT("Column numbers start at 1 (Col1 = first column)");
			return -1;
		}
		
		// Col1 = index 0, Col2 = index 1, etc.
		return UserColNum - 1;
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
	
	// Parse power/exponent operator (^) - higher precedence than * and /
	float ParsePower()
	{
		float Left = ParseUnary();
		
		while (!bHasError)
		{
			SkipWhitespace();
			if (Match('^'))
			{
				float Right = ParseUnary();
				Left = FMath::Pow(Left, Right);
			}
			else
			{
				break;
			}
		}
		
		return Left;
	}
	
	float ParseTerm()
	{
		float Left = ParsePower();
		
		while (!bHasError)
		{
			SkipWhitespace();
			if (Match('*'))
			{
				Left *= ParsePower();
			}
			else if (Match('/'))
			{
				float Right = ParsePower();
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
	
	return outData;
}


FString AIMFWindow::MinutesToTimeString(float minutes) {
	// Handle negative values
	if (minutes < 0) {
		return FString::Printf(TEXT("-%s"), *MinutesToTimeString(-minutes));
	}
	
	int totalMinutes = FMath::RoundToInt(minutes);
	int days = totalMinutes / (24 * 60);
	int remainingMinutes = totalMinutes % (24 * 60);
	int hours = remainingMinutes / 60;
	int mins = remainingMinutes % 60;
	
	// Format as DD:HH:MM if days > 0, otherwise HH:MM
	if (days > 0) {
		return FString::Printf(TEXT("%02d:%02d:%02d"), days, hours, mins);
	}
	else {
		return FString::Printf(TEXT("%02d:%02d"), hours, mins);
	}
}

bool AIMFWindow::TimeStringToMinutes(const FString& timeString, float& outMinutes, FString& outErrorMessage) {
	outMinutes = 0.0f;
	outErrorMessage = TEXT("");
	
	// Trim whitespace
	FString trimmed = timeString.TrimStartAndEnd();
	
	if (trimmed.IsEmpty()) {
		outErrorMessage = TEXT("Time string is empty");
		return false;
	}
	
	// Check for negative sign
	bool isNegative = false;
	if (trimmed.StartsWith(TEXT("-"))) {
		isNegative = true;
		trimmed = trimmed.RightChop(1);
	}
	
	// Count colons to determine format
	int colonCount = 0;
	for (int i = 0; i < trimmed.Len(); i++) {
		if (trimmed[i] == ':') {
			colonCount++;
		}
	}
	
	if (colonCount == 2) {
		// DD:HH:MM format (Day:Hour:Minute)
		int32 firstColon, secondColon;
		trimmed.FindChar(':', firstColon);
		secondColon = trimmed.Find(TEXT(":"), ESearchCase::IgnoreCase, ESearchDir::FromStart, firstColon + 1);
		
		FString daysStr = trimmed.Left(firstColon);
		FString hoursStr = trimmed.Mid(firstColon + 1, secondColon - firstColon - 1);
		FString minsStr = trimmed.RightChop(secondColon + 1);
		
		// Validate all parts are numeric
		if (daysStr.IsEmpty() || hoursStr.IsEmpty() || minsStr.IsEmpty()) {
			outErrorMessage = TEXT("Invalid DD:HH:MM format - missing values");
			return false;
		}
		
		for (int i = 0; i < daysStr.Len(); i++) {
			if (!FChar::IsDigit(daysStr[i])) {
				outErrorMessage = TEXT("Invalid characters in days value");
				return false;
			}
		}
		for (int i = 0; i < hoursStr.Len(); i++) {
			if (!FChar::IsDigit(hoursStr[i])) {
				outErrorMessage = TEXT("Invalid characters in hours value");
				return false;
			}
		}
		for (int i = 0; i < minsStr.Len(); i++) {
			if (!FChar::IsDigit(minsStr[i])) {
				outErrorMessage = TEXT("Invalid characters in minutes value");
				return false;
			}
		}
		
		int days = FCString::Atoi(*daysStr);
		int hours = FCString::Atoi(*hoursStr);
		int mins = FCString::Atoi(*minsStr);
		
		// Validate hours (0-23) and minutes (0-59) are in valid range
		if (hours < 0 || hours > 23) {
			outErrorMessage = FString::Printf(TEXT("Hours must be 0-23 (got %d)"), hours);
			return false;
		}
		if (mins < 0 || mins > 59) {
			outErrorMessage = FString::Printf(TEXT("Minutes must be 0-59 (got %d)"), mins);
			return false;
		}
		
		outMinutes = (float)(days * 24 * 60 + hours * 60 + mins);
	}
	else if (colonCount == 1) {
		// HH:MM format (Hour:Minute)
		int32 colonIndex;
		trimmed.FindChar(':', colonIndex);
		
		FString hoursStr = trimmed.Left(colonIndex);
		FString minsStr = trimmed.RightChop(colonIndex + 1);
		
		// Validate both parts are numeric
		if (hoursStr.IsEmpty() || minsStr.IsEmpty()) {
			outErrorMessage = TEXT("Invalid HH:MM format - missing values");
			return false;
		}
		
		for (int i = 0; i < hoursStr.Len(); i++) {
			if (!FChar::IsDigit(hoursStr[i])) {
				outErrorMessage = TEXT("Invalid characters in hours value");
				return false;
			}
		}
		for (int i = 0; i < minsStr.Len(); i++) {
			if (!FChar::IsDigit(minsStr[i])) {
				outErrorMessage = TEXT("Invalid characters in minutes value");
				return false;
			}
		}
		
		int hours = FCString::Atoi(*hoursStr);
		int mins = FCString::Atoi(*minsStr);
		
		// Validate minutes are in valid range (0-59)
		if (mins < 0 || mins > 59) {
			outErrorMessage = FString::Printf(TEXT("Minutes must be 0-59 (got %d)"), mins);
			return false;
		}
		
		outMinutes = (float)(hours * 60 + mins);
	}
	else if (colonCount == 0) {
		// Try to parse as plain number (minutes)
		// Check if it's a valid number (allow decimal point)
		bool hasDecimal = false;
		for (int i = 0; i < trimmed.Len(); i++) {
			if (trimmed[i] == '.') {
				if (hasDecimal) {
					outErrorMessage = TEXT("Invalid number - multiple decimal points");
					return false;
				}
				hasDecimal = true;
			}
			else if (!FChar::IsDigit(trimmed[i])) {
				outErrorMessage = FString::Printf(TEXT("Invalid character '%c' in number"), trimmed[i]);
				return false;
			}
		}
		
		outMinutes = FCString::Atof(*trimmed);
	}
	else {
		// More than 2 colons is invalid
		outErrorMessage = FString::Printf(TEXT("Invalid format - too many colons (%d)"), colonCount);
		return false;
	}
	
	if (isNegative) {
		outMinutes = -outMinutes;
	}
	
	return true;
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
		
	}
}

void AIMFWindow::RefreshEquationGraphs()
{
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

bool AIMFWindow::IsValidEquation(const FString& equation, FString& outErrorMessage)
{
	outErrorMessage = TEXT("");
	
	// Check for empty equation
	if (equation.IsEmpty())
	{
		outErrorMessage = TEXT("Equation is empty");
		return false;
	}
	
	// Check if we have data to validate column references
	if (imfData.Num() == 0)
	{
		outErrorMessage = TEXT("No data loaded");
		return false;
	}
	
	// Create a test parser with the first row of data to validate the equation
	FExpressionParser TestParser(equation, imfData[0].data);
	float result = TestParser.Evaluate();
	
	// Check for parsing errors
	if (TestParser.bHasError)
	{
		outErrorMessage = TestParser.ErrorMsg;
		return false;
	}
	
	// Check for invalid result (NaN, infinity)
	if (FMath::IsNaN(result) || !FMath::IsFinite(result))
	{
		// This might happen if all values in the first row are placeholders
		// So we should check a few more rows if available
		bool foundValidResult = false;
		for (int i = 1; i < FMath::Min(10, imfData.Num()); i++)
		{
			FExpressionParser RetryParser(equation, imfData[i].data);
			float retryResult = RetryParser.Evaluate();
			if (!RetryParser.bHasError && !FMath::IsNaN(retryResult) && FMath::IsFinite(retryResult))
			{
				foundValidResult = true;
				break;
			}
		}
		
		if (!foundValidResult)
		{
			outErrorMessage = TEXT("Equation produces invalid results (check column references)");
			return false;
		}
	}
	
	return true;
}

float AIMFWindow::EvaluateEquationForRow(const FString& equation, const TArray<float>& rowData, 
	bool& bSuccess, FString& outErrorMessage)
{
	bSuccess = false;
	outErrorMessage = TEXT("");
	
	if (equation.IsEmpty())
	{
		outErrorMessage = TEXT("Equation is empty");
		return 0.0f;
	}
	
	if (rowData.Num() == 0)
	{
		outErrorMessage = TEXT("Row data is empty");
		return 0.0f;
	}
	
	FExpressionParser Parser(equation, rowData);
	float result = Parser.Evaluate();
	
	if (Parser.bHasError)
	{
		outErrorMessage = Parser.ErrorMsg;
		return 0.0f;
	}
	
	if (FMath::IsNaN(result) || !FMath::IsFinite(result))
	{
		outErrorMessage = TEXT("Result is not a valid number");
		return 0.0f;
	}
	
	bSuccess = true;
	return result;
}

void AIMFWindow::RegisterEquationForHover(int key, const FString& equation, const TArray<FLineChain>& chains)
{
	// Check if already registered with same key - just update, don't duplicate
	if (equationGraphs.Contains(key))
	{
		// Already registered, just update the equation string
		equationGraphs[key] = equation;
	}
	else
	{
		// New registration
		equationGraphs.Add(key, equation);
	}
	
	// Combine all chains into one FLineChain for hover detection
	// Add NaN separator between chains to mark segment boundaries
	FLineChain combined;
	bool firstChain = true;
	for (const FLineChain& chain : chains)
	{
		if (chain.points.Num() == 0)
		{
			continue;
		}
		
		// Add NaN separator before each chain (except the first)
		if (!firstChain)
		{
			combined.points.Add(FVector2D(NAN, NAN));
		}
		firstChain = false;
		
		for (const FVector2D& point : chain.points)
		{
			combined.points.Add(point);
		}
		combined.color = chain.color; // Use the last chain's color
	}
	
	if (combined.points.Num() > 0)
	{
		// Use assignment to update existing or add new (prevents duplicates)
		equationGraphLines.FindOrAdd(key) = combined;
	}
}

void AIMFWindow::UnregisterEquationFromHover(int key)
{
	equationGraphs.Remove(key);
	equationGraphLines.Remove(key);
}

void AIMFWindow::ClearAllEquationRegistrations()
{
	equationGraphs.Empty();
	equationGraphLines.Empty();
}

void AIMFWindow::DebugPrintEquationRegistrations()
{
	UE_LOG(LogTemp, Warning, TEXT("=== DEBUG: Equation Registrations ==="));
	UE_LOG(LogTemp, Warning, TEXT("equationGraphs has %d entries:"), equationGraphs.Num());
	for (const auto& pair : equationGraphs)
	{
		UE_LOG(LogTemp, Warning, TEXT("  key=%d, equation=%s"), pair.Key, *pair.Value);
	}
	UE_LOG(LogTemp, Warning, TEXT("equationGraphLines has %d entries:"), equationGraphLines.Num());
	for (const auto& pair : equationGraphLines)
	{
		UE_LOG(LogTemp, Warning, TEXT("  key=%d, points=%d"), pair.Key, pair.Value.points.Num());
	}
	UE_LOG(LogTemp, Warning, TEXT("=== END DEBUG ==="));
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

bool AIMFWindow::GetValueAtMousePosition(float& outTime, float& outValue, FString& outTimeString)
{
	outTime = 0.0f;
	outValue = 0.0f;
	outTimeString = TEXT("");
	
	// Get the player controller
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC)
	{
		return false;
	}
	
	// Get mouse position
	float MouseX, MouseY;
	if (!PC->GetMousePosition(MouseX, MouseY))
	{
		return false;
	}
	
	// Get window corners on screen (unscaled - raw screen coordinates)
	FVector2D bottomLeft, topRight;
	GetWindowCornersOnScreen(bottomLeft, topRight, false);
	
	// Check if mouse is within the graph bounds
	if (MouseX < bottomLeft.X || MouseX > topRight.X ||
		MouseY < topRight.Y || MouseY > bottomLeft.Y)
	{
		return false;
	}
	
	// Calculate normalized position (0-1) within the graph
	float normalizedX = (MouseX - bottomLeft.X) / (topRight.X - bottomLeft.X);
	float normalizedY = (MouseY - topRight.Y) / (bottomLeft.Y - topRight.Y);
	
	// Convert to actual time and value
	// X-axis: time ranges from startTime to endTime
	outTime = startTime + normalizedX * (endTime - startTime);
	
	// Y-axis: value ranges from graphScaleMax (top) to graphScaleMin (bottom)
	outValue = graphScaleMax + normalizedY * (graphScaleMin - graphScaleMax);
	
	// Format time as string
	outTimeString = MinutesToTimeString(outTime);
	
	return true;
}

bool AIMFWindow::GetClosestGraphAtMouse(float& outTime, FString& outTimeString, float& outGraphValue, 
	FString& outGraphName, int& outGraphKey, bool& outIsEquation, FColor& outColor)
{
	// Initialize outputs
	outTime = 0.0f;
	outTimeString = TEXT("");
	outGraphValue = 0.0f;
	outGraphName = TEXT("");
	outGraphKey = -1;
	outIsEquation = false;
	outColor = FColor::White;
	
	// Get the player controller
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC)
	{
		return false;
	}
	
	// Get mouse position (screen coordinates)
	float MouseX, MouseY;
	if (!PC->GetMousePosition(MouseX, MouseY))
	{
		return false;
	}
	
	// Get window corners on screen (unscaled - raw screen coordinates)
	FVector2D bottomLeft, topRight;
	GetWindowCornersOnScreen(bottomLeft, topRight, false);
	
	// Debug: Log mouse position and bounds occasionally
	
	// Check if mouse is within the graph bounds
	if (MouseX < bottomLeft.X || MouseX > topRight.X ||
		MouseY < topRight.Y || MouseY > bottomLeft.Y)
	{
		return false;
	}
	
	// Calculate time at mouse position
	float timeRange = endTime - startTime;
	if (FMath::IsNearlyZero(timeRange))
	{
		return false;
	}
	
	float normalizedX = (MouseX - bottomLeft.X) / (topRight.X - bottomLeft.X);
	outTime = startTime + normalizedX * timeRange;
	outTimeString = MinutesToTimeString(outTime);
	
	// Calculate value range and screen dimensions
	float valueRange = graphScaleMin - graphScaleMax;
	float graphHeight = bottomLeft.Y - topRight.Y;
	float graphWidth = topRight.X - bottomLeft.X;
	
	if (FMath::IsNearlyZero(valueRange) || FMath::IsNearlyZero(graphHeight))
	{
		return false;
	}
	
	// Track closest graph
	float closestScreenDistance = FLT_MAX;
	bool foundGraph = false;
	
	// Tolerance: 30 pixels
	const float maxPixelDistance = 30.0f;
	
	// Helper to convert time to screen X
	auto TimeToScreenX = [&](float time) -> float
	{
		return bottomLeft.X + ((time - startTime) / timeRange) * graphWidth;
	};
	
	// Helper to convert data value to screen Y
	auto ValueToScreenY = [&](float value) -> float
	{
		return topRight.Y + ((value - graphScaleMax) / valueRange) * graphHeight;
	};
	
	// Helper to convert screen Y to data value
	auto ScreenYToValue = [&](float screenY) -> float
	{
		float normalizedY = (screenY - topRight.Y) / graphHeight;
		return graphScaleMax + normalizedY * valueRange;
	};
	
	// Helper to calculate perpendicular distance from point to line segment
	// Returns the distance and the closest point on the segment
	auto DistanceToSegment = [](FVector2D point, FVector2D lineStart, FVector2D lineEnd, FVector2D& closestPoint) -> float
	{
		FVector2D line = lineEnd - lineStart;
		float lineLength = line.Size();
		
		if (lineLength < 0.0001f)
		{
			closestPoint = lineStart;
			return FVector2D::Distance(point, lineStart);
		}
		
		// Project point onto the line, clamping to segment
		float t = FMath::Clamp(FVector2D::DotProduct(point - lineStart, line) / (lineLength * lineLength), 0.0f, 1.0f);
		closestPoint = lineStart + t * line;
		
		return FVector2D::Distance(point, closestPoint);
	};
	
	FVector2D mousePos(MouseX, MouseY);
	
	// === Search through displayed columns by querying imfData directly ===
	for (int col : displayedColumns)
	{
		// Search through all visible line segments for this column
		for (int i = 0; i < imfData.Num() - 1; i++)
		{
			// Check if both points are within visible time range
			float time1 = imfData[i].timeMinutes;
			float time2 = imfData[i + 1].timeMinutes;
			
			// Skip if segment is completely outside visible range
			if (time2 < startTime || time1 > endTime)
			{
				continue;
			}
			
			// Check column bounds
			if (col < 0 || col >= imfData[i].data.Num() || col >= imfData[i + 1].data.Num())
			{
				continue;
			}
			
			float value1 = imfData[i].data[col];
			float value2 = imfData[i + 1].data[col];
			
			// Skip invalid values
			bool invalid1 = FMath::IsNaN(value1) || !FMath::IsFinite(value1) ||
				FMath::IsNearlyEqual(value1, 9999.99f, 0.1f) || FMath::IsNearlyEqual(value1, 999.99f, 0.1f) ||
				FMath::IsNearlyEqual(value1, -9999.99f, 0.1f) || FMath::IsNearlyEqual(value1, -999.99f, 0.1f);
			bool invalid2 = FMath::IsNaN(value2) || !FMath::IsFinite(value2) ||
				FMath::IsNearlyEqual(value2, 9999.99f, 0.1f) || FMath::IsNearlyEqual(value2, 999.99f, 0.1f) ||
				FMath::IsNearlyEqual(value2, -9999.99f, 0.1f) || FMath::IsNearlyEqual(value2, -999.99f, 0.1f);
			
			if (invalid1 || invalid2)
			{
				continue;
			}
			
			// Convert to screen coordinates
			FVector2D screenPoint1(TimeToScreenX(time1), ValueToScreenY(value1));
			FVector2D screenPoint2(TimeToScreenX(time2), ValueToScreenY(value2));
			
			// Calculate distance to this line segment
			FVector2D closestPoint;
			float distance = DistanceToSegment(mousePos, screenPoint1, screenPoint2, closestPoint);
			
			if (distance < closestScreenDistance && distance < maxPixelDistance)
			{
				closestScreenDistance = distance;
				outGraphKey = col;
				outIsEquation = false;
				outGraphName = FString::Printf(TEXT("Col%d"), col + 1); // Display as 1-indexed
				// Calculate the value at the closest point
				float closestTime = startTime + ((closestPoint.X - bottomLeft.X) / graphWidth) * timeRange;
				outTime = closestTime;
				outTimeString = MinutesToTimeString(closestTime);
				outGraphValue = ScreenYToValue(closestPoint.Y);
				outColor = FColor::White;
				foundGraph = true;
			}
		}
	}
	
	// === Search through equation graphs ===
	// Evaluate equations using the same coordinate system as the mouse position (unscaled)
	for (auto& Pair : equationGraphs)
	{
		int eqKey = Pair.Key;
		const FString& equation = Pair.Value;
		
		// Calculate equation values and convert to screen coordinates directly
		// Using the same bottomLeft/topRight we got earlier (unscaled)
		FVector2D prevPoint;
		bool hasPrevPoint = false;
		
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
			
			// Skip invalid results
			if (Parser.bHasError || FMath::IsNaN(result) || !FMath::IsFinite(result))
			{
				hasPrevPoint = false;
				continue;
			}
			
			// Convert to screen coordinates (using unscaled bounds)
			FVector2D currentPoint;
			currentPoint.X = TimeToScreenX(imfData[i].timeMinutes);
			currentPoint.Y = ValueToScreenY(result);
			
			// Check distance to segment from previous point to current point
			if (hasPrevPoint)
			{
				FVector2D closestPoint;
				float distance = DistanceToSegment(mousePos, prevPoint, currentPoint, closestPoint);
				
				if (distance < closestScreenDistance && distance < maxPixelDistance)
				{
					closestScreenDistance = distance;
					outGraphKey = eqKey;
					outIsEquation = true;
					
					// Get color from equationGraphLines if available
					if (equationGraphLines.Contains(eqKey))
					{
						outColor = equationGraphLines[eqKey].color;
					}
					
					// Calculate time and value at closest point
					float closestTime = startTime + ((closestPoint.X - bottomLeft.X) / graphWidth) * timeRange;
					outTime = closestTime;
					outTimeString = MinutesToTimeString(closestTime);
					outGraphValue = ScreenYToValue(closestPoint.Y);
					foundGraph = true;
					outGraphName = equation;
				}
			}
			
			prevPoint = currentPoint;
			hasPrevPoint = true;
		}
	}
	
	if (foundGraph)
	{
		return true;
	}
	
	// If no close graph found, clear outputs
	outGraphKey = -1;
	outGraphName = TEXT("");
	outGraphValue = 0.0f;
	return false;
}

// === Marker System Implementation ===

bool AIMFWindow::AddMarkerAtMousePosition()
{
	float time, value;
	FString timeString;
	
	if (!GetValueAtMousePosition(time, value, timeString))
	{
		return false;
	}
	
	AddMarkerAtTime(time);
	return true;
}

bool AIMFWindow::RemoveMarkerAtMousePosition()
{
	float time, value;
	FString timeString;
	
	if (!GetValueAtMousePosition(time, value, timeString))
	{
		return false;
	}
	
	// Calculate tolerance based on visible time range (about 2% of visible range)
	float toleranceMinutes = (endTime - startTime) * 0.02f;
	
	return RemoveMarkerAtTime(time, toleranceMinutes);
}

void AIMFWindow::AddMarkerAtTime(float timeMinutes)
{
	// Check if marker already exists at this time (within small tolerance)
	for (float existingTime : markerTimes)
	{
		if (FMath::Abs(existingTime - timeMinutes) < 0.1f)
		{
			return; // Marker already exists
		}
	}
	
	markerTimes.Add(timeMinutes);
	markerTimes.Sort();
}

bool AIMFWindow::RemoveMarkerAtTime(float timeMinutes, float toleranceMinutes)
{
	if (markerTimes.Num() == 0)
	{
		return false;
	}
	
	// Find the closest marker
	int closestIndex = -1;
	float closestDistance = FLT_MAX;
	
	for (int i = 0; i < markerTimes.Num(); i++)
	{
		float distance = FMath::Abs(markerTimes[i] - timeMinutes);
		if (distance < closestDistance)
		{
			closestDistance = distance;
			closestIndex = i;
		}
	}
	
	// Check if within tolerance
	if (closestIndex >= 0 && closestDistance <= toleranceMinutes)
	{
		markerTimes.RemoveAt(closestIndex);
		return true;
	}
	
	return false;
}

void AIMFWindow::ClearAllMarkers()
{
	markerTimes.Empty();
}

TArray<float> AIMFWindow::GetMarkerScreenPositions()
{
	TArray<float> screenPositions;
	
	FVector2D bottomLeft, topRight;
	GetWindowCornersOnScreen(bottomLeft, topRight, false);
	
	float graphWidth = topRight.X - bottomLeft.X;
	float timeRange = endTime - startTime;
	
	if (timeRange <= 0)
	{
		return screenPositions;
	}
	
	for (float markerTime : markerTimes)
	{
		// Only include markers within visible range
		if (markerTime >= startTime && markerTime <= endTime)
		{
			float normalizedX = (markerTime - startTime) / timeRange;
			float screenX = bottomLeft.X + normalizedX * graphWidth;
			screenPositions.Add(screenX);
		}
	}
	
	return screenPositions;
}

TArray<FString> AIMFWindow::GetMarkerTimeStrings()
{
	TArray<FString> timeStrings;
	
	for (float markerTime : markerTimes)
	{
		timeStrings.Add(MinutesToTimeString(markerTime));
	}
	
	return timeStrings;
}

// === Displayed Column Tracking ===

void AIMFWindow::AddDisplayedColumn(int col)
{
	if (!displayedColumns.Contains(col))
	{
		displayedColumns.Add(col);
	}
}

void AIMFWindow::RemoveDisplayedColumn(int col)
{
	displayedColumns.Remove(col);
}

void AIMFWindow::ClearDisplayedColumns()
{
	displayedColumns.Empty();
}
