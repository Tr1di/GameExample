// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Interactive.h"
#include "Animation/SkeletalMeshActor.h"
#include "Weapon.generated.h"

class UAnimMontage;
class UAudioComponent;
class UParticleSystemComponent;
class UForceFeedbackEffect;
class USoundCue;
class UMatineeCameraShake;

namespace EWeaponState
{
	enum Type
	{
		Idle,
		Firing,
		Reloading,
		Equipping,
	};
}

USTRUCT()
struct FWeaponData
{
	GENERATED_USTRUCT_BODY()

	/** infinite ammo for reloads */
	UPROPERTY(EditDefaultsOnly, Category=Ammo)
	bool bInfiniteAmmo;

	/** infinite ammo in clip, no reload required */
	UPROPERTY(EditDefaultsOnly, Category=Ammo)
	bool bInfiniteClip;
	
	/** max ammo */
	UPROPERTY(EditDefaultsOnly, Category=Ammo)
	int32 MaxAmmo;

	/** clip size */
	UPROPERTY(EditDefaultsOnly, Category=Ammo)
	int32 AmmoPerClip;

	/** initial clips */
	UPROPERTY(EditDefaultsOnly, Category=Ammo)
	int32 InitialClips;
	
	/** time between two consecutive shots */
	UPROPERTY(EditDefaultsOnly, Category=WeaponStat)
	float FireRate;
	
	/** failsafe reload duration if weapon doesn't have any animation for it */
	UPROPERTY(EditDefaultsOnly, Category=WeaponStat)
	float NoAnimReloadDuration;

	/** defaults */
	FWeaponData()
		: bInfiniteAmmo(false)
		, bInfiniteClip(false)
		, MaxAmmo(100)
		, AmmoPerClip(20)
		, InitialClips(4)
		, FireRate(0.2f)
		, NoAnimReloadDuration(1.0f)
	{}
};

USTRUCT()
struct FWeaponAnim
{
	GENERATED_USTRUCT_BODY()

	/** animation played on pawn (1st person view) */
	UPROPERTY(EditDefaultsOnly, Category=Animation)
	UAnimMontage* Pawn1P;

	/** animation played on pawn (3rd person view) */
	UPROPERTY(EditDefaultsOnly, Category=Animation)
	UAnimMontage* Pawn3P;

	FWeaponAnim()
		: Pawn1P(nullptr)
		, Pawn3P(nullptr)
	{}
};

UCLASS()
class SIAIE_API AWeapon : public ASkeletalMeshActor, public IInteractive
{
	GENERATED_BODY()

	/** pawn owner */
	UPROPERTY(Transient)
	class ASIAIECharacter* MyPawn;

	/** weapon data */
	UPROPERTY(EditDefaultsOnly, Category=Config)
	FWeaponData WeaponConfig;

	UPROPERTY(VisibleDefaultsOnly, Category=Mesh)
	USkeletalMeshComponent* ShadowMesh;

	/** current weapon state */
	EWeaponState::Type CurrentState;
	
public:
	explicit AWeapon(const FObjectInitializer& ObjectInitializer);

private:
	void SetOwningPawn(ASIAIECharacter* NewOwner);
	
	void SetWeaponState(EWeaponState::Type NewState);
	
protected:
	void DetermineWeaponState();

	/** perform initial setup */
	virtual void PostInitializeComponents() override;

	virtual void Destroyed() override;
	
	//////////////////////////////////////////////////////////////////////////
	// Equiping
	
private:
	UPROPERTY(EditDefaultsOnly, Category=Equiping)
	FWeaponAnim EquipAnim;
	
	UPROPERTY(EditDefaultsOnly, Category=Equiping)
	USoundCue* EquipSound;
	
	uint32 bPendingEquip : 1;
	uint32 bIsEquipped : 1;
	
	float EquipStartedTime;
	float EquipDuration;
	
	FTimerHandle TimerHandle_OnEquipFinished;

protected:
	virtual void OnEquipFinished();
	
public:
	virtual void OnEquip();	
	virtual void OnUnEquip();
	
	float GetEquipStartedTime() const;
	float GetEquipDuration() const;
	
	virtual void OnEnterInventory(ASIAIECharacter* NewOwner);
	virtual void OnLeaveInventory();
	
	bool IsEquipped() const;

	//////////////////////////////////////////////////////////////////////////
	// Reloading

private:
	UPROPERTY(EditDefaultsOnly, Category=Reloading)
	FWeaponAnim ReloadAnim;
	
	UPROPERTY(EditDefaultsOnly, Category=Reloading)
	USoundCue* ReloadSound;
	
	UPROPERTY(Transient)
	uint32 bPendingReload : 1;


	FTimerHandle TimerHandle_StopReload;
	FTimerHandle TimerHandle_ReloadWeapon;
	
protected:
	virtual void ReloadWeapon();

	void ReloadIfEmpty();
	
public:
	void StartReload();
	void StopReload();
	
	bool CanReload() const;

	//////////////////////////////////////////////////////////////////////////
	// Fire
	
private:
	UPROPERTY(Transient)
	UAudioComponent* FireAC;
	
	UPROPERTY(EditDefaultsOnly, Category=Firing)
	FWeaponAnim FireAnim;
	
	UPROPERTY(EditDefaultsOnly, Category=Firing)
	uint32 bLoopedFireAnim : 1;
	
	uint32 bPlayingFireAnim : 1;
	
