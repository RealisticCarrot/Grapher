#include "IMFWindow.h"
#include "IMFImport.h"
#include "IMFAxis.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Misc/MessageDialog.h"

bool AIMFWindow::IsClockExpression(const FString& Expression)
{
    FString S = Expression.TrimStartAndEnd();
    while (S.StartsWith(TEXT("(")) && S.EndsWith(TEXT(")")))
    {
        int32 Depth = 0; bool Entire = true;
        for (int32 I = 0; I < S.Len() - 1; ++I) { if (S[I] == '(') ++Depth; if (S[I] == ')') --Depth; if (!Depth) { Entire = false; break; } }
        if (!Entire) break;
        S = S.Mid(1, S.Len() - 2).TrimStartAndEnd();
    }
    if (!S.StartsWith(TEXT("Clock"), ESearchCase::IgnoreCase)) return false;
    S = S.Mid(5).TrimStartAndEnd();
    if (!S.StartsWith(TEXT("("))) return false;
    int32 Depth = 0;
    for (int32 I = 0; I < S.Len(); ++I)
    {
        if (S[I] == '(') ++Depth;
        if (S[I] == ')') --Depth;
        if (Depth == 0) return I == S.Len() - 1;
    }
    return false;
}

TArray<FLineChain> AIMFWindow::MakeValueSegments(const TArray<FRow>& Rows, const FString& Expression, EAngleMode Mode, double GapSeconds, bool bBreakClockWrap)
{
    TArray<FLineChain> Segments;
    FLineChain Chain;
    auto Flush = [&]() { if (!Chain.points.IsEmpty()) Segments.Add(MoveTemp(Chain)); Chain = FLineChain(); };
    const double HalfTurn = Mode == EAngleMode::Degrees ? 180.0 : UE_DOUBLE_PI;
    for (const FRow& Row : Rows)
    {
        bool Good; FString Error;
        const float Value = EvaluateEquationForRow(Expression, Row.data, Good, Error, Mode);
        if (!Good || !FMath::IsFinite(Row.timeMinutes)) { Flush(); continue; }
        if (!Chain.points.IsEmpty())
        {
            const FVector2D Last = Chain.points.Last();
            const double Step = (Row.timeMinutes - Last.X) * 60.0;
            if (Step <= 0 || (GapSeconds > 0 && Step > GapSeconds + 1e-5) || (bBreakClockWrap && FMath::Abs(Value - Last.Y) > HalfTurn)) Flush();
        }
        Chain.points.Emplace(Row.timeMinutes, Value);
    }
    Flush();
    return Segments;
}

static bool ClipSegment(FVector2D& A, FVector2D& B, double X0, double X1, double Y0, double Y1)
{
    const FVector2D D = B - A;
    double Enter = 0, Leave = 1;
    auto Edge = [&](double P, double Q)
    {
        if (P == 0) return Q >= 0;
        const double R = Q / P;
        if (P < 0) { if (R > Leave) return false; Enter = FMath::Max(Enter, R); }
        else { if (R < Enter) return false; Leave = FMath::Min(Leave, R); }
        return true;
    };
    if (!Edge(-D.X, A.X - X0) || !Edge(D.X, X1 - A.X) || !Edge(-D.Y, A.Y - Y0) || !Edge(D.Y, Y1 - A.Y)) return false;
    B = A + Leave * D; A += Enter * D;
    return true;
}

