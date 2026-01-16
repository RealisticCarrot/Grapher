// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"




//for dealing with netcdf files
#include "C:/Program Files/netCDF 4.9.2/include/netcdf.h"

#include "C:/Program Files/netCDF 4.9.2/include/netcdf_aux.h"
#include "C:/Program Files/netCDF 4.9.2/include/netcdf_dispatch.h"
#include "C:/Program Files/netCDF 4.9.2/include/netcdf_filter.h"
//#include "netcdf_filter_build.h"
#include "C:/Program Files/netCDF 4.9.2/include/netcdf_mem.h"
#include "C:/Program Files/netCDF 4.9.2/include/netcdf_meta.h"

#include "Engine/TextureRenderTarget2D.h"

#include "Components/StaticMeshComponent.h"

#include "structs.h"

#include "MSPWindow.generated.h"



USTRUCT()
struct FncAttribute {
	GENERATED_BODY()
public:
	char name[256];
	nc_type type;
	uint32 length;
	TArray<float> data;
};


USTRUCT()
struct FncDimension {
	GENERATED_BODY()

public:
	char name[256];
	uint32 length;
};

USTRUCT()
struct FncVariable {
	GENERATED_BODY()

public:
	int id;
	char name[256];
	nc_type type;
	int dimCount;
	TArray<int> dimIds;
	int attCount;
	TArray<float> data;
};


// https://forums.unrealengine.com/t/dynamic-2d-array-using-tarray/382330/3
USTRUCT(BlueprintType)
struct FloatTArray {
	GENERATED_BODY()

public:
	TArray<float> arr;

	float operator[] (int32 i) {
		return arr[i];
	}

	void add(float inv) {
		arr.Add(inv);
	}

};





class AMSPTimeline;
class AMSPLegend;


UCLASS()
class WAVEMAKER_API AMSPWindow : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AMSPWindow();

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		UStaticMeshComponent* mspWindowPlane;

	UPROPERTY(BlueprintReadOnly)
		UTextureRenderTarget2D* mspRenderTarget;

	

	UPROPERTY()
		UMaterialInstanceDynamic* materialInstance;

	UPROPERTY(BlueprintReadWrite)
		AMSPTimeline* timelineDisplay;


	UPROPERTY(EditAnywhere)
		TSubclassOf<AMSPTimeline> timelineClass;

	UPROPERTY(EditAnywhere)
		TSubclassOf<AMSPLegend> legendClass;

	UPROPERTY(BlueprintReadOnly)
		AMSPLegend* legendDisplay;


	UPROPERTY()
		UTexture2D* mspDataTexture;
	
	UPROPERTY(EditAnywhere)
		UMaterial* mspMaterial;

	UPROPERTY()
		FmspDataProperties mspProperties;




	UPROPERTY(BlueprintReadOnly)
		TArray<FloatTArray> mspValues;

	UPROPERTY(BlueprintReadOnly)
		TArray<float> peakTimes;

	UPROPERTY(BlueprintReadOnly)
		TArray<float> peakData;

	// Flag to check if MSP data has been loaded - check this before accessing peakTimes/peakData
	UPROPERTY(BlueprintReadOnly)
		bool bIsDataLoaded = false;


	UPROPERTY(BlueprintReadOnly)
		TArray<float> mspWavelengths;

	UPROPERTY(BlueprintReadWrite)
		float startTime;
	UPROPERTY(BlueprintReadWrite)
		float endTime;

	UPROPERTY(BlueprintReadWrite)
		float startLoc;
	UPROPERTY(BlueprintReadWrite)
		float endLoc;

	UPROPERTY(BlueprintReadWrite)
		float timeLoc;

	UPROPERTY(BlueprintReadWrite)
		float timeSpanUnit;


	UPROPERTY(BlueprintReadOnly)
		int timeSteps;

	


	UPROPERTY(BlueprintReadOnly)
		float mspHoverValue;

	UPROPERTY(BlueprintReadOnly)
		float mspHoverTime;

	UPROPERTY(BlueprintReadOnly)
		bool showCursorInfo;

	UPROPERTY(BlueprintReadWrite)
		int channel;


	UPROPERTY(BlueprintReadWrite)
		FVector2D mouseUV;



protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable)
		void loadFile(FString fileName);

	UFUNCTION(BlueprintCallable)
		void loadFileAfterConstruction(FString fileName);


	UFUNCTION(BlueprintCallable)
		TArray<float> fetchMSPdata(FString dataKey);

	UFUNCTION(BlueprintCallable)
		void getMspProperties(TArray<float> data);

	UFUNCTION()
		TArray<int> intRange(TArray<int> inA, int start, int stop);


	UFUNCTION()
		TArray<uint32> getDimensionVector(TArray<int> ids, int idCount, TArray<FncDimension> dims);


	UFUNCTION()
		uint32 getSizeOfDimVector(TArray<uint32> dimVector, int dimCount);


	UFUNCTION()
		void mspParsingProcedure(FString filename, TMap<FString, uint32>& refNames, TArray<FloatTArray>& refValues, TArray<uint32>& refSizes);

	UFUNCTION()
		UTexture2D* CreateTextureFrom32BitFloat(TArray<float> data, int width, int height);

	UFUNCTION()
		UTexture2D* UpdateTextureFrom32BitFloat(TArray<float> data, int width, int height, UTexture2D* texture);

	UFUNCTION(BlueprintCallable)
		void setMSPscalar(FString paramName, float value);

	UFUNCTION(BlueprintCallable)
		void printTime(float t);

	// Safe accessor for peakTimes - returns 0 if index is out of bounds or data not loaded
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MSP Data")
		float GetPeakTimeAtIndex(int32 Index) const;

	// Safe accessor for peakData - returns 0 if index is out of bounds or data not loaded
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MSP Data")
		float GetPeakDataAtIndex(int32 Index) const;

	// Get the number of peak time entries
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MSP Data")
		int32 GetPeakTimesCount() const;

	// Get the number of peak data entries
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MSP Data")
		int32 GetPeakDataCount() const;

};
