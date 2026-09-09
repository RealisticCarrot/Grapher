// Fill out your copyright notice in the Description page of Project Settings.


#include "MSPWindow.h"

#include "Kismet/GameplayStatics.h"
#include "Blueprint/WidgetLayoutLibrary.h"

#include <limits>

#include <stdio.h>
#include <stdlib.h>

#include <tchar.h>

#include <string>
#include <string.h>
#include <sstream>


#include "Engine/Texture2D.h"
#include "HAL/UnrealMemory.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "WaveMaker.h"

#include "Materials/MaterialInstance.h"
#include "Materials/MaterialLayersFunctions.h"
#include "Materials/MaterialInstanceDynamic.h"

#include "MSPTimeline.h"

#include "MSPLegend.h"



float ncMax;
float ncMin;
float ncAvg;
float ncTimeSteps;
float ncTimeStepSize;
float ncTimeStart;
float ncTimeStop;

TMap<FString, uint32> mspNames;

TArray<uint32> mspSizes;


TArray<FGuid> mspParameterIds;
TMap<FMaterialParameterInfo, FMaterialParameterMetadata> mspParameterInfo;


bool fileLoadQueued;
FString queuedFile;



float totalTime;

// Sets default values
AMSPWindow::AMSPWindow()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	//mspMaterial = LoadObject<UMaterial>(nullptr, TEXT("/Script/Engine.Material'/Game/Materials/MSPMaterial.MSPMaterial'"));

	mspWindowPlane = CreateDefaultSubobject<UStaticMeshComponent>("Window Plane");
	
	totalTime = 0.0f;
	

	//muv = FVector4(0.0f, 0.0f, 0.0f, 0.0f);

	timeSpanUnit = -1.0f;

	Tags.Add("MSPWindow");
}

// Called when the game starts or when spawned
void AMSPWindow::BeginPlay()
{
	Super::BeginPlay();

	//mspWindowPlane->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Script/Engine.StaticMesh'/Engine/BasicShapes/Plane.Plane'")));

	mspWindowPlane->SetWorldScale3D(FVector(2.6f, 8.7f, 1.0f));

	if (!mspMaterial) {
		FGenericPlatformMisc::RequestExit(false);
	}

	

	
	mspMaterial->GetAllParametersOfType(EMaterialParameterType::Texture, mspParameterInfo);

	for (const TPair<FMaterialParameterInfo, FMaterialParameterMetadata>& info : mspParameterInfo) {
		//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Black, info.Key.Name.ToString());
	}



	//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, FString("Max mips") + FString::SanitizeFloat(MAX_TEXTURE_MIP_COUNT));




	timelineDisplay = GetWorld()->SpawnActor<AMSPTimeline>(timelineClass, GetActorLocation(), GetActorRotation());
	legendDisplay = GetWorld()->SpawnActor<AMSPLegend>(legendClass, GetActorLocation(), GetActorRotation());
	

}

