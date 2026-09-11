#include "Viewer.h"
#include "IMFWindow.h"
#include "IMFImport.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"
#include "Components/TextBlock.h"
#include "Components/EditableTextBox.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Blueprint/WidgetTree.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "ImageUtils.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "Async/Async.h"
#include "Engine/GameViewportClient.h"
#include "HAL/FileManager.h"
#include "TimerManager.h"

bool AViewer::LoadIMFFile(const FString& Filename, bool bShowPreview, FString& Error)
{
    Error.Empty(); FString Text;
    if (!FFileHelper::LoadFileToString(Text, *Filename)) { Error = TEXT("Could not read file. The current graph was kept."); return false; }
    FIMFImportResult Import;
    if (bShowPreview) { if (!IMFImport::Preview(Filename, Text, Import)) return false; }
    else
    {
        auto Profile = IMFImport::DefaultProfile(Filename, Text); IMFImport::LoadProfile(Filename, Profile);
        if (!IMFImport::Parse(Text, Profile, Import, Error)) return false;
    }
    if (!GetWorld() || !imfWindowClass) { Error = TEXT("IMF window class or world is unavailable. The current graph was kept."); return false; }
    // Allocate before replacing the old graph. BeginPlay waits until the new data is assigned.
    AIMFWindow* NewWindow = GetWorld()->SpawnActorDeferred<AIMFWindow>(imfWindowClass, FTransform::Identity, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!NewWindow) { Error = TEXT("Could not create the replacement IMF window. The current graph was kept."); return false; }
    ResetLoadedWindows();
    CurrentIMFImport = MakeShared<FIMFImportResult>(MoveTemp(Import));
    imfData = CurrentIMFImport->Rows;
    currentFileFormat = CurrentIMFImport->Profile.DayOfYear ? EDataFileFormat::LST : EDataFileFormat::TXT;
    dataColumnOffset = currentFileFormat == EDataFileFormat::LST ? 4 : 6;
    imfWindow = NewWindow;
    UGameplayStatics::FinishSpawningActor(NewWindow, FTransform::Identity);
    OnIMFWindowSpawned();
    return true;
}

float AViewer::AverageWithMode(const FString& Equation, const FString& Start, const FString& End, bool bCircular, FString& Error)
{
    Error.Empty();
    if (imfData.IsEmpty()) { Error = TEXT("No data loaded."); return 0; }
    float From, To;
    if (!AIMFWindow::TimeStringToMinutes(Start, From, Error) || !AIMFWindow::TimeStringToMinutes(End, To, Error)) return 0;
    if (From > To) { Error = TEXT("Start must not be after end."); return 0; }
    const EAngleMode Mode = imfWindow ? imfWindow->angleMode : EAngleMode::Radians;
    double Sum = 0, Compensation = 0, SinSum = 0, CosSum = 0;
    int32 Count = 0;
    for (const FRow& Row : imfData)
    {
        if (Row.timeMinutes < From || Row.timeMinutes > To) continue;
        bool Valid; FString EvalError;
        const float V = AIMFWindow::EvaluateEquationForRow(Equation, Row.data, Valid, EvalError, Mode);
        if (!Valid) continue;
        if (bCircular)
        {
            const double Radians = Mode == EAngleMode::Degrees ? FMath::DegreesToRadians(double(V)) : double(V);
            SinSum += FMath::Sin(Radians); CosSum += FMath::Cos(Radians);
        }
        else
        {
            const double Adjusted = double(V) - Compensation, Next = Sum + Adjusted;
            Compensation = (Next - Sum) - Adjusted; Sum = Next;
        }
        ++Count;
    }
    if (!Count) { Error = TEXT("No valid samples in this interval (check equation, time and fill values)."); return 0; }
    double Mean = Sum / Count;
    if (bCircular)
    {
        if (FMath::Sqrt(SinSum * SinSum + CosSum * CosSum) / Count < 1e-7) { Error = TEXT("Circular mean direction is undefined: the directions cancel."); return 0; }
        Mean = FMath::Atan2(SinSum, CosSum);
        if (Mean < 0) Mean += 2 * UE_DOUBLE_PI;
        if (Mean >= 2 * UE_DOUBLE_PI - 1e-7) Mean = 0;
        if (Mode == EAngleMode::Degrees) Mean = FMath::RadiansToDegrees(Mean);
    }
    if (!FMath::IsFinite(float(Mean))) { Error = TEXT("The mean is outside the supported numeric range."); return 0; }
    return float(Mean);
}

