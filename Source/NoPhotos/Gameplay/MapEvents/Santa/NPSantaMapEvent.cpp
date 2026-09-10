#include "Gameplay/MapEvents/Santa/NPSantaMapEvent.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "Gameplay/MapEvents/Santa/NPSantaEventDefinition.h"
#include "Gameplay/MapEvents/Santa/NPSantaFlightActor.h"
#include "Gameplay/MapEvents/Santa/NPSantaFlightRoute.h"
#include "Gameplay/MapEvents/Santa/NPSantaGiftActor.h"
#include "Gameplay/Relic/NPBaseRelic.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogNPSantaEvent, Log, All);

void ANPSantaMapEvent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANPSantaMapEvent, SpawnedSanta);
}

void ANPSantaMapEvent::ApplyEventState_Implementation(bool bNewActive)
{
	Super::ApplyEventState_Implementation(bNewActive);
	if (!HasAuthority())
	{
		return;
	}
	CleanupEvent();
	if (bNewActive && !StartSantaFlight())
	{
		ScheduleFailedFinish();
	}
}

bool ANPSantaMapEvent::StartSantaFlight()
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !IsEventActive() || IsActorBeingDestroyed() || IsValid(SpawnedSanta)
		|| GetRemainingEventTime() <= 0.0f)
	{
		return false;
	}
	const UNPSantaEventDefinition* Definition = Cast<UNPSantaEventDefinition>(GetEventDefinition());
	if (!World || !Definition)
	{
		UE_LOG(LogNPSantaEvent, Warning, TEXT("산타 이벤트 설정 오류: Event=%s. NPSantaEventDefinition DA가 필요합니다."), *GetName());
		return false;
	}
	const TSubclassOf<ANPSantaFlightActor> SantaClass = Definition->GetSantaClass();
	const FNPSantaFlightSchedule& Schedule = Definition->GetFlightSchedule();
	const float FlightDuration = Schedule.FlightDuration;
	const FGameplayTag RouteGroup = Definition->GetRouteGroup();
	if (!SantaClass || SantaClass->HasAnyClassFlags(CLASS_Abstract)
		|| !Schedule.IsValid() || !FMath::IsFinite(GetEventDuration()) || GetEventDuration() < 0.01f
		|| !RouteGroup.IsValid())
	{
		UE_LOG(LogNPSantaEvent, Warning, TEXT("산타 이벤트 설정 오류: Event=%s Class=%s FlightDuration=%f Group=%s. 클래스/그룹, Duration/FlightDuration >= 0.01, 0 <= RespawnDelayMin <= RespawnDelayMax를 확인하세요."),
			*GetName(), *GetNameSafe(SantaClass.Get()), FlightDuration, *RouteGroup.ToString());
		return false;
	}

	struct FRouteCandidate
	{
		ANPSantaFlightRoute* Route;
		FVector Start;
		FVector End;
		double Weight;
	};
	TArray<FRouteCandidate> Candidates;
	// 위치 레벨은 서버에만 로드될 수 있습니다. 클라이언트에는 결과 좌표만 전달합니다.
	for (TActorIterator<ANPSantaFlightRoute> It(World); It; ++It)
	{
		ANPSantaFlightRoute* Route = *It;
		if (!IsValid(Route) || !Route->SupportsRouteGroup(RouteGroup))
		{
			continue;
		}
		const float Weight = Route->GetSelectionWeight();
		FVector Start, End;
		if (!FMath::IsFinite(Weight) || Weight <= 0.0f || !Route->GetFlightEndpoints(Start, End))
		{
			continue;
		}
		Candidates.Add({Route, Start, End, Weight});
	}
	if (Candidates.IsEmpty())
	{
		UE_LOG(LogNPSantaEvent, Warning, TEXT("산타 비행 경로 없음: Event=%s Group=%s. 로드된 레벨의 NPSantaFlightRoute 그룹/가중치/높이/거리를 확인하세요."),
			*GetName(), *RouteGroup.ToString());
		return false;
	}

	// 두 개 이상이면 직전 경로를 제외합니다. 한 개만 남으면 같은 경로도 재사용합니다.
	if (Candidates.Num() > 1)
	{
		Candidates.RemoveAll([this](const FRouteCandidate& Candidate) { return Candidate.Route == LastFlightRoute.Get(); });
	}
	double TotalWeight = 0.0;
	for (const FRouteCandidate& Candidate : Candidates)
	{
		TotalWeight += Candidate.Weight;
	}
	double RemainingWeight = FMath::FRand() * TotalWeight;
	const FRouteCandidate* Selected = &Candidates.Last();
	for (const FRouteCandidate& Candidate : Candidates)
	{
		RemainingWeight -= Candidate.Weight;
		if (RemainingWeight < 0.0)
		{
			Selected = &Candidate;
			break;
		}
	}
	FNPSantaFlightPlan Plan;
	const bool bReverse = FMath::RandBool();
	Plan.SetRouteEndpoints(Selected->Start, Selected->End, bReverse);
	Plan.Duration = FlightDuration;
	const AGameStateBase* GameState = World->GetGameState();
	Plan.StartServerTime = GameState ? GameState->GetServerWorldTimeSeconds() : World->GetTimeSeconds();
	if (!Plan.IsValid())
	{
		UE_LOG(LogNPSantaEvent, Warning, TEXT("산타 비행 계획 오류: Event=%s Route=%s"), *GetName(), *GetNameSafe(Selected->Route));
		return false;
	}

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.OverrideLevel = World->PersistentLevel;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.bDeferConstruction = true;
	const FTransform StartTransform = Plan.GetTransform(0.0f);
	ANPSantaFlightActor* Santa = World->SpawnActor<ANPSantaFlightActor>(SantaClass, StartTransform, Params);
	if (!IsValid(Santa) || !Santa->InitializeFlight(Plan))
	{
		if (IsValid(Santa))
		{
			Santa->Destroy();
		}
		UE_LOG(LogNPSantaEvent, Warning, TEXT("산타 생성/초기화 실패: Event=%s Class=%s"), *GetName(), *GetNameSafe(SantaClass.Get()));
		return false;
	}
	UGameplayStatics::FinishSpawningActor(Santa, StartTransform);
	if (!IsValid(Santa))
	{
		UE_LOG(LogNPSantaEvent, Warning, TEXT("산타가 생성 직후 제거됨: Event=%s. 산타 BP의 Construction/BeginPlay를 확인하세요."), *GetName());
		return false;
	}
	// 산타 BP의 BeginPlay에서 이벤트가 종료/제거되는 경우 뒤늦게 비행을 등록하지 않습니다.
	if (!IsEventActive() || IsActorBeingDestroyed() || GetRemainingEventTime() <= 0.0f)
	{
		Santa->Destroy();
		return false;
	}
	SpawnedSanta = Santa;
	LastFlightRoute = Selected->Route;
	Santa->OnDestroyed.AddDynamic(this, &ThisClass::OnSantaDestroyed);
	ForceNetUpdate();
	UE_LOG(LogNPSantaEvent, Log, TEXT("산타 비행 시작: Event=%s Route=%s Reverse=%d Start=%s End=%s Duration=%.2fs Speed=%.2fcm/s"),
		*GetName(), *GetNameSafe(Selected->Route), bReverse, *Plan.StartLocation.ToString(), *Plan.EndLocation.ToString(),
		Plan.Duration, FVector::Dist(Plan.StartLocation, Plan.EndLocation) / Plan.Duration);
	GetWorldTimerManager().SetTimer(FlightEndTimer, this, &ThisClass::FinishSantaFlight,
		FMath::Min(FlightDuration, GetRemainingEventTime()), false);
	StartGiftDrops(Definition);
	return true;
}