// Called every frame
void AMSPWindow::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	totalTime += DeltaTime;

	if (fileLoadQueued) {
		//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, "Loading File");

		loadFile(queuedFile);
		fileLoadQueued = false;

		timelineDisplay->setDataTextureWhenReady(fetchMSPdata("PeakIntensity"));
		timelineDisplay->setTextureParamsWhenReady(mspProperties);
	}






	//check if the cursor is over an msp window
	FHitResult cursorHit;
	//get the cursor hit result
	APlayerController* PlayerController = Cast<APlayerController>(GEngine->GetFirstLocalPlayerController(GetWorld()));
	if (PlayerController && PlayerController->GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, false, cursorHit)) {
		//if its a main msp window then get the hovered over time and value
		AActor* HitActor = cursorHit.GetActor();
		if (HitActor && HitActor->ActorHasTag("MSPWindow")) {
			APlayerController* viewer = PlayerController;

			FVector2D mouseLoc;
			viewer->GetMousePosition(mouseLoc.X, mouseLoc.Y);

			FVector boundOrigin;
			FVector boundExtents;

			HitActor->GetActorBounds(true, boundOrigin, boundExtents);


			FVector2D topRight;
			viewer->ProjectWorldLocationToScreen(boundOrigin + boundExtents, topRight);
			FVector2D bottomLeft;
			viewer->ProjectWorldLocationToScreen(boundOrigin - boundExtents, bottomLeft);

			// Prevent division by zero
			FVector2D screenDiff = topRight - bottomLeft;
			if (FMath::IsNearlyZero(screenDiff.X) || FMath::IsNearlyZero(screenDiff.Y))
			{
				// Skip this frame if screen bounds are degenerate
			}
			else
			{
				FVector2D MUV = (mouseLoc - bottomLeft) / screenDiff;

				mouseUV.X = MUV.X;
				mouseUV.Y = MUV.Y;

				FVector2D iMouseLoc = mouseLoc - bottomLeft;
				iMouseLoc.Y *= -1.0f;

				// Only process hover values if arrays have data
				if (peakData.Num() > 0 && peakTimes.Num() > 0)
				{
					float timeSize = 181.0f * 6.0f;
					
					float valuepx = (MUV.X * (viewEndLoc - viewStartLoc)) + viewStartLoc;
					float valuepy = ((float)channel + (1.0f - MUV.Y));
					float valuep = valuepx - valuepy;

					int vIndx = (int)((valuepx * peakData.Num()) / timeSize) * (int)timeSize;
					int vIndy = (int)(valuepy * 181.0f);

					int vInd = vIndx + vIndy;

					int valueIndex = (int)(valuep * (float)peakData.Num());
					int timeIndex = (int)(((MUV.X * (viewEndLoc - viewStartLoc)) + viewStartLoc) * (float)peakTimes.Num());

					//GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Yellow, FString::SanitizeFloat( vIndy ));
					//GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::White, (mouseLoc - bottomLeft).ToString());

					vInd = FMath::Clamp(vInd, 0, peakData.Num() - 1);
					timeIndex = FMath::Clamp(timeIndex, 0, peakTimes.Num() - 1);

					mspHoverValue = peakData[vInd];
					mspHoverTime = peakTimes[timeIndex];
				}
			}
		}
	}

	


	// Only auto-compute start/end from timeLoc/timeSpanUnit when NOT in direct range mode
	if (timeSpanUnit > 0.0f && !bUseDirectTimeRange) {
		float autoStart = FMath::Clamp(timeLoc - (timeSpanUnit / 2.0f), 0.0f, 1.0f);
		float autoEnd = FMath::Clamp(timeLoc + (timeSpanUnit / 2.0f), 0.0f, 1.0f);
		setMSPscalar("start", autoStart);
		setMSPscalar("end", autoEnd);

	}




	//materialInstance->SetScalarParameterValue("div", cosf(totalTime));
	//materialInstance->SetScalarParameterValue("a", sinf(totalTime));
	//materialInstance->SetScalarParameterValue("channel", totalTime / 10.0f);
	//GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::White, FString::SanitizeFloat(totalTime / 10.0f));


}


void AMSPWindow::setMSPscalar(FString paramName, float value) {

	if (materialInstance) {
		materialInstance->SetScalarParameterValue(FName(paramName), value);

	}

	//update the parameters in the timeline material too
	if (timelineDisplay) {
		timelineDisplay->setMSPTimelineScalar(paramName, value);
	}

	if (legendDisplay) {
		legendDisplay->setMSPLegendScalar(paramName, value);
	}

	if (paramName == "channel") {
		channel = (int)value;
	}
	else if (paramName == "start") {
		startLoc = value;
		viewStartLoc = value;
	}
	else if (paramName == "end") {
		endLoc = value;
		viewEndLoc = value;
	}
	
}



void AMSPWindow::loadFileAfterConstruction(FString filename) {
	fileLoadQueued = true;
	queuedFile = filename;
}


