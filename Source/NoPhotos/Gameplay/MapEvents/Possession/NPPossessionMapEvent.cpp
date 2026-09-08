#include "NPPossessionMapEvent.h"

#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Gameplay/Character/NPStablePhysicsPawn.h"
#include "Gameplay/Character/Component/NPControlReversalComponent.h"
#include "Gameplay/AbilitySystem/Effects/NPPossessionGameplayEffect.h"
#include "Gameplay/Relic/Case/NPRelicCase.h"
#include "Gameplay/MapEvents/NPMapEventManager.h"
#include "Gameplay/MapEvents/NPMapEventSpawnPoint.h"
#include "Kismet/GameplayStatics.h"
#include "NPGhostFollowerActor.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogNPPossessionEvent, Log, All);

ANPPossessionMapEvent::ANPPossessionMapEvent()
{
	LocationSource = ENPMapEventLocationSource::Point;
	RoamingGhostSpawnGroup = FGameplayTag::RequestGameplayTag(FName(TEXT("Possession")), false);
}

void ANPPossessionMapEvent::BeginPlay()
{
	Super::BeginPlay();
	UpdateTrackingState();
}

void ANPPossessionMapEvent::ApplyEventState_Implementation(bool bNewActive)
{
	UE_LOG(LogNPPossessionEvent, Display,
		TEXT("[PossessionTrace] ApplyEventState 진입: Event=%s Class=%s NewActive=%d Authority=%d BegunPlay=%d CurrentActive=%d"),
		*GetNameSafe(this), *GetNameSafe(GetClass()), bNewActive ? 1 : 0,
		HasAuthority() ? 1 : 0, HasActorBegunPlay() ? 1 : 0, IsEventActive() ? 1 : 0);
	Super::ApplyEventState_Implementation(bNewActive);
	if (HasActorBegunPlay())
	{
		UpdateTrackingState();
	}
	if (HasAuthority())
	{
		if (bNewActive)
		{
			BeginNextChaseCycle();
		}
		else
		{
			GetWorldTimerManager().ClearTimer(ChaseCycleTimer);
			CurrentChaseTarget.Reset();
			PreviousChaseTarget.Reset();
			DestroyRoamingGhost();
		}
	}
}

void ANPPossessionMapEvent::BeginNextChaseCycle()
{
	if (!HasAuthority() || !IsEventActive() || IsActorBeingDestroyed())
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(ChaseCycleTimer);
	FTransform RestartTransform;
	bool bHasRestartTransform = false;
	bool bRestartingAfterPossession = false;
	for (ANPStablePhysicsPawn* AffectedPlayer : AffectedPlayers)
	{
		if (IsValid(AffectedPlayer) && !AffectedPlayer->IsActorBeingDestroyed())
		{
			RestartTransform = AffectedPlayer->GetActorTransform();
			bHasRestartTransform = true;
			bRestartingAfterPossession = true;
			break;
		}
	}
	if (!bHasRestartTransform && IsValid(SpawnedRoamingGhost))
	{
		RestartTransform = SpawnedRoamingGhost->GetActorTransform();
		bHasRestartTransform = true;
	}

	PreviousChaseTarget = CurrentChaseTarget;
	CurrentChaseTarget.Reset();
	DestroyRoamingGhost();
	if (!AffectedPlayers.IsEmpty())
	{
		AffectedPlayers.Reset();
		ForceNetUpdate();
		RefreshAppliedEffects();
	}

	const float ContactDelay = bRestartingAfterPossession && FMath::IsFinite(PostPossessionContactDelay)
		? FMath::Max(0.0f, PostPossessionContactDelay) : 0.0f;
	SpawnRoamingGhost(bHasRestartTransform ? &RestartTransform : nullptr, ContactDelay);
	ANPStablePhysicsPawn* NewTarget = SelectRandomChaseTarget();
	if (IsValid(SpawnedRoamingGhost) && IsValid(NewTarget)
		&& SpawnedRoamingGhost->SetRoamingChaseTarget(NewTarget))
	{
		CurrentChaseTarget = NewTarget;
		UE_LOG(LogNPPossessionEvent, Display,
			TEXT("Roaming 유령 추격 시작: Ghost=%s Target=%s Duration=%.2fs Start=%s"),
			*GetNameSafe(SpawnedRoamingGhost), *GetNameSafe(NewTarget),
			ChaseTargetDuration, *SpawnedRoamingGhost->GetActorLocation().ToCompactString());
	}
	else
	{
		UE_LOG(LogNPPossessionEvent, Warning,
			TEXT("Roaming 유령 추격 대상 없음: Ghost=%s. 다음 주기에 다시 검색합니다."),
			*GetNameSafe(SpawnedRoamingGhost));
	}

	const float CycleDuration = FMath::IsFinite(ChaseTargetDuration)
		? FMath::Max(0.1f, ChaseTargetDuration) : 10.0f;
	GetWorldTimerManager().SetTimer(
		ChaseCycleTimer, this, &ThisClass::BeginNextChaseCycle, CycleDuration, false);
}

