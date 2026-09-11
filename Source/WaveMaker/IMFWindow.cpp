// Fill out your copyright notice in the Description page of Project Settings.


#include "IMFWindow.h"

#include "Viewer.h"
#include "IMFImport.h"
#include "IMFAxis.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/UserWidget.h"

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
    TArray<UUserWidget*> Before;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Before, UUserWidget::StaticClass(), false);
    Super::BeginPlay();
	
	// Clear any leftover state from previous sessions
	equationGraphs.Empty();
	equationGraphLines.Empty();
	displayedColumns.Empty();
	ColumnLegend.Empty();
	EquationLegend.Empty();
	ColumnUserLabels.Empty();
	EquationUserLabels.Empty();
	
	APlayerController* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    AViewer* viewer = Controller ? Cast<AViewer>(Controller->GetPawn()) : nullptr;
    if (!viewer) return;
	imfData = viewer->imfData;
    dataColumnOffset = viewer->dataColumnOffset;
    if (viewer->CurrentIMFImport.IsValid())
    {
        ColumnInfo = viewer->CurrentIMFImport->Profile.Columns;
        TimeOrigin = viewer->CurrentIMFImport->Origin;
        SourceDescription = viewer->CurrentIMFImport->Profile.Source;
        MaxGapSeconds = viewer->CurrentIMFImport->GapSeconds;
    }

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

    if (endTime <= startTime) endTime = startTime + 1;
    CreateIMFGraphWidgets();
    TArray<UUserWidget*> After;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, After, UUserWidget::StaticClass(), false);
    for (UUserWidget* Widget : After) if (!Before.Contains(Widget)) OwnedWidgets.Add(Widget);
    bPlotDirty = true;
}

// Called every frame
void AIMFWindow::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    EnsurePlotCurrent();
}


TArray<FVector2D> AIMFWindow::GetDrawPointsByColumn(int col)
{
    currentColumnSegments = GetDrawSegmentsByColumn(col);
    TArray<FVector2D> Points;
    for (const FLineChain& Chain : currentColumnSegments) Points.Append(Chain.points);
    return Points;
}
TArray<FLineChain> AIMFWindow::GetDrawSegmentsByColumn(int col)
{
    return ProjectSegments(MakeValueSegments(imfData, FString::Printf(TEXT("Col%d"), col + 1), angleMode, MaxGapSeconds, false));
}
TArray<FLineChain> AIMFWindow::GetDrawPointsForArcTan(int colX, int colY)
{
    return GetDrawPointsForEquation(FString::Printf(TEXT("Atan2(Col%d,Col%d)"), colY + 1, colX + 1));
}

struct FExpressionParser
{
	const FString& Expr;
	int32 Pos;
	const TArray<float>& RowData;
	bool bHasError;
	FString ErrorMsg;
	bool bDegreeMode;
	bool bSyntaxOnly = false;
	int32 Depth = 0;
	
