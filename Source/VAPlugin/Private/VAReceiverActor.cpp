// Fill out your copyright notice in the Description page of Project Settings.
// what's up?

#include "VAReceiverActor.h"

#include "VAPlugin.h"
#include "VAUtils.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "VADefines.h"

#include "DisplayClusterPawn.h"
#include "DisplayClusterSceneComponent.h"

#include "Engine.h"									 // For Events
#include "IDisplayCluster.h"						 // For Events
#include "IDisplayClusterClusterManager.h"

#include "VirtualRealityPawn.h"

#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"


AVAReceiverActor* AVAReceiverActor::CurrentReceiverActor;

// ****************************************************************** // 
// ******* Initialization Functions ********************************* //
// ****************************************************************** //

AVAReceiverActor::AVAReceiverActor()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AVAReceiverActor::BeginPlay()
{
	Super::BeginPlay();

	FVAPlugin::SetReceiverActor(this);

	CurrentReceiverActor = this;

	//try to start (remote) VAServer automatically
	const bool StartedVAServer = FVAPlugin::RemoteStartVAServer(GetIPAddress(), RemoteVAStarterPort,
	                                                                  WhichVAServerVersionToStart);
	// Ask if used or not
	FVAPlugin::AskForSettings(GetIPAddress(), GetPort(), bAskForDebugMode, !StartedVAServer);

	if (FVAPlugin::GetIsMaster())
	{
		if (FVAPlugin::GetUseVA())
		{
			RunOnAllNodes("useVA = true");
		}
		else
		{
			RunOnAllNodes("useVA = false");
			return;
		}
	}

	bWallsInitialized = false;

	TimeSinceUpdate = 0.0f;
	TotalTime = 0.0f;

	// Cluster Stuff for Events //
	IDisplayClusterClusterManager* ClusterManager = IDisplayCluster::Get().GetClusterMgr();
	if (ClusterManager && !ClusterEventListenerDelegate.IsBound())
	{
		ClusterEventListenerDelegate = FOnClusterEventListener::CreateUObject(
			this, &AVAReceiverActor::HandleClusterEvent);
		ClusterManager->AddClusterEventListener(ClusterEventListenerDelegate);
	}

	if (!FVAPlugin::GetUseVA())
	{
		return;
	}


	if (FVAPlugin::GetIsMaster())
	{
		FVAPlugin::SetScale(WorldScale);

		if (!FVAPlugin::IsConnected())
		{
			FVAPlugin::ConnectServer(GetIPAddress(), GetPort());
		}
		else
		{
			FVAPlugin::ResetServer();
		}

		// Initialize Receiver Actor
		ReceiverID = FVAPlugin::CreateNewSoundReceiver(this);

		// Initialize the dirManager
		DirManager.ResetManager();
		DirManager.ReadConfigFile(DirMappingFileName);

		// Initialize the hrirManager
		HRIRManager.ResetManager();
	}

	// Initialize Walls for Sound Reflection
	if (!bWallsInitialized)
	{
		InitializeWalls();
	}


	// Handle all sound Sources
	TArray<AActor*> ActorsA;
	UGameplayStatics::GetAllActorsOfClass(this->GetWorld(), AActor::StaticClass(), ActorsA);

	for (AActor* EntryActor : ActorsA)
	{
		TArray<UActorComponent*> VASourceComponents = EntryActor->GetComponentsByClass(UVASourceComponent::StaticClass());
		for (UActorComponent* EntrySourceComponent : VASourceComponents)
		{
			Cast<UVASourceComponent>(EntrySourceComponent)->Initialize();
		}
	}

	if (FVAPlugin::GetIsMaster())
	{
		if (FVAPlugin::GetDebugMode())
		{
			RunOnAllNodes("debugMode = true");
		}
		else
		{
			RunOnAllNodes("debugMode = false");
		}
	}

	bInitialized = true;
}

void AVAReceiverActor::BeginDestroy()
{
	Super::BeginDestroy();

	FVAPlugin::ResetServer();

	DirManager.ResetManager();
	HRIRManager.ResetManager();

	IDisplayClusterClusterManager* ClusterManager = IDisplayCluster::Get().GetClusterMgr();
	if (ClusterManager && ClusterEventListenerDelegate.IsBound())
	{
		ClusterManager->RemoveClusterEventListener(ClusterEventListenerDelegate);
	}
}

