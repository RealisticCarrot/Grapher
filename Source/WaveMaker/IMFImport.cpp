#include "IMFImport.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "HAL/FileManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"

FString FIMFColumnInfo::DisplayName(int32 Index) const
{
    FString Name = Label.IsEmpty() ? FString::Printf(TEXT("Col%d"), Index + 1) : Label;
    if (!Frame.IsEmpty()) Name += TEXT(" (") + Frame + TEXT(")");
    if (!Units.IsEmpty()) Name += TEXT(" [") + Units + TEXT("]");
    return Name;
}

bool IMFImport::ParseNumber(const FString& Text, double& Value)
{
    const FString S = Text.TrimStartAndEnd();
    int32 I = 0, Digits = 0;
    if (I < S.Len() && (S[I] == '+' || S[I] == '-')) ++I;
    while (I < S.Len() && FChar::IsDigit(S[I])) { ++I; ++Digits; }
    if (I < S.Len() && S[I] == '.')
    {
        ++I;
        while (I < S.Len() && FChar::IsDigit(S[I])) { ++I; ++Digits; }
    }
    if (!Digits) return false;
    if (I < S.Len() && (S[I] == 'e' || S[I] == 'E'))
    {
        ++I;
        if (I < S.Len() && (S[I] == '+' || S[I] == '-')) ++I;
        const int32 Start = I;
        while (I < S.Len() && FChar::IsDigit(S[I])) ++I;
        if (I == Start) return false;
    }
    if (I != S.Len()) return false;
    Value = FCString::Atod(*S);
    return FMath::IsFinite(Value);
}

void IMFImport::Tokenize(const FString& Line, TArray<FString>& Tokens)
{
    Line.ParseIntoArrayWS(Tokens);
}

static bool DateForRow(const TArray<double>& Values, const FIMFImportProfile& P, FDateTime& Date, FString& Error)
{
    auto Get = [&](int32 Col, int32 Default, int32& Out) -> bool
    {
        if (!Col) { Out = Default; return true; }
        if (!Values.IsValidIndex(Col - 1) || Values[Col - 1] < 0 || Values[Col - 1] > 9999 || FMath::FloorToDouble(Values[Col - 1]) != Values[Col - 1]) return false;
        Out = int32(Values[Col - 1]); return true;
    };
    int32 Y, M, D, H, Min, Sec, DOY;
    if (!Get(P.Year, 0, Y) || !Get(P.Month, 1, M) || !Get(P.Day, 1, D) || !Get(P.Hour, 0, H) || !Get(P.Minute, 0, Min) || !Get(P.Second, 0, Sec) || !Get(P.DayOfYear, 1, DOY))
    { Error = TEXT("Time fields must contain valid nonnegative integers."); return false; }
    if (Y < 1 || Y > 9999) { Error = TEXT("Year must be between 1 and 9999."); return false; }
    if (P.DayOfYear)
    {
        if (DOY < 1 || DOY > (FDateTime::IsLeapYear(Y) ? 366 : 365)) { Error = TEXT("Invalid day of year."); return false; }
        const FDateTime DayDate = FDateTime(Y, 1, 1) + FTimespan::FromDays(DOY - 1);
        M = DayDate.GetMonth(); D = DayDate.GetDay();
    }
    if (!FDateTime::Validate(Y, M, D, H, Min, Sec, 0)) { Error = TEXT("Invalid calendar date or time."); return false; }
    Date = FDateTime(Y, M, D, H, Min, Sec);
    return true;
}