void ANPSantaMapEvent::FinishSantaFlight()
{
	if (!HasAuthority())
	{
		return;
	}
	CleanupFlight();
	ScheduleNextFlight();
}

void ANPSantaMapEvent::ScheduleNextFlight()
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !World || !IsEventActive() || IsActorBeingDestroyed())
	{
		return;
	}
	World->GetTimerManager().ClearTimer(RespawnTimer);
	const float RemainingTime = GetRemainingEventTime();
	if (RemainingTime <= 0.0f)
	{
		return;
	}
	const UNPSantaEventDefinition* Definition = Cast<UNPSantaEventDefinition>(GetEventDefinition());
	if (!Definition || !Definition->GetFlightSchedule().IsValid())
	{
		ScheduleFailedFinish();
		return;
	}
	const float Delay = Definition->GetFlightSchedule().GetRespawnDelay(FMath::FRand());
	// 이벤트 종료 시점/이후에는 재등장시키지 않습니다. 마지막 비행의 속도는 유지합니다.
	if (Delay >= RemainingTime)
	{
		return;
	}
	UE_LOG(LogNPSantaEvent, Log, TEXT("산타 재등장 예약: Event=%s Delay=%.2fs Remaining=%.2fs"),
		*GetName(), Delay, RemainingTime);
	if (Delay <= 0.0f)
	{
		RespawnTimer = World->GetTimerManager().SetTimerForNextTick(this, &ThisClass::StartNextFlight);
	}
	else
	{
		World->GetTimerManager().SetTimer(RespawnTimer, this, &ThisClass::StartNextFlight, Delay, false);
	}
}

