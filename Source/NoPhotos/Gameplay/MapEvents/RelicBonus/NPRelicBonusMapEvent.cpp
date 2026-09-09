#include "NPRelicBonusMapEvent.h"

#include "Gameplay/MapEvents/NPMapEventManager.h"
#include "NPRelicBonusCountdownActor.h"
#include "NPRelicBonusHelicopterInterface.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Gameplay/Relic/NPRelicReturnZone.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogNPRelicBonus, Log, All);

ANPRelicBonusMapEvent::ANPRelicBonusMapEvent()
{
	EventId = TEXT("RelicBonus");
	EventDisplayName = NSLOCTEXT("MapEvent", "RelicBonusEventName", "유물 보너스");
	EventType = ENPMapEventType::TypeA;
	EventScale = ENPMapEventScale::Medium;
	Duration = 60.0f;
	LocationSource = ENPMapEventLocationSource::Volume;
	ReturnZoneClass = ANPRelicReturnZone::StaticClass();
	CountdownActorClass = ANPRelicBonusCountdownActor::StaticClass();
	ReturnZoneSpawnGroup = FGameplayTag::RequestGameplayTag(FName(TEXT("RelicBonus")), false);

	// Do not load Niagara's WindForce/ChaosNiagara dependencies during native CDO construction.
	GroundWindSystem = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(
		TEXT("/Game/NoPhotos/Blueprints/MapEvent/NS_RelicBonus_GroundWind.NS_RelicBonus_GroundWind")));
}

void ANPRelicBonusMapEvent::ApplyEventState_Implementation(const bool bNewActive)
{
	if (!HasAuthority())
	{
		return;
	}

	UE_LOG(
		LogNPRelicBonus,
		Log,
		TEXT("RelicBonus 상태 변경: Event=%s, Active=%s, Duration=%.2f초"),
		*GetNameSafe(this),
		bNewActive ? TEXT("true") : TEXT("false"),
		GetEventDuration());

	if (bNewActive)
	{
		GetWorldTimerManager().ClearTimer(HelicopterStayTimer);
		GetWorldTimerManager().ClearTimer(HelicopterDepartureTimer);
		GetWorldTimerManager().ClearTimer(NextCycleTimer);
		bRespawnAfterDeparture = false;
		StartNextHelicopterCycle();
		return;
	}

	GetWorldTimerManager().ClearTimer(HelicopterStayTimer);
	GetWorldTimerManager().ClearTimer(NextCycleTimer);
	BeginDeparture(false);
}

void ANPRelicBonusMapEvent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		GetWorldTimerManager().ClearTimer(HelicopterStayTimer);
		GetWorldTimerManager().ClearTimer(HelicopterDepartureTimer);
		GetWorldTimerManager().ClearTimer(NextCycleTimer);
		DestroySpawnedActors();
	}
	StopGroundWindImmediately();

	Super::EndPlay(EndPlayReason);
}

void ANPRelicBonusMapEvent::StartNextHelicopterCycle()
{
	if (!HasAuthority() || !IsEventActive() || IsActorBeingDestroyed())
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(NextCycleTimer);
	bRespawnAfterDeparture = false;
	SpawnReturnZones();
	if (!SpawnedHelicopters.IsEmpty())
	{
		ScheduleHelicopterDeparture();
		return;
	}

	const float RetryDelay = FMath::IsFinite(CycleSpawnRetryDelay)
		? FMath::Max(0.1f, CycleSpawnRetryDelay) : 1.0f;
	UE_LOG(LogNPRelicBonus, Warning,
		TEXT("RelicBonus 사이클 생성 실패, 재시도 예약: Delay=%.2fs Remaining=%.2fs"),
		RetryDelay, GetRemainingEventTime());
	GetWorldTimerManager().SetTimer(
		NextCycleTimer,
		this,
		&ThisClass::StartNextHelicopterCycle,
		RetryDelay,
		false);
}