void AMSPWindow::loadFile(FString fileName) {

	void* viewportHandle = GEngine->GameViewport->GetWindow()->GetNativeWindow()->GetOSWindowHandle();

	TArray<FString> outName;


	mspParsingProcedure(fileName, mspNames, mspValues, mspSizes);
	
	// Check if parsing was successful - if not, return early to prevent crashes
	if (mspNames.Num() == 0 || mspValues.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to parse MSP file: %s"), *fileName);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, TEXT("Failed to load MSP file - parsing failed"));
		}
		return;
	}
	
	//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, mspNames[0]);

	//loop through each variable name and get its data length
	for (const TPair<FString, uint32>& pair : mspNames) {
		//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, pair.Key + " " + FString::SanitizeFloat(mspValues[pair.Value].arr.Num()));

	}

	peakData = fetchMSPdata("PeakIntensity");
	
	// Check if we got valid peak data
	if (peakData.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("No PeakIntensity data found in MSP file"));
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, TEXT("No PeakIntensity data in file"));
		}
		return;
	}
	
	timeSteps = (int)(peakData.Num() / (6.0f * 181.0f));

	peakTimes = fetchMSPdata("Time");
	
	// Check if we got valid time data
	if (peakTimes.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("No Time data found in MSP file"));
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, TEXT("No Time data in file"));
		}
		return;
	}
	
	startTime = peakTimes[0];
	endTime = peakTimes.Last();

	// Prevent divide by zero
	float timeDiff = endTime - startTime;
	if (FMath::IsNearlyZero(timeDiff))
	{
		UE_LOG(LogTemp, Warning, TEXT("Time range is zero, using default timeSpanUnit"));
		timeSpanUnit = 1.0f;
	}
	else
	{
		//the unit length of 1 hour for this data set
		timeSpanUnit = 3600.0f / timeDiff;
	}

	UE_LOG(LogTemp, Warning, TEXT("peakData length %d"), peakData.Num());
	//peakData.SetNum(15000);


	mspWavelengths = fetchMSPdata("Wavelength");


	

	//peakData[362] = 100000.0f;

	//for (int i = 0; i <= 1000; i++) {
	//	peakData[i] = 100000.0f;
	//}


	// 6 channels * 181 degrees = 1086
	mspDataTexture = CreateTextureFrom32BitFloat(peakData, 1086, peakData.Num() / 1086);

	
	//https://forums.unrealengine.com/t/how-to-pass-large-quantities-of-data-to-materials-for-shader-computations/1178337/6
	materialInstance = UMaterialInstanceDynamic::Create(mspMaterial, this);
	materialInstance->SetTextureParameterValue("MSPdata", mspDataTexture);

	getMspProperties(peakData);

	UE_LOG(LogTemp, Warning, TEXT("max %f"), mspProperties.maxValue);
	UE_LOG(LogTemp, Warning, TEXT("min %f"), mspProperties.minValue);
	UE_LOG(LogTemp, Warning, TEXT("avg %f"), mspProperties.avgValue);
	UE_LOG(LogTemp, Warning, TEXT("first %f, last %f"), peakData[0], peakData.Last());



	materialInstance->SetScalarParameterValue("start", 0.0f);
	materialInstance->SetScalarParameterValue("end", 1.0f);
	materialInstance->SetScalarParameterValue("timeSteps", mspProperties.dataLength);
	materialInstance->SetScalarParameterValue("dataSize", mspProperties.dataLength);
	materialInstance->SetScalarParameterValue("maxValue", mspProperties.maxValue);
	materialInstance->SetScalarParameterValue("minValue", mspProperties.minValue);
	materialInstance->SetScalarParameterValue("avgValue", mspProperties.avgValue);
	// Use auto-computed values that fit the actual data range
	setMSPscalar("div", mspProperties.autoDiv);
	setMSPscalar("a", mspProperties.autoA);
	materialInstance->SetScalarParameterValue("channel", 2.0f - 1.0f);


	


	
	mspWindowPlane->SetMaterial(0, materialInstance);

	// Mark data as loaded so Blueprints know it's safe to access peakTimes/peakData
	bIsDataLoaded = true;
}





TArray<float> AMSPWindow::fetchMSPdata(FString dataKey) {
	if (mspValues.Num() > 0 && mspNames.Num() > 0) {

		const uint32* DataIndex = mspNames.Find(dataKey);
		if (DataIndex && mspValues.IsValidIndex(static_cast<int32>(*DataIndex)))
		{
			return mspValues[*DataIndex].arr;
		}

	}

	return TArray<float>({});
}