void AVAReceiverActor::InitializeWalls()
{
	TArray<AActor*> WallsA;
	UGameplayStatics::GetAllActorsOfClass(this->GetWorld(), AVAReflectionWall::StaticClass(), WallsA);
	for (AActor* EntryWalls : WallsA)
	{
		ReflectionWalls.Add(static_cast<AVAReflectionWall*>(EntryWalls));
	}
	bWallsInitialized = true;
}

void AVAReceiverActor::SetUpdateRate(const int Rate)
{
	UpdateRate = Rate;
}

// ****************************************************************** // 
// ******* Tick Function ******************************************** //
// ****************************************************************** //

void AVAReceiverActor::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!FVAPlugin::GetUseVA() || !FVAPlugin::GetIsMaster())
	{
		return;
	}

	TimeSinceUpdate += DeltaTime;
	TotalTime += DeltaTime;

	if (TimeSinceUpdate > (1.0f / float(UpdateRate)))
	{
		UpdateVirtualWorldPose();
		UpdateRealWorldPose();

		TimeSinceUpdate = 0.0f;
	}
}


// ****************************************************************** // 
// ******* Position updates ***************************************** //
// ****************************************************************** //

bool AVAReceiverActor::UpdateVirtualWorldPose()
{
	FVector ViewPos;
	FRotator ViewRot;
	
	GetWorld()->GetFirstPlayerController()->GetPlayerViewPoint(ViewPos, ViewRot);

	FVAPlugin::SetSoundReceiverPosition(ReceiverID, ViewPos);
	FVAPlugin::SetSoundReceiverRotation(ReceiverID, ViewRot);

	return false;
}

bool AVAReceiverActor::UpdateRealWorldPose()
{
	if (!(FVAPlugin::GetIsMaster() && FVAPlugin::GetUseVA()))
	{
		return false;
	}

	if (GetWorld() == nullptr)
	{
		return false;
	}

	APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	if (PlayerController == nullptr)
	{
		return false;
	}

	AVirtualRealityPawn* VirtualRealityPawn = dynamic_cast<AVirtualRealityPawn*>(PlayerController->AcknowledgedPawn);
	if (VirtualRealityPawn == nullptr)
	{
		return false;
	}

	USceneComponent* Head	= VirtualRealityPawn->GetHeadComponent();
	USceneComponent* Origin = VirtualRealityPawn->GetTrackingOriginComponent();

	if (!Head || !Origin)
	{
		return false;
	}

	// calculate positions
	const FQuat InverseOriginRot	= Origin->GetComponentQuat().Inverse();
	const FVector Pos				= InverseOriginRot.RotateVector(
		Head->GetComponentLocation() - Origin->GetComponentLocation());
	const FQuat Quat				= InverseOriginRot * Head->GetComponentQuat();

	return FVAPlugin::SetSoundReceiverRealWorldPose(ReceiverID, Pos, Quat.Rotator());
}


// ****************************************************************** // 
// ******* Directivity / HRIR Handling ****************************** //
// ****************************************************************** //

FVADirectivity* AVAReceiverActor::GetDirectivityByMapping(const FString Phoneme) const
{
	return DirManager.GetDirectivityByPhoneme(Phoneme);
}

FVADirectivity* AVAReceiverActor::GetDirectivityByFileName(const FString FileName)
{
	return DirManager.GetDirectivityByFileName(FileName);
}

void AVAReceiverActor::ReadDirMappingFile(const FString FileName)
{
	if (DirManager.GetFileName() == FileName)
	{
		FVAUtils::LogStuff("[AVAReceiverActor::readDirMappingFile()]: file already loaded");
		return;
	}

	DirMappingFileName = FileName;
	DirManager.ResetManager();
	DirManager.ReadConfigFile(DirMappingFileName);
}

void AVAReceiverActor::SetHRIRByFileName(const FString FileName)
{
	FVAPlugin::SetSoundReceiverHRIR(ReceiverID, HRIRManager.GetHRIRByFileName(FileName)->GetID());
}


