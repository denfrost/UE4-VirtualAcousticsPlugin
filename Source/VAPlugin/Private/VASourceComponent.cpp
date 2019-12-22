// Fill out your copyright notice in the Description page of Project Settings.

#include "VASourceComponent.h"
#include "VAReceiverActor.h"
#include "VAPlugin.h"

/*
#include "CoreMinimal.h"

#include "Kismet/GameplayStatics.h"
#include "Array.h"
#include "Components/SkeletalMeshComponent.h"
*/

// Sets default values for this component's properties
UVASourceComponent::UVASourceComponent() {
	PrimaryComponentTick.bCanEverTick = true;
}


// Called when the game starts
void UVASourceComponent::BeginPlay() {
	Super::BeginPlay();

	TArray<AActor*> recActors;
	UGameplayStatics::GetAllActorsOfClass(this->GetWorld(), AVAReceiverActor::StaticClass(), recActors);
	AVAReceiverActor* tmp = nullptr;

	for (AActor* iter : recActors) {
		// if there is an ReceiverActor in the Szene
		tmp = dynamic_cast<AVAReceiverActor*> (iter);
		if (tmp != nullptr) {
			VAUtils::logStuff("[UVASourceComponent::BeginPlay()]: AVAReceiver found");
			break;
		}
	}

	// If no Rec Actor found spawn one with default parameters
	if (tmp == nullptr) {
	 	VAUtils::logStuff("[UVASourceComponent::BeginPlay()]: No AVAReceiver found! Spawning one with default values");
		tmp = this->GetWorld()->SpawnActor<AVAReceiverActor>(AVAReceiverActor::StaticClass());
	}

	// If the receiver Actor is initialized but this sound Component not, this Component is spawned at runtime and has to be initialized
	if (tmp->isInitialized() && !initialized) {
		initialize();
	}

}


// Called every frame
void UVASourceComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) {
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!FVAPluginModule::getUseVA()) {
		return;
	}

	if (!initialized) {
		VAUtils::openMessageBox("A Sound source is not initialized");
	}

	if (firstTick && FVAPluginModule::getIsMaster()) {
		timeSinceUpdate = 1.0f;
		if (startingPlayAction == EPlayAction::Play) {
			soundSource->playSoundFromSecond(startingTime);
		}
	}
	firstTick = false;


	timeSinceUpdate += DeltaTime;

	if ((movementSetting == EMovement::AttatchToBone || movementSetting == EMovement::MoveWithObject) && 
		timeSinceUpdate > (1.0f / 30.0f)) {
		
		soundSource->setPos(getPosition());
		soundSource->setRot(getRotation());

		timeSinceUpdate = 0.0f;
	}

}


void UVASourceComponent::initialize()
{

	if (!FVAPluginModule::getUseVA() || initialized) {
		return;
	}

	ownerActor = GetOwner();
	firstTick = true;


	skeletal_mesh_component = dynamic_cast<USkeletalMeshComponent*> (ownerActor->GetComponentByClass(USkeletalMeshComponent::StaticClass()));
	
	if (movementSetting == EMovement::AttatchToBone) {
		if (skeletal_mesh_component != nullptr 
			&& skeletal_mesh_component->DoesSocketExist(FName(*boneName))) {
			VAUtils::logStuff("Bone detected.");
		}
		else {
			VAUtils::openMessageBox("Error: Could not find bone, using MoveWithObject instead.");
			movementSetting = EMovement::MoveWithObject;
		}
	}

	if (movementSetting == EMovement::ObjectSpawnPoint) {
		spawnPosition = ownerActor->GetTransform().GetLocation();
		spawnRotation = ownerActor->GetTransform().GetRotation().Rotator();
	}

	soundSource = new VASoundSource(this);

	if (FVAPluginModule::getIsMaster()) {
		switch (directivitySetting) {
			case EDir::DefaultHRIR : 
				soundSource->setDirectivity(VADirectivityManager::getDefaultSourceDirectivity());
				break;

			case EDir::manualFile :
				soundSource->setDirectivity(AVAReceiverActor::getCurrentReceiverActor()->getDirectivityByFileName(directivityByFileName));
				break;

			case EDir::phoneme :
				soundSource->setDirectivity(AVAReceiverActor::getCurrentReceiverActor()->getDirectivityByPhoneme(directivityByPhoneme));
				break;

			default:
				break;
		}
	}
	
	VAUtils::logStuff("SoundSourceComponent initialized successfully");

	initialized = true;
}

// ****************************************************************** // 
// ******* Interface Sound ****************************************** //
// ****************************************************************** //

void UVASourceComponent::playSound()
{
    if (!FVAPluginModule::getUseVA()) {
		return;
    }

	soundSource->playSound();
}

void UVASourceComponent::playSoundFromSecond(float time)
{
	if (!FVAPluginModule::getUseVA()) {
		return;
	}

	soundSource->playSoundFromSecond(time);
}

void UVASourceComponent::stopSound()
{
	if (!FVAPluginModule::getUseVA()) {
		return;
	}

	soundSource->stopSound();
}

