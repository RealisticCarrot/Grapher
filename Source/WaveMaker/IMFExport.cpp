#include "IMFExport.h"
#include "IMFWindow.h"
#include "IMFAxis.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElements.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Styling/CoreStyle.h"
#include "UObject/StrongObjectPtr.h"
#include "RenderingThread.h"
#include "Engine/GameViewportClient.h"

struct FIMFExportText
{
    FString Text;
    FVector2D Position;
    int32 Size = 15;
    FLinearColor Color = FLinearColor::Black;
    bool bBold = false;
};
struct FIMFExportLine
{
    TArray<FVector2f> Points;
    FLinearColor Color = FLinearColor::Black;
    float Thickness = 1;
};

class SIMFExport : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SIMFExport) {} SLATE_END_ARGS()
    void Construct(const FArguments&) {}
    FVector2D Size;
    TArray<FIMFExportText> Labels;
    TArray<FIMFExportLine> Lines;
    virtual FVector2D ComputeDesiredSize(float) const override { return Size; }
    virtual int32 OnPaint(const FPaintArgs&, const FGeometry& G, const FSlateRect&, FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle&, bool) const override
    {
        FSlateDrawElement::MakeBox(Elements, Layer, G.ToPaintGeometry(), FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")), ESlateDrawEffect::None, FLinearColor::White);
        for (const auto& Line : Lines) FSlateDrawElement::MakeLines(Elements, Layer + 1, G.ToPaintGeometry(), Line.Points, ESlateDrawEffect::None, Line.Color, true, Line.Thickness);
        for (const auto& Label : Labels)
        {
            const auto Font = FCoreStyle::GetDefaultFontStyle(Label.bBold ? TEXT("Bold") : TEXT("Regular"), Label.Size);
            FSlateDrawElement::MakeText(Elements, Layer + 2, G.ToPaintGeometry(FVector2f(Size), FSlateLayoutTransform(FVector2f(Label.Position))), Label.Text, Font, ESlateDrawEffect::None, Label.Color);
        }
        return Layer + 2;
    }
};

