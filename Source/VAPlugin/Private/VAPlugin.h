#pragma once

#include "Modules/ModuleManager.h"
#include <string>						// std::string
#include "VAEnums.h"					// EPlayState
#include "Sockets.h"

#define VANET_STATIC
#define VABASE_STATIC
#define VA_STATIC


//forward declarations:
class AVAReceiverActor;		 
class AVAReflectionWall;	

// Interface Classes
class CVAException;			
class IVANetClient;
class IVAInterface;
class VAQuat;
class VAVec3;

class FVAPlugin : public IModuleInterface
{
public:

	// process / output CVAException //
	static void ProcessException(FString Location, CVAException Exception);
	static void ProcessException(FString Location, FString ExceptionString);


	// ******* Initialization Functions ******* //

	// Function called when Starting up the module //
	void StartupModule() override;

	// Function called when Shutting down the module //
	void ShutdownModule() override;

	// Asks whether to use the VA Server and / or the debug mode
	static void AskForSettings(FString Host = "unknown", int Port = 0, bool bAskForDebugMode = true,
	                           bool bAskForUseVA = true);

	// Check if all Library Handles are well initialized //
	static bool CheckLibraryHandles();


	// ******* General Server Functions ******* //

	// connect to Server (called by initializeServer) //
	static bool ConnectServer(FString HostF = "localhost", int Port = 12340);

	// reset Server //
	static bool ResetServer();

	// check if VA Server is connected //
	static bool IsConnected();

	// Disconnect from VA Server 
	static bool DisconnectServer();

	// Remote Start VAServer
	static bool RemoteStartVAServer(const FString& Host = "localhost", int Port = 41578,
	                                const FString& VersionName = TEXT("2018.a"));

	void BeginSession(bool bSomething);
	void EndSession(bool bSomething);


	// ******* Sound Buffer ******* //

	static std::string CreateNewBuffer(FString SoundFileName, bool bLoop = false, float SoundOffset = 0.0f);
	static bool SetSoundBufferAction(std::string BufferID, EPlayAction::Type Action);
	static int GetSoundBufferAction(std::string BufferID);
	static bool SetSoundBufferTime(std::string BufferID, float Time);
	static bool SetSoundBufferLoop(std::string BufferID, bool bLoop);


	// ******* Sound Sources ******* //

	static int CreateNewSoundSource(std::string BufferID, std::string Name, FVector Pos = FVector(0, 0, 0),
		FRotator Rot = FRotator(0, 0, 0), float Power = -1.0f);
	static bool SetSoundSourcePosition(int SoundSourceID, FVector Pos);
	static bool SetSoundSourceRotation(int SoundSourceID, FRotator Rot);
	static bool SetNewBufferForSoundSource(int SoundSourceID, std::string BufferID);
	static bool SetSoundSourceMuted(int SoundSourceID, bool bMuted);
	static bool SetSoundSourcePower(int SoundSourceID, float Power);


	// ******* Directivities ******* //

	static int CreateNewDirectivity(FString FileName);
	static bool SetSoundSourceDirectivity(int SoundSourceID, int DirectivityID);
	static bool RemoveSoundSourceDirectivity(int SoundSourceID);


	// ******* HRIR ******* //

	static int CreateNewHRIR(FString FileName);
	static bool SetSoundReceiverHRIR(int SoundReceiverID, int HRIRID);


	// ******* Sound Receiver ******* //

	static int CreateNewSoundReceiver(AVAReceiverActor* Actor);
	static bool SetSoundReceiverPosition(int SoundReceiverID, FVector Pos);
	static bool SetSoundReceiverRotation(int SoundReceiverID, FRotator Rot);


	// ******* Real World ******* //

	static bool SetSoundReceiverRealWorldPose(int SoundReceiverID, FVector Pos, FRotator Rot);


	// ******* General Setter Functions ******* //

	static void SetReceiverActor(AVAReceiverActor* Actor);
	static void SetScale(float ScaleN);
	static void SetUseVA(bool bUseVAN);
	static void SetDebugMode(bool DebugModeN);


	// ******* Getter Functions ******* //

	static bool GetIsInitialized();
	static bool GetUseVA();
	static bool GetDebugMode();
	static bool GetIsMaster();
	static bool ShouldInteractWithServer();
	static AVAReceiverActor* GetReceiverActor();


protected:


	// Library Handles for dll loading of VA Classes
	static void* LibraryHandleBase;
	static void* LibraryHandleNet;
	static void* LibraryHandleVistaBase;
	static void* LibraryHandleVistaAspects;
	static void* LibraryHandleVistaInterProcComm;


	// States of the plugin
	static bool bPluginInitialized; // To check if its already initialized
	static bool bUseVA; // bool if VA is used 
	static bool bDebugMode; // bool if is in Debug Mode
	static bool bIsMaster; // bool if its the master node 


	// Interface Classes to Server 
	static IVANetClient* VANetClient; // VA Net Client
	static IVAInterface* VAServer; // VA Server Interface


	// Link to the current receiver actor 
	static AVAReceiverActor* ReceiverActor;


	// Scale of the UE4 world (how many units is 1m in "real life")
	static float WorldScale;


	// tmp Var for easier usage
	static TSharedPtr<VAQuat> TmpVAQuatSharedPtr;
	static TSharedPtr<VAVec3> TmpVAVec3SharedPtr;

	static VAQuat* TmpVAQuat;
	static VAVec3* TmpVAVec3;


	//Socket connection to the VAServer Launcher, has to be held open until the program ends
	static FSocket* VAServerLauncherSocket;
};
