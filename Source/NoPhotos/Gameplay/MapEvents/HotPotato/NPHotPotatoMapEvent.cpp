#include "NPHotPotatoMapEvent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/SceneComponent.h"
#include "Core/GameplayTag/NPGameplayTags.h"
#include "Core/Main/NPMainGameState.h"
#include "Core/NPPlayerState.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/GameStateBase.h"
#include "Gameplay/AbilitySystem/Effects/NPCrowdControlImmunityGameplayEffect.h"
#include "Gameplay/Character/Component/NPStablePhysicsMovementComponent.h"
#include "Gameplay/Character/NPReplicatedStablePhysicsPawn.h"
#include "Gameplay/MapEvents/HotPotato/NPHotPotatoBomb.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogNPHotPotato, Log, All);

ANPHotPotatoMapEvent::ANPHotPotatoMapEvent()
{
	EventId = TEXT("HotPotato");
	EventDisplayName = NSLOCTEXT("MapEvent", "HotPotatoEventName", "폭탄 돌리기");
	EventDescription = NSLOCTEXT(
		"MapEvent",
		"HotPotatoEventDescription",
		"폭탄이 터지기 전에 다른 플레이어에게 넘기세요.");
	EventType = ENPMapEventType::TypeA;
	EventScale = ENPMapEventScale::Large;
	Duration = 120.0f;
	BombActorClass = ANPHotPotatoBomb::StaticClass();
	CarrierImmunityEffectClass =
		UNPCrowdControlImmunityGameplayEffect::StaticClass();
}

void ANPHotPotatoMapEvent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANPHotPotatoMapEvent, CurrentBombHolder);
	DOREPLIFETIME(ANPHotPotatoMapEvent, SpawnedBombActor);
	DOREPLIFETIME(ANPHotPotatoMapEvent, BombExplosionServerWorldTime);
}

float ANPHotPotatoMapEvent::GetRemainingBombTime() const
{
	const UWorld* World = GetWorld();
	if (!IsEventActive() || !World || BombExplosionServerWorldTime <= 0.0f)
	{
		return 0.0f;
	}

	const AGameStateBase* GameState = World->GetGameState();
	const float ServerWorldTime = GameState
		? GameState->GetServerWorldTimeSeconds()
		: World->GetTimeSeconds();
	return FMath::Max(0.0f, BombExplosionServerWorldTime - ServerWorldTime);
}

bool ANPHotPotatoMapEvent::SetCurrentBombHolder(ANPPlayerState* NewHolder)
{
	return HasAuthority()
		&& IsEventActive()
		&& IsEligibleParticipant(NewHolder)
		&& SetCurrentBombHolderInternal(NewHolder);
}

void ANPHotPotatoMapEvent::ApplyEventState_Implementation(const bool bNewActive)
{
	if (!HasAuthority())
	{
		return;
	}

	ClearRoundTimers();
	if (bNewActive)
	{
		BuildParticipantOrder();
		NextRoundStarterIndex = 0;
		StartNextRound();
		return;
	}

	DestroySpawnedBomb();
	SetCurrentBombHolderInternal(nullptr);
	ParticipantOrder.Reset();
	NextRoundStarterIndex = 0;
}

void ANPHotPotatoMapEvent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RemoveCarrierMoveSpeed();
	if (HasAuthority())
	{
		ClearRoundTimers();
		DestroySpawnedBomb();
		UnbindFromCurrentCarrierGrab();
		RemoveCarrierImmunity();
		CurrentBombHolder = nullptr;
		ParticipantOrder.Reset();
	}
	Super::EndPlay(EndPlayReason);
}