bool IMFExport::Capture(AIMFWindow* Window, UUserWidget* Menu, float Multiplier, TArray<FColor>& Pixels, int32& Width, int32& Height, FString& Error)
{
    Error.Empty(); Pixels.Empty(); Width = Height = 0;
    if (!IsValid(Window) || !FApp::CanEverRender() || !FSlateApplication::IsInitialized()) { Error = TEXT("A rendering viewport is required to export the graph."); return false; }
    if (!FMath::IsFinite(Multiplier) || Multiplier < 1 || Multiplier > 4) { Error = TEXT("Export resolution multiplier must be between 1 and 4."); return false; }
    Window->EnsurePlotCurrent();
    FVector2D BL, TR; Window->GetWindowCornersOnScreen(BL, TR);
    if (TR.X <= BL.X || BL.Y <= TR.Y || Window->endTime <= Window->startTime || Window->graphScaleMax <= Window->graphScaleMin) { Error = TEXT("The graph needs a valid visible axis range before export."); return false; }
    auto Read = [&](const TCHAR* Name, const FString& Default)
    {
        if (Menu && Menu->WidgetTree) if (auto* Label = Cast<UTextBlock>(Menu->WidgetTree->FindWidget(Name))) if (!Label->GetText().IsEmpty()) return Label->GetText().ToString();
        return Default;
    };
    FVector2D Viewport(1200, 800); if (GEngine && GEngine->GameViewport) GEngine->GameViewport->GetViewportSize(Viewport);
    const double BaseWidth = FMath::Clamp(Viewport.X, 800.0, 2400.0);
    auto Chart = SNew(SIMFExport);
    const auto Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
    auto AddText = [&](const FString& Text, FVector2D Position, int32 Size = 15, FLinearColor Color = FLinearColor::Black, bool Bold = false)
    { Chart->Labels.Add({Text, Position, Size, Color, Bold}); };
    // Wrap by measured glyph width, including long equations without spaces.
    auto Wrapped = [&](const FString& Text, double X, double Y, double Available, int32 Size, FLinearColor Color = FLinearColor::Black, bool Center = false, bool Bold = false)
    {
        const auto Font = FCoreStyle::GetDefaultFontStyle(Bold ? TEXT("Bold") : TEXT("Regular"), Size);
        auto Emit = [&](const FString& Line)
        {
            const double Offset = Center ? FMath::Max(0.0, (Available - Measure->Measure(Line, Font).X) / 2) : 0;
            AddText(Line, FVector2D(X + Offset, Y), Size, Color, Bold); Y += Size + 7;
        };
        TArray<FString> Paragraphs; Text.ParseIntoArrayLines(Paragraphs, false);
        for (const FString& Paragraph : Paragraphs)
        {
            FString Line;
            for (TCHAR C : Paragraph)
            {
                const FString Next = Line + FString::Chr(C);
                if (!Line.IsEmpty() && Measure->Measure(Next, Font).X > Available) { Emit(Line); Line.Empty(); }
                Line.AppendChar(C);
            }
            Emit(Line);
        }
        return Y;
    };
    double Top = Wrapped(Read(TEXT("TitleText"), TEXT("IMF time series")), 35, 24, BaseWidth - 70, 23, FLinearColor::Black, true, true) + 8;
    Top = Wrapped(Read(TEXT("LeftLabel"), TEXT("Value")), 110, Top, BaseWidth - 145, 16) + 12;
    const double Left = 110, Right = BaseWidth - 35, Bottom = Top + FMath::Clamp((Right - Left) * 0.48, 350.0, 850.0);
    auto Line = [&](FVector2D A, FVector2D B, FLinearColor Color, float Thickness = 1)
    { FIMFExportLine L; L.Points = {FVector2f(A), FVector2f(B)}; L.Color = Color; L.Thickness = Thickness; Chart->Lines.Add(MoveTemp(L)); };
    auto Dotted = [&](FVector2D A, FVector2D B, FLinearColor Color, float Thickness, double Dash, double Gap)
    {
        const FVector2D Delta = B - A; const double Length = Delta.Size();
        if (Length <= 0) return;
        const FVector2D Direction = Delta / Length;
        for (double D = 0; D < Length; D += Dash + Gap) Line(A + Direction * D, A + Direction * FMath::Min(D + Dash, Length), Color, Thickness);
    };
    const FLinearColor GridColor(0.35f,0.35f,0.35f,1);
    const int32 NY = FMath::Clamp(Window->gridHorizontalDivisions, 1, 50);
    const auto TimeTicks = IMFAxis::Ticks(Window->startTime, Window->endTime, Right - Left);
    if (Window->bShowGridLines)
    {
        for (const auto& Tick : TimeTicks) { const double X=Left+(Right-Left)*(Tick.Minutes-Window->startTime)/(Window->endTime-Window->startTime); Dotted({X,Top},{X,Bottom},GridColor,1.25f,2,4); }
        for (int32 I=0;I<=NY;++I) { const double Y=Bottom-(Bottom-Top)*I/NY; Dotted({Left,Y},{Right,Y},GridColor,1.25f,2,4); }
    }
    Line({Left,Top},{Left,Bottom},FLinearColor::Black); Line({Left,Bottom},{Right,Bottom},FLinearColor::Black);
    const int32 YTicks = FMath::Min(NY, 10);
    for (int32 I=0;I<=YTicks;++I)
    {
        const double Y=Bottom-(Bottom-Top)*I/YTicks;
        const double V=Window->graphScaleMin+(Window->graphScaleMax-Window->graphScaleMin)*double(I)/YTicks;
        const FString Label=FString::Printf(TEXT("%.6g"),V);
        const double W=Measure->Measure(Label,FCoreStyle::GetDefaultFontStyle(TEXT("Regular"),14)).X;
        AddText(Label,{Left-W-10,Y-9},14); Line({Left-5,Y},{Left,Y},FLinearColor::Black);
    }
    for(const auto& Tick : TimeTicks)
    {
        const double X=Left+(Right-Left)*(Tick.Minutes-Window->startTime)/(Window->endTime-Window->startTime);
        const FString& Label=Tick.Label;
        const double W=Measure->Measure(Label,FCoreStyle::GetDefaultFontStyle(TEXT("Regular"),14)).X;
        AddText(Label,{FMath::Clamp(X-W/2,5.0,BaseWidth-W-5),Bottom+10},14); Line({X,Bottom},{X,Bottom+5},FLinearColor::Black);
    }
    for (const auto& Series : Window->GetPlotSeries()) for (const auto& Chain : Series.Screen)
    {
        FIMFExportLine L; L.Color=FLinearColor(Series.Color); L.Thickness=1.5f;
        for(const FVector2D& P : Chain.points) L.Points.Add(FVector2f(Left+(P.X-BL.X)/(TR.X-BL.X)*(Right-Left),Bottom-(BL.Y-P.Y)/(BL.Y-TR.Y)*(Bottom-Top)));
        if(L.Points.Num()==1) { FVector2f P=L.Points[0]; L.Points={P-FVector2f(1,0),P+FVector2f(1,0)}; L.Thickness=3; }
        if(L.Points.Num()>1) Chart->Lines.Add(MoveTemp(L));
    }
    // Draw markers after the curves so they remain distinct from the dotted grid.
    for (float T : Window->markerTimes)
    {
        if (T < Window->startTime || T > Window->endTime) continue;
        const double X=Left+(Right-Left)*(T-Window->startTime)/(Window->endTime-Window->startTime);
        Dotted({X,Top},{X,Bottom},FLinearColor::Black,2.25f,8,6);
    }
    double Y=Wrapped(Read(TEXT("BottomLabel"),TEXT("Time")),Left,Bottom+42,Right-Left,16,FLinearColor::Black,true)+10;
    for(const FIMFLegendEntry& E : Window->GetLegendEntries())
    {
        Line({Left,Y+10},{Left+24,Y+10},E.Color,2);
        Y=Wrapped(E.UserLabel.IsEmpty()?E.DefaultName:E.UserLabel,Left+34,Y,Right-Left-34,15)+3;
    }
    const double BaseHeight=Y+25;
    Width=FMath::CeilToInt(BaseWidth*Multiplier); Height=FMath::CeilToInt(BaseHeight*Multiplier);
    if(Width>16384||Height>16384||int64(Width)*Height>64000000) { Error=TEXT("Export exceeds 64 million pixels or a 16384-pixel dimension. Reduce the multiplier or shorten labels."); return false; }
    Chart->Size={BaseWidth,BaseHeight};
    FWidgetRenderer* Renderer=new FWidgetRenderer(true,true);
    TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(FVector2D(Width,Height),TF_Bilinear,true));
    if(!Target.IsValid()) { BeginCleanup(Renderer); Error=TEXT("Could not allocate the export render target."); return false; }
    Renderer->DrawWidget(Target.Get(),Chart,Multiplier,FVector2D(Width,Height),0,false);
    FlushRenderingCommands();
    const bool bReadPixels=Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
    BeginCleanup(Renderer);
    if(!bReadPixels || Pixels.Num()!=int64(Width)*Height) { Error=TEXT("Could not read rendered graph pixels."); return false; }
    return true;
}