void UVASourceComponent::pauseSound()
{
	if (!FVAPluginModule::getUseVA()) {
		return;
    }

	soundSource->pauseSound();
}

EPlayAction UVASourceComponent::getPlayState()
{
	return VAUtils::VAActionToEPlayAction(soundSource->getPlayState());
}

// ****************************************************************** // 
// ******* Sound Pose *********************************************** //
// ****************************************************************** //

FVector UVASourceComponent::getPosition()
{
	FVector pos; 
	switch (movementSetting) {
		case EMovement::MoveWithObject:
			pos = ownerActor->GetTransform().GetLocation();
			break;

		case EMovement::ObjectSpawnPoint:
			pos = spawnPosition;
			break;

		case EMovement::AttatchToBone:
			pos = skeletal_mesh_component->GetSocketLocation(FName(*boneName));
			break;
		
		default:
			pos = FVector::ZeroVector;
			VAUtils::logStuff(FString("[UVASourceComponent::getPosition()]: default"));
			break;
	}

	if (usePoseOffset) {
		pos = pos + offsetPosition;
	}

	return pos;
}

FRotator UVASourceComponent::getRotation()
{
	FRotator rot;
	switch (movementSetting) {
	case EMovement::MoveWithObject:
		rot = ownerActor->GetTransform().GetRotation().Rotator();
		break;

	case EMovement::ObjectSpawnPoint:
		rot = spawnRotation;
		break;

	case EMovement::AttatchToBone:
		rot = skeletal_mesh_component->GetSocketRotation(FName(*boneName));
		break;

	default:
		VAUtils::logStuff(FString("[UVASourceComponent::getRotation()]: default"));
		rot = FRotator::ZeroRotator;
		break;
	}

	if (usePoseOffset) {
		FQuat quat(rot);
		quat = quat * FQuat(offsetRotation);
		rot = FRotator(quat);
	}

	return rot;
}


// ****************************************************************** // 
// ******* Sound Settings ******************************************* //
// ****************************************************************** //

void UVASourceComponent::setSoundSourceVisibility(bool vis)
{
  if (!FVAPluginModule::getUseVA()) {
    return;
  }
	soundSource->setVisibility(vis);
}

void UVASourceComponent::setGainFactor(float gainFactor_)
{
	// TODO: setGainFactor
	VAUtils::logStuff("UVASourceComponent::setGainFactor not working yet!!", true);
	return;
}

void UVASourceComponent::setSoundFile(FString soundFile_)
{
	// If already playing back that sound File
	if (soundFile == soundFile_) {
		soundSource->stopSound();
		return;
	}

	soundFile = soundFile_;
	
	if (!initialized || !FVAPluginModule::getIsMaster()) {
		return;
	}

	soundSource->setNewSound(soundFile);	
}

void UVASourceComponent::setLoop(bool loop_) {

	// If this setting is already set properly
	if (loop == loop_) {
		return;
	}

	loop = loop_;

	if (!initialized || !FVAPluginModule::getIsMaster()) {
		return;
	}


	soundSource->setLoop(loop);
}


// ****************************************************************** // 
// ******* Directivity stuff **************************************** //
// ****************************************************************** //

void UVASourceComponent::setDirectivityByPhoneme(FString phoneme)
{
	if (!FVAPluginModule::getUseVA()) {
		return;
	}
	soundSource->setDirectivity(AVAReceiverActor::getCurrentReceiverActor()->getDirectivityByPhoneme(phoneme));
}

void UVASourceComponent::setDirectivityByFileName(FString fileName)
{
	if (!FVAPluginModule::getUseVA()) {
		return;
	}
	soundSource->setDirectivity(AVAReceiverActor::getCurrentReceiverActor()->getDirectivityByFileName(fileName));
}



// ****************************************************************** // 
// ******* Getter Functions ***************************************** //
// ****************************************************************** //

bool UVASourceComponent::getVisibility()
{
	return FVAPluginModule::isInDebugMode();
}

bool UVASourceComponent::getHandleReflections() {
	return handleReflections;
}

bool UVASourceComponent::getLoop() {
	return loop;
}

float UVASourceComponent::getGainFactor() {
	return gainFactor;
}

float UVASourceComponent::getSoundTimeOffset() {
	return startingTime;
}

FString UVASourceComponent::getFileName() {
	return soundFile;
}


// ****************************************************************** // 
// ******* Blueprint Settings *************************************** //
// ****************************************************************** //

#if WITH_EDITOR
bool UVASourceComponent::CanEditChange(const UProperty* InProperty) const
{
	const bool ParentVal = Super::CanEditChange(InProperty);

	// Check Bone Name
	if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UVASourceComponent, boneName))
	{
		return movementSetting == EMovement::AttatchToBone;
	}

	// Check Directivity Config
	if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UVASourceComponent, directivityByFileName))
	{
		return directivitySetting == EDir::manualFile;
	}

	// Check Bone Name
	if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UVASourceComponent, directivityByPhoneme))
	{
		return directivitySetting == EDir::phoneme;
	}


	return ParentVal;
}
#endif