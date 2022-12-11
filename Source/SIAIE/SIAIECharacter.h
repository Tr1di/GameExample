// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SIAIECharacter.generated.h"

struct FWeaponAnim;
class AWeapon;
class UInputComponent;
class USkeletalMeshComponent;
class USceneComponent;
class UCameraComponent;
class UMotionControllerComponent;
class UAnimMontage;
class USoundBase;

USTRUCT(BlueprintType)
struct FHolster
{
	GENERATED_BODY()

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess))
	FName SocketName;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, meta = (AllowPrivateAccess))
	AWeapon* Weapon = nullptr;
	
public:
	void SetWeapon(AWeapon* InWeapon);
	void Detach() { SetWeapon(nullptr); }
	FName GetName() const { return SocketName; }
	bool IsWeaponAttached() const { return (bool)Weapon; }
	AWeapon* GetAttachedWeapon() const { return Weapon; }
	bool IsAttached(AWeapon* InWeapon) const { return GetAttachedWeapon() == InWeapon; }
};

UCLASS(config=Game)
class ASIAIECharacter : public ACharacter
{
	GENERATED_BODY()

	/** Pawn mesh: 1st person view (arms; seen only by self) */
	UPROPERTY(VisibleDefaultsOnly, Category=Mesh)
	USkeletalMeshComponent* ShadowMesh;
	
	/** First person camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FirstPersonCameraComponent;

public:
	ASIAIECharacter();

protected:
	/** Base turn rate, in deg/sec. Other scaling may affect final turn rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Camera)
	float BaseTurnRate;

	/** Base look up/down rate, in deg/sec. Other scaling may affect final rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Camera)
	float BaseLookUpRate;
	
	virtual void BeginPlay();

	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;

	virtual float PlayAnimMontage(USkeletalMeshComponent* UseMesh, UAnimMontage* AnimMontage, float InPlayRate = 1.f, FName StartSectionName=NAME_None);
	virtual void StopAnimMontage(USkeletalMeshComponent* UseMesh, UAnimMontage* AnimMontage);
	
public:
	float PlayAnimMontage(FWeaponAnim AnimMontage, float InPlayRate = 1.f, FName StartSectionName=NAME_None);
	void StopAnimMontage(FWeaponAnim AnimMontage);

	///////////////////////////////////////////
	/// Interaction
	
private:
	UPROPERTY(EditDefaultsOnly)
	float InteractTraceLength;
	
	void Interact(AActor* InInteractiveActor);
	
protected:	
	UFUNCTION(BlueprintCallable)
	void OnInteract();
	
public:
	UFUNCTION(BlueprintCallable)
    AActor* TraceForInteractiveActor(TSubclassOf<UInterface> SearchClass);
	
	///////////////////////////////////////////
	/// Weapon
private:
	enum EWeaponSwitch : int8
	{
		Next = 1,
		Previous = -1
	};
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess))
	TArray<FHolster> Holsters;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, meta = (AllowPrivateAccess))
	TArray<AWeapon*> Weapons;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, meta = (AllowPrivateAccess))
	AWeapon* CurrentWeapon;

	/** Used for draw weapon if it was sheathed*/
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, meta = (AllowPrivateAccess))
	AWeapon* LastWeapon;

	AWeapon* GetWeapon(const EWeaponSwitch InDirection);
protected:
	void OnFire();
	void OnStopFire();

	void HideWeapon();
	
public:
	void PickUp(AWeapon* InWeapon);
	bool CanPickUp(AWeapon* InWeapon) const;
	
	void DropWeapon(AWeapon* InWeapon);
	void DropCurrentWeapon();
	
	void NextWeapon();
	void PreviousWeapon();
	
	void DrawWeapon(AWeapon* InWeapon);
	void DrawWeapon();
	
	void SheathWeapon(AWeapon* InWeapon, FName SocketName = NAME_None);
	void SheathCurrentWeapon();

	bool CanFire() const;
	bool CanReload() const;

	bool IsTargeting() const;
	
	AWeapon* GetWeapon() const { return CurrentWeapon; }
	
private:
	void SetWeapon(AWeapon* NewWeapon);
	FName GetHolsterFor(AWeapon* InWeapon) const;
	
protected:
	///////////////////////////////////////////
	/// Movement

	/** Handles moving forward/backward */
	void MoveForward(float Val);

	/** Handles stafing movement, left and right */
	void MoveRight(float Val);

	/**
	 * Called via input to turn at a given rate.
	 * @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	 */
	void TurnAtRate(float Rate);

	/**
	 * Called via input to turn look up/down at a given rate.
	 * @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	 */
	void LookUpAtRate(float Rate);

public:	
	/** Returns Mesh1P subobject **/
	USkeletalMeshComponent* GetShadowMesh() const { return ShadowMesh; }
	
	/** Returns FirstPersonCameraComponent subobject **/
	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }

	bool IsAlive() const;
	bool IsFirstPerson() const;
};