void AMSPWindow::getMspProperties(TArray<float> data) {
	// Handle empty data array
	if (data.Num() == 0)
	{
		mspProperties.dataLength = 0;
		mspProperties.maxValue = 0.0f;
		mspProperties.minValue = 0.0f;
		mspProperties.avgValue = 0.0f;
		mspProperties.autoDiv = 0.3f;
		mspProperties.autoA = 0.8f;
		return;
	}
	
	float minV = std::numeric_limits<float>::infinity();
	float maxV = -std::numeric_limits<float>::infinity();

	float sumV = 0.0f;

	for (int i = 0; i < data.Num(); i++) {
		maxV = fmaxf(maxV, data[i]);
		minV = fminf(minV, data[i]);
		sumV += data[i];
	}

	mspProperties.dataLength = data.Num();
	mspProperties.maxValue = maxV;
	mspProperties.minValue = minV;
	mspProperties.avgValue = sumV / (float)data.Num();

	// Auto-compute Color Center (div) and Color Width (a) to fit the data range.
	// The material uses a Lorentzian: y = 1 / (1 + (x - 20000*div)^2 / (10000*a))
	// We want the color window centered on the data midpoint with spread covering the full range.
	float dataCenter = (maxV + minV) / 2.0f;
	float dataRange = maxV - minV;

	// Add 10% padding so extreme values get better color contrast
	dataRange *= 1.1f;

	// Prevent degenerate case where all values are identical
	if (dataRange < 1.0f) dataRange = 1.0f;

	// div positions the center at dataCenter: 20000 * div = dataCenter
	mspProperties.autoDiv = dataCenter / 20000.0f;

	// a controls width: at data edges y = yTarget (0.2), distance from center = dataRange/2
	// Solving: a = (dataRange/2)^2 / (10000 * (1/yTarget - 1))
	// With yTarget = 0.2: (1/0.2 - 1) = 4, so a = dataRange^2 / 160000
	mspProperties.autoA = (dataRange * dataRange) / 160000.0f;

	// Ensure a minimum width so the color gradient is always visible
	mspProperties.autoA = FMath::Max(mspProperties.autoA, 0.01f);
}






//takes inspiration from this post (create a texture out of the data and pass that texture to a render target so material shaders can see it)
//https://www.reddit.com/r/unrealengine/comments/gxmmmr/pass_an_array_of_floatsfcolors_to_material/

UTexture2D* AMSPWindow::CreateTextureFrom32BitFloat(TArray<float> data, int width, int height) {
	
	//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, FString::SanitizeFloat(width) + FString("x") + FString::SanitizeFloat(height));
	
	UTexture2D* texture;
	texture = UTexture2D::CreateTransient(width, height, PF_R32_FLOAT);
	if (!texture) {
		//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, "Failed to create MSP data texture.");
		return nullptr;
	}
	texture->NeverStream = true;
	texture->SRGB = 0;
	texture->LODGroup = TextureGroup::TEXTUREGROUP_Pixels2D;
	FTexture2DMipMap& mip = texture->GetPlatformData()->Mips[0];
	void* dataTarget = mip.BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(dataTarget, data.GetData(), width * height * 4);
	mip.BulkData.Unlock();
	texture->UpdateResource();
	return texture;
}

UTexture2D* AMSPWindow::UpdateTextureFrom32BitFloat(TArray<float> data, int width, int height, UTexture2D* texture) {
	if (texture == nullptr) {
		return CreateTextureFrom32BitFloat(data, width, height);
	}

	FTexture2DMipMap& mip = texture->GetPlatformData()->Mips[0];
	void* dataTarget = mip.BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(dataTarget, data.GetData(), width * height * 4);
	mip.BulkData.Unlock();
	texture->UpdateResource();
	return texture;
}


void AMSPWindow::printTime(float t) {
	if (peakTimes.Num() > 0)
	{
		int index = FMath::Clamp((int)(t * (peakTimes.Num() - 1)), 0, peakTimes.Num() - 1);
		//GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Black, FString::SanitizeFloat(peakTimes[index]));
	}
}
















//LOOK HERE: MEMORY LEAK HERE NO TIME TO FIX NOW 
//does not include stop index
TArray<int> AMSPWindow::intRange(TArray<int> inA, int start, int stop) {
	TArray<int> outA;
	outA.SetNum(stop - start);

	for (int i = start; i < stop; i++) {
		outA[i] = inA[i];
	}

	return outA;
}

TArray<uint32> AMSPWindow::getDimensionVector(TArray<int> ids, int idCount, TArray<FncDimension> dims) {
	TArray<uint32> outDims;
	outDims.SetNum(idCount);

	for (int i = 0; i < idCount; i++) {
		outDims[i] = dims[ids[i]].length;
	}
	return outDims;
}

uint32 AMSPWindow::getSizeOfDimVector(TArray<uint32> dimVector, int dimCount) {
	size_t outS = dimVector[0];
	for (int i = 1; i < dimCount; i++) {
		outS = outS * dimVector[i];
	}
	return outS;
}