void ANPHotPotatoMapEvent::BuildParticipantOrder()
{
	ParticipantOrder.Reset();
	ANPMainGameState* MainGameState = GetWorld()
		? GetWorld()->GetGameState<ANPMainGameState>()
		: nullptr;
	if (!MainGameState)
	{
		UE_LOG(LogNPHotPotato, Warning,
			TEXT("폭탄 돌리기 참가자 구성 실패: MainGameState가 없습니다."));
		return;
	}

	MainGameState->RefreshPlayerRankings();
	const int32 ParticipantLimit = FMath::Clamp(MaximumParticipants, 1, 6);
	for (const FNPPlayerRanking& Ranking : MainGameState->GetPlayerRankings())
	{
		ANPPlayerState* PlayerState = Ranking.PlayerState;
		if (!IsValid(PlayerState) || !IsValid(PlayerState->GetPawn()))
		{
			continue;
		}

		ParticipantOrder.Add(PlayerState);
		if (ParticipantOrder.Num() >= ParticipantLimit)
		{
			break;
		}
	}

	UE_LOG(LogNPHotPotato, Log,
		TEXT("폭탄 돌리기 참가자 순위 스냅샷 완료: Participants=%d"),
		ParticipantOrder.Num());
}

void ANPHotPotatoMapEvent::StartNextRound()
{
	if (!HasAuthority() || !IsEventActive() || IsActorBeingDestroyed())
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(NextRoundTimer);
	ANPPlayerState* RoundStarter = FindNextRoundStarter();
	if (!RoundStarter)
	{
		UE_LOG(LogNPHotPotato, Warning,
			TEXT("폭탄 돌리기 라운드 시작 실패: 유효한 참가자가 없습니다."));
		return;
	}

	const float MinimumFuse = FMath::Max(
		0.1f,
		FMath::Min(MinimumFuseDuration, MaximumFuseDuration));
	const float MaximumFuse = FMath::Max(
		MinimumFuse,
		FMath::Max(MinimumFuseDuration, MaximumFuseDuration));
	const float SampledFuse = FMath::FRandRange(MinimumFuse, MaximumFuse);
	const float FuseDuration = FMath::Min(SampledFuse, GetRemainingEventTime());
	if (FuseDuration <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const AGameStateBase* GameState = GetWorld()->GetGameState();
	const float ServerWorldTime = GameState
		? GameState->GetServerWorldTimeSeconds()
		: GetWorld()->GetTimeSeconds();
	BombExplosionServerWorldTime = ServerWorldTime + FuseDuration;

	DestroySpawnedBomb();
	SetCurrentBombHolderInternal(RoundStarter);
	SpawnBombForCurrentHolder();
	GetWorldTimerManager().SetTimer(
		BombFuseTimer,
		this,
		&ThisClass::HandleBombFuseExpired,
		FuseDuration,
		false);
	GetWorldTimerManager().SetTimer(
		ScorePenaltyTimer,
		this,
		&ThisClass::HandleScorePenaltyTick,
		1.0f,
		true);
	ForceNetUpdate();

	UE_LOG(LogNPHotPotato, Log,
		TEXT("폭탄 라운드 시작: Holder=%s Fuse=%.2f Bomb=%s"),
		*GetNameSafe(CurrentBombHolder),
		FuseDuration,
		*GetNameSafe(SpawnedBombActor));
}

ANPPlayerState* ANPHotPotatoMapEvent::FindNextRoundStarter()
{
	if (ParticipantOrder.IsEmpty())
	{
		return nullptr;
	}

	for (int32 Attempt = 0; Attempt < ParticipantOrder.Num(); ++Attempt)
	{
		const int32 CandidateIndex = NextRoundStarterIndex % ParticipantOrder.Num();
		NextRoundStarterIndex = (CandidateIndex + 1) % ParticipantOrder.Num();
		ANPPlayerState* Candidate = ParticipantOrder[CandidateIndex];
		if (IsEligibleParticipant(Candidate))
		{
			return Candidate;
		}
	}
	return nullptr;
}

void ANPHotPotatoMapEvent::HandleScorePenaltyTick()
{
	if (!HasAuthority() || !IsEventActive())
	{
		return;
	}
	ApplyPercentagePenalty(
		CurrentBombHolder,
		ScorePenaltyPercentPerSecond,
		false);
}

void ANPHotPotatoMapEvent::HandleBombFuseExpired()
{
	if (!HasAuthority() || !IsEventActive())
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(BombFuseTimer);
	GetWorldTimerManager().ClearTimer(ScorePenaltyTimer);
	BombExplosionServerWorldTime = 0.0f;
	ANPPlayerState* ExplodedHolder = CurrentBombHolder;
	ApplyPercentagePenalty(
		ExplodedHolder,
		ExplosionScorePenaltyPercent,
		true);

	UE_LOG(LogNPHotPotato, Log,
		TEXT("폭탄 폭발 처리: Holder=%s Bomb=%s"),
		*GetNameSafe(CurrentBombHolder),
		*GetNameSafe(SpawnedBombActor));

	if (IsValid(SpawnedBombActor))
	{
		SpawnedBombActor->TriggerExplosion();
		SpawnedBombActor = nullptr;
	}
	SetCurrentBombHolderInternal(nullptr);
	LaunchExplodedCarrier(ExplodedHolder);
	ForceNetUpdate();
	ScheduleNextRound();
}

void ANPHotPotatoMapEvent::ScheduleNextRound()
{
	if (!HasAuthority() || !IsEventActive())
	{
		return;
	}

	const float RemainingEventTime = GetRemainingEventTime();
	const float SafeDelay = FMath::Max(0.0f, DelayBetweenRounds);
	if (RemainingEventTime <= SafeDelay + KINDA_SMALL_NUMBER)
	{
		return;
	}

	if (SafeDelay <= KINDA_SMALL_NUMBER)
	{
		StartNextRound();
		return;
	}
	GetWorldTimerManager().SetTimer(
		NextRoundTimer,
		this,
		&ThisClass::StartNextRound,
		SafeDelay,
		false);
}

int32 ANPHotPotatoMapEvent::ApplyPercentagePenalty(
	ANPPlayerState* PlayerState,
	const float PenaltyPercent,
	const bool bExplosionPenalty)
{
	if (!HasAuthority() || !IsValid(PlayerState))
	{
		return 0;
	}

	const int32 CurrentScore = FMath::Max(0, PlayerState->GetPlayerScore());
	const float SafePercent = FMath::Clamp(PenaltyPercent, 0.0f, 100.0f);
	if (CurrentScore <= 0 || SafePercent <= 0.0f)
	{
		return 0;
	}

	const int32 CalculatedPenalty = FMath::Max(
		1,
		FMath::RoundToInt(static_cast<float>(CurrentScore) * SafePercent / 100.0f));
	const int32 AppliedPenalty = FMath::Min(CurrentScore, CalculatedPenalty);
	PlayerState->AddScore(-AppliedPenalty);
	OnScorePenaltyApplied.Broadcast(
		PlayerState,
		AppliedPenalty,
		bExplosionPenalty);
	return AppliedPenalty;
}

bool ANPHotPotatoMapEvent::IsEligibleParticipant(
	const ANPPlayerState* PlayerState) const
{
	return IsValid(PlayerState)
		&& ParticipantOrder.Contains(PlayerState)
		&& IsValid(PlayerState->GetPawn());
}

bool ANPHotPotatoMapEvent::SetCurrentBombHolderInternal(
	ANPPlayerState* NewHolder)
{
	if (CurrentBombHolder == NewHolder)
	{
		return false;
	}

	UnbindFromCurrentCarrierGrab();
	RemoveCarrierImmunity();
	RemoveCarrierMoveSpeed();
	CurrentBombHolder = NewHolder;
	ApplyCarrierImmunity();
	ApplyCarrierMoveSpeed();
	BindToCurrentCarrierGrab();
	AttachBombToCurrentHolder();
	OnBombHolderChanged.Broadcast(CurrentBombHolder);
	ForceNetUpdate();
	return true;
}

void ANPHotPotatoMapEvent::SpawnBombForCurrentHolder()
{
	UWorld* World = GetWorld();
	APawn* HolderPawn = CurrentBombHolder ? CurrentBombHolder->GetPawn() : nullptr;
	if (!World || !BombActorClass || !IsValid(HolderPawn))
	{
		UE_LOG(LogNPHotPotato, Warning,
			TEXT("폭탄 생성 생략: Class=%s HolderPawn=%s"),
			*GetNameSafe(BombActorClass),
			*GetNameSafe(HolderPawn));
		return;
	}

	const FTransform SpawnTransform(
		HolderPawn->GetActorQuat(),
		HolderPawn->GetActorLocation());
	ANPHotPotatoBomb* Bomb = World->SpawnActorDeferred<ANPHotPotatoBomb>(
		BombActorClass,
		SpawnTransform,
		this,
		HolderPawn,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Bomb)
	{
		return;
	}

	Bomb->SetReplicates(true);
	Bomb->SetReplicateMovement(true);
	SpawnedBombActor = Cast<ANPHotPotatoBomb>(
		UGameplayStatics::FinishSpawningActor(Bomb, SpawnTransform));
	if (IsValid(SpawnedBombActor))
	{
		SpawnedBombActor->InitializeFuse(BombExplosionServerWorldTime);
	}
	AttachBombToCurrentHolder();
	ForceNetUpdate();
}

void ANPHotPotatoMapEvent::AttachBombToCurrentHolder()
{
	APawn* HolderPawn = CurrentBombHolder ? CurrentBombHolder->GetPawn() : nullptr;
	USceneComponent* AttachComponent = IsValid(HolderPawn)
		? HolderPawn->GetRootComponent()
		: nullptr;
	if (!IsValid(SpawnedBombActor) || !IsValid(AttachComponent))
	{
		return;
	}

	SpawnedBombActor->SetOwner(HolderPawn);
	SpawnedBombActor->AttachToComponent(
		AttachComponent,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		BombAttachSocketName);
	SpawnedBombActor->SetActorRelativeTransform(BombRelativeTransform);
}

void ANPHotPotatoMapEvent::DestroySpawnedBomb()
{
	if (IsValid(SpawnedBombActor))
	{
		SpawnedBombActor->Destroy();
	}
	SpawnedBombActor = nullptr;
}

void ANPHotPotatoMapEvent::ClearRoundTimers()
{
	GetWorldTimerManager().ClearTimer(BombFuseTimer);
	GetWorldTimerManager().ClearTimer(ScorePenaltyTimer);
	GetWorldTimerManager().ClearTimer(NextRoundTimer);
	BombExplosionServerWorldTime = 0.0f;
}

void ANPHotPotatoMapEvent::ApplyCarrierImmunity()
{
	APawn* HolderPawn = CurrentBombHolder ? CurrentBombHolder->GetPawn() : nullptr;
	UAbilitySystemComponent* AbilitySystem = IsValid(HolderPawn)
		? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HolderPawn)
		: nullptr;
	if (!IsValid(AbilitySystem) || !CarrierImmunityEffectClass)
	{
		return;
	}

	FGameplayEffectContextHandle Context = AbilitySystem->MakeEffectContext();
	Context.AddSourceObject(this);
	const FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(
		CarrierImmunityEffectClass,
		1.0f,
		Context);
	if (!Spec.IsValid())
	{
		return;
	}

	CarrierImmunityEffectHandle =
		AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	if (!CarrierImmunityEffectHandle.IsValid())
	{
		return;
	}
	CarrierAbilitySystem = AbilitySystem;

	// 폭탄을 받은 순간 이미 적용 중이던 사진 스턴이 있다면 함께 해제합니다.
	FGameplayTagContainer StunTags;
	StunTags.AddTag(NPGameplayTags::State_CrowdControl_Stunned);
	AbilitySystem->RemoveActiveEffectsWithGrantedTags(StunTags);
}