	FExpressionParser(const FString& InExpr, const TArray<float>& InRowData, bool bInDegreeMode = false)
		: Expr(InExpr), Pos(0), RowData(InRowData), bHasError(false), bDegreeMode(bInDegreeMode) {}
	
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
	bool IsInvalidValue(double Value) { return !bSyntaxOnly && !FMath::IsFinite(Value); }
    double ParseNumber()
    {
        SkipWhitespace();
        const int32 Start = Pos;
        while (Pos < Expr.Len() && FChar::IsDigit(Expr[Pos])) ++Pos;
        if (Pos < Expr.Len() && Expr[Pos] == '.') { ++Pos; while (Pos < Expr.Len() && FChar::IsDigit(Expr[Pos])) ++Pos; }
        if (Pos < Expr.Len() && (Expr[Pos] == 'e' || Expr[Pos] == 'E'))
        {
            ++Pos;
            if (Pos < Expr.Len() && (Expr[Pos] == '+' || Expr[Pos] == '-')) ++Pos;
            while (Pos < Expr.Len() && FChar::IsDigit(Expr[Pos])) ++Pos;
        }
        double Value = 0;
        if (!IMFImport::ParseNumber(Expr.Mid(Start, Pos - Start), Value))
        { bHasError = true; ErrorMsg = TEXT("Expected a valid finite number, including digits after an exponent."); }
        return Value;
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
	
	double ParsePrimary()
	{
		SkipWhitespace();
		
		// Try function calls first
		// Clock(By, Bz) - IMF clock angle (0-360 deg or 0-2pi rad), measured from +Z toward +Y
		if (!TryMatchFunction(TEXT("Clock")).IsEmpty())
		{
			double By = ParseExpression();
			if (!Match(','))
			{
				bHasError = true;
				ErrorMsg = TEXT("Clock requires two arguments: Clock(By, Bz)");
				return 0.0;
			}
			double Bz = ParseExpression();
			if (!Match(')'))
			{
				bHasError = true;
				ErrorMsg = TEXT("Expected closing parenthesis ')' after Clock");
			}
			if (By == 0.0 && Bz == 0.0)
			{
				return NAN;
			}
			double Theta = FMath::Atan2(By, Bz);
			if (Theta < 0.0) Theta += 2.0 * PI;
			return bDegreeMode ? FMath::RadiansToDegrees(Theta) : Theta;
		}
		
		// Atan2(y, x) - two argument arctangent
		if (!TryMatchFunction(TEXT("Atan2")).IsEmpty())
		{
			double Arg1 = ParseExpression();
			if (!Match(','))
			{
				bHasError = true;
				ErrorMsg = TEXT("Atan2 requires two arguments: Atan2(y, x)");
				return 0.0;
			}
			double Arg2 = ParseExpression();
			if (!Match(')'))
			{
				bHasError = true;
				ErrorMsg = TEXT("Expected closing parenthesis ')' after Atan2");
			}
			double Result = FMath::Atan2(Arg1, Arg2);
			return bDegreeMode ? FMath::RadiansToDegrees(Result) : Result;
		}
		
		// Arctan(x) - single argument arctangent
		if (!TryMatchFunction(TEXT("Arctan")).IsEmpty())
		{
			double Arg = ParseExpression();
			if (!Match(')'))
			{
				bHasError = true;
				ErrorMsg = TEXT("Expected closing parenthesis ')' after Arctan");
			}
			double Result = FMath::Atan(Arg);
			return bDegreeMode ? FMath::RadiansToDegrees(Result) : Result;
		}
		
		// Sqrt(x) - square root
		if (!TryMatchFunction(TEXT("Sqrt")).IsEmpty())
		{
			double Arg = ParseExpression();
			if (!Match(')'))
			{
				bHasError = true;
				ErrorMsg = TEXT("Expected closing parenthesis ')' after Sqrt");
			}
			if (!bSyntaxOnly && Arg < 0.0)
			{
				bHasError = true;
				ErrorMsg = TEXT("Square root of negative number");
				return 0.0;
			}
			return FMath::Sqrt(Arg);
		}
		
		// Abs(x) - absolute value
		if (!TryMatchFunction(TEXT("Abs")).IsEmpty())
		{
			double Arg = ParseExpression();
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
			double Arg = ParseExpression();
			if (!Match(')'))
			{
				bHasError = true;
				ErrorMsg = TEXT("Expected closing parenthesis ')' after Sin");
			}
			return FMath::Sin(bDegreeMode ? FMath::DegreesToRadians(Arg) : Arg);
		}
		
		// Cos(x) - cosine
		if (!TryMatchFunction(TEXT("Cos")).IsEmpty())
		{
			double Arg = ParseExpression();
			if (!Match(')'))
			{
				bHasError = true;
				ErrorMsg = TEXT("Expected closing parenthesis ')' after Cos");
			}
			return FMath::Cos(bDegreeMode ? FMath::DegreesToRadians(Arg) : Arg);
		}
		
		// Tan(x) - tangent
		if (!TryMatchFunction(TEXT("Tan")).IsEmpty())
		{
			double Arg = ParseExpression();
			if (!Match(')'))
			{
				bHasError = true;
				ErrorMsg = TEXT("Expected closing parenthesis ')' after Tan");
			}
			return FMath::Tan(bDegreeMode ? FMath::DegreesToRadians(Arg) : Arg);
		}
		
		// Pow(x, y) - power function (x^y)
		if (!TryMatchFunction(TEXT("Pow")).IsEmpty())
		{
			double Base = ParseExpression();
			if (!Match(','))
			{
				bHasError = true;
				ErrorMsg = TEXT("Pow requires two arguments: Pow(base, exponent)");
				return 0.0;
			}
			double Exponent = ParseExpression();
			if (!Match(')'))
			{
				bHasError = true;
				ErrorMsg = TEXT("Expected closing parenthesis ')' after Pow");
			}
			return FMath::Pow(Base, Exponent);
		}
		
		// Ln(x) - natural logarithm
		if (!TryMatchFunction(TEXT("Ln")).IsEmpty())
		{
			double Arg = ParseExpression();
			if (!Match(')'))
			{
				bHasError = true;
				ErrorMsg = TEXT("Expected closing parenthesis ')' after Ln");
			}
			if (!bSyntaxOnly && Arg <= 0.0)
			{
				bHasError = true;
				ErrorMsg = TEXT("Logarithm of non-positive number");
				return 0.0;
			}
			return FMath::Loge(Arg);
		}
		
		// Log10(x) - base-10 logarithm
		if (!TryMatchFunction(TEXT("Log10")).IsEmpty())
		{
			double Arg = ParseExpression();
			if (!Match(')'))
			{
				bHasError = true;
				ErrorMsg = TEXT("Expected closing parenthesis ')' after Log10");
			}
			if (!bSyntaxOnly && Arg <= 0.0)
			{
				bHasError = true;
				ErrorMsg = TEXT("Logarithm of non-positive number");
				return 0.0;
			}
			return FMath::LogX(10.0, Arg);
		}
		
		// Log2(x) - base-2 logarithm
		if (!TryMatchFunction(TEXT("Log2")).IsEmpty())
		{
			double Arg = ParseExpression();
			if (!Match(')'))
			{
				bHasError = true;
				ErrorMsg = TEXT("Expected closing parenthesis ')' after Log2");
			}
			if (!bSyntaxOnly && Arg <= 0.0)
			{
				bHasError = true;
				ErrorMsg = TEXT("Logarithm of non-positive number");
				return 0.0;
			}
			return FMath::Log2(Arg);
		}
		
		// Floor(x) - round down
		if (!TryMatchFunction(TEXT("Floor")).IsEmpty())
		{
			double Arg = ParseExpression();
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
			double Arg = ParseExpression();
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
			double Arg = ParseExpression();
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
			double Arg1 = ParseExpression();
			if (!Match(','))
			{
				bHasError = true;
				ErrorMsg = TEXT("Min requires two arguments: Min(a, b)");
				return 0.0;
			}
			double Arg2 = ParseExpression();
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
			double Arg1 = ParseExpression();
			if (!Match(','))
			{
				bHasError = true;
				ErrorMsg = TEXT("Max requires two arguments: Max(a, b)");
				return 0.0;
			}
			double Arg2 = ParseExpression();
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
			double Value = ParseExpression();
			if (!Match(','))
			{
				bHasError = true;
				ErrorMsg = TEXT("Clamp requires three arguments: Clamp(value, min, max)");
				return 0.0;
			}
			double MinVal = ParseExpression();
			if (!Match(','))
			{
				bHasError = true;
				ErrorMsg = TEXT("Clamp requires three arguments: Clamp(value, min, max)");
				return 0.0;
			}
			double MaxVal = ParseExpression();
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
			double Result = ParseExpression();
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
				return 0.0;
			}
			
			double Value = RowData[ColNum];
			if (IsInvalidValue(Value))
			{
				bHasError = true;
				ErrorMsg = FString::Printf(TEXT("Column %d has invalid/placeholder value"), ColNum);
				return 0.0;
			}
			return bSyntaxOnly ? 1.0 : Value;
		}
		Pos = SavedPos; // Reset if not a column ref
		
		// Try constants (case-insensitive, longer names checked first)
		SkipWhitespace();
		
		// Helper: match a constant name (case-insensitive) that isn't part of a longer identifier
		auto TryMatchConstant = [&](const TCHAR* Name, int32 Len, double Value) -> bool
		{
			if (Pos + Len <= Expr.Len())
			{
				FString Candidate = Expr.Mid(Pos, Len);
				if (Candidate.Equals(Name, ESearchCase::IgnoreCase))
				{
					if (Pos + Len >= Expr.Len() || !FChar::IsAlnum(Expr[Pos + Len]))
					{
						Pos += Len;
						return true;
					}
				}
			}
			return false;
		};
		
		// Physics constants (SI units: kg, m, s, C, etc.)
		if (TryMatchConstant(TEXT("Eps0"), 4, 0.0))  return 8.8541878128e-12;   // permittivity of free space (F/m)
		if (TryMatchConstant(TEXT("Mu0"),  3, 0.0))   return 1.25663706212e-6;   // permeability of free space (H/m)
		if (TryMatchConstant(TEXT("Me"),   2, 0.0))    return 9.1093837015e-31;   // electron mass (kg)
		if (TryMatchConstant(TEXT("Mp"),   2, 0.0))    return 1.67262192369e-27;  // proton mass (kg)
		if (TryMatchConstant(TEXT("Qe"),   2, 0.0))    return 1.602176634e-19;    // elementary charge (C)
		
		// Mathematical constants
		if (TryMatchConstant(TEXT("Pi"),   2, 0.0))    return PI;
		
		if (Pos + 1 <= Expr.Len())
		{
			TCHAR c = Expr[Pos];
			if (c == 'E' || c == 'e')
			{
				// Make sure it's not part of a longer identifier or scientific notation
				if (Pos + 1 >= Expr.Len() || (!FChar::IsAlnum(Expr[Pos + 1]) && Expr[Pos + 1] != '_'))
				{
					Pos += 1;
					return UE_EULERS_NUMBER; // e ≈ 2.71828
				}
			}
		}
		
		// Parse number
		return ParseNumber();
	}
	