// The game module owns the netCDF DLL handle for the lifetime of the application.
static bool IsNetCDFAvailable()
{
	const FnetcdfModule* NetCDFModule = FModuleManager::GetModulePtr<FnetcdfModule>(TEXT("WaveMaker"));
	return NetCDFModule && NetCDFModule->IsAvailable();
}


void AMSPWindow::mspParsingProcedure(FString filename, TMap<FString, uint32>& refNames, TArray<FloatTArray>& refValues, TArray<uint32>& refSizes) {
	refNames.Empty();
	refValues.Empty();
	refSizes.Empty();
	ncTimeSteps = 0.0f;
	ncTimeStepSize = 0.0f;
	mspProperties.dataLength = 0.0f;
	mspProperties.timeStepSize = 0.0f;

	// Check if netCDF library is available before attempting to use it
	if (!IsNetCDFAvailable())
	{
		UE_LOG(LogTemp, Error, TEXT("netcdf.dll not found! Please ensure the netCDF library DLLs are in ThirdParty/netCDF/bin/ or in your system PATH."));
		UE_LOG(LogTemp, Error, TEXT("Required DLLs: netcdf.dll, hdf5.dll, hdf5_hl.dll, zlib1.dll, and their dependencies."));
		
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, TEXT("ERROR: netcdf.dll not found!"));
			GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, TEXT("Copy netCDF DLLs to ThirdParty/netCDF/bin/ folder"));
		}
		
		// Initialize empty arrays to prevent further crashes
		refNames.Empty();
		refValues.Empty();
		refSizes.Empty();
		return;
	}


	//print("mspProcedure running");

	int netcdfID = -1;


	int status = nc_open(TCHAR_TO_ANSI(*filename), NC_SHARE, &netcdfID);
	if (status != NC_NOERR)
	{
		UE_LOG(LogTemp, Error, TEXT("Could not open MSP file %s: %s"), *filename, ANSI_TO_TCHAR(nc_strerror(status)));
		return;
	}

	bool bParsingSucceeded = false;
	ON_SCOPE_EXIT
	{
		nc_close(netcdfID);
		if (!bParsingSucceeded)
		{
			refNames.Empty();
			refValues.Empty();
			refSizes.Empty();
		}
	};

	int ndims = 0;

	int nvars = 0;

	int natts = 0;

	int nunlimdim = -1;

	status = nc_inq(netcdfID, &ndims, &nvars, &natts, &nunlimdim);
	if (status != NC_NOERR || ndims < 0 || nvars < 0 || natts < 0)
	{
		UE_LOG(LogTemp, Error, TEXT("Could not read MSP file metadata: %s"), ANSI_TO_TCHAR(nc_strerror(status)));
		return;
	}

	



	//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::White, FString::SanitizeFloat(nvars));
	//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::White, FString::SanitizeFloat(natts));
	//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::White, FString::SanitizeFloat(ndims));




	//get all of the global attribute information
	TArray<FncAttribute> attribs;
	attribs.SetNum(natts);

	//print("lengths");
	for (int i = 0; i < natts; i++) {

		char attributeName[NC_MAX_NAME + 1] = {};
		size_t attributeLength = 0;
		status = nc_inq_attname(netcdfID, NC_GLOBAL, i, attributeName);
		if (status == NC_NOERR)
		{
			status = nc_inq_att(netcdfID, NC_GLOBAL, attributeName, &attribs[i].type, &attributeLength);
		}
		if (status != NC_NOERR || attributeLength > MAX_uint32 || strlen(attributeName) >= sizeof(attribs[i].name))
		{
			UE_LOG(LogTemp, Error, TEXT("Invalid or unsupported MSP global attribute %d (netCDF status %d)"), i, status);
			return;
		}
		FCStringAnsi::Strncpy(attribs[i].name, attributeName, UE_ARRAY_COUNT(attribs[i].name));
		attribs[i].length = static_cast<uint32>(attributeLength);


		
		UE_LOG(LogTemp, Warning, TEXT("attr %s"), ANSI_TO_TCHAR(attribs[i].name));

		//attribs[i].data.SetNum(attribs[i].length);
		//print(std::to_string(attribs[i].length));
	}


	//get all of the global dimensions


	TArray<FncDimension> dimensions;
	dimensions.SetNum(ndims);

	for (int i = 0; i < ndims; i++) {
		char dimensionName[NC_MAX_NAME + 1] = {};
		size_t dimensionLength = 0;
		status = nc_inq_dim(netcdfID, i, dimensionName, &dimensionLength);
		if (status != NC_NOERR || dimensionLength > MAX_uint32 || strlen(dimensionName) >= sizeof(dimensions[i].name))
		{
			UE_LOG(LogTemp, Error, TEXT("Invalid or unsupported MSP dimension %d (netCDF status %d)"), i, status);
			return;
		}
		FCStringAnsi::Strncpy(dimensions[i].name, dimensionName, UE_ARRAY_COUNT(dimensions[i].name));
		dimensions[i].length = static_cast<uint32>(dimensionLength);
		UE_LOG(LogTemp, Warning, TEXT("dim %s, len %u"), ANSI_TO_TCHAR(dimensions[i].name), dimensions[i].length);


		
		//print(dimensions[i].name);
	}


	//print("variables");

	TArray<FncVariable> variables;
	FString hold;

	variables.SetNum(nvars);


	for (int i = 0; i < nvars; i++) {
		variables[i].id = i;

		TArray<int> tempDimIds;
		status = nc_inq_varndims(netcdfID, i, &variables[i].dimCount);
		if (status != NC_NOERR || variables[i].dimCount < 0 || variables[i].dimCount > NC_MAX_VAR_DIMS)
		{
			UE_LOG(LogTemp, Error, TEXT("Invalid MSP variable dimensions for variable %d (netCDF status %d)"), i, status);
			return;
		}
		tempDimIds.SetNum(variables[i].dimCount);

		char variableName[NC_MAX_NAME + 1] = {};
		status = nc_inq_var(netcdfID, i, variableName, &variables[i].type, nullptr, tempDimIds.GetData(), &variables[i].attCount);
		if (status != NC_NOERR || strlen(variableName) >= sizeof(variables[i].name))
		{
			UE_LOG(LogTemp, Error, TEXT("Invalid or unsupported MSP variable %d (netCDF status %d)"), i, status);
			return;
		}
		FCStringAnsi::Strncpy(variables[i].name, variableName, UE_ARRAY_COUNT(variables[i].name));

		UE_LOG(LogTemp, Warning, TEXT("var %s"), ANSI_TO_TCHAR(variables[i].name));

		variables[i].dimIds = intRange(tempDimIds, 0, variables[i].dimCount);
		//print(variables[i].name);
		//print(variables[i].dimCount);
		//print(variables[i].attCount);

		hold += char(97 + i);

		TArray<FString> listStrs = { variables[i].name, hold, FString::SanitizeFloat(variables[i].dimCount) };




		hold = "";



	}






	//this only works for when each variable has exactly one attribute
	for (int i = 0; i < nvars; i++) {
		//print(FString(variables[i].name));
		//print(std::to_string(variables[i].attCount));


	}




	//load all of the variable data

	refValues.Empty();
	refSizes.Empty();
	refNames.Empty();

	TArray<float> tempData2;

	for (int i = 0; i < variables.Num(); i++) {
		//get base data


		uint64 elementCount = 1; // A scalar variable has one value and no dimensions.
		for (int dimensionId : variables[i].dimIds)
		{
			if (!dimensions.IsValidIndex(dimensionId))
			{
				UE_LOG(LogTemp, Error, TEXT("Invalid dimension ID in MSP variable %s"), ANSI_TO_TCHAR(variables[i].name));
				return;
			}
			elementCount *= dimensions[dimensionId].length;
			// refSizes stores bytes in uint32; TArray stores the element count in int32.
			if (elementCount > MAX_int32 || elementCount > MAX_uint32 / sizeof(float))
			{
				UE_LOG(LogTemp, Error, TEXT("MSP variable %s is too large to load"), ANSI_TO_TCHAR(variables[i].name));
				return;
			}
		}

		tempData2.SetNum(static_cast<int32>(elementCount));
		if (elementCount > 0)
		{
			status = nc_get_var_float(netcdfID, variables[i].id, tempData2.GetData());
			if (status != NC_NOERR)
			{
				UE_LOG(LogTemp, Error, TEXT("Could not read MSP variable %s: %s"), ANSI_TO_TCHAR(variables[i].name), ANSI_TO_TCHAR(nc_strerror(status)));
				return;
			}
		}

		//must append like this because its a 2d array/struct
		refValues.SetNum(refValues.Num() + 1);
		refValues.Last().arr = tempData2;

		// append doesn't work here for some reason
		uint32 a = static_cast<uint32>(elementCount * sizeof(float));
		refSizes.SetNum(refSizes.Num() + 1);
		refSizes.Last() = a;

		refNames.Add(variables[i].name, i);

	}





	//print("data size");
	//print(std::to_string(refSizes[0]));

	const uint32* timeIndex = refNames.Find(TEXT("Time"));
	if (!timeIndex || !refValues.IsValidIndex(static_cast<int32>(*timeIndex)) || refValues[*timeIndex].arr.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("No Time data found in MSP file"));
		return;
	}
	const TArray<float>& timeValues = refValues[*timeIndex].arr;
	ncTimeSteps = timeValues.Num();
	ncTimeStepSize = timeValues.Num() >= 2 ? timeValues[1] - timeValues[0] : 0.0f;





	mspProperties.dataLength = ncTimeSteps;
	mspProperties.timeStepSize = ncTimeStepSize;


	bParsingSucceeded = true;
}


