// Class to represent a Directivity for the VA Server

#pragma once


#include "Containers/UnrealString.h"			// FString

class VADirectivity
{

public:
	VADirectivity(FString fileName_);
	VADirectivity(FString fileName_, FString phoneme);
	VADirectivity(FString fileName_, TArray<FString> phonemes);


	void addPhoneme(FString phoneme);
	void addPhoneme(TArray<FString> phoneme);
	
	int getID();
	
	void logInfo();
	

	bool containsPhoneme(FString phoneme);
	
	bool isValid();

	FString getFileName();

protected:
	void createNewDirectivity();

	int dirID;
	FString fileName;
	TArray<FString> phonemes;

};