bool IMFImport::Parse(const FString& Text, const FIMFImportProfile& P, FIMFImportResult& Result, FString& Error)
{
    Result = FIMFImportResult(); Result.Profile = P; Error.Empty();
    if (P.Year <= 0 || P.Hour <= 0 || P.Minute <= 0 || (P.DayOfYear > 0) == (P.Month > 0 || P.Day > 0) || (!P.DayOfYear && (P.Month <= 0 || P.Day <= 0)))
    { Error = TEXT("Map year, hour and minute, and choose either day of year OR month and day. Use 0 for unused fields."); return false; }
    TSet<int32> TimeColumns;
    for (int32 C : {P.Year, P.DayOfYear, P.Month, P.Day, P.Hour, P.Minute, P.Second})
    {
        if (C < 0 || C > 4096 || (C && TimeColumns.Contains(C))) { Error = TEXT("Time column numbers must be unique, from 1 to 4096 (0 = unused)."); return false; }
        if (C) TimeColumns.Add(C);
    }
    if (P.SkipLines < -1 || !FMath::IsFinite(P.GapSeconds) || P.GapSeconds < 0) { Error = TEXT("Invalid header count or gap interval."); return false; }
    TArray<TArray<double>> Fills;
    Fills.SetNum(P.Columns.Num());
    for (int32 C = 0; C < P.Columns.Num(); ++C)
    {
        TArray<FString> Tokens; P.Columns[C].FillTokens.ParseIntoArray(Tokens, TEXT(","), true);
        for (const FString& Token : Tokens)
        {
            double Fill;
            if (!ParseNumber(Token, Fill)) { Error = FString::Printf(TEXT("Col%d: invalid fill token '%s'."), C + 1, *Token); return false; }
            Fills[C].Add(Fill);
        }
    }
    TArray<FString> Lines; Text.ParseIntoArrayLines(Lines, false);
    int32 Width = 0;
    FDateTime Previous;
    TArray<double> Steps;
    for (int32 L = 0; L < Lines.Num(); ++L)
    {
        if (L < P.SkipLines) { ++Result.SkippedLines; continue; }
        TArray<FString> Tokens; Tokenize(Lines[L], Tokens);
        if (!Tokens.Num()) continue;
        TArray<double> Values;
        bool Numeric = true;
        for (const FString& T : Tokens)
        {
            double V;
            if (!ParseNumber(T, V)) { Numeric = false; V = NAN; }
            Values.Add(V);
        }
        // Text headers and the supplied all-zero time metadata row may precede data.
        // A numeric year with broken data is never silently skipped as a header.
        const bool NumericYear = Values.IsValidIndex(P.Year - 1) && Values[P.Year - 1] >= 1000;
        bool ZeroTime = Numeric;
        for (int32 C : TimeColumns) ZeroTime &= Values.IsValidIndex(C - 1) && Values[C - 1] == 0;
        if (!Result.Rows.Num() && P.SkipLines == -1 && ((!Numeric && !NumericYear) || ZeroTime))
        { ++Result.SkippedLines; continue; }
        auto Fail = [&](const FString& Why) { Error = FString::Printf(TEXT("Line %d: %s"), L + 1, *Why); return false; };
        if (!Numeric) return Fail(TEXT("Non-numeric or nonfinite token. Correct the row or configure its fill value; it will not become zero."));
        if (!Width) Width = Tokens.Num();
        if (Width != Tokens.Num()) return Fail(TEXT("Column count differs from the first data row."));
        FDateTime Date; FString DateError;
        if (!DateForRow(Values, P, Date, DateError)) return Fail(DateError);
        if (!Result.Rows.Num()) Result.Origin = Date.GetDate();
        else
        {
            double Step = (Date - Previous).GetTotalSeconds();
            if (Step <= 0) return Fail(TEXT("Timestamps must be strictly increasing; duplicate or reversed time found."));
            Steps.Add(Step);
        }
        Previous = Date;
        FRow Row; Row.stringData = Tokens;
        Row.timeMinutes = (Date - Result.Origin).GetTotalMinutes();
        for (int32 C = 0; C < Values.Num(); ++C)
        {
            const bool Missing = !TimeColumns.Contains(C + 1) && Fills.IsValidIndex(C) && Fills[C].Contains(Values[C]);
            if (!Missing && !FMath::IsFinite(float(Values[C]))) return Fail(TEXT("Value exceeds supported numeric range."));
            Row.data.Add(Missing ? NAN : float(Values[C]));
            Result.MissingValues += Missing ? 1 : 0;
        }
        Result.Rows.Add(MoveTemp(Row));
    }
    if (!Result.Rows.Num()) { Error = TEXT("No valid data records were found with this mapping."); return false; }
    if (Steps.Num()) { Steps.Sort(); Result.CadenceSeconds = Steps[Steps.Num()/2]; }
    Result.GapSeconds = P.GapSeconds > 0 ? P.GapSeconds : Result.CadenceSeconds * 1.5;
    Result.Profile.Columns.SetNum(Width);
    return true;
}