void ANPHotPotatoMapEvent::RemoveCarrierImmunity()
{
	if (CarrierImmunityEffectHandle.IsValid())
	{
		if (UAbilitySystemComponent* AbilitySystem = CarrierAbilitySystem.Get())
		{
			AbilitySystem->RemoveActiveGameplayEffect(CarrierImmunityEffectHandle);
		}
	}
	CarrierAbilitySystem.Reset();
	CarrierImmunityEffectHandle.Invalidate();
}

void ANPHotPotatoMapEvent::ApplyCarrierMoveSpeed()
{
	static const FName SpeedSource(TEXT("HotPotatoCarrier"));
	ANPReplicatedStablePhysicsPawn* CarrierPawn = CurrentBombHolder
		? Cast<ANPReplicatedStablePhysicsPawn>(CurrentBombHolder->GetPawn())
		: nullptr;
	UNPStablePhysicsMovementComponent* MovementComponent = IsValid(CarrierPawn)
		? CarrierPawn->GetStablePhysicsMovementComponent()
		: nullptr;
	if (!IsValid(MovementComponent))
	{
		return;
	}

	MovementComponent->SetMoveSpeedMultiplier(
		SpeedSource,
		FMath::Max(0.0f, CarrierMoveSpeedMultiplier));
	CarrierMovementComponent = MovementComponent;
}

