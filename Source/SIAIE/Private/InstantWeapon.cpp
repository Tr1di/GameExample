// Fill out your copyright notice in the Description page of Project Settings.


#include "InstantWeapon.h"

#include "ImpactEffect.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystemComponent.h"
#include "SIAIE/SIAIECharacter.h"

AInstantWeapon::AInstantWeapon(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CurrentFiringSpread = 0.0f;
}

float AInstantWeapon::GetCurrentSpread() const
{
	float FinalSpread = InstantConfig.WeaponSpread + CurrentFiringSpread;
	if (GetPawnOwner() && GetPawnOwner()->IsTargeting())
	{
		FinalSpread *= InstantConfig.TargetingSpreadMod;
	}

	return FinalSpread;
}

bool AInstantWeapon::ShouldDealDamage(AActor* TestActor) const
{
	return static_cast<bool>(TestActor);
}

void AInstantWeapon::DealDamage(const FHitResult& Impact, const FVector& ShootDir)
{
	FPointDamageEvent PointDmg;
	PointDmg.DamageTypeClass = InstantConfig.DamageType;
	PointDmg.HitInfo = Impact;
	PointDmg.ShotDirection = ShootDir;
	PointDmg.Damage = InstantConfig.HitDamage;

	Impact.GetActor()->TakeDamage(PointDmg.Damage, PointDmg, GetPawnOwner()->Controller, this);
}

void AInstantWeapon::ProcessInstantHit(const FHitResult& Impact, const FVector& Origin, const FVector& ShootDir)
{
	if (ShouldDealDamage(Impact.GetActor()))
	{
		DealDamage(Impact, ShootDir);
	}
	
	const FVector EndTrace = Origin + ShootDir * InstantConfig.WeaponRange;
	const FVector EndPoint = Impact.GetActor() ? Impact.ImpactPoint : EndTrace;

	SpawnTrailEffect(EndPoint);
	SpawnImpactEffects(Impact);
}

void AInstantWeapon::FireWeapon()
{
	const int32 RandomSeed = FMath::Rand();
	FRandomStream WeaponRandomStream(RandomSeed);
	const float CurrentSpread = GetCurrentSpread();
	const float ConeHalfAngle = FMath::DegreesToRadians(CurrentSpread * 0.5f);

	const FVector AimDir = GetAdjustedAim();
	const FVector StartTrace = GetCameraDamageStartLocation(AimDir);
	const FVector ShootDir = WeaponRandomStream.VRandCone(AimDir, ConeHalfAngle, ConeHalfAngle);
	const FVector EndTrace = StartTrace + ShootDir * InstantConfig.WeaponRange;

	const FHitResult Impact = WeaponTrace(StartTrace, EndTrace);
	ProcessInstantHit(Impact, StartTrace, ShootDir);

	CurrentFiringSpread = FMath::Min(InstantConfig.FiringSpreadMax, CurrentFiringSpread + InstantConfig.FiringSpreadIncrement);
}

void AInstantWeapon::OnBurstFinished()
{
	Super::OnBurstFinished();
	CurrentFiringSpread = 0.0f;
}

void AInstantWeapon::SimulateInstantHit(const FVector& Origin, int32 RandomSeed, float ReticleSpread)
{
	FRandomStream WeaponRandomStream(RandomSeed);
	const float ConeHalfAngle = FMath::DegreesToRadians(ReticleSpread * 0.5f);

	const FVector StartTrace = Origin;
	const FVector AimDir = GetAdjustedAim();
	const FVector ShootDir = WeaponRandomStream.VRandCone(AimDir, ConeHalfAngle, ConeHalfAngle);
	const FVector EndTrace = StartTrace + ShootDir * InstantConfig.WeaponRange;

	FHitResult Impact = WeaponTrace(StartTrace, EndTrace);
	if (Impact.bBlockingHit)
	{
		SpawnImpactEffects(Impact);
		SpawnTrailEffect(Impact.ImpactPoint);
	}
	else
	{
		SpawnTrailEffect(EndTrace);
	}
}

void AInstantWeapon::SpawnImpactEffects(const FHitResult& Impact)
{
	if (ImpactTemplate && Impact.bBlockingHit)
	{
		FHitResult UseImpact = Impact;

		// trace again to find component lost during replication
		if (!Impact.Component.IsValid())
		{
			const FVector StartTrace = Impact.ImpactPoint + Impact.ImpactNormal * 10.0f;
			const FVector EndTrace = Impact.ImpactPoint - Impact.ImpactNormal * 10.0f;
			FHitResult Hit = WeaponTrace(StartTrace, EndTrace);
			UseImpact = Hit;
		}

		FTransform const SpawnTransform(Impact.ImpactNormal.Rotation(), Impact.ImpactPoint);
		AImpactEffect* EffectActor = GetWorld()->SpawnActorDeferred<AImpactEffect>(ImpactTemplate, SpawnTransform);
		if (EffectActor)
		{
			EffectActor->SurfaceHit = UseImpact;
			UGameplayStatics::FinishSpawningActor(EffectActor, SpawnTransform);
		}
	}
}

void AInstantWeapon::SpawnTrailEffect(const FVector& EndPoint)
{
	if (TrailFX)
	{
		const FVector Origin = GetMuzzleLocation();

		UParticleSystemComponent* TrailPSC = UGameplayStatics::SpawnEmitterAtLocation(this, TrailFX, Origin);
		if (TrailPSC)
		{
			TrailPSC->SetVectorParameter(TrailTargetParam, EndPoint);
		}
	}
}
