#include "Gameplay/Interaction/Components/NPImpactSoundComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Core/Audio/NPSoundSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Sound/SoundBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogNPImpactSound, Log, All);

UNPImpactSoundComponent::UNPImpactSoundComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UNPImpactSoundComponent::BeginPlay()
{
	Super::BeginPlay();
	BindImpactTarget();
}

void UNPImpactSoundComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindImpactTarget();
	Super::EndPlay(EndPlayReason);
}

void UNPImpactSoundComponent::SetImpactTargetComponent(UPrimitiveComponent* InTargetComponent)
{
	UnbindImpactTarget();
	ImpactTargetComponent = InTargetComponent;
	if (HasBegunPlay())
	{
		BindImpactTarget();
	}
}

void UNPImpactSoundComponent::BindImpactTarget()
{
	if (!IsValid(ImpactTargetComponent))
	{
		AActor* Owner = GetOwner();
		ImpactTargetComponent = Owner
			? Cast<UPrimitiveComponent>(Owner->GetRootComponent())
			: nullptr;
	}

	if (IsValid(ImpactTargetComponent))
	{
		ImpactTargetComponent->SetNotifyRigidBodyCollision(true);
		ImpactTargetComponent->OnComponentHit.AddUniqueDynamic(this, &ThisClass::HandleTargetHit);
	}
}

void UNPImpactSoundComponent::UnbindImpactTarget()
{
	if (IsValid(ImpactTargetComponent))
	{
		ImpactTargetComponent->OnComponentHit.RemoveDynamic(this, &ThisClass::HandleTargetHit);
	}
}

void UNPImpactSoundComponent::HandleTargetHit(
	UPrimitiveComponent* HitComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	FVector NormalImpulse,
	const FHitResult& Hit)
{
	UWorld* World = GetWorld();
	if (!World || !IsValid(HitComponent) || World->GetNetMode() == NM_DedicatedServer
		|| OtherActor == GetOwner()
		|| (OtherComponent && OtherComponent->GetOwner() == GetOwner()))
	{
		return;
	}

	const float ImpactImpulse = NormalImpulse.Size();
	const float Mass = HitComponent->GetMass();
	const float RequiredImpulse = Mass * FMath::Max(0.0f, MinimumImpactImpulse);
	UE_LOG(LogNPImpactSound, Log,
		TEXT("%s.%s: Impulse=%.2f, Mass=%.2f kg, ImpulsePerMass=%.2f, RequiredImpulse=%.2f"),
		*GetNameSafe(GetOwner()), *GetNameSafe(HitComponent),
		ImpactImpulse, Mass, Mass > 0.0f ? ImpactImpulse / Mass : 0.0f, RequiredImpulse);

	if (Mass <= 0.0f || NormalImpulse.IsNearlyZero() || ImpactImpulse < RequiredImpulse)
	{
		return;
	}

	const double CurrentTime = World->GetTimeSeconds();
	if (CurrentTime < NextImpactAllowedTime)
	{
		return;
	}

	UNPSoundSubsystem* SoundSubsystem = UNPSoundSubsystem::Get(this);
	if (!SoundSubsystem)
	{
		return;
	}

	TArray<USoundBase*> Candidates;
	USoundBase* FallbackSound = nullptr;
	for (const TObjectPtr<USoundBase>& Sound : ImpactSounds)
	{
		if (IsValid(Sound.Get()))
		{
			FallbackSound = Sound.Get();
			if (Sound != LastPlayedSound)
			{
				Candidates.AddUnique(Sound.Get());
			}
		}
	}

	USoundBase* SelectedSound = Candidates.IsEmpty()
		? FallbackSound
		: Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
	if (!SelectedSound)
	{
		return;
	}

	const float MinPitch = FMath::Max(0.01f, FMath::Min(MinimumPitch, MaximumPitch));
	const float MaxPitch = FMath::Max(MinPitch, FMath::Max(MinimumPitch, MaximumPitch));
	const float Pitch = FMath::FRandRange(MinPitch, MaxPitch);
	SoundSubsystem->PlaySFXAtLocation(SelectedSound, Hit.ImpactPoint, FRotator::ZeroRotator, 1.0f, Pitch);
	LastPlayedSound = SelectedSound;
	NextImpactAllowedTime = CurrentTime + FMath::Max(0.0f, ImpactCooldown);
}
