// Fill out your copyright notice in the Description page of Project Settings.


#include "MSPWindow.h"




#include <stdio.h>
#include <stdlib.h>

#include <tchar.h>

#include <string>
#include <string.h>
#include <sstream>


#include "Engine/Texture2D.h"
#include "C:/Program Files/Epic Games/UE_5.1/Engine/Source/Runtime/Core/Public/HAL/UnrealMemory.h"

#include "Materials/MaterialInstance.h"
#include "Materials/MaterialLayersFunctions.h"
#include "Materials/MaterialInstanceDynamic.h"

#include "MSPTimeline.h"




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

	mspWindowPlane->SetWorldScale3D(FVector(2.6f, 9.0f, 1.0f));

	if (!mspMaterial) {
		FGenericPlatformMisc::RequestExit(false);
	}

	

	
	mspMaterial->GetAllParametersOfType(EMaterialParameterType::Texture, mspParameterInfo);

	for (const TPair<FMaterialParameterInfo, FMaterialParameterMetadata>& info : mspParameterInfo) {
		//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Black, info.Key.Name.ToString());
	}



	//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, FString("Max mips") + FString::SanitizeFloat(MAX_TEXTURE_MIP_COUNT));




	timelineDisplay = GetWorld()->SpawnActor<AMSPTimeline>(timelineClass, GetActorLocation(), GetActorRotation());
	

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
	if (Cast<APlayerController>(GEngine->GetFirstLocalPlayerController(GetWorld()))->GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, false, cursorHit)) {
		//if its a main msp window then get the hovered over time and value
		if (cursorHit.GetActor()->ActorHasTag("MSPWindow")) {
			APlayerController* viewer = Cast<APlayerController>(GEngine->GetFirstLocalPlayerController(GetWorld()));

			FVector2D mouseLoc;
			viewer->GetMousePosition(mouseLoc.X, mouseLoc.Y);

			FVector boundOrigin;
			FVector boundExtents;

			cursorHit.GetActor()->GetActorBounds(true, boundOrigin, boundExtents);


			FVector2D topRight;
			viewer->ProjectWorldLocationToScreen(boundOrigin + boundExtents, topRight);
			FVector2D bottomLeft;
			viewer->ProjectWorldLocationToScreen(boundOrigin - boundExtents, bottomLeft);

			FVector2D MUV = (mouseLoc - bottomLeft) / (topRight - bottomLeft);

			

			mouseUV.X = MUV.X;
			mouseUV.Y = MUV.Y;



			FVector2D iMouseLoc = mouseLoc - bottomLeft;
			iMouseLoc.Y *= -1.0f;

			float timeSize = 181.0f * 6.0f;

			
			
			float valuepx = (MUV.X * (endLoc - startLoc)) + startLoc;
			float valuepy = ((float)channel + (1.0f - MUV.Y));
			float valuep = valuepx - valuepy;


			int vIndx = (int)((valuepx * peakData.Num()) / timeSize) * (int)timeSize;
			int vIndy = (int)(valuepy * 181.0f);

			int vInd = vIndx + vIndy;


			int valueIndex = (int)(valuep * (float)peakData.Num());
			int timeIndex = (int)(((MUV.X * (endLoc - startLoc)) + startLoc) * (float)peakTimes.Num());

			//GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Yellow, FString::SanitizeFloat( vIndy ));

			//GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::White, (mouseLoc - bottomLeft).ToString());

			vInd = FMath::Clamp(vInd, 0, peakData.Num() - 1);
			timeIndex = FMath::Clamp(timeIndex, 0, peakTimes.Num() - 1);


			mspHoverValue = peakData[vInd];
			mspHoverTime = peakTimes[timeIndex];

		}
	}

	


	if (timeSpanUnit > 0.0f) {
		setMSPscalar("start", timeLoc - (timeSpanUnit / 2.0f));
		setMSPscalar("end", timeLoc + (timeSpanUnit / 2.0f));

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

	if (paramName == "channel") {
		channel = (int)value;
	}
	else if (paramName == "start") {
		startLoc = value;
	}
	else if (paramName == "end") {
		endLoc = value;
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
	//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, mspNames[0]);

	//loop through each variable name and get its data length
	for (const TPair<FString, uint32>& pair : mspNames) {
		//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, pair.Key + " " + FString::SanitizeFloat(mspValues[pair.Value].arr.Num()));

	}

	peakData = fetchMSPdata("PeakIntensity");
	
	timeSteps = (int)(peakData.Num() / (6.0f * 181.0f));

	peakTimes = fetchMSPdata("Time");
	startTime = peakTimes[0];
	endTime = peakTimes.Last();

	//the unit length of 1 hour for this data set
	timeSpanUnit = 3600.0f / (endTime - startTime);

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
	materialInstance->SetScalarParameterValue("div", 0.3f);
	materialInstance->SetScalarParameterValue("a", 0.8f);
	materialInstance->SetScalarParameterValue("channel", 2.0f - 1.0f);


	


	
	mspWindowPlane->SetMaterial(0, materialInstance);

}





TArray<float> AMSPWindow::fetchMSPdata(FString dataKey) {
	if (mspValues.Num() > 0 && mspNames.Num() > 0) {

		return mspValues[mspNames[dataKey]].arr;

	}

	return TArray<float>({});
}


void AMSPWindow::getMspProperties(TArray<float> data) {
	float minV = INFINITY;
	float maxV = -INFINITY;
	
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
	FTexture2DMipMap& mip = texture->PlatformData->Mips[0];
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

	FTexture2DMipMap& mip = texture->PlatformData->Mips[0];
	void* dataTarget = mip.BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(dataTarget, data.GetData(), width * height * 4);
	mip.BulkData.Unlock();
	texture->UpdateResource();
	return texture;
}


void AMSPWindow::printTime(float t) {
	//GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Black, FString::SanitizeFloat(peakTimes[(int)(t * (peakTimes.Num() - 1))]));
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




void AMSPWindow::mspParsingProcedure(FString filename, TMap<FString, uint32>& refNames, TArray<FloatTArray>& refValues, TArray<uint32>& refSizes) {


	//print("mspProcedure running");

	int netcdfID;


	nc_open(TCHAR_TO_ANSI(*filename), NC_SHARE, &netcdfID);

	int ndims;

	int nvars;

	int natts;

	int nunlimdim;

	nc_inq(netcdfID, &ndims, &nvars, &natts, &nunlimdim);

	



	//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::White, FString::SanitizeFloat(nvars));
	//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::White, FString::SanitizeFloat(natts));
	//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::White, FString::SanitizeFloat(ndims));




	//get all of the global attribute information
	TArray<FncAttribute> attribs;
	attribs.SetNum(natts);

	const char* charholder = new char[128];
	//print("lengths");
	for (int i = 0; i < natts; i++) {

		nc_inq_att(netcdfID, i, attribs[i].name, &attribs[i].type, (size_t*)&attribs[i].length);


		
		UE_LOG(LogTemp, Warning, TEXT("attr %s"), ANSI_TO_TCHAR(attribs[i].name));

		//attribs[i].data.SetNum(attribs[i].length);
		//print(std::to_string(attribs[i].length));
	}

	delete[] charholder;

	//get all of the global dimensions


	TArray<FncDimension> dimensions;
	dimensions.SetNum(ndims);

	for (int i = 0; i < ndims; i++) {
		nc_inq_dim(netcdfID, i, dimensions[i].name, (size_t*)&dimensions[i].length);
		UE_LOG(LogTemp, Warning, TEXT("dim %s, len %d"), ANSI_TO_TCHAR(dimensions[i].name), dimensions[i].length);


		
		//print(dimensions[i].name);
	}


	//print("variables");

	TArray<FncVariable> variables;
	FString hold;

	variables.SetNum(nvars);


	for (int i = 0; i < nvars; i++) {
		variables[i].id = i;

		TArray<int> tempDimIds;
		tempDimIds.SetNum(ndims);

		nc_inq_var(netcdfID, i, variables[i].name, &variables[i].type, &variables[i].dimCount, tempDimIds.GetData(), &variables[i].attCount);

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

	int testVar = 2;


	//print("here");

	//print(FString(variables[testVar].name));

	//print(std::to_string(variables[testVar].dimCount));

	//print(std::to_string(variables[testVar].dimIds[0]));

	for (int i = 0; i < variables[testVar].dimCount; i++) {
		//print(FString(dimensions[variables[testVar].dimIds[i]].name));

	}



	//load all of the variable data

	refValues.Empty();
	refSizes.Empty();
	refNames.Empty();

	TArray<float> tempData2;

	for (int i = 0; i < variables.Num(); i++) {
		//get base data


		float* tempData = new float[getSizeOfDimVector(getDimensionVector(variables[i].dimIds, variables[i].dimCount, dimensions), variables[i].dimCount)];



		tempData2.Empty();
		tempData2.SetNum(getSizeOfDimVector(getDimensionVector(variables[i].dimIds, variables[i].dimCount, dimensions), variables[i].dimCount));

		nc_get_var_float(netcdfID, variables[i].id, &tempData2[0]);

		//must append like this because its a 2d array/struct
		refValues.SetNum(refValues.Num() + 1);
		refValues.Last().arr = tempData2;

		// append doesn't work here for some reason
		size_t a = getSizeOfDimVector(getDimensionVector(variables[i].dimIds, variables[i].dimCount, dimensions), variables[i].dimCount) * (size_t)sizeof(float);
		refSizes.SetNum(refSizes.Num() + 1);
		refSizes.Last() = a;

		refNames.Add(variables[i].name, i);

		delete[] tempData;
	}





	//print("data size");
	//print(std::to_string(refSizes[0]));

	ncTimeSteps = refValues[0].arr.Num();
	ncTimeStepSize = refValues[0][1] - refValues[0][0];




	for (int i = 0; i < fminf(refSizes[0], static_cast<size_t>(1000)); i++) {
		//print(std::to_string(refValues[0][i]));
	}

	mspProperties.dataLength = ncTimeSteps;
	mspProperties.timeStepSize = ncTimeStepSize;


	nc_close(netcdfID);
}