TArray<FLineChain> AIMFWindow::ProjectSegments(const TArray<FLineChain>& Values) const
{
    TArray<FLineChain> Result;
    if (!FMath::IsFinite(startTime) || !FMath::IsFinite(endTime) || !FMath::IsFinite(graphScaleMin) || !FMath::IsFinite(graphScaleMax) || endTime <= startTime || graphScaleMax <= graphScaleMin) return Result;
    FVector2D BL, TR; const_cast<AIMFWindow*>(this)->GetWindowCornersOnScreen(BL, TR);
    if (TR.X <= BL.X || BL.Y <= TR.Y) return Result;
    auto Project = [&](const FVector2D& P) { return FVector2D(BL.X + (P.X - startTime) / (endTime - startTime) * (TR.X - BL.X), BL.Y - (P.Y - graphScaleMin) / (graphScaleMax - graphScaleMin) * (BL.Y - TR.Y)); };
    for (const FLineChain& ValueChain : Values)
    {
        FLineChain Chain; Chain.color = ValueChain.color;
        auto Flush = [&]() { if (!Chain.points.IsEmpty()) Result.Add(MoveTemp(Chain)); Chain = FLineChain(); Chain.color = ValueChain.color; };
        if (ValueChain.points.Num() == 1)
        {
            const FVector2D P = ValueChain.points[0];
            if (P.X >= startTime && P.X <= endTime && P.Y >= graphScaleMin && P.Y <= graphScaleMax) Chain.points.Add(Project(P));
        }
        for (int32 I = 1; I < ValueChain.points.Num(); ++I)
        {
            FVector2D A = ValueChain.points[I - 1], B = ValueChain.points[I];
            if (!ClipSegment(A, B, startTime, endTime, graphScaleMin, graphScaleMax)) { Flush(); continue; }
            A = Project(A); B = Project(B);
            if (!Chain.points.IsEmpty() && !Chain.points.Last().Equals(A, 0.001)) Flush();
            if (Chain.points.IsEmpty()) Chain.points.Add(A);
            Chain.points.Add(B);
        }
        Flush();
    }
    return Result;
}

FString AIMFWindow::GetColumnDisplayName(int32 Column) const
{
    return ColumnInfo.IsValidIndex(Column) ? ColumnInfo[Column].DisplayName(Column) : FString::Printf(TEXT("Col%d"), Column + 1);
}

void AIMFWindow::SetColumnVisible(int32 Column, bool bVisible, FColor Color)
{
    if (imfData.IsEmpty() || !imfData[0].data.IsValidIndex(Column)) return;
    if (bVisible) { AddDisplayedColumn(Column); RegisterLegendEntry(false, Column, FLinearColor(Color), GetColumnDisplayName(Column)); }
    else { RemoveDisplayedColumn(Column); UnregisterLegendEntry(false, Column); }
    RefreshPlot();
}

void AIMFWindow::SetEquationVisible(int32 Key, bool bVisible, FColor Color)
{
    const FString* Equation = EquationDefinitions.Find(Key);
    if (!Equation) return;
    if (bVisible) { equationGraphs.Add(Key, *Equation); RegisterLegendEntry(true, Key, FLinearColor(Color), *Equation); }
    else { equationGraphs.Remove(Key); UnregisterLegendEntry(true, Key); }
    RefreshPlot();
}

int32 AIMFWindow::AddEquationFromText(FText Text)
{
    FString Error;
    if (!IsValidEquation(Text.ToString(), Error)) { FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Error)); return INDEX_NONE; }
    return AddEquationGraph(Text.ToString());
}

void AIMFWindow::EnsurePlotCurrent()
{
    FVector2D BL, TR; GetWindowCornersOnScreen(BL, TR);
    const FVector4 Ranges(startTime, endTime, graphScaleMin, graphScaleMax);
    if (bPlotDirty || Ranges != LastRanges || !BL.Equals(LastBottomLeft, 0.05) || !TR.Equals(LastTopRight, 0.05) || angleMode != LastAngleMode || bShowGridLines != LastGridVisible) RefreshPlot();
}