void ANPRelicBonusMapEvent::ScheduleHelicopterDeparture()
{
	if (!HasAuthority() || !IsEventActive() || SpawnedHelicopters.IsEmpty())
	{
		return;
	}

	const float MinimumStay = FMath::Max(
		0.1f,
		FMath::Min(MinimumHelicopterStayDuration, MaximumHelicopterStayDuration));
	const float MaximumStay = FMath::Max(
		0.1f,
		FMath::Max(MinimumHelicopterStayDuration, MaximumHelicopterStayDuration));
	const float SampledStayDuration = FMath::FRandRange(MinimumStay, MaximumStay);
	const float RemainingEventTime = GetRemainingEventTime();
	const float StayDuration = FMath::Min(SampledStayDuration, RemainingEventTime);
	if (StayDuration <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	for (ANPRelicBonusCountdownActor* Countdown : SpawnedCountdownActors)
	{
		if (IsValid(Countdown))
		{
			Countdown->SetCountdownDuration(StayDuration);
		}
	}
	GetWorldTimerManager().SetTimer(
		HelicopterStayTimer,
		this,
		&ThisClass::HandleHelicopterStayFinished,
		StayDuration,
		false);
	UE_LOG(LogNPRelicBonus, Log,
		TEXT("RelicBonus 헬리콥터 체류 시작: Duration=%.2fs Sampled=%.2fs Range=[%.2f, %.2f] EventRemaining=%.2fs Helicopters=%d"),
		StayDuration, SampledStayDuration, MinimumStay, MaximumStay, RemainingEventTime,
		SpawnedHelicopters.Num());
}

void ANPRelicBonusMapEvent::HandleHelicopterStayFinished()
{
	if (!HasAuthority() || !IsEventActive())
	{
		return;
	}

	UE_LOG(LogNPRelicBonus, Log,
		TEXT("RelicBonus 헬리콥터 체류 종료, 현재 사이클 정리: Remaining=%.2fs"),
		GetRemainingEventTime());
	BeginDeparture(true);
}

void ANPRelicBonusMapEvent::SpawnReturnZones()
{
	DestroySpawnedActors();
	UNPMapEventManagerComponent* EventManager = GetOwner()
		? GetOwner()->FindComponentByClass<UNPMapEventManagerComponent>()
		: nullptr;
	if (!EventManager || !ReturnZoneClass || !ReturnZoneSpawnGroup.IsValid())
	{
		UE_LOG(
			LogNPRelicBonus,
			Error,
			TEXT("RelicBonus 생성 준비 실패: Manager=%s, ReturnZoneClass=%s, SpawnGroup=%s"),
			*GetNameSafe(EventManager),
			*GetNameSafe(ReturnZoneClass),
			*ReturnZoneSpawnGroup.ToString());
		return;
	}

	const int32 MinimumCount = FMath::Max(0, FMath::Min(MinimumReturnZoneCount, MaximumReturnZoneCount));
	const int32 MaximumCount = FMath::Max(0, FMath::Max(MinimumReturnZoneCount, MaximumReturnZoneCount));
	const int32 TargetCount = FMath::RandRange(MinimumCount, MaximumCount);
	const FVector ReturnZoneHalfExtent = GetReturnZoneHalfExtent();
	const int32 PlacementAttempts = FMath::Max(1, MaximumPlacementAttemptsPerZone);
	int32 LocationSearchFailureCount = 0;
	int32 DistanceFailureCount = 0;
	int32 ReturnZoneSpawnFailureCount = 0;

	UE_LOG(
		LogNPRelicBonus,
		Log,
		TEXT("RelicBonus 생성 시작: TargetCount=%d, SpawnGroup=%s, ReturnZoneClass=%s, HelicopterClass=%s, HalfExtent=%s, AttemptsPerZone=%d"),
		TargetCount,
		*ReturnZoneSpawnGroup.ToString(),
		*GetNameSafe(ReturnZoneClass),
		*GetNameSafe(HelicopterClass),
		*ReturnZoneHalfExtent.ToCompactString(),
		PlacementAttempts);

	for (int32 ZoneIndex = 0; ZoneIndex < TargetCount; ++ZoneIndex)
	{
		bool bZoneSpawned = false;
		for (int32 Attempt = 0; Attempt < PlacementAttempts; ++Attempt)
		{
			FTransform GroundTransform;
			if (!EventManager->FindRandomSpawnTransformBySource(
					ReturnZoneSpawnGroup,
					ReturnZoneHalfExtent,
					GetLocationSource(),
					GroundTransform))
			{
				++LocationSearchFailureCount;
				continue;
			}

			if (!IsFarEnoughFromSpawnedZones(
					GroundTransform.GetLocation(),
					ReturnZoneHalfExtent))
			{
				++DistanceFailureCount;
				continue;
			}

			if (ANPRelicReturnZone* ReturnZone = SpawnReturnZoneAt(GroundTransform))
			{
				SpawnedReturnZones.Add(ReturnZone);
				bZoneSpawned = true;
				UE_LOG(
					LogNPRelicBonus,
					Log,
					TEXT("RelicReturnZone 생성 성공: Index=%d, Actor=%s, GroundLocation=%s, SpawnLocation=%s"),
					ZoneIndex,
					*GetNameSafe(ReturnZone),
					*GroundTransform.GetLocation().ToCompactString(),
					*ReturnZone->GetActorLocation().ToCompactString());
				if (ANPRelicBonusCountdownActor* Countdown = SpawnCountdownAt(GroundTransform))
				{
					SpawnedCountdownActors.Add(Countdown);
				}
				else
				{
					UE_LOG(
						LogNPRelicBonus,
						Warning,
						TEXT("반환 존 카운트다운 생성 실패: ZoneIndex=%d Class=%s EndServerTime=%.2f"),
						ZoneIndex,
						*GetNameSafe(CountdownActorClass),
						GetEventEndServerWorldTime());
				}
				if (AActor* Helicopter = SpawnHelicopterAt(GroundTransform))
				{
					SpawnedHelicopters.Add(Helicopter);
					UE_LOG(
						LogNPRelicBonus,
						Log,
						TEXT("Helicopter 생성 성공: ZoneIndex=%d, Actor=%s, Location=%s"),
						ZoneIndex,
						*GetNameSafe(Helicopter),
						*Helicopter->GetActorLocation().ToCompactString());
					MulticastSpawnGroundWind(GroundTransform.GetLocation());
				}
				else
				{
					UE_LOG(
						LogNPRelicBonus,
						Warning,
						TEXT("Helicopter 생성 실패: ZoneIndex=%d, Class=%s, GroundLocation=%s"),
						ZoneIndex,
						*GetNameSafe(HelicopterClass),
						*GroundTransform.GetLocation().ToCompactString());
				}
				break;
			}

			++ReturnZoneSpawnFailureCount;
		}

		if (!bZoneSpawned)
		{
			UE_LOG(
				LogNPRelicBonus,
				Warning,
				TEXT("RelicReturnZone 배치 실패: Index=%d, Attempts=%d"),
				ZoneIndex,
				PlacementAttempts);
		}
	}

	UE_LOG(
		LogNPRelicBonus,
		Log,
		TEXT("RelicBonus 생성 완료: Requested=%d, ReturnZones=%d, Helicopters=%d, LocationSearchFailures=%d, DistanceFailures=%d, ReturnZoneSpawnFailures=%d"),
		TargetCount,
		SpawnedReturnZones.Num(),
		SpawnedHelicopters.Num(),
		LocationSearchFailureCount,
		DistanceFailureCount,
		ReturnZoneSpawnFailureCount);
}

ANPRelicReturnZone* ANPRelicBonusMapEvent::SpawnReturnZoneAt(
	const FTransform& GroundTransform)
{
	UWorld* World = GetWorld();
	if (!World || !ReturnZoneClass)
	{
		UE_LOG(
			LogNPRelicBonus,
			Error,
			TEXT("RelicReturnZone Spawn 중단: World=%s, Class=%s"),
			*GetNameSafe(World),
			*GetNameSafe(ReturnZoneClass));
		return nullptr;
	}

	FTransform SpawnTransform = GroundTransform;
	SpawnTransform.AddToTranslation(FVector::UpVector * GetReturnZoneHalfExtent().Z);
	ANPRelicReturnZone* ReturnZone = World->SpawnActorDeferred<ANPRelicReturnZone>(
		ReturnZoneClass,
		SpawnTransform,
		this,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!ReturnZone)
	{
		UE_LOG(
			LogNPRelicBonus,
			Error,
			TEXT("RelicReturnZone Deferred Spawn 실패: Class=%s, Transform=%s"),
			*GetNameSafe(ReturnZoneClass),
			*SpawnTransform.ToHumanReadableString());
		return nullptr;
	}

	ReturnZone->SetReplicates(true);
	ReturnZone->SetReplicateMovement(false);
	ReturnZone->SetDeliveryEffectEnabled(true);
	UGameplayStatics::FinishSpawningActor(ReturnZone, SpawnTransform);
	return ReturnZone;
}

ANPRelicBonusCountdownActor* ANPRelicBonusMapEvent::SpawnCountdownAt(
	const FTransform& GroundTransform)
{
	UWorld* World = GetWorld();
	if (!World || !CountdownActorClass)
	{
		return nullptr;
	}

	FTransform SpawnTransform = GroundTransform;
	SpawnTransform.AddToTranslation(FVector::UpVector * CountdownHeightOffset);
	ANPRelicBonusCountdownActor* Countdown =
		World->SpawnActorDeferred<ANPRelicBonusCountdownActor>(
			CountdownActorClass,
			SpawnTransform,
			this,
			nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Countdown)
	{
		return nullptr;
	}

	UGameplayStatics::FinishSpawningActor(Countdown, SpawnTransform);
	UE_LOG(
		LogNPRelicBonus,
		Log,
		TEXT("반환 존 카운트다운 생성 성공: Actor=%s EventRemaining=%.2f초 Location=%s (체류시간 추첨 대기)"),
		*GetNameSafe(Countdown),
		GetRemainingEventTime(),
		*SpawnTransform.GetLocation().ToCompactString());
	return Countdown;
}

AActor* ANPRelicBonusMapEvent::SpawnHelicopterAt(
	const FTransform& GroundTransform)
{
	UWorld* World = GetWorld();
	if (!World || !HelicopterClass)
	{
		UE_LOG(
			LogNPRelicBonus,
			Error,
			TEXT("Helicopter Spawn 중단: World=%s, Class=%s"),
			*GetNameSafe(World),
			*GetNameSafe(HelicopterClass));
		return nullptr;
	}

	const float MinimumHeight = FMath::Max(
		0.0f,
		FMath::Min(MinimumHelicopterHeight, MaximumHelicopterHeight));
	const float MaximumHeight = FMath::Max(
		0.0f,
		FMath::Max(MinimumHelicopterHeight, MaximumHelicopterHeight));
	FTransform SpawnTransform = GroundTransform;
	SpawnTransform.AddToTranslation(
		FVector::UpVector * FMath::FRandRange(MinimumHeight, MaximumHeight));

	AActor* Helicopter = World->SpawnActorDeferred<AActor>(
		HelicopterClass,
		SpawnTransform,
		this,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Helicopter)
	{
		UE_LOG(
			LogNPRelicBonus,
			Error,
			TEXT("Helicopter Deferred Spawn 실패: Class=%s, Transform=%s"),
			*GetNameSafe(HelicopterClass),
			*SpawnTransform.ToHumanReadableString());
		return nullptr;
	}

	Helicopter->SetReplicates(true);
	Helicopter->SetReplicateMovement(true);
	UGameplayStatics::FinishSpawningActor(Helicopter, SpawnTransform);
	return Helicopter;
}

void ANPRelicBonusMapEvent::BeginDeparture(const bool bShouldRespawn)
{
	GetWorldTimerManager().ClearTimer(HelicopterStayTimer);
	bRespawnAfterDeparture = bShouldRespawn && IsEventActive();
	MulticastFadeGroundWind();
	// 판정 존과 카운트다운은 즉시 닫고, 헬리콥터만 퇴장 연출 동안 유지합니다.
	DestroyReturnZonesAndCountdowns();

	// 이벤트가 종료되어 재호출되면 기존 퇴장 연출은 유지하되 다음 사이클만 막습니다.
	if (GetWorldTimerManager().IsTimerActive(HelicopterDepartureTimer))
	{
		return;
	}

	TArray<AActor*> ValidHelicopters;
	ValidHelicopters.Reserve(SpawnedHelicopters.Num());
	for (AActor* Helicopter : SpawnedHelicopters)
	{
		if (IsValid(Helicopter))
		{
			ValidHelicopters.Add(Helicopter);
		}
	}

	if (ValidHelicopters.IsEmpty())
	{
		FinishDepartureCycle();
		return;
	}

	const float SafeDepartureDuration = FMath::IsFinite(HelicopterDepartureDuration)
		? FMath::Max(0.0f, HelicopterDepartureDuration)
		: 0.0f;
	MulticastBeginHelicopterDeparture(ValidHelicopters, SafeDepartureDuration);

	if (SafeDepartureDuration <= KINDA_SMALL_NUMBER)
	{
		FinishDepartureCycle();
		return;
	}

	GetWorldTimerManager().SetTimer(
		HelicopterDepartureTimer,
		this,
		&ThisClass::FinishDepartureCycle,
		SafeDepartureDuration,
		false);
}

void ANPRelicBonusMapEvent::FinishDepartureCycle()
{
	GetWorldTimerManager().ClearTimer(HelicopterDepartureTimer);
	const bool bStartAnotherCycle = bRespawnAfterDeparture
		&& IsEventActive() && !IsActorBeingDestroyed();
	bRespawnAfterDeparture = false;
	DestroySpawnedActors();

	if (!bStartAnotherCycle)
	{
		return;
	}

	const float MinimumDelay = FMath::Max(
		0.0f,
		FMath::Min(MinimumCycleRespawnDelay, MaximumCycleRespawnDelay));
	const float MaximumDelay = FMath::Max(
		0.0f,
		FMath::Max(MinimumCycleRespawnDelay, MaximumCycleRespawnDelay));
	const float RespawnDelay = FMath::FRandRange(MinimumDelay, MaximumDelay);
	const float RemainingTime = GetRemainingEventTime();
	if (RespawnDelay >= RemainingTime)
	{
		UE_LOG(LogNPRelicBonus, Log,
			TEXT("RelicBonus 다음 사이클 생략: RespawnDelay=%.2fs Remaining=%.2fs"),
			RespawnDelay, RemainingTime);
		return;
	}

	UE_LOG(LogNPRelicBonus, Log,
		TEXT("RelicBonus 현재 사이클 정리 완료, 다음 사이클 예약: Delay=%.2fs Range=[%.2f, %.2f] Remaining=%.2fs"),
		RespawnDelay, MinimumDelay, MaximumDelay, RemainingTime);
	if (RespawnDelay <= KINDA_SMALL_NUMBER)
	{
		GetWorldTimerManager().SetTimerForNextTick(
			this,
			&ThisClass::StartNextHelicopterCycle);
	}
	else
	{
		GetWorldTimerManager().SetTimer(
			NextCycleTimer,
			this,
			&ThisClass::StartNextHelicopterCycle,
			RespawnDelay,
			false);
	}
}

void ANPRelicBonusMapEvent::MulticastSpawnGroundWind_Implementation(
	const FVector GroundLocation)
{
	if (GetNetMode() == NM_DedicatedServer || GroundWindSystem.IsNull())
	{
		return;
	}
	UNiagaraSystem* System = GroundWindSystem.LoadSynchronous();
	if (!System)
	{
		UE_LOG(LogNPRelicBonus, Warning, TEXT("GroundWind Niagara 로드 실패: %s"), *GroundWindSystem.ToString());
		return;
	}

	if (UNiagaraComponent* GroundWind = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		this,
		System,
		GroundLocation + FVector::UpVector * GroundWindHeightOffset,
		FRotator::ZeroRotator,
		FVector::OneVector,
		true,
		true,
		ENCPoolMethod::None,
		true))
	{
		GroundWindComponents.Add(GroundWind);
	}
}

