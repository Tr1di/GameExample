// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapon.h"

#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystemComponent.h"
#include "SIAIE/SIAIECharacter.h"
#include "Sound/SoundCue.h"

AWeapon::AWeapon(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GetSkeletalMeshComponent()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	GetSkeletalMeshComponent()->bReceivesDecals = false;
	GetSkeletalMeshComponent()->CastShadow = false;
	GetSkeletalMeshComponent()->SetCollisionObjectType(ECC_WorldDynamic);
	GetSkeletalMeshComponent()->SetCollisionResponseToAllChannels(ECR_Block);

	ShadowMesh = ObjectInitializer.CreateDefaultSubobject<USkeletalMeshComponent>(this, TEXT("WeaponMesh3P"));
	GetShadowMesh()->bReceivesDecals = false;
	GetShadowMesh()->CastShadow = true;
	GetShadowMesh()->bRenderInMainPass = false;
	GetShadowMesh()->bRenderInDepthPass = false;
	GetShadowMesh()->SetCollisionObjectType(ECC_WorldDynamic);
	GetShadowMesh()->SetCollisionResponseToAllChannels(ECR_Ignore);
	GetShadowMesh()->SetupAttachment(GetSkeletalMeshComponent());

	bLoopedMuzzleFX = false;
	bLoopedFireAnim = false;
	bPlayingFireAnim = false;
	bIsEquipped = false;
	bWantsToFire = false;
	bPendingReload = false;
	bPendingEquip = false;
	CurrentState = EWeaponState::Idle;

	CurrentAmmo = 0;
	CurrentAmmoInClip = 0;
	BurstCounter = 0;
	LastFireTime = 0.0f;
	
	bReplicates = true;
	bNetUseOwnerRelevancy = true;
	
	SetCollisionEnabled(true);
}

void AWeapon::SetOwningPawn(ASIAIECharacter* NewOwner)
{	
	if (MyPawn != NewOwner)
	{
		SetInstigator(NewOwner);
		MyPawn = NewOwner;
		SetOwner(NewOwner);
	}
}

void AWeapon::SetWeaponState(EWeaponState::Type NewState)
{
	const EWeaponState::Type PrevState = CurrentState;

	if (PrevState == EWeaponState::Firing && NewState != EWeaponState::Firing)
	{
		OnBurstFinished();
	}

	CurrentState = NewState;

	if (PrevState != EWeaponState::Firing && NewState == EWeaponState::Firing)
	{
		OnBurstStarted();
	}
}

void AWeapon::DetermineWeaponState()
{
	EWeaponState::Type NewState = EWeaponState::Idle;

	if (bIsEquipped)
	{
		if(bPendingReload)
		{
			if(!CanReload())
			{
				NewState = CurrentState;
			}
			else
			{
				NewState =  EWeaponState::Reloading;
			}
		}		
		else if (!bPendingReload && bWantsToFire && CanFire())
		{
			NewState = EWeaponState::Firing;
		}
	}
	else if (bPendingEquip)
	{
		NewState = EWeaponState::Equipping;
	}

	SetWeaponState(NewState);
}

void AWeapon::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (WeaponConfig.InitialClips > 0)
	{
		CurrentAmmoInClip = WeaponConfig.AmmoPerClip;
		CurrentAmmo = WeaponConfig.AmmoPerClip * WeaponConfig.InitialClips;
	}
}

void AWeapon::Destroyed()
{
	Super::Destroyed();

	StopSimulatingWeaponFire();
}

void AWeapon::OnEquipFinished()
{
	bIsEquipped = true;
	bPendingEquip = false;

	// Determine the state so that the can reload checks will work
	DetermineWeaponState(); 
	
	if (GetPawnOwner())
	{
		// try to reload empty clip
		if (GetPawnOwner()->IsLocallyControlled() &&
			CurrentAmmoInClip <= 0 &&
			CanReload())
		{
			StartReload();
		}
	}
}

void AWeapon::OnEquip()
{
	bPendingEquip = true;
	DetermineWeaponState();

	float Duration = PlayWeaponAnimation(EquipAnim);
	if (Duration <= 0.0f)
	{
		// failsafe
		Duration = 0.5f;
	}
	EquipStartedTime = GetWorld()->GetTimeSeconds();
	EquipDuration = Duration;

	GetWorldTimerManager().SetTimer(TimerHandle_OnEquipFinished, this, &AWeapon::OnEquipFinished, Duration, false);

	if (GetPawnOwner() && GetPawnOwner()->IsLocallyControlled())
	{
		PlayWeaponSound(EquipSound);
	}
}

