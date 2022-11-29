// Copyright Epic Games, Inc. All Rights Reserved.

#include "SIAIECharacter.h"
#include "SIAIEProjectile.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/InputSettings.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "MotionControllerComponent.h"
#include "Weapon.h"
#include "XRMotionControllerBase.h" // for FXRMotionControllerBase::RightHandSourceId
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
	bReplicates = true;
	
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);

	// set our turn rates for input
	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;

	// Create a CameraComponent	
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCameraComponent->SetupAttachment(GetCapsuleComponent());
	FirstPersonCameraComponent->SetRelativeLocation(FVector(-39.56f, 1.75f, 64.f)); // Position the camera
	FirstPersonCameraComponent->bUsePawnControlRotation = true;

	GetMesh()->SetOwnerNoSee(true);
	
	// Create a mesh component that will be used when being viewed from a '1st person' view (when controlling this pawn)
	Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh1P"));
	Mesh1P->SetOnlyOwnerSee(true);
	Mesh1P->SetupAttachment(FirstPersonCameraComponent);
	Mesh1P->bCastDynamicShadow = false;
	Mesh1P->CastShadow = false;
	Mesh1P->SetRelativeRotation(FRotator(1.9f, -19.19f, 5.2f));
	Mesh1P->SetRelativeLocation(FVector(-0.5f, -4.4f, -155.7f));
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

	PlayerInputComponent->BindAction("Interact", IE_Pressed, this, &ASIAIECharacter::Interact);

	PlayerInputComponent->BindAction("NextWeapon", IE_Pressed, this, &ASIAIECharacter::NextWeapon);
	PlayerInputComponent->BindAction("PreviousWeapon", IE_Released, this, &ASIAIECharacter::PreviousWeapon);
	
	PlayerInputComponent->BindAction("SheathWeapon", IE_Released, this, &ASIAIECharacter::SheathWeapon);
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

void ASIAIECharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASIAIECharacter, CurrentWeapon);
	DOREPLIFETIME(ASIAIECharacter, Weapons);
}

float ASIAIECharacter::PlayAnimMontage(UAnimMontage* AnimMontage, float InPlayRate, FName StartSectionName)
{
	USkeletalMeshComponent* UseMesh = GetPawnMesh();
	if (AnimMontage && UseMesh && UseMesh->AnimScriptInstance)
	{
		return UseMesh->AnimScriptInstance->Montage_Play(AnimMontage, InPlayRate);
	}

	return 0.0f;
}

void ASIAIECharacter::StopAnimMontage(UAnimMontage* AnimMontage)
{
	USkeletalMeshComponent* UseMesh = GetPawnMesh();
	if (AnimMontage && UseMesh && UseMesh->AnimScriptInstance &&
		UseMesh->AnimScriptInstance->Montage_IsPlaying(AnimMontage))
	{
		UseMesh->AnimScriptInstance->Montage_Stop(AnimMontage->BlendOut.GetBlendTime(), AnimMontage);
	}
}

AActor* ASIAIECharacter::InteractTrace(TSubclassOf<UInterface> SearchClass)
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

void ASIAIECharacter::Interact()
{
	if ( AActor* Interactive = InteractTrace(UInteractive::StaticClass()) )
	{
		ServerInteract(Interactive);
	}
}

bool ASIAIECharacter::ServerInteract_Validate(AActor* InInteractiveActor)
{
	return true;
}

void ASIAIECharacter::ServerInteract_Implementation(AActor* InInteractiveActor)
{
	if ( HasAuthority() || ! InInteractiveActor )
	{
		return;
	}
	
	IInteractive::Execute_Interact(InInteractiveActor, this);
}


void ASIAIECharacter::OnFire()
{
	
}

void ASIAIECharacter::OnStopFire()
{
	
}

void ASIAIECharacter::OnRep_Weapon(AWeapon* InWeapon)
{
	if (InWeapon)
	{
		LocalDrawWeapon(InWeapon);
	}
	else
	{
		LocalSheathWeapon(LastWeapon);
	}
}

void ASIAIECharacter::OnRep_Weapons(TArray<AWeapon*> InWeapons)
{
	LastWeapon = Weapons.Contains(LastWeapon) ? LastWeapon : nullptr;
	for ( const auto Weapon : InWeapons )
	{
		if (Weapon != CurrentWeapon)
		{
			LocalSheathWeapon(Weapon);
		}
	}
}

void ASIAIECharacter::SetWeapon_Implementation(AWeapon* NewWeapon)
{
	if ( !HasAuthority() || NewWeapon == CurrentWeapon )
	{
		return;
	}

	CurrentWeapon = NewWeapon;
}

