// Copyright Epic Games, Inc. All Rights Reserved.

#include "TestComplexSystemGameMode.h"
#include "TestComplexSystemCharacter.h"
#include "UObject/ConstructorHelpers.h"

ATestComplexSystemGameMode::ATestComplexSystemGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPersonCPP/Blueprints/ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