void ANPHotPotatoMapEvent::RemoveCarrierMoveSpeed()
{
	static const FName SpeedSource(TEXT("HotPotatoCarrier"));
	if (UNPStablePhysicsMovementComponent* MovementComponent =
		CarrierMovementComponent.Get())
	{
		MovementComponent->ClearMoveSpeedMultiplier(SpeedSource);
	}
	CarrierMovementComponent.Reset();
}

void ANPHotPotatoMapEvent::LaunchExplodedCarrier(
	ANPPlayerState* ExplodedHolder)
{
	if (!HasAuthority() || !IsValid(ExplodedHolder))
	{
		return;
	}

	ANPReplicatedStablePhysicsPawn* ExplodedPawn =
		Cast<ANPReplicatedStablePhysicsPawn>(ExplodedHolder->GetPawn());
	if (!IsValid(ExplodedPawn))
	{
		return;
	}

	FVector BackwardDirection = -ExplodedPawn->GetActorForwardVector();
	BackwardDirection.Z = 0.0f;
	BackwardDirection = BackwardDirection.GetSafeNormal();
	const FVector LaunchVelocity =
		BackwardDirection * FMath::Max(0.0f, ExplosionHorizontalLaunchSpeed)
		+ FVector::UpVector * FMath::Max(0.0f, ExplosionVerticalLaunchSpeed);

	ExplodedPawn->StartTemporaryRagdoll();
	ExplodedPawn->AddExternalVelocityChange(LaunchVelocity);
}

