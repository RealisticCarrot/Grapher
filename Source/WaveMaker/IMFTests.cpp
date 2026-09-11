#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "IMFImport.h"
#include "IMFWindow.h"
#include "IMFAxis.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIMFAxisTests, "WaveMaker.IMF.ReadableAxisTicks", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FIMFAxisTests::RunTest(const FString&)
{
    auto Ticks = IMFAxis::Ticks(300, 539.833333, 1560);
    TestEqual(TEXT("Rounded twenty-minute tick count"), Ticks.Num(), 12);
    TestEqual(TEXT("First label has no day or seconds clutter"), Ticks[0].Label, FString(TEXT("05:00")));
    TestEqual(TEXT("Exact twenty-minute interval"), Ticks[1].Minutes, 320.0);
    TestEqual(TEXT("Last rounded tick stays in data range"), Ticks.Last().Label, FString(TEXT("08:40")));
    TestTrue(TEXT("Narrow window reduces ticks"), IMFAxis::Ticks(300,539.833333,500).Num() < Ticks.Num());
    Ticks = IMFAxis::Ticks(300,301,700);
    TestEqual(TEXT("Zoomed labels retain seconds"), Ticks[0].Label, FString(TEXT("05:00:00")));
    TestEqual(TEXT("Day rollover remains unambiguous"), IMFAxis::Label(1440,false), FString(TEXT("D+1 00:00")));
    TestEqual(TEXT("Negative time uses previous day"), IMFAxis::Label(-1,false), FString(TEXT("D-1 23:59")));
    TestTrue(TEXT("Invalid range is empty"), IMFAxis::Ticks(10,10,1000).IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIMFNumericTests, "WaveMaker.IMF.NumbersAndEquations", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FIMFNumericTests::RunTest(const FString&)
{
    auto Check = [&](const FString& Expression, double Expected, EAngleMode Mode = EAngleMode::Degrees)
    {
        bool Valid; FString Error;
        const double Actual = AIMFWindow::EvaluateEquationForRow(Expression, {1,2}, Valid, Error, Mode);
        TestTrue(*Expression, Valid && FMath::Abs(Actual - Expected) <= FMath::Max(1e-5, FMath::Abs(Expected) * 1e-6));
        if (!Valid) AddError(Error);
    };
    Check(TEXT("-2^2"), -4); Check(TEXT("2^3^2"), 512); Check(TEXT("2^-2"), .25); Check(TEXT("(-2)^2"),4);
    Check(TEXT("E+1"), UE_DOUBLE_EULERS_NUMBER+1); Check(TEXT("E-1"),UE_DOUBLE_EULERS_NUMBER-1);
    Check(TEXT("1e-9/1e-9"),1); Check(TEXT("1/Qe"),1/1.602176634e-19);
    Check(TEXT("1e20*1e20/1e20"),1e20);
    Check(TEXT("Clock(0,1)"),0); Check(TEXT("Clock(1,0)"),90); Check(TEXT("Clock(0,-1)"),180); Check(TEXT("Clock(-1,0)"),270); Check(TEXT("Clock(-1,1)"),315);
    Check(TEXT("Clock(-1,0)"),1.5*UE_DOUBLE_PI,EAngleMode::Radians);
    Check(TEXT("Sin(90)"),1); Check(TEXT("Cos(180)"),-1); Check(TEXT("Ln(E)"),1);
    Check(TEXT("Clock(9999.99,1)"),FMath::RadiansToDegrees(FMath::Atan2(9999.99,1.0)));
    for (const FString Bad : {TEXT("."),TEXT("1e"),TEXT("1e+"),TEXT("1/0"),TEXT("Sqrt(-1)"),TEXT("Ln(0)"),TEXT("Clock(0,0)"),TEXT("Col0"),TEXT("Col3"),TEXT("Clock(1,)"),TEXT("1e309")})
    {
        bool Valid; FString Error; AIMFWindow::EvaluateEquationForRow(Bad,{1,2},Valid,Error);
        TestFalse(*Bad,Valid);
    }
    for (float Value : {99.99f,999.81f,9999.81f,9999999.f})
    {
        bool Valid; FString Error;
        TestEqual(TEXT("Finite raw number without fill metadata"),AIMFWindow::EvaluateEquationForRow(TEXT("Col1"),{Value},Valid,Error),Value);
        TestTrue(TEXT("Not a universal fill"),Valid);
    }
    double Value;
    TestFalse(TEXT("Malformed input doesn't become zero"),IMFImport::ParseNumber(TEXT("INVALID"),Value));
    TestFalse(TEXT("Lone decimal rejected"),IMFImport::ParseNumber(TEXT("."),Value));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIMFImportTests, "WaveMaker.IMF.ImportCalendarAndMapping", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FIMFImportTests::RunTest(const FString&)
{
    FIMFImportResult R; FString E; FIMFImportProfile P;
    TestTrue(TEXT("Leap-year rollover"),IMFImport::Parse(TEXT("2016 366 23 59 1\n2017 1 0 0 2"),P,R,E));
    TestEqual(TEXT("One minute at leap-year boundary"),R.Rows.Last().timeMinutes-R.Rows[0].timeMinutes,1.0);
    TestFalse(TEXT("Invalid nonleap DOY rejected"),IMFImport::Parse(TEXT("2017 366 0 0 1"),P,R,E));
    P=IMFImport::DefaultProfile(TEXT("test.txt"),TEXT(""));
    TestTrue(TEXT("Headerless TXT, month end"),IMFImport::Parse(TEXT("2016 1 31 23 59 50 7\n2016 2 1 0 0 0 8"),P,R,E));
    TestEqual(TEXT("First row retained"),R.Rows.Num(),2);
    TestTrue(TEXT("Ten seconds across month boundary"),FMath::IsNearlyEqual((R.Rows.Last().timeMinutes-R.Rows[0].timeMinutes)*60,10.0,1e-7));
    TestTrue(TEXT("February 29 valid"),IMFImport::Parse(TEXT("2016 2 28 23 59 0 7\n2016 2 29 0 0 0 8"),P,R,E));
    TestFalse(TEXT("February 29 invalid in 2017"),IMFImport::Parse(TEXT("2017 2 29 0 0 0 7"),P,R,E));
    TestTrue(TEXT("Numeric metadata skipped"),IMFImport::Parse(TEXT("0 0 0 0 0 0 78.92\n2016 1 13 5 0 0 7\n2016 1 13 5 0 10 8"),P,R,E));
    TestEqual(TEXT("Only metadata skipped"),R.SkippedLines,1);
    P.Year=3;P.Month=5;P.Day=4;P.Hour=2;P.Minute=6;P.Second=7;
    TestTrue(TEXT("Arbitrary time columns and tabs"),IMFImport::Parse(TEXT("2.5\t6\t2017\t18\t12\t0\t0\n3.5 6 2017 18 12 0 10"),P,R,E));
    TestEqual(TEXT("Raw data column number retained"),R.Rows[0].data[0],2.5f);
    TestEqual(TEXT("Real timestamp origin"),R.Origin,FDateTime(2017,12,18));
    TestFalse(TEXT("Invalid data before reordered year not skipped"),IMFImport::Parse(TEXT("INVALID 6 2017 18 12 0 0"),P,R,E));
    P=FIMFImportProfile();
    TestFalse(TEXT("Malformed row rejected"),IMFImport::Parse(TEXT("2017 352 6 0 1\n2017 352 6 1 INVALID"),P,R,E));
    TestFalse(TEXT("Duplicate timestamps rejected"),IMFImport::Parse(TEXT("2017 352 6 0 1\n2017 352 6 0 2"),P,R,E));
    TestFalse(TEXT("Reversed timestamps rejected"),IMFImport::Parse(TEXT("2017 352 6 1 1\n2017 352 6 0 2"),P,R,E));
    P.Columns.SetNum(6);P.Columns[4].FillTokens=TEXT("99.99");
    TestTrue(TEXT("Per-column fill parsing"),IMFImport::Parse(TEXT("2017 352 6 0 99.99 99.99\n2017 352 6 1 99.98 9999.81"),P,R,E));
    TestTrue(TEXT("Configured pressure fill invalid"),FMath::IsNaN(R.Rows[0].data[4]));
    TestEqual(TEXT("Same number in another column retained"),R.Rows[0].data[5],99.99f);
    TestEqual(TEXT("Near fill retained"),R.Rows[1].data[4],99.98f);
    TestEqual(TEXT("False-positive value retained"),R.Rows[1].data[5],9999.81f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIMFGapTests, "WaveMaker.IMF.GapsAndClockWrap", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FIMFGapTests::RunTest(const FString&)
{
    TArray<FRow> Rows;
    for(float Value : {1.f,0.f,2.f}) { FRow R; R.data={Value}; R.timeMinutes=Rows.Num(); Rows.Add(R); }
    auto Segments=AIMFWindow::MakeValueSegments(Rows,TEXT("1/Col1"),EAngleMode::Degrees,90,false);
    TestEqual(TEXT("Infinity gap splits valid singletons"),Segments.Num(),2);
    TestEqual(TEXT("First isolated observation retained"),Segments[0].points.Num(),1);
    TestEqual(TEXT("Last isolated observation retained"),Segments[1].points.Num(),1);
    Rows[1].data={NAN};
    Segments=AIMFWindow::MakeValueSegments(Rows,TEXT("Col1"),EAngleMode::Degrees,90,false);
    TestEqual(TEXT("Imported fill remains a gap"),Segments.Num(),2);
    Rows.SetNum(2);Rows[0].data={1};Rows[1].data={2};Rows[1].timeMinutes=10;
    Segments=AIMFWindow::MakeValueSegments(Rows,TEXT("Col1"),EAngleMode::Degrees,90,false);
    TestEqual(TEXT("Absent timestamp interval remains a gap"),Segments.Num(),2);
    Rows[0].data={float(FMath::Sin(FMath::DegreesToRadians(359.0))),float(FMath::Cos(FMath::DegreesToRadians(359.0)))};
    Rows[1].data={float(FMath::Sin(FMath::DegreesToRadians(1.0))),float(FMath::Cos(FMath::DegreesToRadians(1.0)))};Rows[1].timeMinutes=1;
    Segments=AIMFWindow::MakeValueSegments(Rows,TEXT("Clock(Col1,Col2)"),EAngleMode::Degrees,90,true);
    TestEqual(TEXT("Clock wrap isn't interpolated through 180"),Segments.Num(),2);
    TestTrue(TEXT("Bare Clock recognized"),AIMFWindow::IsClockExpression(TEXT(" ( clock(Col1,Col2) ) ")));
    TestFalse(TEXT("Arbitrary derived quantity isn't angle-tagged"),AIMFWindow::IsClockExpression(TEXT("Clock(Col1,Col2)*2")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIMFTimeTests, "WaveMaker.IMF.TimeDisplayAndValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FIMFTimeTests::RunTest(const FString&)
{
    float Minutes;FString Error;
    TestEqual(TEXT("Seconds retained"),AIMFWindow::MinutesToTimeString(300.f+20.f/60),FString(TEXT("00:05:00:20")));
    TestTrue(TEXT("Seconds format roundtrip"),AIMFWindow::TimeStringToMinutes(TEXT("00:05:00:20"),Minutes,Error));
    TestTrue(TEXT("Same seconds"),FMath::IsNearlyEqual(Minutes,300.f+20.f/60,.0001f));
    TestTrue(TEXT("Legacy day format"),AIMFWindow::TimeStringToMinutes(TEXT("01:00:00"),Minutes,Error));
    TestEqual(TEXT("One day remains one day"),Minutes,1440.f);
    for(const FString Bad:{TEXT("."),TEXT("1e"),TEXT("00:60"),TEXT("00:00:00:60"),TEXT("00::01"),TEXT("")}) TestFalse(*Bad,AIMFWindow::TimeStringToMinutes(Bad,Minutes,Error));
    AIMFWindow* Window=GetMutableDefault<AIMFWindow>();
    const auto Saved=Window->imfData;
    FRow A,B;A.data={NAN,1};B.data={1,1};Window->imfData={A,B};
    TestTrue(TEXT("Leading fill doesn't invalidate syntax"),Window->IsValidEquation(TEXT("Clock(Col1,Col2)"),Error));
    Window->imfData[0].data={0,1};
    TestTrue(TEXT("Leading division singularity doesn't invalidate syntax"),Window->IsValidEquation(TEXT("1/Col1"),Error));
    Window->imfData.Init(A,30);
    TestTrue(TEXT("All invalid samples can form an empty valid equation"),Window->IsValidEquation(TEXT("Col1"),Error));
    TestFalse(TEXT("Bad syntax still rejected"),Window->IsValidEquation(TEXT("Col1+"),Error));
    Window->imfData=Saved;
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIMFAverageTests, "WaveMaker.IMF.ScalarAndCircularMeans", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FIMFAverageTests::RunTest(const FString&)
{
    AViewer* Viewer=GetMutableDefault<AViewer>();AIMFWindow* Window=GetMutableDefault<AIMFWindow>();
    const auto SavedRows=Viewer->imfData;AIMFWindow* SavedWindow=Viewer->imfWindow;const auto SavedMode=Window->angleMode;
    Viewer->imfWindow=Window;Window->angleMode=EAngleMode::Degrees;
    FRow A,B;A.data={359};A.timeMinutes=0;B.data={1};B.timeMinutes=1;Viewer->imfData={A,B};FString Error;
    TestTrue(TEXT("Mean direction crosses zero"),FMath::IsNearlyZero(Viewer->AverageWithMode(TEXT("Col1"),TEXT("0"),TEXT("1"),true,Error),.0001f));
    TestTrue(TEXT("Circular result has no error"),Error.IsEmpty());
    TestEqual(TEXT("Arithmetic mode remains available"),Viewer->AverageWithMode(TEXT("Col1"),TEXT("0"),TEXT("1"),false,Error),180.f);
    Viewer->imfData[0].data={0};Viewer->imfData[1].data={180};
    Viewer->AverageWithMode(TEXT("Col1"),TEXT("0"),TEXT("1"),true,Error);
    TestFalse(TEXT("Opposing directions report undefined"),Error.IsEmpty());
    Viewer->imfData[0].data={NAN};Viewer->imfData[1].data={2};
    TestEqual(TEXT("Scalar average excludes gaps"),Viewer->AverageWithMode(TEXT("Col1"),TEXT("0"),TEXT("1"),false,Error),2.f);
    Viewer->AverageWithMode(TEXT("Col1"),TEXT("5"),TEXT("6"),false,Error);
    TestFalse(TEXT("Empty interval reports error"),Error.IsEmpty());
    Viewer->imfData=SavedRows;Viewer->imfWindow=SavedWindow;Window->angleMode=SavedMode;
    return true;
}
#endif