bool ASIAIECharacter::ServerPickUp_Validate(AWeapon* InWeapon)
{
	return true;
}

void ASIAIECharacter::ServerPickUp_Implementation(AWeapon* InWeapon)
{
	PickUp(InWeapon);
}

bool ASIAIECharacter::CanPickUp(AWeapon* InWeapon) const
{
	return !GetHolsterFor(InWeapon).IsNone();
}

void ASIAIECharacter::PickUp(AWeapon* InWeapon)
{
	if ( !HasAuthority() )
	{
		return;
	}

	if ( !CanPickUp(InWeapon) )
	{
		return;
	}
	
	Weapons.Add(InWeapon);
	// InWeapon-> Что-то типа что подобрано оружие

	if (!CurrentWeapon)
	{
		DrawWeapon(InWeapon);
	}
}

void ASIAIECharacter::ServerDropWeapon_Implementation(AWeapon* InWeapon)
{
	DropWeapon(InWeapon);
}

void ASIAIECharacter::DropWeapon(AWeapon* InWeapon)
{
	if ( !HasAuthority() )
	{
		ServerDropWeapon(InWeapon);
	}

	if ( HasAuthority() )
	{
		NextWeapon();
		Weapons.Remove(InWeapon);
		if (Weapons.Num() < 1) SetWeapon(nullptr);
	}

	for (auto Holster : Holsters)
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
	LastWeapon = nullptr;
	DropWeapon(CurrentWeapon);
}

void ASIAIECharacter::NextWeapon()
{
	if (Weapons.Num() < 1)
	{
		return;
	}

	if ( !CurrentWeapon && LastWeapon )
	{
		DrawWeapon(LastWeapon);
		return;
	}
	
	const int Count = Weapons.Num();
	const int Index = Weapons.IndexOfByKey(CurrentWeapon) + 1;
	DrawWeapon(Weapons[Index  % Count]);
}

void ASIAIECharacter::PreviousWeapon()
{
	if (Weapons.Num() < 1)
	{
		return;
	}

	if ( !CurrentWeapon && LastWeapon )
	{
		DrawWeapon(LastWeapon);
		return;
	}
	
	const int Count = Weapons.Num();
	const int Index = Weapons.IndexOfByKey(CurrentWeapon) + Count - 1;
	DrawWeapon(Weapons[Index % Count]);
}

void ASIAIECharacter::DrawWeapon(AWeapon* InWeapon)
{
	if (!InWeapon)
	{
		return;
	}

	SetWeapon(InWeapon);
}

void ASIAIECharacter::SheathWeapon()
{
	SetWeapon(nullptr);
}

void ASIAIECharacter::LocalDrawWeapon(AWeapon* InWeapon)
{
	AWeapon* OldWeapon = LastWeapon;
	
	if (InWeapon)
	{
		LastWeapon = InWeapon;
	}

	if (OldWeapon)
	{
		LocalSheathWeapon(OldWeapon);
	}

	if ( !IsBotControlled() )
	{
		USkeletalMeshComponent* FP = InWeapon->GetMesh1P();
		FP->AttachToComponent(GetMesh1P(),
			FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true),
			TEXT("GripPointFP"));
	}
	
	USkeletalMeshComponent* TP = InWeapon->GetMeshTP();
	TP->AttachToComponent(GetMesh(),
		FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true),
		TEXT("GripPoint"));
}

void ASIAIECharacter::LocalSheathWeapon(AWeapon* InWeapon, FName SocketName)
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
	USkeletalMeshComponent* TP = InWeapon->GetMeshTP();
		
	FP->AttachToComponent(GetMesh1P(),
		FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true),
		SocketName);
	TP->AttachToComponent(GetMesh(),
		FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true),
		SocketName);
}

FName ASIAIECharacter::GetHolsterOf(AWeapon* InWeapon) const
{
	for (auto Holster : Holsters)
	{		
		if (Holster.IsAttached(InWeapon))
		{
			return Holster.GetName();
		}
	}
	return NAME_None;
}

FName ASIAIECharacter::GetHolsterFor(AWeapon* InWeapon) const
{
	const FName Result = GetHolsterOf(InWeapon);

	if (Result.IsNone())
	{
		for (auto Holster : Holsters)
		{		
			if (!Holster.IsWeaponAttached())
			{
				return Holster.GetName();
			}
		}
	}

	return Result;
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
	return IsAlive() && Controller && Controller->IsLocalPlayerController();
}