FIMFImportProfile IMFImport::DefaultProfile(const FString& Filename, const FString& Text)
{
    FIMFImportProfile P; P.Source = FPaths::GetCleanFilename(Filename);
    if (FPaths::GetExtension(Filename).Equals(TEXT("txt"), ESearchCase::IgnoreCase))
    { P.DayOfYear = 0; P.Month = 2; P.Day = 3; P.Hour = 4; P.Minute = 5; P.Second = 6; }
    TArray<FString> Lines; Text.ParseIntoArrayLines(Lines);
    for (const FString& Line : Lines)
    {
        TArray<FString> Tokens; Tokenize(Line, Tokens);
        if (Tokens.Num() > P.Columns.Num() && Tokens.Num() <= 4096) P.Columns.SetNum(Tokens.Num());
    }
    return P;
}

static FString ProfilePath(const FString& Filename)
{
    return FPaths::ProjectSavedDir() / TEXT("IMFImportProfiles") / (FMD5::HashAnsiString(*FPaths::ConvertRelativePathToFull(Filename)) + TEXT(".json"));
}

bool IMFImport::LoadProfile(const FString& Filename, FIMFImportProfile& P)
{
    FString Json;
    if (!FFileHelper::LoadFileToString(Json, *ProfilePath(Filename)) && !FFileHelper::LoadFileToString(Json, *(FPaths::ProjectConfigDir()/TEXT("IMFImportProfiles")/(FPaths::GetBaseFilename(Filename)+TEXT(".json"))))) return false;
    TSharedPtr<FJsonObject> O;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), O) || !O.IsValid()) return false;
    auto Int = [&](const TCHAR* Name, int32& V) { double N; if (O->TryGetNumberField(Name, N)) V = int32(N); };
    Int(TEXT("year"),P.Year); Int(TEXT("doy"),P.DayOfYear); Int(TEXT("month"),P.Month); Int(TEXT("day"),P.Day); Int(TEXT("hour"),P.Hour); Int(TEXT("minute"),P.Minute); Int(TEXT("second"),P.Second); Int(TEXT("skipLines"),P.SkipLines);
    O->TryGetNumberField(TEXT("gapSeconds"), P.GapSeconds); O->TryGetStringField(TEXT("source"), P.Source);
    const TArray<TSharedPtr<FJsonValue>>* Columns;
    if (O->TryGetArrayField(TEXT("columns"), Columns))
    {
        P.Columns.SetNum(Columns->Num());
        for (int32 I=0; I<Columns->Num(); ++I)
        {
            const TSharedPtr<FJsonObject>* C;
            if (!(*Columns)[I]->TryGetObject(C)) continue;
            (*C)->TryGetStringField(TEXT("label"),P.Columns[I].Label); (*C)->TryGetStringField(TEXT("units"),P.Columns[I].Units);
            (*C)->TryGetStringField(TEXT("frame"),P.Columns[I].Frame); (*C)->TryGetStringField(TEXT("fill"),P.Columns[I].FillTokens);
        }
    }
    return true;
}