void ANPHotPotatoMapEvent::BindToCurrentCarrierGrab()
{
	if (!HasAuthority() || !IsValid(CurrentBombHolder))
	{
		return;
	}

	ANPReplicatedStablePhysicsPawn* CarrierPawn =
		Cast<ANPReplicatedStablePhysicsPawn>(CurrentBombHolder->GetPawn());
	if (!IsValid(CarrierPawn))
	{
		return;
	}

	CarrierPawn->OnPlayerPawnGrabbed.AddUObject(
		this,
		&ThisClass::HandleCarrierGrabbedPlayer);
	BoundCarrierPawn = CarrierPawn;
}

void ANPHotPotatoMapEvent::UnbindFromCurrentCarrierGrab()
{
	if (ANPReplicatedStablePhysicsPawn* CarrierPawn = BoundCarrierPawn.Get())
	{
		CarrierPawn->OnPlayerPawnGrabbed.RemoveAll(this);
	}
	BoundCarrierPawn.Reset();
}

void ANPHotPotatoMapEvent::HandleCarrierGrabbedPlayer(
	ANPReplicatedStablePhysicsPawn* GrabbedPawn)
{
	if (!HasAuthority() || !IsEventActive() || !IsValid(GrabbedPawn))
	{
		return;
	}

	ANPPlayerState* GrabbedPlayerState =
		GrabbedPawn->GetPlayerState<ANPPlayerState>();
	if (!IsEligibleParticipant(GrabbedPlayerState)
		|| GrabbedPlayerState == CurrentBombHolder)
	{
		return;
	}

	ANPPlayerState* PreviousHolder = CurrentBombHolder;
	if (SetCurrentBombHolderInternal(GrabbedPlayerState))
	{
		UE_LOG(LogNPHotPotato, Log,
			TEXT("폭탄 전달: PreviousHolder=%s NewHolder=%s"),
			*GetNameSafe(PreviousHolder),
			*GetNameSafe(GrabbedPlayerState));
	}
}

void ANPHotPotatoMapEvent::OnRep_CurrentBombHolder()
{
	RemoveCarrierMoveSpeed();
	ApplyCarrierMoveSpeed();
	OnBombHolderChanged.Broadcast(CurrentBombHolder);
}