// ****************************************************************** // 
// ******* Getter Functions ***************************************** //
// ****************************************************************** //

bool AVAReceiverActor::IsInitialized() const
{
	return bInitialized;
}

float AVAReceiverActor::GetScale() const
{
	return WorldScale;
}


FString AVAReceiverActor::GetIPAddress() const
{
	switch (AddressSetting)
	{
	case Automatic:
#if PLATFORM_WINDOWS
		return FString("127.0.0.1");
#else
		return FString("10.0.1.240");
#endif
		break;
	case Cave:
		return FString("10.0.1.240");
		break;
	case Localhost:
		return FString("127.0.0.1");
		break;
	case Manual:
		return ServerIPAddress;
		break;
	default:
		break;
	}

	FVAUtils::LogStuff("Error in AVAReceiverActor::getIPAddress()", true);

	return FString("127.0.0.1");
}

int AVAReceiverActor::GetPort() const
{
	switch (AddressSetting)
	{
	case Automatic:
	case Cave:
	case Localhost:
		return 12340;
		break;
	case Manual:
		return ServerPort;
		break;
	default:
		break;
	}

	FVAUtils::LogStuff("Error in AVAReceiverActor::getPort()", true);

	return -1;
}

int AVAReceiverActor::GetUpdateRate() const
{
	return UpdateRate;
}

TArray<AVAReflectionWall*> AVAReceiverActor::GetReflectionWalls()
{
	if (!bWallsInitialized)
	{
		InitializeWalls();
	}
	return ReflectionWalls;
}

AVAReceiverActor* AVAReceiverActor::GetCurrentReceiverActor()
{
	return CurrentReceiverActor;
}


// ****************************************************************** // 
// ******* Cluster Stuff ******************************************** // 
// ****************************************************************** //

void AVAReceiverActor::RunOnAllNodes(const FString Command)
{
	IDisplayClusterClusterManager* const Manager = IDisplayCluster::Get().GetClusterMgr();
	if (Manager)
	{
		if (Manager->IsStandalone())
		{
			//in standalone (e.g., desktop editor play) cluster events are not executed....
			HandleClusterCommand(Command);
			FVAUtils::LogStuff("Cluster Command " + Command + " ran local");
		}
		else
		{
			// else create a cluster event to react to
			FDisplayClusterClusterEvent ClusterEvent;
			ClusterEvent.Name		= Command;
			ClusterEvent.Category	= "VAPlugin";
			ClusterEvent.Type		= "command";
			
			Manager->EmitClusterEvent(ClusterEvent, true);

			FVAUtils::LogStuff("Cluster Command " + Command + " sent");
		}
	}
}

void AVAReceiverActor::HandleClusterEvent(const FDisplayClusterClusterEvent& Event)
{
	if (Event.Category == "VAPlugin" && Event.Type == "command")
	{
		HandleClusterCommand(Event.Name);
	}
}

void AVAReceiverActor::HandleClusterCommand(const FString Command)
{
	FVAUtils::LogStuff("Cluster Command " + Command + " received");
	if (Command == "useVA = true")
	{
		FVAPlugin::SetUseVA(true);
	}
	else if (Command == "useVA = false")
	{
		FVAPlugin::SetUseVA(false);
	}
	else if (Command == "debugMode = true")
	{
		FVAPlugin::SetDebugMode(true);
	}
	else if (Command == "debugMode = false")
	{
		FVAPlugin::SetDebugMode(false);
	}
	else
	{
		FVAUtils::LogStuff("Cluster Command " + Command + " could not have been found.");
	}
}

// ****************************************************************** // 
// ******* Blueprint Settings *************************************** // 
// ****************************************************************** //

#if WITH_EDITOR
bool AVAReceiverActor::CanEditChange(const UProperty* InProperty) const
{
	// Check manual Address
	if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(AVAReceiverActor, ServerIPAddress))
	{
		return AddressSetting == Manual;
	}

	// Check manual Port
	if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(AVAReceiverActor, ServerPort))
	{
		return AddressSetting == Manual;
	}

	return true;
}
#endif
