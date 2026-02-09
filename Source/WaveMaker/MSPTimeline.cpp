// Fill out your copyright notice in the Description page of Project Settings.


#include "MSPTimeline.h"

#include "MSPWindow.h"

#include "Engine/Texture2D.h"
#include "C:/Program Files/Epic Games/UE_5.1/Engine/Source/Runtime/Core/Public/HAL/UnrealMemory.h"



// Sets default values
AMSPTimeline::AMSPTimeline()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	//timelineMaterial = LoadObject<UMaterial>(nullptr, TEXT("/Script/Engine.Material'/Game/Materials/MSPTimeLineMaterial.MSPTimeLineMaterial'"));
	//timelineMaterial = LoadObject<UMaterial>(nullptr, TEXT("/Script/Engine.Material'/Game/Materials/MSPMaterial.MSPMaterial'"));


	timelinePlane = CreateDefaultSubobject<UStaticMeshComponent>("TimelinePlane");

	Tags.Add("Timeline");

	

}

// Called when the game starts or when spawned
void AMSPTimeline::BeginPlay()
{
	Super::BeginPlay();
	
	SetActorLocation(GetActorLocation() + FVector(-10.0f, 0.0f, -265.0f));


	//timelinePlane->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Script/Engine.StaticMesh'/Engine/BasicShapes/Plane.Plane'")));

	timelinePlane->SetWorldScale3D(FVector(.25f, 2.5f, 1.0f));

	//https://forums.unrealengine.com/t/how-to-pass-large-quantities-of-data-to-materials-for-shader-computations/1178337/6
	materialInstance = UMaterialInstanceDynamic::Create(timelineMaterial, this);

	
	//GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Yellow, GetActorLocation().ToString());


	
	timelinePlane->SetMaterial(0, materialInstance);


	this->OnBeginCursorOver.AddDynamic(this, &AMSPTimeline::cursorEnter);
	this->OnEndCursorOver.AddDynamic(this, &AMSPTimeline::cursorExit);


}

// Called every frame
void AMSPTimeline::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	totalTime += DeltaTime;

	//timelinePlane->SetWorldScale3D(FVector(.1f, .5f, 1.0f) * totalTime / 10.0f);

	//GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::White, GetActorScale().ToString());

	if (textureUpdateQueued && materialInstance) {
		textureUpdateQueued = false;
		materialInstance->SetTextureParameterValue("mspData", timelineDataTexture);

		//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, "Timeline texture Updated");

	}

	if (paramUpdateQueued && materialInstance) {
		paramUpdateQueued = false;

		materialInstance->SetScalarParameterValue("start", 0.0f);
		materialInstance->SetScalarParameterValue("end", 1.0f);
		materialInstance->SetScalarParameterValue("timeSteps", mspDataProps.dataLength);
		materialInstance->SetScalarParameterValue("dataSize", mspDataProps.dataLength);
		materialInstance->SetScalarParameterValue("maxValue", mspDataProps.maxValue);
		materialInstance->SetScalarParameterValue("minValue", mspDataProps.minValue);
		materialInstance->SetScalarParameterValue("avgValue", mspDataProps.avgValue);
		// Use auto-computed values that fit the actual data range
		materialInstance->SetScalarParameterValue("div", mspDataProps.autoDiv);
		materialInstance->SetScalarParameterValue("a", mspDataProps.autoA);
		materialInstance->SetScalarParameterValue("channel", 2.0f - 1.0f);


		//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, "Parameters Updated");
	}

}


void AMSPTimeline::setMSPTimelineScalar(FString name, float value) {

	materialInstance->SetScalarParameterValue(FName(name), value);

	//GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Green, name + " updated: " + FString::SanitizeFloat(value));

}


void AMSPTimeline::setDataTextureWhenReady(TArray<float> data) {

	timelineDataTexture = CreateTextureFrom32BitFloat(data, 1086, data.Num() / 1086);;
	textureUpdateQueued = true;
}



//takes inspiration from this post (create a texture out of the data and pass that texture to a render target so material shaders can see it)
//https://www.reddit.com/r/unrealengine/comments/gxmmmr/pass_an_array_of_floatsfcolors_to_material/

UTexture2D* AMSPTimeline::CreateTextureFrom32BitFloat(TArray<float> data, int width, int height) {

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

UTexture2D* AMSPTimeline::UpdateTextureFrom32BitFloat(TArray<float> data, int width, int height, UTexture2D* texture) {
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






void AMSPTimeline::setTextureParamsWhenReady(FmspDataProperties props) {

	mspDataProps = props;
	paramUpdateQueued = true;

	
	//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, "Parameter Update Queued");

}




void AMSPTimeline::cursorEnter(AActor* TouchedActor) {
	cursorOver = true;
	//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, "Cursor Entered");

}

void AMSPTimeline::cursorExit(AActor* TouchedActor) {
	cursorOver = false;
	//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, "Cursor Exited");

}