void AIMFWindow::RefreshPlot()
{
    PlotSeries.Empty(); graphLines.Empty(); equationGraphLines.Empty();
    GetWindowCornersOnScreen(LastBottomLeft, LastTopRight);
    LastRanges = FVector4(startTime, endTime, graphScaleMin, graphScaleMax);
    LastAngleMode = angleMode; LastGridVisible = bShowGridLines; bPlotDirty = false;
    // Legend registries are the visible-curve state. Rendered segment keys are transient.
    auto Add = [&](bool bEquation, int32 Key, const FString& Expression, FColor Color)
    {
        FIMFPlotSeries Series; Series.bEquation = bEquation; Series.Key = Key; Series.Expression = Expression; Series.Color = Color;
        Series.Values = MakeValueSegments(imfData, Expression, angleMode, MaxGapSeconds, IsClockExpression(Expression));
        for (auto& Chain : Series.Values) Chain.color = Color;
        Series.Screen = ProjectSegments(Series.Values);
        PlotSeries.Add(MoveTemp(Series));
    };
    TArray<int32> Keys; ColumnLegend.GetKeys(Keys); Keys.Sort();
    for (int32 Key : Keys) Add(false, Key, FString::Printf(TEXT("Col%d"), Key + 1), ColumnLegend[Key].Color.ToFColor(true));
    Keys.Empty(); EquationLegend.GetKeys(Keys); Keys.Sort();
    for (int32 Key : Keys) if (const FString* Expression = equationGraphs.Find(Key)) Add(true, Key, *Expression, EquationLegend[Key].Color.ToFColor(true));
    int32 RenderKey = 1;
    for (const auto& Series : PlotSeries) for (const auto& Chain : Series.Screen) graphLines.Add(RenderKey++, Chain);
    if (GetWorld() && GetWorld()->GetFirstPlayerController()) { RefreshGridLines(); RefreshMarkerGraphLines(); }
}

void AIMFWindow::PaintGraph(FPaintContext& Context)
{
    EnsurePlotCurrent();
    TArray<int32> Keys; graphLines.GetKeys(Keys); Keys.Sort(); // grid/markers behind curves
    for (int32 Key : Keys)
    {
        const auto& Chain = graphLines[Key];
        if (Chain.points.Num() == 1)
        {
            const FVector2D P = Chain.points[0];
            UWidgetBlueprintLibrary::DrawLine(Context, P - FVector2D(1, 0), P + FVector2D(1, 0), FLinearColor(Chain.color), true, 3);
        }
        else UWidgetBlueprintLibrary::DrawLines(Context, Chain.points, FLinearColor(Chain.color), true, Key < 0 ? 1.0f : 1.5f);
    }
}

FString AIMFWindow::FormatGraphTime(double Minutes) const
{
    if (TimeOrigin.GetTicks() > 0 && FMath::IsFinite(Minutes))
    {
        const double Seconds = FMath::RoundToDouble(Minutes * 60.0);
        return (TimeOrigin + FTimespan::FromSeconds(Seconds)).ToString(TEXT("%Y-%m-%d %H:%M:%S"));
    }
    return MinutesToTimeString(float(Minutes));
}

FString AIMFWindow::PositionTimeAxisLabel(UUserWidget* LabelWidget, int32 Index, int32 Count)
{
    if (!IsValid(LabelWidget)) return FString();
    FVector2D BL, TR; GetWindowCornersOnScreen(BL, TR);
    const auto Ticks = IMFAxis::Ticks(startTime, endTime, TR.X - BL.X, FMath::Min(Count, 13));
    if (!Ticks.IsValidIndex(Index)) { LabelWidget->SetVisibility(ESlateVisibility::Hidden); return FString(); }
    const auto& Tick = Ticks[Index];
    LabelWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
    LabelWidget->SetPositionInViewport({BL.X + (TR.X - BL.X) * (Tick.Minutes - startTime) / (endTime - startTime), BL.Y}, false);
    if (LabelWidget->WidgetTree)
    {
        if (auto* Text = Cast<UTextBlock>(LabelWidget->WidgetTree->FindWidget(TEXT("TextBlock_49"))))
        {
            auto Font = Text->GetFont(); Font.Size = 16; Text->SetFont(Font);
            Text->SetJustification(ETextJustify::Center);
            if (auto* Slot = Cast<UCanvasPanelSlot>(Text->Slot))
            {
                Slot->SetAnchors(FAnchors(0,0)); Slot->SetAlignment(FVector2D::ZeroVector);
                Slot->SetAutoSize(false); Slot->SetPosition({-85,8}); Slot->SetSize({170,30});
            }
        }
    }
    return Tick.Label;
}