    double ParseUnary()
    {
        TGuardValue<int32> Guard(Depth, Depth + 1);
        if (Depth > 128) { bHasError = true; ErrorMsg = TEXT("Expression nesting is too deep."); return 0; }
        if (Match('-')) return -ParseUnary();
        if (Match('+')) return ParseUnary();
        return ParsePower();
    }
    double ParsePower()
    {
        double Left = ParsePrimary();
        if (!bHasError && Match('^')) Left = FMath::Pow(Left, ParseUnary());
        return Left;
    }
    double ParseTerm()
    {
        double Left = ParseUnary();
        while (!bHasError)
        {
            if (Match('*')) Left *= ParseUnary();
            else if (Match('/'))
            {
                double Right = ParseUnary();
                if (!bSyntaxOnly && Right == 0.0) { bHasError = true; ErrorMsg = TEXT("Division by zero"); return 0; }
                Left = bSyntaxOnly ? 1.0 : Left / Right;
            }
            else break;
        }
        return Left;
    }
    

	double ParseExpression()
	{
        TGuardValue<int32> Guard(Depth, Depth + 1);
        if (Depth > 128) { bHasError = true; ErrorMsg = TEXT("Expression nesting is too deep."); return 0; }
		double Left = ParseTerm();
		
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
	
	double Evaluate()
	{
		if (Expr.Len() > 4096) { bHasError = true; ErrorMsg = TEXT("Expression exceeds 4096 characters."); return 0; }
        double Result = ParseExpression();
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
    return ProjectSegments(MakeValueSegments(imfData, equation, angleMode, MaxGapSeconds, IsClockExpression(equation)));
}

FString AIMFWindow::MinutesToTimeString(float minutes)
{
    if (!FMath::IsFinite(minutes)) return TEXT("Invalid time");
    const int64 Total = FMath::RoundToInt64(FMath::Abs(double(minutes)) * 60.0);
    const int64 Days = Total / 86400, Hours = (Total / 3600) % 24, Mins = (Total / 60) % 60, Secs = Total % 60;
    FString S;
    if (Secs) S = FString::Printf(TEXT("%02lld:%02lld:%02lld:%02lld"), Days, Hours, Mins, Secs);
    else if (Days) S = FString::Printf(TEXT("%02lld:%02lld:%02lld"), Days, Hours, Mins);
    else S = FString::Printf(TEXT("%02lld:%02lld"), Hours, Mins);
    return minutes < 0 ? TEXT("-") + S : S;
}
bool AIMFWindow::TimeStringToMinutes(const FString& timeString, float& outMinutes, FString& outErrorMessage)
{
    FString S = timeString.TrimStartAndEnd(); outMinutes = 0; outErrorMessage.Empty();
    double Sign = 1;
    if (S.StartsWith(TEXT("-"))) { Sign = -1; S.RightChopInline(1); }
    TArray<FString> Parts; S.ParseIntoArray(Parts, TEXT(":"), false);
    auto Fail = [&]() { outErrorMessage = TEXT("Use minutes, HH:MM, DD:HH:MM or DD:HH:MM:SS (0–23 hours and 0–59 minutes/seconds in day format)."); return false; };
    if (Parts.Num() < 1 || Parts.Num() > 4) return Fail();
    double Values[4] = {};
    for (int32 I = 0; I < Parts.Num(); ++I)
    {
        if (!IMFImport::ParseNumber(Parts[I], Values[I]) || Values[I] < 0) return Fail();
        if (Parts.Num() > 1 && FMath::FloorToDouble(Values[I]) != Values[I]) return Fail();
    }
    double Minutes = Values[0];
    if (Parts.Num() == 2) { if (Values[1] >= 60) return Fail(); Minutes = Values[0] * 60 + Values[1]; }
    if (Parts.Num() >= 3) { if (Values[1] >= 24 || Values[2] >= 60 || Values[3] >= 60) return Fail(); Minutes = Values[0] * 1440 + Values[1] * 60 + Values[2] + Values[3] / 60; }
    if (!FMath::IsFinite(Minutes) || Minutes > 5258964960.0) return Fail();
    outMinutes = float(Sign * Minutes); return true;
}


int AIMFWindow::AddEquationGraph(const FString& equation)
{
    FString Error; if (!IsValidEquation(equation, Error)) return INDEX_NONE;
    const int32 Key = NextEquationKey++;
    EquationDefinitions.Add(Key, equation);
    SetEquationVisible(Key, true, FColor::MakeRandomColor());
    return Key;
}
void AIMFWindow::RemoveEquationGraph(int key)
{
    SetEquationVisible(key, false, FColor::White);
    EquationDefinitions.Remove(key); EquationUserLabels.Remove(key);
}
void AIMFWindow::RefreshEquationGraphs() { RefreshPlot(); }
TArray<FLineChain> AIMFWindow::GetEquationGraphLines(int key)
{
    if (const FString* Equation = equationGraphs.Find(key)) return GetDrawPointsForEquation(*Equation);
    return {};
}


bool AIMFWindow::IsValidEquation(const FString& equation, FString& outErrorMessage)
{
    outErrorMessage.Empty();
    if (imfData.IsEmpty()) { outErrorMessage = TEXT("No data loaded"); return false; }
    FExpressionParser Parser(equation, imfData[0].data, angleMode == EAngleMode::Degrees);
    Parser.bSyntaxOnly = true;
    Parser.Evaluate();
    outErrorMessage = Parser.ErrorMsg;
    return !Parser.bHasError;
}


float AIMFWindow::EvaluateEquationForRow(const FString& equation, const TArray<float>& rowData, 
	bool& bSuccess, FString& outErrorMessage, EAngleMode inAngleMode)
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
	
	FExpressionParser Parser(equation, rowData, inAngleMode == EAngleMode::Degrees);
	float result = float(Parser.Evaluate());
	
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
    EquationDefinitions.Add(key, equation); equationGraphs.Add(key, equation); bPlotDirty = true;
}
void AIMFWindow::UnregisterEquationFromHover(int key) { equationGraphs.Remove(key); bPlotDirty = true; }
void AIMFWindow::ClearAllEquationRegistrations() { equationGraphs.Empty(); EquationLegend.Empty(); bPlotDirty = true; }
void AIMFWindow::DebugPrintEquationRegistrations() { UE_LOG(LogTemp, Log, TEXT("Visible IMF equations: %d"), equationGraphs.Num()); }


void AIMFWindow::GetWindowCornersOnScreen(FVector2D &bottomLeft, FVector2D &topRight, bool scaled) {
    bottomLeft = FVector2D::ZeroVector; topRight = FVector2D::ZeroVector;
    if (!GetWorld() || !GetWorld()->GetFirstPlayerController()) return;

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

FUIPositions AIMFWindow::GetUIPositions() const
{
	FUIPositions Pos;

	FVector origin;
	FVector extent;
	GetActorBounds(true, origin, extent);

	float scale = UWidgetLayoutLibrary::GetViewportScale(GetWorld());

	FVector2D bottomLeft, topRight;
	UGameplayStatics::ProjectWorldToScreen(GetWorld()->GetFirstPlayerController(), origin - extent, bottomLeft);
	UGameplayStatics::ProjectWorldToScreen(GetWorld()->GetFirstPlayerController(), origin + extent, topRight);

	bottomLeft /= scale;
	topRight /= scale;

	float left   = bottomLeft.X;
	float right  = topRight.X;
	float top    = topRight.Y;
	float bottom = bottomLeft.Y;

	Pos.TitlePosition       = FVector2D((left + right) * 0.5f, top - 30.0f);
	Pos.LeftLabelPosition   = FVector2D(left - 60.0f, (top + bottom) * 0.5f);
	Pos.BottomLabelPosition = FVector2D((left + right) * 0.5f, bottom + 50.0f);
	Pos.LegendPosition      = FVector2D(right + 10.0f, top + 10.0f);
	Pos.GraphSize            = FVector2D(right - left, bottom - top);

	return Pos;
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
	RefreshMarkerGraphLines();
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
		RefreshMarkerGraphLines();
		return true;
	}
	
	return false;
}

void AIMFWindow::ClearAllMarkers()
{
	markerTimes.Empty();
	RefreshMarkerGraphLines();
}

void AIMFWindow::RefreshMarkerGraphLines()
{
	// Remove old marker entries from graphLines (keys -10000 to -19999 only, not grid lines)
	TArray<int> keysToRemove;
	for (auto& pair : graphLines)
	{
		if (pair.Key <= -10000 && pair.Key > -20000)
		{
			keysToRemove.Add(pair.Key);
		}
	}
	for (int k : keysToRemove)
	{
		graphLines.Remove(k);
	}
	
	// Get current screen-space graph bounds (scaled, matching GetDrawPointsByColumn)
	FVector2D bottomLeft, topRight;
	GetWindowCornersOnScreen(bottomLeft, topRight);
	
	float timeRange = endTime - startTime;
	if (FMath::IsNearlyZero(timeRange))
	{
		return;
	}
	
	// Add dashed vertical lines in graphLines for each marker
	int keyCounter = 0;
	const float dashLength = 8.0f;   // pixels per dash
	const float gapLength = 6.0f;    // pixels per gap
	const float stepSize = dashLength + gapLength;
	
	for (int i = 0; i < markerTimes.Num(); i++)
	{
		float markerTime = markerTimes[i];
		
		// Only include markers within visible range
		if (markerTime >= startTime && markerTime <= endTime)
		{
			float normalizedX = (markerTime - startTime) / timeRange;
			float screenX = bottomLeft.X + normalizedX * (topRight.X - bottomLeft.X);
			
			float lineTop = topRight.Y;
			float lineBottom = bottomLeft.Y;
			
			// Create individual dash segments
			for (float y = lineTop; y < lineBottom; y += stepSize)
			{
				float dashEnd = FMath::Min(y + dashLength, lineBottom);
				
				FLineChain dashSegment;
				dashSegment.points.Add(FVector2D(screenX, y));
				dashSegment.points.Add(FVector2D(screenX, dashEnd));
				dashSegment.color = FColor::Black;
				
				graphLines.Add(-10000 - keyCounter, dashSegment);
				keyCounter++;
			}
		}
	}
}

void AIMFWindow::RefreshGridLines()
{
	// Remove old grid entries from graphLines (keys <= -20000 and > -30000)
	TArray<int> keysToRemove;
	for (auto& pair : graphLines)
	{
		if (pair.Key <= -20000 && pair.Key > -30000)
		{
			keysToRemove.Add(pair.Key);
		}
	}
	for (int k : keysToRemove)
	{
		graphLines.Remove(k);
	}

	if (!bShowGridLines)
	{
		return;
	}

	// Get current screen-space graph bounds
	FVector2D bottomLeft, topRight;
	GetWindowCornersOnScreen(bottomLeft, topRight);

	float timeRange = endTime - startTime;
	float scaleRange = graphScaleMin - graphScaleMax;

	if (FMath::IsNearlyZero(timeRange) || FMath::IsNearlyZero(scaleRange))
	{
		return;
	}

	int keyCounter = 0;

	// Horizontal grid lines (matching Y-axis ticks)
	if (gridHorizontalDivisions > 0)
	{
		for (int i = 0; i <= gridHorizontalDivisions; i++)
		{
			float normalizedY = (float)i / (float)gridHorizontalDivisions;
			float screenY = topRight.Y + normalizedY * (bottomLeft.Y - topRight.Y);

			FLineChain gridLine;
			gridLine.points.Add(FVector2D(bottomLeft.X, screenY));
			gridLine.points.Add(FVector2D(topRight.X, screenY));
			gridLine.color = gridLineColor;

			graphLines.Add(-20000 - keyCounter, gridLine);
			keyCounter++;
		}
	}

	// Vertical grid lines (matching X-axis ticks)
	if (gridVerticalDivisions > 0)
	{
		for (const auto& Tick : IMFAxis::Ticks(startTime, endTime, topRight.X - bottomLeft.X))
		{
			float normalizedX = (Tick.Minutes - startTime) / timeRange;
			float screenX = bottomLeft.X + normalizedX * (topRight.X - bottomLeft.X);

			FLineChain gridLine;
			gridLine.points.Add(FVector2D(screenX, topRight.Y));
			gridLine.points.Add(FVector2D(screenX, bottomLeft.Y));
			gridLine.color = gridLineColor;

			graphLines.Add(-20000 - keyCounter, gridLine);
			keyCounter++;
		}
	}
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

// === Legend System Implementation ===

void AIMFWindow::RegisterLegendEntry(bool bIsEquation, int32 Key, FLinearColor Color, const FString& DefaultName)
{
	FIMFLegendEntry Entry;
	Entry.bIsEquation = bIsEquation;
	Entry.Key = Key;
	Entry.Color = Color;
	Entry.DefaultName = DefaultName.IsEmpty() ? (bIsEquation ? EquationDefinitions.FindRef(Key) : GetColumnDisplayName(Key)) : DefaultName;

	// Restore persisted user label if one exists
	if (bIsEquation)
	{
		if (const FString* Saved = EquationUserLabels.Find(Key))
		{
			Entry.UserLabel = *Saved;
		}
		EquationLegend.Add(Key, Entry);
	}
	else
	{
		if (const FString* Saved = ColumnUserLabels.Find(Key))
		{
			Entry.UserLabel = *Saved;
		}
		ColumnLegend.Add(Key, Entry);
	}

	bPlotDirty = true;
	OnIMFLegendChanged.Broadcast();
}

void AIMFWindow::UnregisterLegendEntry(bool bIsEquation, int32 Key)
{
	if (bIsEquation)
	{
		EquationLegend.Remove(Key);
	}
	else
	{
		ColumnLegend.Remove(Key);
	}

	bPlotDirty = true;
	OnIMFLegendChanged.Broadcast();
}

void AIMFWindow::SetLegendUserLabel(bool bIsEquation, int32 Key, const FString& UserLabel)
{
	// Persist the label so it survives toggle off/on
	if (bIsEquation)
	{
		EquationUserLabels.Add(Key, UserLabel);
		if (FIMFLegendEntry* Entry = EquationLegend.Find(Key))
		{
			Entry->UserLabel = UserLabel;
		}
	}
	else
	{
		ColumnUserLabels.Add(Key, UserLabel);
		if (FIMFLegendEntry* Entry = ColumnLegend.Find(Key))
		{
			Entry->UserLabel = UserLabel;
		}
	}
}

TArray<FIMFLegendEntry> AIMFWindow::GetLegendEntries() const
{
	TArray<FIMFLegendEntry> Result;

	for (const auto& Pair : ColumnLegend)
	{
		FIMFLegendEntry Entry = Pair.Value;
        if (!Entry.UserLabel.IsEmpty()) Entry.DefaultName = Entry.UserLabel;
        Result.Add(Entry);
	}
	for (const auto& Pair : EquationLegend)
	{
		FIMFLegendEntry Entry = Pair.Value;
        if (!Entry.UserLabel.IsEmpty()) Entry.DefaultName = Entry.UserLabel;
        Result.Add(Entry);
	}

	// Sort by DefaultName for stable ordering
	Result.Sort([](const FIMFLegendEntry& A, const FIMFLegendEntry& B)
	{
		return A.DefaultName < B.DefaultName;
	});

	return Result;
}

void AIMFWindow::ClearLegend()
{
	ColumnLegend.Empty();
	EquationLegend.Empty();
	ColumnUserLabels.Empty();
	EquationUserLabels.Empty();

	bPlotDirty = true;
	OnIMFLegendChanged.Broadcast();
}