void AWeapon::OnUnEquip()
{
	bIsEquipped = false;
	StopFire();

	if (bPendingReload)
	{
		StopWeaponAnimation(ReloadAnim);
		bPendingReload = false;

		GetWorldTimerManager().ClearTimer(TimerHandle_StopReload);
		GetWorldTimerManager().ClearTimer(TimerHandle_ReloadWeapon);
	}

	if (bPendingEquip)
	{
		StopWeaponAnimation(EquipAnim);
		bPendingEquip = false;

		GetWorldTimerManager().ClearTimer(TimerHandle_OnEquipFinished);
	}

	DetermineWeaponState();
}

float AWeapon::GetEquipStartedTime() const
{
	return EquipStartedTime;
}

float AWeapon::GetEquipDuration() const
{
	return EquipDuration;
}

void AWeapon::OnEnterInventory(ASIAIECharacter* NewOwner)
{
	SetOwningPawn(NewOwner);
	SetCollisionEnabled(false);
}

void AWeapon::OnLeaveInventory()
{
	if (IsAttachedToPawn())
	{
		OnUnEquip();
	}
	
	SetOwningPawn(nullptr);
	SetCollisionEnabled(true);
}

bool AWeapon::IsEquipped() const
{
	return bIsEquipped;
}

void AWeapon::ReloadWeapon()
{
	int32 ClipDelta = FMath::Min(WeaponConfig.AmmoPerClip - CurrentAmmoInClip, CurrentAmmo - CurrentAmmoInClip);

	if (HasInfiniteClip())
	{
		ClipDelta = WeaponConfig.AmmoPerClip - CurrentAmmoInClip;
	}

	if (ClipDelta > 0)
	{
		CurrentAmmoInClip += ClipDelta;
	}

	if (HasInfiniteClip())
	{
		CurrentAmmo = FMath::Max(CurrentAmmoInClip, CurrentAmmo);
	}
}

void AWeapon::StartReload()
{
	if ( CanReload() )
	{
		bPendingReload = true;
		DetermineWeaponState();

		float AnimDuration = PlayWeaponAnimation(ReloadAnim);		
		if (AnimDuration <= 0.0f)
		{
			AnimDuration = WeaponConfig.NoAnimReloadDuration;
		}

		GetWorldTimerManager().SetTimer(TimerHandle_StopReload, this, &AWeapon::StopReload, AnimDuration, false);
		if (GetLocalRole() == ROLE_Authority)
		{
			GetWorldTimerManager().SetTimer(TimerHandle_ReloadWeapon, this, &AWeapon::ReloadWeapon, FMath::Max(0.1f, AnimDuration - 0.1f), false);
		}
		
		if (MyPawn && MyPawn->IsLocallyControlled())
		{
			PlayWeaponSound(ReloadSound);
		}
	}
}

void AWeapon::StopReload()
{
	if (bPendingReload) 
	{
		bPendingReload = false;
		StopWeaponAnimation(ReloadAnim);
		GetWorldTimerManager().ClearTimer(TimerHandle_StopReload);
		GetWorldTimerManager().ClearTimer(TimerHandle_ReloadWeapon);
		
		DetermineWeaponState();
	}
}

bool AWeapon::CanReload() const
{
	const bool bCanReload = !MyPawn || MyPawn->CanReload();
	const bool bGotAmmo = CurrentAmmoInClip < WeaponConfig.AmmoPerClip && (CurrentAmmo - CurrentAmmoInClip > 0 || HasInfiniteClip());
	const bool bStateOKToReload = CurrentState < EWeaponState::Reloading;
	return bCanReload && bGotAmmo && bStateOKToReload;
}

void AWeapon::ConsumeAmmo()
{
	if (!HasInfiniteAmmo())
	{
		CurrentAmmoInClip--;
	}

	if (!HasInfiniteAmmo() && !HasInfiniteClip())
	{
		CurrentAmmo--;
	}
}

void AWeapon::SimulateWeaponFire()
{
	if (CurrentState != EWeaponState::Firing)
	{
		return;
	}

	if (MuzzleFX)
	{
		if (!bLoopedFireSound || !MuzzlePSC)
		{
			if(GetPawnOwner() && GetPawnOwner()->IsLocallyControlled())
			{
				MuzzlePSC = UGameplayStatics::SpawnEmitterAttached(MuzzleFX, GetWeaponMesh(), MuzzleAttachPoint);
				MuzzlePSC->bOwnerNoSee = !IsOwnerPlayerControlled();
				MuzzlePSC->bOnlyOwnerSee = IsOwnerPlayerControlled();
			}
			else
			{
				MuzzlePSC = UGameplayStatics::SpawnEmitterAttached(MuzzleFX, GetWeaponMesh(), MuzzleAttachPoint);
			}
		}
	}

	if (!bPlayingFireAnim)
	{
		PlayWeaponAnimation(FireAnim);
		bPlayingFireAnim = bLoopedFireAnim;
	}
	
	if (bLoopedFireSound)
	{
		if (!FireAC)
		{
			FireAC = PlayWeaponSound(FireSound);
		}
	}
	else
	{
		PlayWeaponSound(FireSound);
	}

	APlayerController* PC = GetPawnOwner() ? Cast<APlayerController>(GetPawnOwner()->Controller) : nullptr;
	if (PC && PC->IsLocalController())
	{
		//if (FireCameraShake)
		//{
			//PC->ClientStartCameraShake(FireCameraShake, 1);
		//}
		if (FireForceFeedback)
		{
			FForceFeedbackParameters FFParams;
			FFParams.Tag = "Weapon";
			PC->ClientPlayForceFeedback(FireForceFeedback, FFParams);
		}
	}
}