void ANPSantaMapEvent::StartNextFlight()
{
	if (!HasAuthority() || !IsEventActive() || IsActorBeingDestroyed() || GetRemainingEventTime() <= 0.0f)
	{
		return;
	}
	if (!StartSantaFlight())
	{
		// 비행 사이 경로 레벨이 언로드되거나 생성이 실패해도 이벤트 수명은 유지하고 재시도합니다.
		ScheduleNextFlight();
	}
}

void ANPSantaMapEvent::StartGiftDrops(const UNPSantaEventDefinition* Definition)
{
	ActiveGiftDrops = Definition->GetGiftDrops();
	if (ActiveGiftDrops.Count == 0)
	{
		return;
	}
	ActiveGiftClass = Definition->GetGiftClass();
	ActiveDropHeightOffset = Definition->GetGiftDropHeightOffset();
	ActivePrimaryRelicClass = Definition->GetPrimaryRelicClass();
	ActivePrimaryRelicChancePercent = FMath::IsFinite(Definition->GetPrimaryRelicChancePercent())
		? FMath::Clamp(Definition->GetPrimaryRelicChancePercent(), 0.0f, 100.0f) : 0.0f;
	if (!ActivePrimaryRelicClass || ActivePrimaryRelicClass->HasAnyClassFlags(CLASS_Abstract))
	{
		ActivePrimaryRelicClass = nullptr;
		ActivePrimaryRelicChancePercent = 0.0f;
	}
	if (!ActiveGiftDrops.IsValid() || !ActiveGiftClass || ActiveGiftClass->HasAnyClassFlags(CLASS_Abstract)
		|| !FMath::IsFinite(ActiveDropHeightOffset) || ActiveDropHeightOffset < 0.0f)
	{
		UE_LOG(LogNPSantaEvent, Warning, TEXT("선물 투하 설정 오류: Event=%s. GiftClass, Count(1~128), 진행 구간(0 이상 1 미만), 랜덤 반경, 높이 오프셋을 확인하세요. 비행만 진행합니다."), *GetName());
		return;
	}
	for (const TSubclassOf<ANPBaseRelic>& RelicClass : Definition->GetRelicClasses())
	{
		if (RelicClass && !RelicClass->HasAnyClassFlags(CLASS_Abstract))
		{
			ActiveRelicClasses.AddUnique(RelicClass);
		}
		else
		{
			UE_LOG(LogNPSantaEvent, Warning, TEXT("선물 유물 후보 제외: Event=%s Class=%s (비어 있거나 추상 클래스)"), *GetName(), *GetNameSafe(RelicClass.Get()));
		}
	}
	if (ActiveRelicClasses.IsEmpty())
	{
		UE_LOG(LogNPSantaEvent, Warning, TEXT("선물 유물 후보 없음: Event=%s. DA의 RelicClasses에 유물 BP를 넣으세요. 비행만 진행합니다."), *GetName());
		return;
	}
	NextGiftIndex = 0;
	ScheduleNextGiftDrop();
}

void ANPSantaMapEvent::ScheduleNextGiftDrop()
{
	float TargetProgress = 0.0f;
	if (!HasAuthority() || !IsEventActive() || !IsValid(SpawnedSanta)
		|| GetRemainingEventTime() <= 0.0f
		|| !ActiveGiftDrops.GetDropProgress(NextGiftIndex, TargetProgress))
	{
		return;
	}
	const float CurrentProgress = SpawnedSanta->GetFlightProgress();
	if (CurrentProgress >= 1.0f)
	{
		return;
	}
	const float Delay = FMath::Max(0.001f, (TargetProgress - CurrentProgress) * SpawnedSanta->GetFlightPlan().Duration);
	GetWorldTimerManager().SetTimer(GiftDropTimer, this, &ThisClass::DropGift, Delay, false);
}