bool IMFImport::SaveProfile(const FString& Filename, const FIMFImportProfile& P, FString& Error)
{
    auto O = MakeShared<FJsonObject>();
    O->SetNumberField(TEXT("year"),P.Year); O->SetNumberField(TEXT("doy"),P.DayOfYear); O->SetNumberField(TEXT("month"),P.Month); O->SetNumberField(TEXT("day"),P.Day); O->SetNumberField(TEXT("hour"),P.Hour); O->SetNumberField(TEXT("minute"),P.Minute); O->SetNumberField(TEXT("second"),P.Second); O->SetNumberField(TEXT("skipLines"),P.SkipLines); O->SetNumberField(TEXT("gapSeconds"),P.GapSeconds); O->SetStringField(TEXT("source"),P.Source);
    TArray<TSharedPtr<FJsonValue>> Columns;
    for (const auto& C : P.Columns)
    {
        auto J=MakeShared<FJsonObject>(); J->SetStringField(TEXT("label"),C.Label); J->SetStringField(TEXT("units"),C.Units); J->SetStringField(TEXT("frame"),C.Frame); J->SetStringField(TEXT("fill"),C.FillTokens); Columns.Add(MakeShared<FJsonValueObject>(J));
    }
    O->SetArrayField(TEXT("columns"), Columns);
    FString Json; FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&Json));
    const FString Path=ProfilePath(Filename);
    if (!IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path),true) || !FFileHelper::SaveStringToFile(Json,*Path)) { Error=TEXT("Could not save the import profile."); return false; }
    return true;
}

