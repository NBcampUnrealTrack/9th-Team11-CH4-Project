#include "NPPossessionMapEvent.h"

#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Gameplay/Character/NPStablePhysicsPawn.h"
#include "Gameplay/Character/Component/NPControlReversalComponent.h"
#include "Gameplay/AbilitySystem/Effects/NPPossessionGameplayEffect.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "NPGhostFollowerActor.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogNPPossessionEvent, Log, All);

void ANPPossessionMapEvent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANPPossessionMapEvent, AffectedPlayers);
}

void ANPPossessionMapEvent::BeginPlay()
{
	Super::BeginPlay();
	// 초기 복제의 RepNotify가 BeginPlay보다 먼저 실행된 경우도 복원합니다.
	UpdateTrackingState();
}

void ANPPossessionMapEvent::ApplyEventState_Implementation(bool bNewActive)
{
	Super::ApplyEventState_Implementation(bNewActive);
	if (HasActorBegunPlay())
	{
		UpdateTrackingState();
	}
}

void ANPPossessionMapEvent::UpdateTrackingState()
{
	GetWorldTimerManager().ClearTimer(PlayerRefreshTimer);
	if (!IsEventActive())
	{
		RemoveAppliedEffects();
		ClearLocalGhosts();
		if (HasAuthority() && !AffectedPlayers.IsEmpty())
		{
			AffectedPlayers.Reset();
			ForceNetUpdate();
		}
		return;
	}
	bWarnedMissingGhostClass = false;
	bWarnedSpawnFailure = false;
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
	if ((!GhostClass || GhostClass->HasAnyClassFlags(CLASS_Abstract)) && !bWarnedMissingGhostClass)
	{
		UE_LOG(LogNPPossessionEvent, Warning, TEXT("빙의 유령 클래스 없음: Event=%s. 이벤트 BP의 GhostClass에 NPGhostFollowerActor 자식 BP를 지정하세요."), *GetName());
		bWarnedMissingGhostClass = true;
	}
	if (HasAuthority())
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
		RefreshAppliedEffects();
	}
	RefreshLocalGhosts();
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
		// 타깃 NetGUID가 아직 해결되지 않았거나 Pawn이 초기화 중이면 다음 갱신에서 재시도합니다.
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
		ANPGhostFollowerActor* Ghost = GetWorld()->SpawnActor<ANPGhostFollowerActor>(GhostClass, SpawnTransform, Params);
		if (IsValid(Ghost) && Ghost->InitializeFollower(Target))
		{
			UGameplayStatics::FinishSpawningActor(Ghost, Ghost->GetActorTransform());
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
			UE_LOG(LogNPPossessionEvent, Warning, TEXT("빙의 유령 생성 실패: Event=%s Class=%s. 유령 BP의 Construction/BeginPlay 설정을 확인하세요."),
				*GetName(), *GetNameSafe(GhostClass.Get()));
			bWarnedSpawnFailure = true;
		}
	}
}

void ANPPossessionMapEvent::ClearLocalGhosts(bool bImmediately)
{
	// 유령 BP의 종료 콜백이 이벤트 상태를 바꾸더라도 순회 중인 컨테이너는 유지합니다.
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
	RemoveAppliedEffects();
	// 게임 중 이벤트 액터 파괴는 페이드, 레벨/PIE 종료는 지연 없이 정리합니다.
	ClearLocalGhosts(EndPlayReason != EEndPlayReason::Destroyed);
	AffectedPlayers.Reset();
	Super::EndPlay(EndPlayReason);
}
