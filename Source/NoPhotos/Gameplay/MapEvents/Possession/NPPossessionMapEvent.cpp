#include "NPPossessionMapEvent.h"

#include "AbilitySystemComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Gameplay/Character/NPStablePhysicsPawn.h"
#include "Gameplay/Character/Component/NPControlReversalComponent.h"
#include "Gameplay/AbilitySystem/Effects/NPPossessionGameplayEffect.h"
#include "Gameplay/Relic/Case/NPRelicCase.h"
#include "Kismet/GameplayStatics.h"
#include "NPGhostFollowerActor.h"
#include "NPGhostPatrolRoute.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogNPPossessionEvent, Log, All);

ANPPossessionMapEvent::ANPPossessionMapEvent()
{
	LocationSource = ENPMapEventLocationSource::Point;
	RoamingGhostRouteGroup = FGameplayTag::RequestGameplayTag(FName(TEXT("Possession")), false);
}

void ANPPossessionMapEvent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANPPossessionMapEvent, AffectedPlayers);
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
			SpawnRoamingGhostsToCount();
		}
		else
		{
			GetWorldTimerManager().ClearTimer(RoamingSpawnRetryTimer);
			ClearPossessionTimers();
			DestroyRoamingGhosts();
		}
	}
}

void ANPPossessionMapEvent::SpawnRoamingGhostsToCount(const float ContactDelay)
{
	if (!HasAuthority() || !IsEventActive() || IsActorBeingDestroyed())
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(RoamingSpawnRetryTimer);
	SpawnedRoamingGhosts.RemoveAll(
		[](const ANPGhostFollowerActor* Ghost)
		{
			return !IsValid(Ghost) || Ghost->IsActorBeingDestroyed();
		});

	const int32 TotalGhostCount = FMath::Clamp(RoamingGhostCount, 1, 50);
	const int32 DesiredPatrolCount = FMath::Max(0, TotalGhostCount - PossessionTimers.Num());
	while (SpawnedRoamingGhosts.Num() < DesiredPatrolCount)
	{
		if (!SpawnRoamingGhost(ContactDelay))
		{
			ScheduleRoamingSpawnRetry();
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

void ANPPossessionMapEvent::ScheduleRoamingSpawnRetry()
{
	if (!HasAuthority() || !IsEventActive() || IsActorBeingDestroyed())
	{
		return;
	}

	const float RetryDelay = FMath::IsFinite(PatrolSpawnRetryInterval)
		? FMath::Max(0.1f, PatrolSpawnRetryInterval) : 2.0f;
	FTimerDelegate RetryDelegate = FTimerDelegate::CreateUObject(
		this, &ThisClass::SpawnRoamingGhostsToCount, 0.0f);
	GetWorldTimerManager().SetTimer(
		RoamingSpawnRetryTimer, RetryDelegate, RetryDelay, false);
}

ANPGhostPatrolRoute* ANPPossessionMapEvent::FindAvailablePatrolRoute() const
{
	UWorld* World = GetWorld();
	if (!World || !RoamingGhostRouteGroup.IsValid())
	{
		return nullptr;
	}

	TSet<const ANPGhostPatrolRoute*> AssignedRoutes;
	for (const ANPGhostFollowerActor* Ghost : SpawnedRoamingGhosts)
	{
		if (IsValid(Ghost))
		{
			if (const ANPGhostPatrolRoute* AssignedRoute = Ghost->GetRoamingPatrolRoute())
			{
				AssignedRoutes.Add(AssignedRoute);
			}
		}
	}

	TArray<ANPGhostPatrolRoute*> Candidates;
	for (TActorIterator<ANPGhostPatrolRoute> It(World); It; ++It)
	{
		ANPGhostPatrolRoute* Route = *It;
		if (IsValid(Route) && Route->SupportsRouteGroup(RoamingGhostRouteGroup)
			&& Route->IsUsableRoute() && !AssignedRoutes.Contains(Route))
		{
			Candidates.Add(Route);
		}
	}
	return Candidates.IsEmpty() ? nullptr : Candidates[FMath::RandHelper(Candidates.Num())];
}

bool ANPPossessionMapEvent::SpawnRoamingGhost(
	const float ContactDelay)
{
	UE_LOG(LogNPPossessionEvent, Display,
		TEXT("[PossessionTrace] SpawnRoamingGhost 진입: Event=%s Authority=%d Active=%d PatrolCount=%d"),
		*GetNameSafe(this), HasAuthority() ? 1 : 0, IsEventActive() ? 1 : 0,
		SpawnedRoamingGhosts.Num());
	if (!HasAuthority() || !IsEventActive() || IsActorBeingDestroyed())
	{
		return false;
	}

	const TSubclassOf<ANPGhostFollowerActor> SelectedGhostClass = RoamingGhostClass
		? RoamingGhostClass : GhostClass;
	ANPGhostPatrolRoute* PatrolRoute = FindAvailablePatrolRoute();
	if (!SelectedGhostClass || !RoamingGhostRouteGroup.IsValid() || !PatrolRoute)
	{
		UE_LOG(LogNPPossessionEvent, Warning,
			TEXT("빙의 유령 순찰 생성 대기: Class=%s RouteGroup=%s AvailableRoute=%s ActiveGhosts=%d RequestedGhosts=%d. 한 루트에는 한 마리만 배정합니다."),
			*GetNameSafe(SelectedGhostClass.Get()), *RoamingGhostRouteGroup.ToString(),
			*GetNameSafe(PatrolRoute), SpawnedRoamingGhosts.Num(),
			FMath::Clamp(RoamingGhostCount, 1, 50));
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const bool bStartForward = FMath::RandBool();
	const float RouteLength = PatrolRoute->GetSpline()->GetSplineLength();
	const float StartDistance = FMath::FRandRange(0.0f, RouteLength);
	FVector StartDirection = PatrolRoute->GetWorldDirectionAtDistance(StartDistance)
		* (bStartForward ? 1.0f : -1.0f);
	StartDirection.Z = 0.0f;
	const FRotator StartRotation = StartDirection.IsNearlyZero()
		? PatrolRoute->GetActorRotation() : StartDirection.Rotation();
	const FTransform SpawnTransform(StartRotation, PatrolRoute->GetWorldLocationAtDistance(StartDistance));
	ANPGhostFollowerActor* Ghost = World->SpawnActorDeferred<ANPGhostFollowerActor>(
		SelectedGhostClass,
		SpawnTransform,
		this,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Ghost || !Ghost->InitializeRoamingGhost(PatrolRoute, StartDistance, bStartForward))
	{
		if (Ghost)
		{
			Ghost->Destroy();
		}
		UE_LOG(LogNPPossessionEvent, Error, TEXT("빙의 유령 초기화 실패: Class=%s Route=%s"),
			*GetNameSafe(SelectedGhostClass.Get()), *GetNameSafe(PatrolRoute));
		return false;
	}
	Ghost->SetRoamingContactDelay(ContactDelay);

	UGameplayStatics::FinishSpawningActor(Ghost, SpawnTransform);
	if (!IsValid(Ghost) || Ghost->IsActorBeingDestroyed())
	{
		return false;
	}
	SpawnedRoamingGhosts.Add(Ghost);
	Ghost->OnDestroyed.AddDynamic(this, &ThisClass::HandleRoamingGhostDestroyed);
	Ghost->ForceNetUpdate();
	UE_LOG(LogNPPossessionEvent, Display,
		TEXT("빙의 유령 순찰 시작: Ghost=%s Route=%s Group=%s Direction=%s Location=%s ContactDelay=%.2fs PatrolCount=%d/%d"),
		*GetNameSafe(Ghost), *GetNameSafe(PatrolRoute), *RoamingGhostRouteGroup.ToString(),
		bStartForward ? TEXT("Forward") : TEXT("Reverse"),
		*Ghost->GetActorLocation().ToCompactString(), ContactDelay,
		SpawnedRoamingGhosts.Num(), FMath::Clamp(RoamingGhostCount, 1, 50));
	return true;
}

void ANPPossessionMapEvent::DestroyRoamingGhosts()
{
	const auto GhostsToDestroy = MoveTemp(SpawnedRoamingGhosts);
	SpawnedRoamingGhosts.Reset();
	if (!HasAuthority())
	{
		return;
	}
	for (ANPGhostFollowerActor* Ghost : GhostsToDestroy)
	{
		if (IsValid(Ghost))
		{
			Ghost->ConsumeRoamingGhost();
		}
	}
}

void ANPPossessionMapEvent::HandleRoamingGhostContact(
	ANPGhostFollowerActor* Ghost,
	ANPStablePhysicsPawn* PlayerPawn)
{
	if (!HasAuthority() || !IsEventActive() || !SpawnedRoamingGhosts.Contains(Ghost)
		|| !IsValid(Ghost) || !IsValid(PlayerPawn) || PlayerPawn->IsActorBeingDestroyed()
		|| !PlayerPawn->IsPlayerControlled() || AffectedPlayers.Contains(PlayerPawn))
	{
		return;
	}

	SpawnedRoamingGhosts.RemoveSingleSwap(Ghost);
	AffectedPlayers.AddUnique(PlayerPawn);
	ForceNetUpdate();

	Ghost->ConsumeRoamingGhost();
	RefreshAppliedEffects();
	RefreshLocalGhosts();
	if (!IsEventActive() || IsActorBeingDestroyed())
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(ChaseCycleTimer);
	const float SafePossessionDuration = FMath::IsFinite(PossessionDuration)
		? FMath::Max(0.1f, PossessionDuration) : 5.0f;
	const TWeakObjectPtr<ANPStablePhysicsPawn> PlayerKey(PlayerPawn);
	FTimerHandle& PossessionTimer = PossessionTimers.FindOrAdd(PlayerKey);
	FTimerDelegate FinishDelegate = FTimerDelegate::CreateUObject(
		this, &ThisClass::FinishPossession, PlayerKey);
	GetWorldTimerManager().SetTimer(PossessionTimer, FinishDelegate, SafePossessionDuration, false);

	UE_LOG(LogNPPossessionEvent, Display,
		TEXT("빙의 유령 접촉 적용 완료: Player=%s AffectedCount=%d PatrolCount=%d ReverseInput=%d Duration=%.2fs"),
		*GetNameSafe(PlayerPawn), AffectedPlayers.Num(), SpawnedRoamingGhosts.Num(),
		bReverseHorizontalInput ? 1 : 0,
		SafePossessionDuration);
}

bool ANPPossessionMapEvent::CanRoamingGhostTarget(
	const ANPStablePhysicsPawn* PlayerPawn) const
{
	return HasAuthority() && IsEventActive() && !IsActorBeingDestroyed()
		&& IsValid(PlayerPawn) && !PlayerPawn->IsActorBeingDestroyed()
		&& PlayerPawn->IsPlayerControlled() && !AffectedPlayers.Contains(PlayerPawn);
}

void ANPPossessionMapEvent::FinishPossession(
	const TWeakObjectPtr<ANPStablePhysicsPawn> PlayerKey)
{
	PossessionTimers.Remove(PlayerKey);
	const ANPStablePhysicsPawn* Player = PlayerKey.Get();
	const int32 RemovedCount = AffectedPlayers.RemoveAll(
		[Player](const ANPStablePhysicsPawn* AffectedPlayer)
		{
			return !IsValid(AffectedPlayer) || AffectedPlayer == Player;
		});
	if (RemovedCount > 0)
	{
		ForceNetUpdate();
		RefreshAppliedEffects();
		RefreshLocalGhosts();
	}

	const float ContactDelay = FMath::IsFinite(PostPossessionContactDelay)
		? FMath::Max(0.0f, PostPossessionContactDelay) : 0.0f;
	SpawnRoamingGhostsToCount(ContactDelay);
}

void ANPPossessionMapEvent::ClearPossessionTimers()
{
	for (auto& Entry : PossessionTimers)
	{
		GetWorldTimerManager().ClearTimer(Entry.Value);
	}
	PossessionTimers.Reset();
}

void ANPPossessionMapEvent::HandleRoamingGhostDestroyed(AActor* DestroyedActor)
{
	const int32 RemovedCount = SpawnedRoamingGhosts.RemoveAll(
		[DestroyedActor](const ANPGhostFollowerActor* Ghost)
		{
			return Ghost == DestroyedActor || !IsValid(Ghost);
		});
	if (RemovedCount > 0)
	{
		ScheduleRoamingSpawnRetry();
	}
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
	if (!AffectedPlayers.IsEmpty()
		&& (!GhostClass || GhostClass->HasAnyClassFlags(CLASS_Abstract))
		&& !bWarnedMissingGhostClass)
	{
		UE_LOG(LogNPPossessionEvent, Warning,
			TEXT("빙의 추적 유령 생성 대기: Event=%s GhostClass가 비어 있거나 추상 클래스입니다."),
			*GetName());
		bWarnedMissingGhostClass = true;
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

void ANPPossessionMapEvent::OnRep_AffectedPlayers()
{
	if (HasActorBegunPlay())
	{
		RefreshLocalGhosts();
	}
}

void ANPPossessionMapEvent::RefreshLocalGhosts()
{
	if (!IsEventActive() || GetNetMode() == NM_DedicatedServer)
	{
		ClearLocalGhosts();
		return;
	}

	TSet<TWeakObjectPtr<ANPStablePhysicsPawn>> DesiredPlayers;
	for (ANPStablePhysicsPawn* Pawn : AffectedPlayers)
	{
		if (IsValid(Pawn) && !Pawn->IsActorBeingDestroyed() && Pawn->HasActorBegunPlay())
		{
			DesiredPlayers.Add(Pawn);
		}
	}

	TArray<TWeakObjectPtr<ANPGhostFollowerActor>> GhostsToRemove;
	for (auto It = LocalGhosts.CreateIterator(); It; ++It)
	{
		if (!DesiredPlayers.Contains(It.Key()) || !It.Value().IsValid())
		{
			GhostsToRemove.Add(It.Value());
			It.RemoveCurrent();
		}
	}
	for (const TWeakObjectPtr<ANPGhostFollowerActor>& GhostPtr : GhostsToRemove)
	{
		if (ANPGhostFollowerActor* Ghost = GhostPtr.Get())
		{
			Ghost->RequestFadeOut();
		}
	}

	if (!IsEventActive() || IsActorBeingDestroyed())
	{
		return;
	}
	if (!GhostClass || GhostClass->HasAnyClassFlags(CLASS_Abstract))
	{
		ClearLocalGhosts();
		return;
	}

	for (const TWeakObjectPtr<ANPStablePhysicsPawn>& TargetKey : DesiredPlayers)
	{
		if (LocalGhosts.Contains(TargetKey))
		{
			continue;
		}
		ANPStablePhysicsPawn* Target = TargetKey.Get();
		if (!IsValid(Target))
		{
			continue;
		}

		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.OverrideLevel = GetWorld()->PersistentLevel;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.bDeferConstruction = true;
		const FTransform SpawnTransform(Target->GetVisualFacingRotation(), Target->GetActorLocation());
		ANPGhostFollowerActor* Ghost = GetWorld()->SpawnActor<ANPGhostFollowerActor>(
			GhostClass, SpawnTransform, Params);
		if (IsValid(Ghost) && Ghost->InitializeFollower(Target))
		{
			UGameplayStatics::FinishSpawningActor(Ghost, Ghost->GetActorTransform());
			if (IsValid(Ghost))
			{
				Ghost->SetReplicates(false);
				Ghost->SetReplicateMovement(false);
			}
			if (IsValid(Ghost) && IsEventActive() && !IsActorBeingDestroyed())
			{
				LocalGhosts.Add(TargetKey, Ghost);
				continue;
			}
		}

		if (IsValid(Ghost))
		{
			Ghost->Destroy();
		}
		if (!IsEventActive() || IsActorBeingDestroyed())
		{
			return;
		}
		if (!bWarnedSpawnFailure)
		{
			UE_LOG(LogNPPossessionEvent, Warning,
				TEXT("빙의 유령 생성 실패: Event=%s Class=%s. 유령 BP의 Construction/BeginPlay 설정을 확인하세요."),
				*GetName(), *GetNameSafe(GhostClass.Get()));
			bWarnedSpawnFailure = true;
		}
	}
}

void ANPPossessionMapEvent::ClearLocalGhosts(const bool bImmediately)
{
	const auto GhostsToDestroy = MoveTemp(LocalGhosts);
	LocalGhosts.Reset();
	for (const auto& Entry : GhostsToDestroy)
	{
		if (ANPGhostFollowerActor* Ghost = Entry.Value.Get())
		{
			if (bImmediately)
			{
				Ghost->Destroy();
			}
			else
			{
				Ghost->RequestFadeOut();
			}
		}
	}
}

void ANPPossessionMapEvent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(PlayerRefreshTimer);
	GetWorldTimerManager().ClearTimer(RoamingSpawnRetryTimer);
	ClearPossessionTimers();
	RemoveTemporaryCaseUnlocks();
	RemoveAppliedEffects();
	DestroyRoamingGhosts();
	ClearLocalGhosts(EndPlayReason != EEndPlayReason::Destroyed);
	DestroyRoamingGhost();
	AffectedPlayers.Reset();
	Super::EndPlay(EndPlayReason);
}