void ANPRelicBonusMapEvent::MulticastFadeGroundWind_Implementation()
{
	// Deactivate는 새 파티클 생성을 중단하고 기존 파티클의 Lifetime 종료를 기다립니다.
	for (UNiagaraComponent* GroundWind : GroundWindComponents)
	{
		if (IsValid(GroundWind))
		{
			GroundWind->Deactivate();
		}
	}
	GroundWindComponents.Reset();
}

void ANPRelicBonusMapEvent::MulticastBeginHelicopterDeparture_Implementation(
	const TArray<AActor*>& Helicopters,
	const float DepartureDuration)
{
	for (AActor* Helicopter : Helicopters)
	{
		if (IsValid(Helicopter)
			&& Helicopter->GetClass()->ImplementsInterface(
				UNPRelicBonusHelicopterInterface::StaticClass()))
		{
			INPRelicBonusHelicopterInterface::Execute_BeginRelicBonusDeparture(
				Helicopter,
				DepartureDuration);
		}
	}
}

void ANPRelicBonusMapEvent::StopGroundWindImmediately()
{
	for (UNiagaraComponent* GroundWind : GroundWindComponents)
	{
		if (IsValid(GroundWind))
		{
			GroundWind->DestroyComponent();
		}
	}
	GroundWindComponents.Reset();
}

