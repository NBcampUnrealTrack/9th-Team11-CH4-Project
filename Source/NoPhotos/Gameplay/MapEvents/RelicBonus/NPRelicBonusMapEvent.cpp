#include "NPRelicBonusMapEvent.h"

#include "Gameplay/MapEvents/NPMapEventManager.h"
#include "NPRelicBonusCountdownActor.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Curves/CurveFloat.h"
#include "Engine/World.h"
#include "Gameplay/Relic/NPRelicReturnZone.h"
#include "Gameplay/Rope/NPRopeAnchorActor.h"
#include "Gameplay/Rope/NPRopeSegmentActor.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogNPRelicBonus, Log, All);

ANPRelicBonusMapEvent::ANPRelicBonusMapEvent()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	EventId = TEXT("RelicBonus");
	EventDisplayName = NSLOCTEXT("MapEvent", "RelicBonusEventName", "유물 보너스");
	EventType = ENPMapEventType::TypeA;
	EventScale = ENPMapEventScale::Medium;
	Duration = 60.0f;
	LocationSource = ENPMapEventLocationSource::Volume;
	ReturnZoneClass = ANPRelicReturnZone::StaticClass();
	CountdownActorClass = ANPRelicBonusCountdownActor::StaticClass();
	ReturnZoneSpawnGroup = FGameplayTag::RequestGameplayTag(FName(TEXT("RelicBonus")), false);
	RopeClass = ANPRopeSegmentActor::StaticClass();
	RopeTipClass = ANPRopeAnchorActor::StaticClass();

	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> GroundWindAsset(
		TEXT("/Game/NoPhotos/Blueprints/MapEvent/NS_RelicBonus_GroundWind.NS_RelicBonus_GroundWind"));
	if (GroundWindAsset.Succeeded())
	{
		GroundWindSystem = GroundWindAsset.Object;
	}
}

void ANPRelicBonusMapEvent::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority())
	{
		SetActorTickEnabled(false);
		return;
	}

	for (int32 Index = ActiveRopeDeployments.Num() - 1; Index >= 0; --Index)
	{
		FActiveRopeDeployment& Deployment = ActiveRopeDeployments[Index];
		ANPRopeAnchorActor* RopeTip = Deployment.RopeTip.Get();
		if (!IsValid(RopeTip))
		{
			ActiveRopeDeployments.RemoveAtSwap(Index);
			continue;
		}

		Deployment.ElapsedTime += DeltaSeconds;
		const float NormalizedTime = RopeLoweringDuration <= KINDA_SMALL_NUMBER
			? 1.0f
			: FMath::Clamp(Deployment.ElapsedTime / RopeLoweringDuration, 0.0f, 1.0f);
		const float MoveAlpha = RopeLoweringCurve
			? FMath::Clamp(RopeLoweringCurve->GetFloatValue(NormalizedTime), 0.0f, 1.0f)
			: FMath::InterpEaseInOut(0.0f, 1.0f, NormalizedTime, 2.0f);
		RopeTip->SetActorLocation(
			FMath::Lerp(Deployment.StartLocation, Deployment.TargetLocation, MoveAlpha),
			false,
			nullptr,
			ETeleportType::TeleportPhysics);

		if (NormalizedTime >= 1.0f)
		{
			RopeTip->SetActorLocation(
				Deployment.TargetLocation,
				false,
				nullptr,
				ETeleportType::TeleportPhysics);
			ActiveRopeDeployments.RemoveAtSwap(Index);
		}
	}

	for (int32 Index = ActiveDepartures.Num() - 1; Index >= 0; --Index)
	{
		FActiveDeparture& Departure = ActiveDepartures[Index];
		AActor* Carrier = Departure.Carrier.Get();
		if (!IsValid(Carrier))
		{
			ActiveDepartures.RemoveAtSwap(Index);
			continue;
		}

		Departure.ElapsedTime += DeltaSeconds;
		const float NormalizedTime = DepartureDuration <= KINDA_SMALL_NUMBER
			? 1.0f
			: FMath::Clamp(Departure.ElapsedTime / DepartureDuration, 0.0f, 1.0f);
		const float MoveAlpha = DepartureCurve
			? FMath::Clamp(DepartureCurve->GetFloatValue(NormalizedTime), 0.0f, 1.0f)
			: FMath::InterpEaseIn(0.0f, 1.0f, NormalizedTime, 2.0f);
		Carrier->SetActorLocation(
			FMath::Lerp(Departure.StartLocation, Departure.TargetLocation, MoveAlpha),
			false,
			nullptr,
			ETeleportType::TeleportPhysics);

		if (NormalizedTime >= 1.0f)
		{
			Carrier->SetActorLocation(
				Departure.TargetLocation,
				false,
				nullptr,
				ETeleportType::TeleportPhysics);
			ActiveDepartures.RemoveAtSwap(Index);
		}
	}

	if (ActiveRopeDeployments.IsEmpty() && ActiveDepartures.IsEmpty())
	{
		if (!IsEventActive() && !SpawnedHelicopters.IsEmpty())
		{
			DestroySpawnedActors();
			return;
		}
		SetActorTickEnabled(false);
	}
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
		SpawnReturnZones();
		return;
	}

	BeginDeparture();
}