void ANPSantaMapEvent::DropGift()
{
	if (!HasAuthority() || !IsEventActive() || !IsValid(SpawnedSanta) || !ActiveGiftClass
		|| GetRemainingEventTime() <= 0.0f)
	{
		return;
	}
	const float CurrentProgress = SpawnedSanta->GetFlightProgress();
	if (CurrentProgress >= 1.0f)
	{
		return;
	}
	// Santa Tick 호출 순서에 의존하지 않고 같은 비행 계획/현재 서버 시각에서 투하 위치를 계산합니다.
	FTransform DropTransform = SpawnedSanta->GetFlightPlan().GetTransform(CurrentProgress);
	// 서버에서만 난수를 뽑아 생성 위치를 결정합니다. 선물 액터의 이동 복제로 모든 클라이언트에 같은 결과가 전달됩니다.
	const FVector RandomOffset = ActiveGiftDrops.GetRandomDropOffset(FMath::FRand(), FMath::FRand());
	DropTransform.AddToTranslation(RandomOffset + FVector::DownVector * ActiveDropHeightOffset);
	FActorSpawnParameters Params;
	Params.OverrideLevel = GetWorld()->PersistentLevel;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.bDeferConstruction = true;
	// Owner를 이벤트로 지정하거나 부착하지 않습니다. 이미 투하된 선물은 비행 종료 후에도 남습니다.
	ANPSantaGiftActor* Gift = GetWorld()->SpawnActor<ANPSantaGiftActor>(ActiveGiftClass, DropTransform, Params);
	if (IsValid(Gift) && Gift->InitializeGift(
		ActiveRelicClasses,
		ActivePrimaryRelicClass,
		ActivePrimaryRelicChancePercent))
	{
		Gift->SetReplicates(true);
		Gift->SetReplicateMovement(true);
		UGameplayStatics::FinishSpawningActor(Gift, DropTransform);
		if (IsValid(Gift))
		{
			UE_LOG(LogNPSantaEvent, Log, TEXT("산타 선물 투하: Event=%s Gift=%s Slot=%d/%d Progress=%.3f Location=%s RandomOffset=%s"),
				*GetName(), *GetNameSafe(Gift), NextGiftIndex + 1, ActiveGiftDrops.Count, CurrentProgress,
				*DropTransform.GetLocation().ToString(), *RandomOffset.ToString());
		}
	}
	else
	{
		if (IsValid(Gift))
		{
			Gift->Destroy();
		}
		UE_LOG(LogNPSantaEvent, Warning, TEXT("산타 선물 생성 실패: Event=%s Class=%s"), *GetName(), *GetNameSafe(ActiveGiftClass.Get()));
	}
	++NextGiftIndex;
	// 서버 지연으로 여러 투하 시각이 지난 경우 같은 위치에 한꺼번에 쌓지 않고 지난 슬롯은 건너뜁니다.
	float NextProgress = 0.0f;
	while (ActiveGiftDrops.GetDropProgress(NextGiftIndex, NextProgress) && NextProgress <= CurrentProgress)
	{
		++NextGiftIndex;
	}
	ScheduleNextGiftDrop();
}

void ANPSantaMapEvent::CleanupFlight()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FlightEndTimer);
		World->GetTimerManager().ClearTimer(GiftDropTimer);
	}
	ActiveGiftClass = nullptr;
	ActiveRelicClasses.Reset();
	ActivePrimaryRelicClass = nullptr;
	ActivePrimaryRelicChancePercent = 0.0f;
	NextGiftIndex = 0;
	if (IsValid(SpawnedSanta))
	{
		SpawnedSanta->OnDestroyed.RemoveDynamic(this, &ThisClass::OnSantaDestroyed);
		SpawnedSanta->Destroy();
	}
	SpawnedSanta = nullptr;
	ForceNetUpdate();
}

void ANPSantaMapEvent::CleanupEvent()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FailedStartTimer);
		World->GetTimerManager().ClearTimer(RespawnTimer);
	}
	CleanupFlight();
	LastFlightRoute.Reset();
}

void ANPSantaMapEvent::ScheduleFailedFinish()
{
	if (UWorld* World = GetWorld())
	{
		// StartEvent 내부에서는 Apply 이후 OnEventStarted를 방송합니다.
		// 여기서 즉시 Finish하면 Started/Finished 순서가 뒤집히므로 다음 틱에 종료합니다.
		World->GetTimerManager().ClearTimer(FailedStartTimer);
		FailedStartTimer = World->GetTimerManager().SetTimerForNextTick(this, &ThisClass::FinishFailedStart);
	}
}

void ANPSantaMapEvent::FinishFailedStart()
{
	if (IsEventActive())
	{
		FinishEvent();
	}
}

void ANPSantaMapEvent::OnSantaDestroyed(AActor* DestroyedActor)
{
	if (HasAuthority() && DestroyedActor == SpawnedSanta)
	{
		SpawnedSanta = nullptr;
		CleanupFlight();
		ScheduleNextFlight();
	}
}

void ANPSantaMapEvent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		CleanupEvent();
	}
	Super::EndPlay(EndPlayReason);
}