void ANPRelicBonusMapEvent::DestroyReturnZonesAndCountdowns()
{
	for (ANPRelicReturnZone* ReturnZone : SpawnedReturnZones)
	{
		if (IsValid(ReturnZone))
		{
			ReturnZone->Destroy();
		}
	}

	SpawnedReturnZones.Reset();

	for (ANPRelicBonusCountdownActor* Countdown : SpawnedCountdownActors)
	{
		if (IsValid(Countdown))
		{
			Countdown->Destroy();
		}
	}
	SpawnedCountdownActors.Reset();
}

void ANPRelicBonusMapEvent::DestroySpawnedActors()
{
	StopGroundWindImmediately();
	DestroyReturnZonesAndCountdowns();

	for (AActor* Helicopter : SpawnedHelicopters)
	{
		if (IsValid(Helicopter))
		{
			Helicopter->Destroy();
		}
	}

	SpawnedHelicopters.Reset();
}

FVector ANPRelicBonusMapEvent::GetReturnZoneHalfExtent() const
{
	const ANPRelicReturnZone* DefaultReturnZone = ReturnZoneClass
		? ReturnZoneClass->GetDefaultObject<ANPRelicReturnZone>()
		: nullptr;
	const UBoxComponent* ReturnVolume = DefaultReturnZone
		? DefaultReturnZone->FindComponentByClass<UBoxComponent>()
		: nullptr;
	if (!ReturnVolume)
	{
		return FVector(100.0f);
	}

	const FVector Extent = ReturnVolume->GetScaledBoxExtent();
	return FVector(
		FMath::Max(1.0f, Extent.X),
		FMath::Max(1.0f, Extent.Y),
		FMath::Max(1.0f, Extent.Z));
}

bool ANPRelicBonusMapEvent::IsFarEnoughFromSpawnedZones(
	const FVector& CandidateLocation,
	const FVector& ReturnZoneHalfExtent) const
{
	const float RequiredDistance = FMath::Max(0.0f, MinimumDistanceBetweenReturnZones)
		+ 2.0f * FMath::Max(ReturnZoneHalfExtent.X, ReturnZoneHalfExtent.Y);
	for (const ANPRelicReturnZone* ReturnZone : SpawnedReturnZones)
	{
		if (IsValid(ReturnZone)
			&& FVector::Dist2D(CandidateLocation, ReturnZone->GetActorLocation()) < RequiredDistance)
		{
			return false;
		}
	}

	return true;
}
