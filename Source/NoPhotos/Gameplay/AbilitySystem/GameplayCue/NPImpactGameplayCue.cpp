#include "Gameplay/AbilitySystem/GameplayCue/NPImpactGameplayCue.h"

#include "Components/SceneComponent.h"
#include "Core/GameplayTag/NPGameplayTags.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NoPhotos.h"

ANPImpactGameplayCue::ANPImpactGameplayCue()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	bAutoAttachToOwner = false;
	GameplayCueTag = NPGameplayTags::GameplayCue_Impact;
	GameplayCueName = GameplayCueTag.GetTagName();

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	ImpactEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("ImpactEffect"));
	ImpactEffect->SetupAttachment(SceneRoot);
	ImpactEffect->SetAutoActivate(false);
}

bool ANPImpactGameplayCue::OnExecute_Implementation(
	AActor* Target, const FGameplayCueParameters& Parameters)
{
	Super::OnExecute_Implementation(Target, Parameters);
	UE_LOG(
		LogNoPhotos,
		Warning,
		TEXT("[ImpactGameplayCue] OnExecute. Cue=%s Target=%s Niagara=%s NetMode=%d"),
		*GetNameSafe(this),
		*GetNameSafe(Target),
		*GetNameSafe(ImpactEffect->GetAsset()),
		static_cast<int32>(GetNetMode()));
	if (GetNetMode() == NM_DedicatedServer || !ImpactEffect->GetAsset())
	{
		UE_LOG(
			LogNoPhotos,
			Warning,
			TEXT("[ImpactGameplayCue] Finishing without playback. DedicatedServer=%s HasNiagara=%s"),
			GetNetMode() == NM_DedicatedServer ? TEXT("true") : TEXT("false"),
			ImpactEffect->GetAsset() ? TEXT("true") : TEXT("false"));
		GameplayCueFinishedCallback();
		return true;
	}

	const FHitResult* Hit = Parameters.EffectContext.GetHitResult();
	const FVector Location = Hit ? FVector(Hit->ImpactPoint) : FVector(Parameters.Location);
	const FVector Normal = Hit ? FVector(Hit->ImpactNormal) : FVector(Parameters.Normal);
	SetActorLocationAndRotation(Location, Normal.Rotation());
	SetActorHiddenInGame(false);
	ImpactEffect->SetHiddenInGame(false);
	ImpactEffect->SetVisibility(true, true);
	ImpactEffect->OnSystemFinished.AddUniqueDynamic(this, &ANPImpactGameplayCue::OnImpactFinished);
	bIsPlaying = true;
	ImpactEffect->ReinitializeSystem();
	UE_LOG(
		LogNoPhotos,
		Warning,
		TEXT("[ImpactGameplayCue] Niagara started. ActorLocation=%s ComponentLocation=%s Scale=%s Registered=%s Active=%s Visible=%s"),
		*GetActorLocation().ToCompactString(),
		*ImpactEffect->GetComponentLocation().ToCompactString(),
		*ImpactEffect->GetComponentScale().ToCompactString(),
		ImpactEffect->IsRegistered() ? TEXT("true") : TEXT("false"),
		ImpactEffect->IsActive() ? TEXT("true") : TEXT("false"),
		ImpactEffect->IsVisible() ? TEXT("true") : TEXT("false"));
	return true;
}

bool ANPImpactGameplayCue::GameplayCuePendingRemove()
{
	// 재생 중인 인스턴스는 다음 피격에서 재사용하지 않습니다.
	return bIsPlaying || Super::GameplayCuePendingRemove();
}

bool ANPImpactGameplayCue::Recycle()
{
	bIsPlaying = false;
	ImpactEffect->OnSystemFinished.RemoveDynamic(this, &ANPImpactGameplayCue::OnImpactFinished);
	ImpactEffect->DeactivateImmediate();
	return Super::Recycle();
}

void ANPImpactGameplayCue::OnImpactFinished(UNiagaraComponent* FinishedComponent)
{
	UE_LOG(
		LogNoPhotos,
		Warning,
		TEXT("[ImpactGameplayCue] Niagara finished. Cue=%s Niagara=%s"),
		*GetNameSafe(this),
		*GetNameSafe(FinishedComponent));
	GameplayCueFinishedCallback();
}
