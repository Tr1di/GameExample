// Copyright Epic Games, Inc. All Rights Reserved.

#include "SIAIEGameMode.h"
#include "SIAIEHUD.h"
#include "SIAIECharacter.h"
#include "UObject/ConstructorHelpers.h"

ASIAIEGameMode::ASIAIEGameMode()
	: Super()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/FirstPersonCPP/Blueprints/FirstPersonCharacter"));
	DefaultPawnClass = PlayerPawnClassFinder.Class;

	// use our custom HUD class
	HUDClass = ASIAIEHUD::StaticClass();
}