// Safe accessor for peakTimes - returns 0 if index is out of bounds or data not loaded
float AMSPWindow::GetPeakTimeAtIndex(int32 Index) const
{
	if (!bIsDataLoaded || peakTimes.Num() == 0)
	{
		return 0.0f;
	}
	
	if (Index < 0 || Index >= peakTimes.Num())
	{
		return 0.0f;
	}
	
	return peakTimes[Index];
}

// Safe accessor for peakData - returns 0 if index is out of bounds or data not loaded
float AMSPWindow::GetPeakDataAtIndex(int32 Index) const
{
	if (!bIsDataLoaded || peakData.Num() == 0)
	{
		return 0.0f;
	}
	
	if (Index < 0 || Index >= peakData.Num())
	{
		return 0.0f;
	}
	
	return peakData[Index];
}

// Get the number of peak time entries
int32 AMSPWindow::GetPeakTimesCount() const
{
	return peakTimes.Num();
}

// Get the number of peak data entries
int32 AMSPWindow::GetPeakDataCount() const
{
	return peakData.Num();
}

float AMSPWindow::GetTimeAtViewFraction(float fraction) const
{
	if (!bIsDataLoaded || peakTimes.Num() == 0)
	{
		return 0.0f;
	}

	// Use the zoomed range if in direct mode, otherwise use startLoc/endLoc
	float rangeStart = bUseDirectTimeRange ? viewStartLoc : startLoc;
	float rangeEnd = bUseDirectTimeRange ? viewEndLoc : endLoc;

	// Interpolate within the visible range, clamp to valid [0,1] so we never
	// read a time from outside the dataset (avoids negative times on first label)
	float normalizedPos = FMath::Clamp(
		rangeStart + fraction * (rangeEnd - rangeStart), 0.0f, 1.0f);

	// Map to peak times array index
	int32 timeIndex = FMath::Clamp(
		FMath::RoundToInt(normalizedPos * (float)(peakTimes.Num() - 1)),
		0, peakTimes.Num() - 1
	);

	return FMath::Max(peakTimes[timeIndex], 0.0f);
}

FUIPositions AMSPWindow::GetUIPositions() const
{
	FUIPositions Pos;

	FVector origin;
	FVector extent;
	GetActorBounds(true, origin, extent);

	float scale = UWidgetLayoutLibrary::GetViewportScale(GetWorld());

	FVector2D bottomLeft, topRight;
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	UGameplayStatics::ProjectWorldToScreen(PC, origin - extent, bottomLeft);
	UGameplayStatics::ProjectWorldToScreen(PC, origin + extent, topRight);

	bottomLeft /= scale;
	topRight   /= scale;

	float left   = bottomLeft.X;
	float right  = topRight.X;
	float top    = topRight.Y;
	float bottom = bottomLeft.Y;

	Pos.TitlePosition       = FVector2D((left + right) * 0.5f, top - 30.0f);
	Pos.LeftLabelPosition   = FVector2D(left - 20.0f, (top + bottom) * 0.5f);
	Pos.BottomLabelPosition = FVector2D((left + right) * 0.5f, bottom + 45.0f);
	Pos.LegendPosition      = FVector2D(0.0f, 0.0f);
	Pos.GraphSize            = FVector2D(right - left, bottom - top);

	return Pos;
}