bool AIMFWindow::GetClosestGraphAtMouse(float& outTime, FString& outTimeString, float& outGraphValue, FString& outGraphName, int& outGraphKey, bool& outIsEquation, FColor& outColor)
{
    outTime = 0; outGraphValue = 0; outTimeString.Empty(); outGraphName.Empty(); outGraphKey = INDEX_NONE; outIsEquation = false; outColor = FColor::White;
    APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    float X, Y;
    if (!PC || !UWidgetLayoutLibrary::GetMousePositionScaledByDPI(PC, X, Y)) return false;
    EnsurePlotCurrent();
    const FVector2D BL = LastBottomLeft, TR = LastTopRight, Mouse(X, Y);
    if (TR.X <= BL.X || BL.Y <= TR.Y || X < BL.X || X > TR.X || Y < TR.Y || Y > BL.Y) return false;
    double Best = 30.0; bool Found = false;
    for (const auto& Series : PlotSeries)
    {
        for (const auto& Chain : Series.Screen)
        {
            for (int32 I = 0; I < Chain.points.Num(); ++I)
            {
                const FVector2D A = Chain.points[I], B = Chain.points[FMath::Min(I + 1, Chain.points.Num() - 1)], D = B - A;
                const double T = D.SizeSquared() > 0 ? FMath::Clamp(FVector2D::DotProduct(Mouse - A, D) / D.SizeSquared(), 0.0, 1.0) : 0;
                const FVector2D P = A + T * D;
                const double Distance = FVector2D::Distance(Mouse, P);
                if (Distance > Best) continue;
                Best = Distance; Found = true;
                outTime = startTime + (P.X - BL.X) / (TR.X - BL.X) * (endTime - startTime);
                outGraphValue = graphScaleMin + (BL.Y - P.Y) / (BL.Y - TR.Y) * (graphScaleMax - graphScaleMin);
                outTimeString = FormatGraphTime(outTime);
                outGraphKey = Series.Key; outIsEquation = Series.bEquation; outColor = Series.Color;
                const auto* Legend = (Series.bEquation ? EquationLegend : ColumnLegend).Find(Series.Key);
                outGraphName = Legend ? (Legend->UserLabel.IsEmpty() ? Legend->DefaultName : Legend->UserLabel) : Series.Expression;
                if (T > 0 && T < 1) outGraphName += TEXT(" (interpolated)");
            }
        }
    }
    return Found;
}

void AIMFWindow::ApplyAxisText(FName Axis, FText Text)
{
    float V = 0; double D = 0; FString Error;
    const bool TimeAxis = Axis == TEXT("start") || Axis == TEXT("end");
    if (TimeAxis)
    {
        if (!TimeStringToMinutes(Text.ToString(), V, Error)) { FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Error)); return; }
    }
    else
    {
        if (!IMFImport::ParseNumber(Text.ToString(), D) || !FMath::IsFinite(float(D))) { FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Enter a finite numeric axis limit."))); return; }
        V = float(D);
    }
    float Start = startTime, End = endTime, Min = graphScaleMin, Max = graphScaleMax;
    if (Axis == TEXT("start")) Start = V; else if (Axis == TEXT("end")) End = V; else if (Axis == TEXT("min")) Min = V; else if (Axis == TEXT("max")) Max = V; else return;
    if (Start >= End || Min >= Max) { FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Start must be before end, and minimum must be below maximum. The graph was not changed."))); return; }
    startTime = Start; endTime = End; graphScaleMin = Min; graphScaleMax = Max;
    RefreshPlot();
}

void AIMFWindow::EndPlay(const EEndPlayReason::Type Reason)
{
    ClearLegend();
    OnIMFLegendChanged.Clear();
    for (UUserWidget* Widget : OwnedWidgets) if (IsValid(Widget)) Widget->RemoveFromParent();
    OwnedWidgets.Empty(); PlotSeries.Empty(); graphLines.Empty();
    Super::EndPlay(Reason);
}
