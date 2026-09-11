#pragma once
#include "CoreMinimal.h"

// Display ticks are separate from editable time strings, whose legacy day syntax is retained.
namespace IMFAxis
{
    struct FTick { double Minutes; FString Label; };
    inline FString Label(double Minutes, bool Seconds)
    {
        const int64 Total = FMath::RoundToInt64(Minutes * 60);
        const int64 Day = FMath::FloorToInt64(double(Total) / 86400);
        const int64 WithinDay = Total - Day * 86400;
        FString Time = FString::Printf(TEXT("%02lld:%02lld"), WithinDay / 3600, WithinDay / 60 % 60);
        if (Seconds) Time += FString::Printf(TEXT(":%02lld"), WithinDay % 60);
        return Day ? FString::Printf(TEXT("D%+lld "), Day) + Time : Time;
    }
    inline TArray<FTick> Ticks(double Start, double End, double Width, int32 Capacity = 13)
    {
        TArray<FTick> Result;
        if (!FMath::IsFinite(Start) || !FMath::IsFinite(End) || End <= Start || Width <= 0 || Capacity < 2) return Result;
        const double Spacing = Start < 0 || End >= 1440 ? 155 : 110;
        const int32 Budget = FMath::Clamp(FMath::FloorToInt(Width / Spacing), 1, Capacity - 1);
        const double Desired = (End - Start) * 60 / Budget;
        double Step = 1;
        const double Steps[] = {1,2,5,10,15,20,30,60,120,300,600,900,1200,1800,3600,7200,10800,21600,43200,86400};
        for (double Candidate : Steps) { Step = Candidate; if (Step >= Desired) break; }
        if (Desired > Step) Step *= FMath::CeilToDouble(Desired / Step);
        const double First = FMath::CeilToDouble((Start * 60 - 1e-6) / Step) * Step;
        for (int32 I = 0; I < Capacity; ++I)
        {
            const double Time = (First + I * Step) / 60;
            if (Time > End + 1e-8) break;
            Result.Add({Time, Label(Time, Step < 60)});
        }
        return Result;
    }
}
