// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SIAIECharacter.generated.h"

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
	bool IsWeaponAttached() const { return Weapon; }
	AWeapon* GetAttachedWeapon() const { return Weapon; }
	bool IsAttached(AWeapon* InWeapon) const { return GetAttachedWeapon() == InWeapon }
};

UCLASS(config=Game)
class ASIAIECharacter : public ACharacter
{
	GENERATED_BODY()

	/** Pawn mesh: 1st person view (arms; seen only by self) */
	UPROPERTY(VisibleDefaultsOnly, Category=Mesh)
	USkeletalMeshComponent* Mesh1P;
	
	/** First person camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FirstPersonCameraComponent;

public:
	ASIAIECharacter();
	
protected:
	virtual void BeginPlay();

	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;
	virtual float PlayAnimMontage(UAnimMontage* AnimMontage, float InPlayRate, FName StartSectionName) override;
	virtual void StopAnimMontage(UAnimMontage* AnimMontage) override;

public:
	/** Base turn rate, in deg/sec. Other scaling may affect final turn rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Camera)
	float BaseTurnRate;

	/** Base look up/down rate, in deg/sec. Other scaling may affect final rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Camera)
	float BaseLookUpRate;

	///////////////////////////////////////////
	/// Interaction
	
private:
	UPROPERTY(EditDefaultsOnly)
	float InteractTraceLength;

protected:	
	UFUNCTION(BlueprintCallable)
	void Interact();
	
public:
	UFUNCTION(BlueprintCallable)
    AActor* InteractTrace(TSubclassOf<UInterface> SearchClass);

private:
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerInteract(AActor* InInteractiveActor);
	
	///////////////////////////////////////////
	/// Weapon
private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess))
	TArray<FHolster> Holsters;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, meta = (AllowPrivateAccess))
	TArray<AWeapon*> Weapons;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, ReplicatedUsing=OnRep_Weapon, meta = (AllowPrivateAccess))
	AWeapon* CurrentWeapon;

	/** Used for draw weapon if it was sheathed*/
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, meta = (AllowPrivateAccess))
	AWeapon* LastWeapon;
	
protected:
	void OnFire();
	void OnStopFire();
	
	UFUNCTION()
	void OnRep_Weapon(AWeapon* InWeapon);
	
	UFUNCTION()
	void OnRep_Weapons(TArray<AWeapon*> InWeapons);
	
public:
	void PickUp(AWeapon* InWeapon);
	bool CanPickUp(AWeapon* InWeapon) const;
	
	void DropWeapon(AWeapon* InWeapon);
	void DropCurrentWeapon();
	
	void NextWeapon();
	void PreviousWeapon();
	
	void DrawWeapon(AWeapon* InWeapon);
	void SheathWeapon();
	
private:
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerPickUp(AWeapon* InWeapon);
	
	UFUNCTION(Server, Reliable)
	void ServerDropWeapon(AWeapon* InWeapon);
	
	UFUNCTION(Server, Reliable)
	void SetWeapon(AWeapon* NewWeapon);
	
	void LocalDrawWeapon(AWeapon* InWeapon);
	void LocalSheathWeapon(AWeapon* InWeapon, FName SocketName = NAME_None);

	FName GetHolsterOf(AWeapon* InWeapon) const;
	FName GetHolsterFor(AWeapon* InWeapon) const;

	//////////
	// Health
private:
	//UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category=Mesh, meta = (AllowPrivateAccess = "true"))
	//UHealthComponent* Health;

public:
	//UFUNCTION(BlueprintPure, Category = Health)
	//virtual bool IsAlive() const { return Health->Get() > 0.f; }
	
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
	USkeletalMeshComponent* GetMesh1P() const { return Mesh1P; }
	
	/** Returns FirstPersonCameraComponent subobject **/
	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }

	bool IsAlive() const;
	bool IsFirstPerson() const;
	USkeletalMeshComponent* GetPawnMesh() const { return IsFirstPerson() ? GetMesh1P() : GetMesh(); }
};

