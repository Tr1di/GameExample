// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ImpactEffect.generated.h"

USTRUCT()
struct FDecalData
{
	GENERATED_USTRUCT_BODY()

	/** material */
	UPROPERTY(EditDefaultsOnly, Category=Decal)
	UMaterial* DecalMaterial;

	/** quad size (width & height) */
	UPROPERTY(EditDefaultsOnly, Category=Decal)
	float DecalSize;

	/** lifespan */
	UPROPERTY(EditDefaultsOnly, Category=Decal)
	float LifeSpan;

	/** defaults */
	FDecalData()
		: DecalMaterial(nullptr)
		, DecalSize(256.f)
		, LifeSpan(10.f)
	{
	}
};

UCLASS()
class SIAIE_API AImpactEffect : public AActor
{
	GENERATED_BODY()

	/** default impact FX used when material specific override doesn't exist */
	UPROPERTY(EditDefaultsOnly, Category=Defaults)
	UParticleSystem* DefaultFX;

	/** default impact sound used when material specific override doesn't exist */
	UPROPERTY(EditDefaultsOnly, Category=Defaults)
	class USoundCue* DefaultSound;

	/** default decal when material specific override doesn't exist */
	UPROPERTY(EditDefaultsOnly, Category=Defaults)
	FDecalData DefaultDecal;

public:
	/** surface data for spawning */
	UPROPERTY(BlueprintReadOnly, Category=Surface)
	FHitResult SurfaceHit;

	explicit AImpactEffect(const FObjectInitializer& ObjectInitializer);
	
protected:
	/** get FX for material type */
	UParticleSystem* GetImpactFX(TEnumAsByte<EPhysicalSurface> SurfaceType) const;

	/** get sound for material type */
	USoundCue* GetImpactSound(TEnumAsByte<EPhysicalSurface> SurfaceType) const;

	/** spawn effect */
	virtual void PostInitializeComponents() override;
};