void ANPRelicBonusMapEvent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		DestroySpawnedActors();
	}
	StopGroundWindImmediately();

	Super::EndPlay(EndPlayReason);
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
					if (!BeginRopeDeployment(Helicopter, ReturnZone, GroundTransform))
					{
						UE_LOG(
							LogNPRelicBonus,
							Warning,
							TEXT("로프 하강 시작 실패: ZoneIndex=%d RopeClass=%s RopeTipClass=%s"),
							ZoneIndex,
							*GetNameSafe(RopeClass),
							*GetNameSafe(RopeTipClass));
					}
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
		TEXT("RelicBonus 생성 완료: Requested=%d, ReturnZones=%d, Helicopters=%d, Ropes=%d, LocationSearchFailures=%d, DistanceFailures=%d, ReturnZoneSpawnFailures=%d"),
		TargetCount,
		SpawnedReturnZones.Num(),
		SpawnedHelicopters.Num(),
		SpawnedRopes.Num(),
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
	UGameplayStatics::FinishSpawningActor(ReturnZone, SpawnTransform);
	return ReturnZone;
}

ANPRelicBonusCountdownActor* ANPRelicBonusMapEvent::SpawnCountdownAt(
	const FTransform& GroundTransform)
{
	UWorld* World = GetWorld();
	const float EventDuration = GetEventDuration();
	if (!World || !CountdownActorClass || EventDuration <= 0.0f)
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

	Countdown->SetCountdownEndServerWorldTime(GetEventEndServerWorldTime());
	UGameplayStatics::FinishSpawningActor(Countdown, SpawnTransform);
	UE_LOG(
		LogNPRelicBonus,
		Log,
		TEXT("반환 존 카운트다운 생성 성공: Actor=%s Remaining=%.2f초 Location=%s"),
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

bool ANPRelicBonusMapEvent::BeginRopeDeployment(
	AActor* Helicopter,
	ANPRelicReturnZone* ReturnZone,
	const FTransform& GroundTransform)
{
	UWorld* World = GetWorld();
	if (!World || !IsValid(Helicopter) || !IsValid(ReturnZone)
		|| !RopeClass || !RopeTipClass)
	{
		return false;
	}

	USceneComponent* RopeStartComponent = FindTaggedSceneComponent(
		Helicopter,
		RopeStartComponentTag);
	if (!RopeStartComponent)
	{
		RopeStartComponent = Helicopter->GetRootComponent();
	}
	if (!RopeStartComponent)
	{
		return false;
	}

	const FVector StartLocation = RopeStartComponent->GetComponentLocation();
	USceneComponent* RopeEndComponent = FindTaggedSceneComponent(
		ReturnZone,
		RopeEndComponentTag);
	const FVector TargetLocation = RopeEndComponent
		? RopeEndComponent->GetComponentLocation()
		: GroundTransform.GetLocation() + FVector::UpVector * RopeEndHeightOffset;

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	const FTransform TipTransform(FRotator::ZeroRotator, StartLocation);
	ANPRopeAnchorActor* RopeTip = World->SpawnActor<ANPRopeAnchorActor>(
		RopeTipClass,
		TipTransform,
		SpawnParameters);
	if (!RopeTip)
	{
		return false;
	}

	ANPRopeSegmentActor* Rope = World->SpawnActor<ANPRopeSegmentActor>(
		RopeClass,
		TipTransform,
		SpawnParameters);
	if (!Rope)
	{
		RopeTip->Destroy();
		return false;
	}

	FNPRopeEndpoint StartEndpoint;
	StartEndpoint.TargetActor = Helicopter;
	StartEndpoint.ComponentTag = RopeStartComponentTag;
	FNPRopeEndpoint EndEndpoint;
	EndEndpoint.TargetActor = RopeTip;
	Rope->SetStartEndpoint(StartEndpoint);
	Rope->SetEndEndpoint(EndEndpoint);
	Rope->AttachStart();
	Rope->AttachEnd();
	Rope->SetRopeVisible(true);

	SpawnedRopes.Add(Rope);
	SpawnedRopeTips.Add(RopeTip);
	FActiveRopeDeployment& Deployment = ActiveRopeDeployments.AddDefaulted_GetRef();
	Deployment.RopeTip = RopeTip;
	Deployment.StartLocation = StartLocation;
	Deployment.TargetLocation = TargetLocation;
	UE_LOG(
		LogNPRelicBonus,
		Log,
		TEXT("로프 하강 시작: Rope=%s Tip=%s Start=%s Target=%s Duration=%.2f초"),
		*GetNameSafe(Rope),
		*GetNameSafe(RopeTip),
		*StartLocation.ToCompactString(),
		*TargetLocation.ToCompactString(),
		RopeLoweringDuration);
	SetActorTickEnabled(true);
	return true;
}

void ANPRelicBonusMapEvent::BeginDeparture()
{
	ActiveRopeDeployments.Reset();
	ActiveDepartures.Reset();
	MulticastFadeGroundWind();

	// 아래쪽 끝을 먼저 풀어 운반체에 매달린 로프가 끊어지는 연출을 만듭니다.
	for (ANPRopeSegmentActor* Rope : SpawnedRopes)
	{
		if (IsValid(Rope))
		{
			Rope->ReleaseEnd();
		}
	}

	// 이벤트 판정은 종료 시점에 즉시 제거하고 운반체와 로프만 퇴장 연출에 남깁니다.
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

	const float ClampedDepartureHeight = FMath::Max(0.0f, DepartureHeight);
	for (AActor* Helicopter : SpawnedHelicopters)
	{
		if (!IsValid(Helicopter))
		{
			continue;
		}

		FActiveDeparture& Departure = ActiveDepartures.AddDefaulted_GetRef();
		Departure.Carrier = Helicopter;
		Departure.StartLocation = Helicopter->GetActorLocation();
		Departure.TargetLocation = Departure.StartLocation
			+ FVector::UpVector * ClampedDepartureHeight;
	}

	if (ActiveDepartures.IsEmpty())
	{
		DestroySpawnedActors();
		return;
	}

	SetActorTickEnabled(true);
}

void ANPRelicBonusMapEvent::MulticastSpawnGroundWind_Implementation(
	const FVector GroundLocation)
{
	if (!GroundWindSystem)
	{
		return;
	}

	if (UNiagaraComponent* GroundWind = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		this,
		GroundWindSystem,
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

USceneComponent* ANPRelicBonusMapEvent::FindTaggedSceneComponent(
	AActor* Actor,
	const FName ComponentTag) const
{
	if (!IsValid(Actor))
	{
		return nullptr;
	}

	if (!ComponentTag.IsNone())
	{
		const TArray<UActorComponent*> TaggedComponents = Actor->GetComponentsByTag(
			USceneComponent::StaticClass(),
			ComponentTag);
		if (!TaggedComponents.IsEmpty())
		{
			return Cast<USceneComponent>(TaggedComponents[0]);
		}
	}

	return ComponentTag.IsNone() ? Actor->GetRootComponent() : nullptr;
}

void ANPRelicBonusMapEvent::DestroySpawnedActors()
{
	SetActorTickEnabled(false);
	ActiveRopeDeployments.Reset();
	ActiveDepartures.Reset();
	StopGroundWindImmediately();

	for (ANPRopeSegmentActor* Rope : SpawnedRopes)
	{
		if (IsValid(Rope))
		{
			Rope->Destroy();
		}
	}
	SpawnedRopes.Reset();

	for (ANPRopeAnchorActor* RopeTip : SpawnedRopeTips)
	{
		if (IsValid(RopeTip))
		{
			RopeTip->Destroy();
		}
	}
	SpawnedRopeTips.Reset();

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