bool IMFImport::Preview(const FString& Filename, const FString& Text, FIMFImportResult& Result)
{
    FIMFImportProfile P=DefaultProfile(Filename,Text); LoadProfile(Filename,P);
    bool Accepted=false;
    TSharedRef<SWindow> Window=SNew(SWindow).Title(FText::FromString(TEXT("Import time series — ")+FPaths::GetCleanFilename(Filename))).ClientSize(FVector2D(900,710)).SupportsMaximize(true).SupportsMinimize(false);
    TSharedPtr<STextBlock> Status;
    TSharedRef<SVerticalBox> Body=SNew(SVerticalBox);
    Body->AddSlot().AutoHeight().Padding(8)[SNew(STextBlock).Text(FText::FromString(TEXT("Map raw file columns (1-based; 0 = unused). Choose day of year OR month + day. Set each data column's exact fill token; blank means no fill. Profiles are remembered for this file."))).AutoWrapText(true)];
    TSharedRef<SHorizontalBox> TimeRow=SNew(SHorizontalBox);
    auto AddInt=[&](const TCHAR* Label,int32& Value)
    {
        int32* Target=&Value;
        TimeRow->AddSlot().AutoWidth().Padding(5)[SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(Label))]
            +SVerticalBox::Slot().AutoHeight()[SNew(SBox).WidthOverride(70)[SNew(SEditableTextBox).Text(FText::AsNumber(Value)).OnTextChanged_Lambda([Target](const FText& T){ double N; *Target=IMFImport::ParseNumber(T.ToString(),N)&&N>=-1&&N<=4096&&FMath::FloorToDouble(N)==N?int32(N):-2; })]]];
    };
    AddInt(TEXT("Year"),P.Year); AddInt(TEXT("DOY"),P.DayOfYear); AddInt(TEXT("Month"),P.Month); AddInt(TEXT("Day"),P.Day); AddInt(TEXT("Hour"),P.Hour); AddInt(TEXT("Minute"),P.Minute); AddInt(TEXT("Second"),P.Second); AddInt(TEXT("Skip (-1 auto)"),P.SkipLines);
    Body->AddSlot().AutoHeight()[TimeRow];
    Body->AddSlot().AutoHeight().Padding(8)[SNew(SHorizontalBox)
        +SHorizontalBox::Slot().AutoWidth().Padding(0,0,8,0)[SNew(STextBlock).Text(FText::FromString(TEXT("Maximum connecting interval (seconds; 0 = 1.5 × median cadence):")))]
        +SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(90)[SNew(SEditableTextBox).Text(FText::AsNumber(P.GapSeconds)).OnTextChanged_Lambda([&P](const FText& T){ if(!IMFImport::ParseNumber(T.ToString(),P.GapSeconds))P.GapSeconds=-1; })]]];
    Body->AddSlot().AutoHeight().Padding(8)[SNew(SEditableTextBox).Text(FText::FromString(P.Source)).HintText(FText::FromString(TEXT("Source / product description"))).OnTextChanged_Lambda([&P](const FText& T){P.Source=T.ToString();})];
    TSharedRef<SVerticalBox> Columns=SNew(SVerticalBox);
    Columns->AddSlot().AutoHeight().Padding(4)[SNew(STextBlock).Text(FText::FromString(TEXT("Column        Label                                  Units                 Frame               Fill tokens (comma-separated)")))];
    for(int32 I=0;I<P.Columns.Num();++I)
    {
        auto C=&P.Columns[I];
        Columns->AddSlot().AutoHeight().Padding(3)[SNew(SHorizontalBox)
            +SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(72)[SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("Col%d"),I+1)))]]
            +SHorizontalBox::Slot().FillWidth(2).Padding(3,0)[SNew(SEditableTextBox).Text(FText::FromString(C->Label)).OnTextChanged_Lambda([C](const FText& T){C->Label=T.ToString();})]
            +SHorizontalBox::Slot().FillWidth(1).Padding(3,0)[SNew(SEditableTextBox).Text(FText::FromString(C->Units)).OnTextChanged_Lambda([C](const FText& T){C->Units=T.ToString();})]
            +SHorizontalBox::Slot().FillWidth(1).Padding(3,0)[SNew(SEditableTextBox).Text(FText::FromString(C->Frame)).OnTextChanged_Lambda([C](const FText& T){C->Frame=T.ToString();})]
            +SHorizontalBox::Slot().FillWidth(2).Padding(3,0)[SNew(SEditableTextBox).Text(FText::FromString(C->FillTokens)).OnTextChanged_Lambda([C](const FText& T){C->FillTokens=T.ToString();})]];
    }
    Body->AddSlot().FillHeight(1).Padding(8)[SNew(SScrollBox)+SScrollBox::Slot()[Columns]];
    auto Validate=[&]() -> bool
    {
        FString Error;
        if(!Parse(Text,P,Result,Error)){Status->SetText(FText::FromString(Error));return false;}
        FString Summary=FString::Printf(TEXT("%d records; %d columns; %d leading lines skipped; %d missing values.\n%s → %s; median cadence %.3g s. Connecting intervals above %.3g s will remain gaps.\nFirst record: "),Result.Rows.Num(),Result.Rows[0].data.Num(),Result.SkippedLines,Result.MissingValues,*(Result.Origin+FTimespan::FromMinutes(Result.Rows[0].timeMinutes)).ToString(),*(Result.Origin+FTimespan::FromMinutes(Result.Rows.Last().timeMinutes)).ToString(),Result.CadenceSeconds,Result.GapSeconds);
        for(int32 I=0;I<Result.Rows[0].stringData.Num();++I)Summary+=FString::Printf(TEXT("Col%d=%s  "),I+1,*Result.Rows[0].stringData[I]);
        Status->SetText(FText::FromString(Summary));return true;
    };
    Body->AddSlot().AutoHeight().Padding(8)[SAssignNew(Status,STextBlock).AutoWrapText(true)];
    Body->AddSlot().AutoHeight().Padding(8)[SNew(SHorizontalBox)
        +SHorizontalBox::Slot().AutoWidth().Padding(4)[SNew(SButton).Text(FText::FromString(TEXT("Preview mapping"))).OnClicked_Lambda([&](){Validate();return FReply::Handled();})]
        +SHorizontalBox::Slot().AutoWidth().Padding(4)[SNew(SButton).Text(FText::FromString(TEXT("Import"))).OnClicked_Lambda([&](){if(Validate()){FString Error;if(SaveProfile(Filename,Result.Profile,Error)){Accepted=true;Window->RequestDestroyWindow();}else Status->SetText(FText::FromString(Error));}return FReply::Handled();})]
        +SHorizontalBox::Slot().AutoWidth().Padding(4)[SNew(SButton).Text(FText::FromString(TEXT("Cancel"))).OnClicked_Lambda([&](){Window->RequestDestroyWindow();return FReply::Handled();})]];
    Window->SetContent(Body); Validate();
    const TSharedPtr<SWindow> Parent=GEngine&&GEngine->GameViewport?GEngine->GameViewport->GetWindow():nullptr;
    FSlateApplication::Get().AddModalWindow(Window,Parent,false);
    return Accepted;
}