void AWeapon::StopSimulatingWeaponFire()
{
	if(MuzzlePSC)
	{
		MuzzlePSC->DeactivateSystem();
		MuzzlePSC = nullptr;
	}

	if (bPlayingFireAnim)
	{
		StopWeaponAnimation(FireAnim);
		bPlayingFireAnim = false;
	}

	if (FireAC)
	{
		FireAC->FadeOut(0.1f, 0.0f);
		FireAC = nullptr;

		PlayWeaponSound(FireFinishSound);
	}
}

void AWeapon::HandleReFiring()
{
	HandleFiring();
}

void AWeapon::ReloadIfEmpty()
{
	if (CurrentAmmoInClip <= 0 && CanReload())
	{
		StartReload();
	}
}

void AWeapon::HandleFiring()
{
	if ((CurrentAmmoInClip > 0 || HasInfiniteClip() || HasInfiniteAmmo()) && CanFire())
	{
		SimulateWeaponFire();
		
		FireWeapon();
		ConsumeAmmo();
		BurstCounter++;
	}
	else if (CanReload())
	{
		StartReload();
	}
	else if (IsOwnerPlayerControlled())
	{
		if (GetCurrentAmmo() == 0 && !bRefiring)
		{
			PlayWeaponSound(OutOfAmmoSound);
		}
		
		// stop weapon fire FX, but stay in Firing state
		if (BurstCounter > 0)
		{
			OnBurstFinished();
		}
	}
	else
	{
		OnBurstFinished();
	}

	if (IsOwnerPlayerControlled())
	{
		// setup refire timer
		bRefiring = CurrentState == EWeaponState::Firing && WeaponConfig.FireRate > 0.0f;
		if (bRefiring)
		{
			GetWorldTimerManager().SetTimer(TimerHandle_HandleFiring, this, &AWeapon::HandleReFiring, FMath::Max<float>(WeaponConfig.FireRate + TimerIntervalAdjustment, SMALL_NUMBER), false);
		}
	}

	LastFireTime = GetWorld()->GetTimeSeconds();
}

void AWeapon::OnBurstStarted()
{
	const float GameTime = GetWorld()->GetTimeSeconds();
	if (LastFireTime > 0 && WeaponConfig.FireRate > 0.0f &&
		LastFireTime + WeaponConfig.FireRate > GameTime)
	{
		GetWorldTimerManager().SetTimer(TimerHandle_HandleFiring, this, &AWeapon::HandleFiring, LastFireTime + WeaponConfig.FireRate - GameTime, false);
	}
	else
	{
		HandleFiring();
	}
}

void AWeapon::OnBurstFinished()
{
	// stop firing FX on remote clients
	BurstCounter = 0;

	// stop firing FX locally, unless it's a dedicated server
	//if (GetNetMode() != NM_DedicatedServer)
	//{
	StopSimulatingWeaponFire();
	//}
	
	GetWorldTimerManager().ClearTimer(TimerHandle_HandleFiring);
	bRefiring = false;

	// reset firing interval adjustment
	TimerIntervalAdjustment = 0.0f;
}

void AWeapon::StartFire()
{
	if (!bWantsToFire)
	{
		bWantsToFire = true;
		DetermineWeaponState();
	}
}

void AWeapon::StopFire()
{
	if (bWantsToFire)
	{
		bWantsToFire = false;
		DetermineWeaponState();
	}
}

bool AWeapon::CanFire() const
{
	const bool bCanFire = GetPawnOwner() && GetPawnOwner()->CanFire();
	const bool bStateOKToFire = ( ( CurrentState ==  EWeaponState::Idle ) || ( CurrentState == EWeaponState::Firing) );	
	return (( bCanFire == true ) && ( bStateOKToFire == true ) && ( bPendingReload == false ));
}

