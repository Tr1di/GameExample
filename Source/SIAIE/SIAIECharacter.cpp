// Copyright Epic Games, Inc. All Rights Reserved.

#include "SIAIECharacter.h"
#include "SIAIEProjectile.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/InputSettings.h"
#include "Kismet/GameplayStatics.h"
#include "MotionControllerComponent.h"
#include "Weapon.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogFPChar, Warning, All);

//////////////////////////////////////////////////////////////////////////
// ASIAIECharacter

void FHolster::SetWeapon(AWeapon* InWeapon)
{
	Weapon = InWeapon;
}

ASIAIECharacter::ASIAIECharacter()
{
	SetCanBeDamaged(true);
	
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(40.f, 96.0f);

	// set our turn rates for input
	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;
	
	// Create a CameraComponent	
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCameraComponent->SetupAttachment(GetCapsuleComponent());
	FirstPersonCameraComponent->SetRelativeLocation(FVector(-39.56f, 1.75f, 64.f)); // Position the camera
	FirstPersonCameraComponent->bUsePawnControlRotation = true;
	
	// Create a mesh component that will be used when being viewed from a '1st person' view (when controlling this pawn)
	ShadowMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh1P"));
	ShadowMesh->SetOnlyOwnerSee(true);
	ShadowMesh->bRenderInMainPass = false;
	ShadowMesh->bRenderInDepthPass = false;
	ShadowMesh->SetupAttachment(GetMesh());
	ShadowMesh->SetRelativeRotation(FRotator(1.9f, -19.19f, 5.2f));
	ShadowMesh->SetRelativeLocation(FVector(-0.5f, -4.4f, -155.7f));
}

void ASIAIECharacter::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();
}

//////////////////////////////////////////////////////////////////////////
// Input

void ASIAIECharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// set up gameplay key bindings
	check(PlayerInputComponent);

	// Bind jump events
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	// Bind fire event
	PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &ASIAIECharacter::OnFire);
	PlayerInputComponent->BindAction("Fire", IE_Released, this, &ASIAIECharacter::OnStopFire);

	PlayerInputComponent->BindAction("Interact", IE_Pressed, this, &ASIAIECharacter::OnInteract);

	PlayerInputComponent->BindAction("NextWeapon", IE_Pressed, this, &ASIAIECharacter::NextWeapon);
	PlayerInputComponent->BindAction("PreviousWeapon", IE_Released, this, &ASIAIECharacter::PreviousWeapon);
	
	PlayerInputComponent->BindAction("SheathWeapon", IE_Released, this, &ASIAIECharacter::SheathCurrentWeapon);
	PlayerInputComponent->BindAction("DropWeapon", IE_Released, this, &ASIAIECharacter::DropCurrentWeapon);
	
	// Bind movement events
	PlayerInputComponent->BindAxis("MoveForward", this, &ASIAIECharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &ASIAIECharacter::MoveRight);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("TurnRate", this, &ASIAIECharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("LookUpRate", this, &ASIAIECharacter::LookUpAtRate);
}

float ASIAIECharacter::PlayAnimMontage(USkeletalMeshComponent* UseMesh, UAnimMontage* AnimMontage, float InPlayRate,
	FName StartSectionName)
{
	float Result = 0.f;
	
	if (AnimMontage && UseMesh && UseMesh->AnimScriptInstance)
	{
		Result = UseMesh->AnimScriptInstance->Montage_Play(AnimMontage, InPlayRate);
		UseMesh->AnimScriptInstance->Montage_JumpToSection(StartSectionName);
	}

	return Result;
}

void ASIAIECharacter::StopAnimMontage(USkeletalMeshComponent* UseMesh, UAnimMontage* AnimMontage)
{
	if (AnimMontage && UseMesh && UseMesh->AnimScriptInstance &&
		UseMesh->AnimScriptInstance->Montage_IsPlaying(AnimMontage))
	{
		UseMesh->AnimScriptInstance->Montage_Stop(AnimMontage->BlendOut.GetBlendTime(), AnimMontage);
	}
}