	UPROPERTY(EditDefaultsOnly, Category=Firing)
	USoundCue* FireSound;
	
	UPROPERTY(EditDefaultsOnly, Category=Firing)
	uint32 bLoopedFireSound : 1;
	
	UPROPERTY(EditDefaultsOnly, Category=Firing)
	USoundCue* FireLoopSound;
	
	UPROPERTY(EditDefaultsOnly, Category=Firing)
	UParticleSystem* MuzzleFX;
	
	UPROPERTY(EditDefaultsOnly, Category=Firing)
	uint32 bLoopedMuzzleFX : 1;
	
	UPROPERTY(EditDefaultsOnly, Category=Firing)
	USoundCue* FireFinishSound;
	
	UPROPERTY(EditDefaultsOnly, Category=Firing)
	USoundCue* OutOfAmmoSound;
	
	UPROPERTY(EditDefaultsOnly, Category=Firing)
	FName MuzzleAttachPoint;

	/** spawned component for muzzle FX */
	UPROPERTY(Transient)
	UParticleSystemComponent* MuzzlePSC;
	
	//UPROPERTY(EditDefaultsOnly, Category=Firing)
	//TSubclassOf<UMatineeCameraShake> FireCameraShake;
	
	UPROPERTY(EditDefaultsOnly, Category=Firing)
	UForceFeedbackEffect* FireForceFeedback;	

	/** is weapon fire active? */
	uint32 bWantsToFire : 1;

	/** weapon is refiring */
	uint32 bRefiring;

	/** time of last successful weapon fire */
	float LastFireTime;

	/** Adjustment to handle frame rate affecting actual timer interval. */
	UPROPERTY(Transient)
	float TimerIntervalAdjustment;

	/** Whether to allow automatic weapons to catch up with shorter refire cycles */
	UPROPERTY(Config)
	bool bAllowAutomaticWeaponCatchup = true;
	
	/** current total ammo */
	UPROPERTY(Transient)
	int32 CurrentAmmo;

	/** current ammo - inside clip */
	UPROPERTY(Transient)
	int32 CurrentAmmoInClip;

	/** burst counter, used for replicating fire events to remote clients */
	UPROPERTY(Transient)
	int32 BurstCounter;

	/** Handle for efficient management of HandleFiring timer */
	FTimerHandle TimerHandle_HandleFiring;

	void ConsumeAmmo();
	
protected:	
	virtual void SimulateWeaponFire();
	virtual void StopSimulatingWeaponFire();
	
	virtual void FireWeapon() PURE_VIRTUAL(AShooterWeapon::FireWeapon,);
	
	void HandleReFiring();
	void HandleFiring();
	
	virtual void OnBurstStarted();
	virtual void OnBurstFinished();
	
public:
	virtual void StartFire();	
	virtual void StopFire();
	
	bool CanFire() const;

protected:
	/** play weapon sounds */
	UAudioComponent* PlayWeaponSound(USoundCue* Sound);
	
	float PlayWeaponAnimation(const FWeaponAnim& Animation);
	void StopWeaponAnimation(const FWeaponAnim& Animation);

	/** Get the aim of the weapon, allowing for adjustments to be made by the weapon */
	virtual FVector GetAdjustedAim() const;

	/** Get the aim of the camera */
	FVector GetCameraAim() const;

	/** get the originating location for camera damage */
	FVector GetCameraDamageStartLocation(const FVector& AimDir) const;

	/** get the muzzle location of the weapon */
	FVector GetMuzzleLocation() const;

	/** get direction of weapon's muzzle */
	FVector GetMuzzleDirection() const;

	/** find hit */
	FHitResult WeaponTrace(const FVector& TraceFrom, const FVector& TraceTo) const;
	
public:
	/** get current weapon state */
	EWeaponState::Type GetCurrentState() const;

	/** get current ammo amount (total) */
	int32 GetCurrentAmmo() const;

	/** get current ammo amount (clip) */
	int32 GetCurrentAmmoInClip() const;

	/** get clip size */
	int32 GetAmmoPerClip() const;

	/** get max ammo amount */
	int32 GetMaxAmmo() const;

	/** get weapon mesh (needs pawn owner to determine variant) */
	USkeletalMeshComponent* GetWeaponMesh() const;

	/** check if mesh is already attached */
	bool IsAttachedToPawn() const;

	/** check if weapon has infinite ammo (include owner's cheats) */
	bool HasInfiniteAmmo() const;

	/** check if weapon has infinite clip (include owner's cheats) */
	bool HasInfiniteClip() const;
	
	// IInteractive
	virtual void Interact_Implementation(ASIAIECharacter* InInstigator) override;
	virtual bool CanBeInteractedBy_Implementation(const ASIAIECharacter* InInstigator) const override;

	bool IsOwnerPlayerControlled() const;
	void SetCollisionEnabled(bool bEnableCollision);

	/** get pawn owner */
	UFUNCTION(BlueprintCallable)
	FORCEINLINE class ASIAIECharacter* GetPawnOwner() const { return MyPawn; }
	
	/** Returns Mesh1P subobject **/
	FORCEINLINE USkeletalMeshComponent* GetMesh1P() const { return GetSkeletalMeshComponent(); }
	/** Returns ShadowMesh subobject **/
	FORCEINLINE USkeletalMeshComponent* GetShadowMesh() const { return ShadowMesh; }
};
