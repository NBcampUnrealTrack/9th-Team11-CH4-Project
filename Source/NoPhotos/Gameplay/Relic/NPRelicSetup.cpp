#include "Gameplay/Relic/NPRelicSetup.h"

#include "Gameplay/Relic/Gimmick/Components/NPRelicGimmickComponent.h"
#include "Gameplay/Relic/NPBaseRelic.h"
#include "Net/UnrealNetwork.h"
#include "NoPhotos.h"

ANPRelicSetup::ANPRelicSetup()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
}

void ANPRelicSetup::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ANPRelicSetup, Relic);
}

void ANPRelicSetup::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}
	if (!PrepareRelicSetup())
	{
		return;
	}
	if (!Relic)
	{
		UE_LOG(
			LogNoPhotos,
			Warning,
			TEXT("[%s] RelicSetup has no Relic assigned."),
			*GetNameSafe(this));
		return;
	}

	CollectGimmicks();
	UE_LOG(
		LogNoPhotos,
		Log,
		TEXT("[%s] Collected %d relic gimmick(s) for %s."),
		*GetNameSafe(this),
		Gimmicks.Num(),
		*GetNameSafe(Relic));
	RefreshRelicLock();
}

bool ANPRelicSetup::PrepareRelicSetup()
{
	return true;
}

void ANPRelicSetup::CollectGimmicks()
{
	CollectGimmicksFromActor(Relic);

	for (AActor* GimmickActor : GimmickActors)
	{
		CollectGimmicksFromActor(GimmickActor);
	}
}

void ANPRelicSetup::CollectGimmicksFromActor(AActor* GimmickActor)
{
	if (!GimmickActor)
	{
		return;
	}

	TArray<UNPRelicGimmickComponent*> ActorGimmicks;
	GimmickActor->GetComponents<UNPRelicGimmickComponent>(ActorGimmicks);
	UE_LOG(
		LogNoPhotos,
		Log,
		TEXT("[%s] Found %d gimmick component(s) on %s."),
		*GetNameSafe(this),
		ActorGimmicks.Num(),
		*GetNameSafe(GimmickActor));
	for (UNPRelicGimmickComponent* Gimmick : ActorGimmicks)
	{
		if (Gimmick && !Gimmicks.Contains(Gimmick))
		{
			Gimmicks.Add(Gimmick);
			Gimmick->OnCompleted.AddUObject(
				this,
				&ANPRelicSetup::HandleGimmickCompleted);
		}
	}
}

void ANPRelicSetup::HandleGimmickCompleted()
{
	RefreshRelicLock();
}

void ANPRelicSetup::RefreshRelicLock()
{
	const bool bAllGimmicksCompleted = AreAllGimmicksCompleted();

	Relic->SetUnlocked(bAllGimmicksCompleted);
	UE_LOG(
		LogNoPhotos,
		Log,
		TEXT("[%s] Relic %s unlocked=%s. Gimmicks=%d."),
		*GetNameSafe(this),
		*GetNameSafe(Relic),
		bAllGimmicksCompleted ? TEXT("true") : TEXT("false"),
		Gimmicks.Num());
}

bool ANPRelicSetup::AreAllGimmicksCompleted() const
{
	for (const UNPRelicGimmickComponent* Gimmick : Gimmicks)
	{
		if (!Gimmick || !Gimmick->IsCompleted())
		{
			return false;
		}
	}

	return true;
}