float ASIAIECharacter::PlayAnimMontage(FWeaponAnim AnimMontage, float InPlayRate, FName StartSectionName)
{
	float Result = 0.f;
	
	USkeletalMeshComponent* UseMesh = GetMesh();
	UAnimMontage* UseMontage = IsFirstPerson() ? AnimMontage.Pawn1P : AnimMontage.Pawn3P;

	Result = PlayAnimMontage(UseMesh, UseMontage, InPlayRate, StartSectionName);
	
	if (IsFirstPerson())
	{
		PlayAnimMontage(ShadowMesh, AnimMontage.Pawn3P, InPlayRate, StartSectionName);
	}
	
	return Result;
}

void ASIAIECharacter::StopAnimMontage(FWeaponAnim AnimMontage)
{
	USkeletalMeshComponent* UseMesh = GetMesh();
	UAnimMontage* UseMontage = IsFirstPerson() ? AnimMontage.Pawn1P : AnimMontage.Pawn3P;

	StopAnimMontage(UseMesh, UseMontage);
	
	if (IsFirstPerson())
	{
		StopAnimMontage(ShadowMesh, AnimMontage.Pawn3P);
	}
}

AActor* ASIAIECharacter::TraceForInteractiveActor(TSubclassOf<UInterface> SearchClass)
{
	FHitResult OutHit;
	
	const FVector Start = GetPawnViewLocation();
	const FVector Forward = GetControlRotation().Vector();
	const FVector End = Start + Forward * InteractTraceLength;

	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(this);
	GetAttachedActors(ActorsToIgnore, false);

	FCollisionQueryParams Params;
	Params.bTraceComplex = true;
	Params.AddIgnoredActors(ActorsToIgnore);
	
	if (GetWorld()->LineTraceSingleByChannel(OutHit, Start, End, ECC_Visibility, Params))
	{
		if (OutHit.bBlockingHit && OutHit.GetActor())
		{
			if ( OutHit.GetActor()->GetClass()->ImplementsInterface(SearchClass) )
			{
				return OutHit.GetActor();
			}
		}
	}

	return nullptr;
}

void ASIAIECharacter::OnInteract()
{
	Interact(TraceForInteractiveActor(UInteractive::StaticClass()));
}

void ASIAIECharacter::Interact(AActor* InInteractiveActor)
{
	if ( !InInteractiveActor || !IInteractive::Execute_CanBeInteractedBy(InInteractiveActor, this) )
	{
		return;
	}
	
	IInteractive::Execute_Interact(InInteractiveActor, this);
}

void ASIAIECharacter::OnFire()
{
	if (CurrentWeapon)
	{
		CurrentWeapon->StartFire();
	}
}

void ASIAIECharacter::OnStopFire()
{
	if (CurrentWeapon)
	{
		CurrentWeapon->StopFire();
	}
}

void ASIAIECharacter::HideWeapon()
{
	if (CurrentWeapon)
	{
		SheathCurrentWeapon();
	}
	else
	{
		DrawWeapon();
	}
}

void ASIAIECharacter::SetWeapon(AWeapon* NewWeapon)
{
	if ( NewWeapon == CurrentWeapon )
	{
		return;
	}

	if (Weapons.Contains(CurrentWeapon))
	{
		SheathWeapon(CurrentWeapon);
		CurrentWeapon->OnUnEquip();
		LastWeapon = CurrentWeapon;
	}
	
	CurrentWeapon = NewWeapon;

	DrawWeapon(CurrentWeapon);
	CurrentWeapon->OnEquip();
}

bool ASIAIECharacter::CanPickUp(AWeapon* InWeapon) const
{
	return !GetHolsterFor(InWeapon).IsNone();
}

void ASIAIECharacter::PickUp(AWeapon* InWeapon)
{
	if ( !CanPickUp(InWeapon) )
	{
		return;
	}

	InWeapon->OnEnterInventory(this);
	Weapons.Add(InWeapon);
	
	if (!CurrentWeapon)
	{
		SetWeapon(InWeapon);
	}
	else
	{
		SheathWeapon(InWeapon);
	}
}