ANPStablePhysicsPawn* ANPPossessionMapEvent::SelectRandomChaseTarget() const
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !World)
	{
		return nullptr;
	}

	TArray<ANPStablePhysicsPawn*> Candidates;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		ANPStablePhysicsPawn* Pawn = IsValid(PC) ? Cast<ANPStablePhysicsPawn>(PC->GetPawn()) : nullptr;
		if (IsValid(Pawn) && !Pawn->IsActorBeingDestroyed() && Pawn->HasActorBegunPlay())
		{
			Candidates.AddUnique(Pawn);
		}
	}
	if (Candidates.Num() > 1 && PreviousChaseTarget.IsValid())
	{
		Candidates.Remove(PreviousChaseTarget.Get());
	}
	return Candidates.IsEmpty() ? nullptr : Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
}

void ANPPossessionMapEvent::SpawnRoamingGhost(
	const FTransform* OverrideTransform,
	const float ContactDelay)
{
	UE_LOG(LogNPPossessionEvent, Display,
		TEXT("[PossessionTrace] SpawnRoamingGhost 진입: Event=%s Authority=%d Active=%d ExistingGhost=%s"),
		*GetNameSafe(this), HasAuthority() ? 1 : 0, IsEventActive() ? 1 : 0,
		*GetNameSafe(SpawnedRoamingGhost));
	if (!HasAuthority() || !IsEventActive() || IsValid(SpawnedRoamingGhost))
	{
		UE_LOG(LogNPPossessionEvent, Warning,
			TEXT("[PossessionTrace] SpawnRoamingGhost 중단: Authority=%d Active=%d ExistingGhostValid=%d ExistingGhost=%s"),
			HasAuthority() ? 1 : 0, IsEventActive() ? 1 : 0,
			IsValid(SpawnedRoamingGhost) ? 1 : 0, *GetNameSafe(SpawnedRoamingGhost));
		return;
	}

	UNPMapEventManagerComponent* EventManager = GetOwner()
		? GetOwner()->FindComponentByClass<UNPMapEventManagerComponent>()
		: nullptr;
	const TSubclassOf<ANPGhostFollowerActor> SelectedGhostClass = RoamingGhostClass
		? RoamingGhostClass : GhostClass;
	ANPMapEventSpawnPoint* SpawnPoint = !OverrideTransform && EventManager && RoamingGhostSpawnGroup.IsValid()
		? EventManager->FindRandomSpawnPoint(RoamingGhostSpawnGroup)
		: nullptr;
	UE_LOG(LogNPPossessionEvent, Display,
		TEXT("[PossessionTrace] 유령 생성 설정 확인: Manager=%s GhostClass=%s Group=%s GroupValid=%d InitialPoint=%s"),
		*GetNameSafe(EventManager), *GetNameSafe(SelectedGhostClass.Get()),
		*RoamingGhostSpawnGroup.ToString(), RoamingGhostSpawnGroup.IsValid() ? 1 : 0,
		*GetNameSafe(SpawnPoint));
	FGameplayTag SelectedSpawnGroup = RoamingGhostSpawnGroup;
	if (!OverrideTransform && EventManager && !SpawnPoint)
	{
		const FGameplayTag CommonSpawnGroup = FGameplayTag::RequestGameplayTag(FName(TEXT("Common")), false);
		SpawnPoint = EventManager->FindRandomSpawnPoint(CommonSpawnGroup);
		if (SpawnPoint)
		{
			SelectedSpawnGroup = CommonSpawnGroup;
			UE_LOG(LogNPPossessionEvent, Warning,
				TEXT("Possession 그룹 Point가 없어 Common Point를 사용합니다: Point=%s"),
				*GetNameSafe(SpawnPoint));
		}
	}
	if (!EventManager || !SelectedGhostClass || !RoamingGhostSpawnGroup.IsValid()
		|| (!OverrideTransform && !SpawnPoint))
	{
		UE_LOG(LogNPPossessionEvent, Error,
			TEXT("빙의 유령 Point 생성 실패: Manager=%s Class=%s Group=%s Point=%s"),
			*GetNameSafe(EventManager), *GetNameSafe(SelectedGhostClass.Get()),
			*RoamingGhostSpawnGroup.ToString(), *GetNameSafe(SpawnPoint));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FTransform SpawnTransform = OverrideTransform
		? *OverrideTransform : SpawnPoint->GetActorTransform();
	ANPGhostFollowerActor* Ghost = World->SpawnActorDeferred<ANPGhostFollowerActor>(
		SelectedGhostClass,
		SpawnTransform,
		this,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Ghost || !Ghost->InitializeRoamingGhost())
	{
		if (Ghost)
		{
			Ghost->Destroy();
		}
		UE_LOG(LogNPPossessionEvent, Error, TEXT("빙의 유령 초기화 실패: Class=%s Point=%s"),
			*GetNameSafe(SelectedGhostClass.Get()), *GetNameSafe(SpawnPoint));
		return;
	}
	Ghost->SetRoamingContactDelay(ContactDelay);

	SpawnedRoamingGhost = Ghost;
	UGameplayStatics::FinishSpawningActor(Ghost, SpawnTransform);
	if (!IsValid(Ghost) || Ghost->IsActorBeingDestroyed())
	{
		SpawnedRoamingGhost = nullptr;
		return;
	}
	Ghost->ForceNetUpdate();
	UE_LOG(LogNPPossessionEvent, Display,
		TEXT("빙의 유령 생성 성공: Ghost=%s Point=%s Group=%s Location=%s Restart=%d ContactDelay=%.2fs"),
		*GetNameSafe(Ghost), *GetNameSafe(SpawnPoint), *SelectedSpawnGroup.ToString(),
		*Ghost->GetActorLocation().ToCompactString(), OverrideTransform ? 1 : 0, ContactDelay);
}

void ANPPossessionMapEvent::DestroyRoamingGhost()
{
	ANPGhostFollowerActor* Ghost = SpawnedRoamingGhost;
	SpawnedRoamingGhost = nullptr;
	if (HasAuthority() && IsValid(Ghost))
	{
		Ghost->ConsumeRoamingGhost();
	}
}

void ANPPossessionMapEvent::HandleRoamingGhostContact(
	ANPGhostFollowerActor* Ghost,
	ANPStablePhysicsPawn* PlayerPawn)
{
	if (!HasAuthority() || !IsEventActive() || Ghost != SpawnedRoamingGhost
		|| !IsValid(Ghost) || !IsValid(PlayerPawn) || PlayerPawn->IsActorBeingDestroyed()
		|| !PlayerPawn->IsPlayerControlled())
	{
		return;
	}

	SpawnedRoamingGhost = nullptr;
	AffectedPlayers.AddUnique(PlayerPawn);
	ForceNetUpdate();

	Ghost->ConsumeRoamingGhost();
	RefreshAppliedEffects();
	GetWorldTimerManager().ClearTimer(ChaseCycleTimer);
	const float SafePossessionDuration = FMath::IsFinite(PossessionDuration)
		? FMath::Max(0.1f, PossessionDuration) : 5.0f;
	GetWorldTimerManager().SetTimer(
		ChaseCycleTimer, this, &ThisClass::BeginNextChaseCycle,
		SafePossessionDuration, false);

	UE_LOG(LogNPPossessionEvent, Display,
		TEXT("빙의 유령 접촉 적용 완료: Player=%s AffectedCount=%d ReverseInput=%d Duration=%.2fs"),
		*GetNameSafe(PlayerPawn), AffectedPlayers.Num(), bReverseHorizontalInput ? 1 : 0,
		SafePossessionDuration);
}

void ANPPossessionMapEvent::UpdateTrackingState()
{
	if (!HasAuthority())
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(PlayerRefreshTimer);
	if (!IsEventActive())
	{
		RemoveTemporaryCaseUnlocks();
		RemoveAppliedEffects();
		if (HasAuthority() && !AffectedPlayers.IsEmpty())
		{
			AffectedPlayers.Reset();
			ForceNetUpdate();
		}
		return;
	}
	bWarnedUnsupportedPawn = false;
	RefreshPlayersAndGhosts();
	if (!IsEventActive() || IsActorBeingDestroyed())
	{
		return;
	}
	const float Interval = FMath::IsFinite(PlayerRefreshInterval)
		? FMath::Max(0.05f, PlayerRefreshInterval) : 0.2f;
	GetWorldTimerManager().SetTimer(PlayerRefreshTimer, this, &ThisClass::RefreshPlayersAndGhosts, Interval, true);
}

void ANPPossessionMapEvent::RefreshPlayersAndGhosts()
{
	UWorld* World = GetWorld();
	if (!World || !IsEventActive())
	{
		return;
	}
	if (HasAuthority())
	{
		RefreshRelicCases();
		if (!IsEventActive() || IsActorBeingDestroyed())
		{
			return;
		}
		if (bEnableGhostAndControlEffects)
		{
			TArray<TObjectPtr<ANPStablePhysicsPawn>> CurrentPlayers;
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				const APlayerController* PC = It->Get();
				ANPStablePhysicsPawn* Pawn = IsValid(PC) ? Cast<ANPStablePhysicsPawn>(PC->GetPawn()) : nullptr;
				if (IsValid(Pawn) && !Pawn->IsActorBeingDestroyed() && Pawn->HasActorBegunPlay())
				{
					CurrentPlayers.AddUnique(Pawn);
				}
			}
			if (CurrentPlayers != AffectedPlayers)
			{
				AffectedPlayers = MoveTemp(CurrentPlayers);
				ForceNetUpdate();
			}
		}
		else
		{
			const int32 RemovedCount = AffectedPlayers.RemoveAll(
				[](const ANPStablePhysicsPawn* Pawn)
				{
					return !IsValid(Pawn) || Pawn->IsActorBeingDestroyed();
				});
			if (RemovedCount > 0)
			{
				ForceNetUpdate();
			}
		}
		RefreshAppliedEffects();
	}
}

