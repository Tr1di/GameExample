// Copyright Epic Games, Inc. All Rights Reserved.

#include "SIAIEHUD.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"
#include "CanvasItem.h"
#include "Interactive.h"
#include "SIAIECharacter.h"
#include "UObject/ConstructorHelpers.h"

ASIAIEHUD::ASIAIEHUD()
{
	// Set the crosshair texture
	static ConstructorHelpers::FObjectFinder<UTexture2D> CrosshairTexObj(TEXT("/Game/FirstPerson/Textures/FirstPersonCrosshair"));
	CrosshairTex = CrosshairTexObj.Object;
}


void ASIAIEHUD::DrawHUD()
{
	Super::DrawHUD();

	// Draw very simple crosshair

	// find center of the Canvas
	const FVector2D Center(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f);

	// offset by half the texture's dimensions so that the center of the texture aligns with the center of the Canvas
	const FVector2D CrosshairDrawPosition( (Center.X),
										   (Center.Y + 20.0f));

	// draw the crosshair
	FCanvasTileItem TileItem( CrosshairDrawPosition, CrosshairTex->Resource, FLinearColor::White);
	TileItem.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem( TileItem );

	if (ASIAIECharacter* Pawn = Cast<ASIAIECharacter>(GetOwningPawn()))
	{
		if (const AActor* Weapon = Pawn->TraceForInteractiveActor(UInteractive::StaticClass()))
		{
			const FString ActorName = Weapon->GetName();
	
			float XSize;
			float YSize;
			GetTextSize(ActorName, XSize, YSize);

			FVector ActorLocation = Project(Weapon->GetActorLocation());
			ActorLocation.Y -= YSize;
	
			DrawText(ActorName, FLinearColor::White, ActorLocation.X, ActorLocation.Y);
		}
	}
}