void ASIAIECharacter::DropWeapon(AWeapon* InWeapon)
{
	if (!InWeapon)
	{
		return;
	}
	
	Weapons.Remove(InWeapon);
	InWeapon->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	InWeapon->OnLeaveInventory();
	
	for ( auto Holster : Holsters )
	{
		if (Holster.IsAttached(InWeapon))
		{
			Holster.Detach();
			break;
		}
	}
}

void ASIAIECharacter::DropCurrentWeapon()
{
	AWeapon* NewWeapon = GetWeapon(Next);
	NewWeapon = NewWeapon != CurrentWeapon ? NewWeapon : nullptr; 
	DropWeapon(CurrentWeapon);
	SetWeapon(NewWeapon);
}

AWeapon* ASIAIECharacter::GetWeapon(const EWeaponSwitch InDirection)
{
	if ( Weapons.Num() < 1 )
	{
		return CurrentWeapon;
	}
	
	const int Count = Weapons.Num();
	const int Index = Weapons.IndexOfByKey(CurrentWeapon) + Count + InDirection;
	return Weapons[Index % Count];
}

void ASIAIECharacter::NextWeapon()
{
	SetWeapon(GetWeapon(Next));
}

void ASIAIECharacter::PreviousWeapon()
{
	SetWeapon(GetWeapon(Previous));
}

void ASIAIECharacter::DrawWeapon(AWeapon* InWeapon)
{
	if (!InWeapon)
	{
		return;
	}
	
	USkeletalMeshComponent* FP = InWeapon->GetWeaponMesh();
	USkeletalMeshComponent* Shadow = InWeapon->GetShadowMesh();
	
	FP->AttachToComponent(GetMesh(),
		FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true),
		TEXT("GripPoint"));

	if (IsPlayerControlled())
	{
		Shadow->AttachToComponent(GetShadowMesh(),
			FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true),
			TEXT("GripPoint"));
	}
}

void ASIAIECharacter::DrawWeapon()
{
	if ( CurrentWeapon )
	{
		return;
	}
	
	if ( LastWeapon )
	{
		DrawWeapon(LastWeapon);
		return;
	}

	NextWeapon();
}

void ASIAIECharacter::SheathCurrentWeapon()
{
	SheathWeapon(CurrentWeapon);
}

bool ASIAIECharacter::CanFire() const
{
	return IsAlive();
}

bool ASIAIECharacter::CanReload() const
{
	return IsAlive();
}

void ASIAIECharacter::SheathWeapon(AWeapon* InWeapon, FName SocketName)
{
	if (!InWeapon)
	{
		return;
	}
	
	if (SocketName.IsNone())
	{
		SocketName = GetHolsterFor(InWeapon);
	}
	
	USkeletalMeshComponent* FP = InWeapon->GetMesh1P();
	USkeletalMeshComponent* TP = InWeapon->GetShadowMesh();
		
	FP->AttachToComponent(GetMesh(),
		FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true),
		SocketName);
	TP->AttachToComponent(GetShadowMesh(),
		FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true),
		SocketName);
}

FName ASIAIECharacter::GetHolsterFor(AWeapon* InWeapon) const
{
	for (auto Holster : Holsters)
	{		
		if (Holster.IsAttached(InWeapon))
		{
			return Holster.GetName();
		}
	}
	
	for (auto Holster : Holsters)
	{		
		if (!Holster.IsWeaponAttached())
		{
			return Holster.GetName();
		}
	}

	return NAME_None;
}

void ASIAIECharacter::MoveForward(float Value)
{
	if (Value != 0.0f)
	{
		// add movement in that direction
		AddMovementInput(GetActorForwardVector(), Value);
	}
}

void ASIAIECharacter::MoveRight(float Value)
{
	if (Value != 0.0f)
	{
		// add movement in that direction
		AddMovementInput(GetActorRightVector(), Value);
	}
}

void ASIAIECharacter::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void ASIAIECharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}

bool ASIAIECharacter::IsAlive() const
{
	return true;
}

bool ASIAIECharacter::IsFirstPerson() const
{
	return IsAlive() && Controller && Controller->IsPlayerController();
}
