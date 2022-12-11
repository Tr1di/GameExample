// Fill out your copyright notice in the Description page of Project Settings.


#include "ImpactEffect.h"

#include "Kismet/GameplayStatics.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Sound/SoundCue.h"

AImpactEffect::AImpactEffect(const FObjectInitializer& ObjectInitializer)
{
	SetAutoDestroyWhenFinished(true);
}

UParticleSystem* AImpactEffect::GetImpactFX(TEnumAsByte<EPhysicalSurface> SurfaceType) const
{
	return DefaultFX;
}

USoundCue* AImpactEffect::GetImpactSound(TEnumAsByte<EPhysicalSurface> SurfaceType) const
{
	return DefaultSound;
}

void AImpactEffect::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	//UPhysicalMaterial* HitPhysMat = SurfaceHit.PhysMaterial.Get();
	//EPhysicalSurface HitSurfaceType = UPhysicalMaterial::DetermineSurfaceType(HitPhysMat);
	EPhysicalSurface HitSurfaceType = SurfaceType_Default;

	// show particles
	UParticleSystem* ImpactFX = GetImpactFX(HitSurfaceType);
	if (ImpactFX)
	{
		UGameplayStatics::SpawnEmitterAtLocation(this, ImpactFX, GetActorLocation(), GetActorRotation());
	}

	// play sound
	USoundCue* ImpactSound = GetImpactSound(HitSurfaceType);
	if (ImpactSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, GetActorLocation());
	}

	if (DefaultDecal.DecalMaterial)
	{
		FRotator RandomDecalRotation = SurfaceHit.ImpactNormal.Rotation();
		RandomDecalRotation.Roll = FMath::FRandRange(-180.0f, 180.0f);

		UGameplayStatics::SpawnDecalAttached(DefaultDecal.DecalMaterial, FVector(1.0f, DefaultDecal.DecalSize, DefaultDecal.DecalSize),
			SurfaceHit.Component.Get(), SurfaceHit.BoneName,
			SurfaceHit.ImpactPoint, RandomDecalRotation, EAttachLocation::KeepWorldPosition,
			DefaultDecal.LifeSpan);
	}
}