UAudioComponent* AWeapon::PlayWeaponSound(USoundCue* Sound)
{
	UAudioComponent* AC = nullptr;
	
	if (Sound && MyPawn)
	{
		AC = UGameplayStatics::SpawnSoundAttached(Sound, MyPawn->GetRootComponent());
	}

	return AC;
}

float AWeapon::PlayWeaponAnimation(const FWeaponAnim& Animation)
{
	float Duration = 0.0f;
	
	if (MyPawn)
	{
		Duration = GetPawnOwner()->PlayAnimMontage(Animation);
	}

	return Duration;
}

void AWeapon::StopWeaponAnimation(const FWeaponAnim& Animation)
{
	if (MyPawn)
	{
		GetPawnOwner()->StopAnimMontage(Animation);
	}
}

FVector AWeapon::GetAdjustedAim() const
{
	return GetInstigator() ? GetInstigator()->GetBaseAimRotation().Vector() : FVector::ZeroVector;
}

FVector AWeapon::GetCameraAim() const
{
	APlayerController* const PlayerController = GetInstigatorController<APlayerController>();
	FVector FinalAim = FVector::ZeroVector;

	if (PlayerController)
	{
		FVector CamLoc;
		FRotator CamRot;
		PlayerController->GetPlayerViewPoint(CamLoc, CamRot);
		FinalAim = CamRot.Vector();
	}
	else if (GetInstigator())
	{
		FinalAim = GetInstigator()->GetBaseAimRotation().Vector();		
	}

	return FinalAim;
}

FVector AWeapon::GetCameraDamageStartLocation(const FVector& AimDir) const
{
	if (!MyPawn)
	{
		return FVector::ZeroVector;
	}
	
	FVector OutStartTrace = GetMuzzleLocation();

	if (IsOwnerPlayerControlled())
	{
		APlayerController* PC = MyPawn->GetController<APlayerController>();

		FRotator UnusedRot;
		PC->GetPlayerViewPoint(OutStartTrace, UnusedRot);

		// Adjust trace so there is nothing blocking the ray between the camera and the pawn, and calculate distance from adjusted start
		OutStartTrace = OutStartTrace + AimDir * (GetInstigator()->GetActorLocation() - OutStartTrace | AimDir);
	}

	return OutStartTrace;
}

FVector AWeapon::GetMuzzleLocation() const
{
	return GetWeaponMesh()->GetSocketLocation(MuzzleAttachPoint);
}

FVector AWeapon::GetMuzzleDirection() const
{
	return GetWeaponMesh()->GetSocketRotation(MuzzleAttachPoint).Vector();
}

FHitResult AWeapon::WeaponTrace(const FVector& TraceFrom, const FVector& TraceTo) const
{
	// Perform trace to retrieve hit info
	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(WeaponTrace), true, GetInstigator());
	TraceParams.bReturnPhysicalMaterial = true;

	FHitResult Hit(ForceInit);
	GetWorld()->LineTraceSingleByChannel(Hit, TraceFrom, TraceTo, ECC_Visibility, TraceParams);

	return Hit;
}

EWeaponState::Type AWeapon::GetCurrentState() const
{
	return CurrentState;
}

int32 AWeapon::GetCurrentAmmo() const
{
	return CurrentAmmo;
}

int32 AWeapon::GetCurrentAmmoInClip() const
{
	return CurrentAmmoInClip;
}

int32 AWeapon::GetAmmoPerClip() const
{
	return WeaponConfig.AmmoPerClip;
}

int32 AWeapon::GetMaxAmmo() const
{
	return WeaponConfig.MaxAmmo;
}

USkeletalMeshComponent* AWeapon::GetWeaponMesh() const
{
	return GetSkeletalMeshComponent();
}

bool AWeapon::IsAttachedToPawn() const
{
	return bIsEquipped || bPendingEquip;
}

bool AWeapon::HasInfiniteAmmo() const
{
	return WeaponConfig.bInfiniteAmmo;
}

bool AWeapon::HasInfiniteClip() const 
{
	return WeaponConfig.bInfiniteClip;
}

void AWeapon::Interact_Implementation(ASIAIECharacter* InInstigator)
{
	if (!Execute_CanBeInteractedBy(this, InInstigator))
	{
		return;
	}
	
	InInstigator->PickUp(this);
}

bool AWeapon::CanBeInteractedBy_Implementation(const ASIAIECharacter* InInstigator) const
{
	return !GetPawnOwner();
}

bool AWeapon::IsOwnerPlayerControlled() const
{
	return GetPawnOwner() && GetPawnOwner()->IsPlayerControlled();
}

void AWeapon::SetCollisionEnabled(const bool bEnableCollision)
{
	GetSkeletalMeshComponent()->SetSimulatePhysics(bEnableCollision);
	GetSkeletalMeshComponent()->SetCollisionEnabled(bEnableCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
}
