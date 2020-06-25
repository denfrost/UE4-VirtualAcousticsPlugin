// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "VAUtils.h"

#include "VAReflectionWall.h"
#include "VAPlugin.h"

#include "VA.h"

DEFINE_LOG_CATEGORY(VALog);

void FVAUtils::OpenMessageBox(const FString Text, const bool bError)
{
	if (!FVAPlugin::GetIsMaster())
	{
		return;
	}
	
	FString TrueStatement;
	if (bError)
	{
		TrueStatement = "TRUE";
	}
	else
	{
		TrueStatement = "FALSE";
	}
	
	LogStuff("[FVAUtils::OpenMessageBox(ERROR = " + TrueStatement + ")]: Opening Message Box with message: " + Text);

	if (bError)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(AddExclamationMarkAroundChar(Text)));
	}
	else
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Text));
	}

}


FString FVAUtils::AddExclamationMarkAroundChar(const FString Text)
{
	size_t Length = Text.Len();
	FString ReturnedString = FString("!!!!!");
	ReturnedString.Append(Text).Append("!!!!!");

	return ReturnedString;
}

bool FVAUtils::CheckLibraryHandle(const void* LibraryHandle)
{
	if (LibraryHandle)
	{
		return true;
	}
	return false;
}

bool FVAUtils::FVecToVAVec3(FVector& VecF, VAVec3& VecVA)
{
	if (&VecF == nullptr || &VecVA == nullptr)
	{	
		return false;
	}

	VecVA.Set(VecF.X, VecF.Y, VecF.Z);

	return true;
}


FVector FVAUtils::ToVACoordinateSystem(const FVector VecF, const float Scale)
{
	// ++ VA Server ++//   // ++ Unreal En ++//
	//*****|yP********//   //*****|z****x****//
	//*****|**********//   //*****|***/******//		  / forward
	//*****|********xR//   //*****|*/*******y//		 /
	//*****0----------//   //*****0----------//		/
	//****/***********//   //****************//
	//*zY/************//   //****************//
	const float ScaleInv = 1 / (Scale);
	return FVector(ScaleInv * VecF.Y, ScaleInv * VecF.Z, -ScaleInv * VecF.X);
}

FRotator FVAUtils::ToVACoordinateSystem(const FRotator RotF)
{
	// ++ VA Server ++//   // ++ Unreal En ++//
	//*****|yP********//   //*****|zY**xR****//
	//*****|**/******//   //*****|**/*******//		  / forward
	//*****|*/******xR//   //*****|*/******yP//		 /
	//*****0----------//   //*****0----------//		/
	//****/***********//   //****************//
	//*zY/************//   //****************//
	//
	//	https://gamedev.stackexchange.com/questions/157946/converting-a-quaternion-in-a-right-to-left-handed-coordinate-system
	//
	//			  Unreal			VAServer
	//	forward		x					-z
	//	up			z					y
	//	right		y					x
	//
	//
	//	All *(-1) due to changing direction
	//Quaternion(
	// 	-input.y,			(where VA x?)
	// 	-input.z,			(where VA y?)
	// 	input.x,			(where VA z?)
	// 	input.w		
	// )

	const FQuat Quat = RotF.Quaternion();
	const FQuat Tmp = FQuat(-Quat.Y, -Quat.Z, Quat.X, Quat.W);
	return Tmp.Rotator();
}

bool FVAUtils::FQuatToVAQuat(FQuat& QuatF, VAQuat& QuatVA)
{
	if (&QuatF == nullptr || &QuatVA == nullptr)
	{	
		return false;
	}

	QuatVA.Set(QuatF.X, QuatF.Y, QuatF.Z, QuatF.W);

	return true;
}


FVector FVAUtils::ComputeReflectedPos(const AVAReflectionWall* Wall, const FVector Pos)
{
	const FVector Normal = Wall->GetNormalVector();
	const float D = Wall->GetHessianD();
	const float T = D - FVector::DotProduct(Normal, Pos);

	return (Pos + 2.0 * T * Normal);
}


FRotator FVAUtils::ComputeReflectedRot(const AVAReflectionWall* Wall, const FRotator Rot)
{
	// TODO Maybe make easier computation?
	
	const FVector NormalVec = Wall->GetNormalVector();

	const FVector Direction = Rot.Vector();

	const FVector Pos1 = Wall->GetSupportVector() + (1000 * NormalVec);
	const FVector Pos2 = Pos1 + (500 * Direction);

	const FVector Pos1R = ComputeReflectedPos(Wall, Pos1);
	const FVector Pos2R = ComputeReflectedPos(Wall, Pos2);

	const FVector Tmp = Pos2R - Pos1R;

	FVAUtils::LogStuff(FString("Rot Reflected: " + Tmp.Rotation().ToString()));
	
	return Tmp.Rotation() + FRotator(0,0,2*Wall->GetActorRotation().Pitch-Rot.Roll);
}


void FVAUtils::LogStuff(const FString Text, const bool Error)
{
	if (Error)
	{
		UE_LOG(VALog, Error, TEXT("%s"), *Text);
	}
	else
	{
		UE_LOG(VALog, Log, TEXT("%s"), *Text);
	}
}