void ANPPossessionMapEvent::RefreshRelicCases()
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !World || !IsEventActive() || IsActorBeingDestroyed())
	{
		return;
	}

	for (auto It = TemporarilyUnlockedCases.CreateIterator(); It; ++It)
	{
		if (!It->IsValid())
		{
			It.RemoveCurrent();
		}
	}
	// 기존 갱신 타이머를 재사용하여 진행 중 생성/스트리밍된 케이스도 포함합니다.
	for (TActorIterator<ANPRelicCase> It(World); It; ++It)
	{
		ANPRelicCase* RelicCase = *It;
		if (!IsValid(RelicCase) || RelicCase->IsActorBeingDestroyed() ||
			!RelicCase->HasActorBegunPlay() || RelicCase->IsBroken())
		{
			continue;
		}
		const TWeakObjectPtr<ANPRelicCase> CaseKey(RelicCase);
		if (TemporarilyUnlockedCases.Contains(CaseKey))
		{
			continue;
		}
		// OnCaseUnlocked BP에서 이벤트를 종료해도 방금 추가한 해제를 회수할 수 있습니다.
		TemporarilyUnlockedCases.Add(CaseKey);
		RelicCase->SetTemporaryUnlock(this, true);
		if (!IsEventActive() || IsActorBeingDestroyed())
		{
			return;
		}
	}
}