void AViewer::ShowAverageOptions(UUserWidget* Menu)
{
    if (!Menu || !Menu->WidgetTree) return;
    auto Read = [&](const TCHAR* Name)
    {
        UWidget* Widget = Menu->WidgetTree->FindWidget(Name);
        if (auto* Box = Cast<UEditableTextBox>(Widget)) return Box->GetText().ToString();
        if (auto* Box = Cast<UMultiLineEditableTextBox>(Widget)) return Box->GetText().ToString();
        return FString();
    };
    const FString Equation = Read(TEXT("EquationTextBox")), Start = Read(TEXT("StartTimeTextBox")), End = Read(TEXT("EndtimeTextBox"));
    UTextBlock* Output = Cast<UTextBlock>(Menu->WidgetTree->FindWidget(TEXT("AverageText")));
    if (!Output) return;
    TSharedRef<SWindow> Window = SNew(SWindow).Title(FText::FromString(TEXT("Average over selected interval"))).ClientSize(FVector2D(590,200)).SupportsMinimize(false).SupportsMaximize(false);
    auto Calculate = [&](bool Circular)
    {
        FString Error;
        const float Value = AverageWithMode(Equation, Start, End, Circular, Error);
        const FString Units = imfWindow && imfWindow->angleMode == EAngleMode::Degrees ? TEXT("deg") : TEXT("rad");
        Output->SetText(FText::FromString(Error.IsEmpty() ? FString::Printf(TEXT("%.9g (%s)"), Value, Circular ? *(TEXT("circular, ") + Units) : TEXT("arithmetic")) : TEXT("Error: ") + Error));
        Window->RequestDestroyWindow();
        return FReply::Handled();
    };
    Window->SetContent(SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight().Padding(12)[SNew(STextBlock).Text(FText::FromString(Equation + TEXT("\n") + Start + TEXT(" → ") + End)).AutoWrapText(true)]
        +SVerticalBox::Slot().AutoHeight().Padding(12)[SNew(STextBlock).Text(FText::FromString(TEXT("Use circular mean for angle direction. It uses the current Deg/Rad setting and differs from the direction of the mean field vector."))).AutoWrapText(true)]
        +SVerticalBox::Slot().AutoHeight().Padding(12)[SNew(SHorizontalBox)
            +SHorizontalBox::Slot().AutoWidth().Padding(4)[SNew(SButton).Text(FText::FromString(TEXT("Arithmetic mean"))).OnClicked_Lambda([&](){return Calculate(false);})]
            +SHorizontalBox::Slot().AutoWidth().Padding(4)[SNew(SButton).Text(FText::FromString(TEXT("Circular mean direction"))).OnClicked_Lambda([&](){return Calculate(true);})]
            +SHorizontalBox::Slot().AutoWidth().Padding(4)[SNew(SButton).Text(FText::FromString(TEXT("Cancel"))).OnClicked_Lambda([&](){Window->RequestDestroyWindow();return FReply::Handled();})]]);
    FSlateApplication::Get().AddModalWindow(Window, GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWindow() : nullptr, false);
}

void AViewer::WriteGraphPNG(TArray<FColor> Pixels, int32 Width, int32 Height, FString Path)
{
    const TWeakObjectPtr<AViewer> WeakThis(this);
    const bool RestoreUI = bRestoreScreenshotUI;
    bRestoreScreenshotUI = false;
    // Resolve the module on the game thread; the worker owns only pixels, dimensions and filename.
    IImageWrapperModule* Module = &FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
    for (FColor& Pixel : Pixels) Pixel.A = 255;
    Async(EAsyncExecution::ThreadPool, [WeakThis, RestoreUI, Module, Pixels=MoveTemp(Pixels), Width, Height, Path=MoveTemp(Path)]() mutable
    {
        FString Error;
        const auto Encoder = Module->CreateImageWrapper(EImageFormat::PNG);
        bool Success = false;
        if (Encoder.IsValid() && Encoder->SetRaw(Pixels.GetData(), int64(Pixels.Num()) * sizeof(FColor), Width, Height, ERGBFormat::BGRA, 8))
        {
            const TArray64<uint8>& Bytes = Encoder->GetCompressed(0);
            if (!Bytes.IsEmpty())
            {
                TUniquePtr<FArchive> Writer(IFileManager::Get().CreateFileWriter(*Path));
                if (Writer)
                {
                    Writer->Serialize(const_cast<uint8*>(Bytes.GetData()), Bytes.Num());
                    const bool Written = !Writer->IsError();
                    const bool Closed = Writer->Close();
                    Success = Written && Closed && !Writer->IsError();
                }
            }
        }
        if (!Success) Error = TEXT("PNG export could not be completed: ") + Path;
        AsyncTask(ENamedThreads::GameThread, [WeakThis, RestoreUI, Success, Error, Path]()
        {
            if (AViewer* Viewer = WeakThis.Get(); IsValid(Viewer))
            {
                Viewer->bExportInProgress = false;
                if (RestoreUI) Viewer->OnAfterExportScreenshot();
                if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 8, Success ? FColor::Green : FColor::Red, Success ? TEXT("Graph exported to ") + Path : Error);
            }
        });
    });
}

void AViewer::EndPlay(const EEndPlayReason::Type Reason)
{
    GetWorldTimerManager().ClearTimer(ExportTimer);
    if (GEngine && GEngine->GameViewport && ScreenshotDelegateHandle.IsValid()) GEngine->GameViewport->OnScreenshotCaptured().Remove(ScreenshotDelegateHandle);
    ScreenshotDelegateHandle.Reset();
    PendingSavePath.Empty(); bExportInProgress = false;
    Super::EndPlay(Reason);
}
