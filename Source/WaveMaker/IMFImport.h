#pragma once

#include "CoreMinimal.h"
#include "Viewer.h"

// Column numbers in import profiles are one-based; zero means an unused time field.
struct FIMFColumnInfo
{
    FString Label, Units, Frame, FillTokens;
    FString DisplayName(int32 Index) const;
};

struct FIMFImportProfile
{
    int32 Year = 1, DayOfYear = 2, Month = 0, Day = 0, Hour = 3, Minute = 4, Second = 0;
    int32 SkipLines = -1; // -1: detect leading text/metadata; never skip a valid first record.
    double GapSeconds = 0; // 0: infer cadence; positive: explicit maximum connecting interval.
    FString Source;
    TArray<FIMFColumnInfo> Columns;
};

struct FIMFImportResult
{
    TArray<FRow> Rows;
    FDateTime Origin;
    double CadenceSeconds = 0, GapSeconds = 0;
    int32 SkippedLines = 0, MissingValues = 0;
    FIMFImportProfile Profile;
};

namespace IMFImport
{
    bool ParseNumber(const FString& Text, double& Value);
    void Tokenize(const FString& Line, TArray<FString>& Tokens);
    bool Parse(const FString& Text, const FIMFImportProfile& Profile, FIMFImportResult& Result, FString& Error);
    FIMFImportProfile DefaultProfile(const FString& Filename, const FString& Text);
    bool LoadProfile(const FString& Filename, FIMFImportProfile& Profile);
    bool SaveProfile(const FString& Filename, const FIMFImportProfile& Profile, FString& Error);
    // The preview is the import confirmation, not a separate permission prompt.
    bool Preview(const FString& Filename, const FString& Text, FIMFImportResult& Result);
}