void ANPPossessionMapEvent::RemoveTemporaryCaseUnlocks()
{
	const auto CasesToRestore = MoveTemp(TemporarilyUnlockedCases);
	TemporarilyUnlockedCases.Reset();
	for (const TWeakObjectPtr<ANPRelicCase>& CaseKey : CasesToRestore)
	{
		if (ANPRelicCase* RelicCase = CaseKey.Get();
			IsValid(RelicCase) && !RelicCase->IsActorBeingDestroyed())
		{
			RelicCase->SetTemporaryUnlock(this, false);
		}
	}
}

void ANPPossessionMapEvent::RefreshAppliedEffects()
{
	if (!HasAuthority())
	{
		return;
	}
	if (!IsEventActive() || !bReverseHorizontalInput)
	{
		RemoveAppliedEffects();
		return;
	}

	TSet<TWeakObjectPtr<UAbilitySystemComponent>> EligibleSystems;
	// 태그 콜백에서 이벤트가 종료되어도 순회할 대상 목록은 유지합니다.
	const auto Players = AffectedPlayers;
	for (ANPStablePhysicsPawn* Pawn : Players)
	{
		if (!IsValid(Pawn) || Pawn->IsActorBeingDestroyed())
		{
			continue;
		}
		UAbilitySystemComponent* AbilitySystem = Pawn->FindComponentByClass<UAbilitySystemComponent>();
		if (!IsValid(AbilitySystem) || AbilitySystem->GetAvatarActor() != Pawn
			|| !Pawn->FindComponentByClass<UNPControlReversalComponent>())
		{
			if (!bWarnedUnsupportedPawn)
			{
				UE_LOG(LogNPPossessionEvent, Warning, TEXT("빙의 입력 반전 대기: Pawn=%s. 초기화된 ASC와 ControlReversal 컴포넌트가 필요합니다. NPReplicatedStablePhysicsPawn 계열인지 확인하세요."), *GetNameSafe(Pawn));
				bWarnedUnsupportedPawn = true;
			}
			continue;
		}
		const TWeakObjectPtr<UAbilitySystemComponent> SystemKey(AbilitySystem);
		EligibleSystems.Add(SystemKey);
		const FActiveGameplayEffectHandle* ExistingHandle = AppliedEffects.Find(SystemKey);
		if (ExistingHandle && AbilitySystem->GetActiveGameplayEffect(*ExistingHandle))
		{
			continue;
		}
		FGameplayEffectContextHandle Context = AbilitySystem->MakeEffectContext();
		Context.AddSourceObject(this);
		const FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(
			UNPPossessionGameplayEffect::StaticClass(), 1.0f, Context);
		if (Spec.IsValid())
		{
			const FActiveGameplayEffectHandle Handle = AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
			if (!IsEventActive() || IsActorBeingDestroyed())
			{
				if (Handle.IsValid())
				{
					AbilitySystem->RemoveActiveGameplayEffect(Handle);
				}
				return;
			}
			if (Handle.IsValid())
			{
				AppliedEffects.Add(SystemKey, Handle);
			}
		}
	}

	TMap<TWeakObjectPtr<UAbilitySystemComponent>, FActiveGameplayEffectHandle> EffectsToRemove;
	for (auto It = AppliedEffects.CreateIterator(); It; ++It)
	{
		if (!EligibleSystems.Contains(It.Key()))
		{
			EffectsToRemove.Add(It.Key(), It.Value());
			It.RemoveCurrent();
		}
	}
	for (const auto& Entry : EffectsToRemove)
	{
		if (UAbilitySystemComponent* AbilitySystem = Entry.Key.Get())
		{
			AbilitySystem->RemoveActiveGameplayEffect(Entry.Value);
		}
	}
}

void ANPPossessionMapEvent::RemoveAppliedEffects()
{
	// 이 이벤트가 만든 효과만 제거합니다. 다른 빙의가 남아 있으면 반전을 유지합니다.
	const auto EffectsToRemove = MoveTemp(AppliedEffects);
	AppliedEffects.Reset();
	for (const auto& Entry : EffectsToRemove)
	{
		if (UAbilitySystemComponent* AbilitySystem = Entry.Key.Get())
		{
			AbilitySystem->RemoveActiveGameplayEffect(Entry.Value);
		}
	}
}

void ANPPossessionMapEvent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(PlayerRefreshTimer);
	GetWorldTimerManager().ClearTimer(ChaseCycleTimer);
	RemoveTemporaryCaseUnlocks();
	RemoveAppliedEffects();
	DestroyRoamingGhost();
	AffectedPlayers.Reset();
	CurrentChaseTarget.Reset();
	PreviousChaseTarget.Reset();
	Super::EndPlay(EndPlayReason);
}
