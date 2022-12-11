// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Weapon.h"
#include "InstantWeapon.generated.h"

USTRUCT()
struct FInstantWeaponData
{
	GENERATED_BODY()

	/** base weapon spread (degrees) */
	UPROPERTY(EditDefaultsOnly, Category=Accuracy)
	float WeaponSpread;

	/** targeting spread modifier */
	UPROPERTY(EditDefaultsOnly, Category=Accuracy)
	float TargetingSpreadMod;

	/** continuous firing: spread increment */
	UPROPERTY(EditDefaultsOnly, Category=Accuracy)
	float FiringSpreadIncrement;

	/** continuous firing: max increment */
	UPROPERTY(EditDefaultsOnly, Category=Accuracy)
	float FiringSpreadMax;

	/** weapon range */
	UPROPERTY(EditDefaultsOnly, Category=WeaponStat)
	float WeaponRange;

	/** damage amount */
	UPROPERTY(EditDefaultsOnly, Category=WeaponStat)
	int32 HitDamage;

	/** type of damage */
	UPROPERTY(EditDefaultsOnly, Category=WeaponStat)
	TSubclassOf<UDamageType> DamageType;

	/** defaults */
    	FInstantWeaponData()
    		: WeaponSpread(5.0f)
    		, TargetingSpreadMod(0.25f)
    		, FiringSpreadIncrement(1.0f)
    		, FiringSpreadMax(10.0f)
    		, WeaponRange(10000.0f)
    		, HitDamage(10)
    		, DamageType(UDamageType::StaticClass())
    	{}
};

/**
 * 
 */
UCLASS()
class SIAIE_API AInstantWeapon : public AWeapon
{
	GENERATED_BODY()
	
	/** weapon config */
    UPROPERTY(EditDefaultsOnly, Category=Config)
    FInstantWeaponData InstantConfig;
	
	/** impact effects */
    UPROPERTY(EditDefaultsOnly, Category=Effects)
    TSubclassOf<class AImpactEffect> ImpactTemplate;
	
	/** smoke trail */
	UPROPERTY(EditDefaultsOnly, Category=Effects)
	UParticleSystem* TrailFX;

	/** param name for beam target in smoke trail */
	UPROPERTY(EditDefaultsOnly, Category=Effects)
	FName TrailTargetParam;

	/** current spread from continuous firing */
	float CurrentFiringSpread;

public:
	explicit AInstantWeapon(const FObjectInitializer& ObjectInitializer);
	
protected:
	void DealDamage(const FHitResult& Impact, const FVector& ShootDir);
	
	void ProcessInstantHit(const FHitResult& Impact, const FVector& Origin, const FVector& ShootDir);

	virtual void FireWeapon() override;
	
	virtual void OnBurstFinished() override;
	
	void SimulateInstantHit(const FVector& Origin, int32 RandomSeed, float ReticleSpread);
	
	void SpawnImpactEffects(const FHitResult& Impact);
	
	void SpawnTrailEffect(const FVector& EndPoint);

public:
	bool ShouldDealDamage(AActor* TestActor) const;
	
	float GetCurrentSpread() const;
};
