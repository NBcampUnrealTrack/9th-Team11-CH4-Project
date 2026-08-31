#include "Gameplay/MapEvents/Santa/NPSantaMapEvent.h"

#include "Engine/World.h"
#include "EngineUtils.h"
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
	CleanupFlight();
	if (bNewActive && !StartSantaFlight())
	{
		ScheduleFailedFinish();
	}
}

bool ANPSantaMapEvent::StartSantaFlight()
{
	UWorld* World = GetWorld();
	const UNPSantaEventDefinition* Definition = Cast<UNPSantaEventDefinition>(GetEventDefinition());
	if (!World || !Definition)
	{
		UE_LOG(LogNPSantaEvent, Warning, TEXT("산타 이벤트 설정 오류: Event=%s. NPSantaEventDefinition DA가 필요합니다."), *GetName());
		return false;
	}
	const TSubclassOf<ANPSantaFlightActor> SantaClass = Definition->GetSantaClass();
	const float FlightDuration = GetEventDuration();
	const FGameplayTag RouteGroup = Definition->GetRouteGroup();
	if (!SantaClass || SantaClass->HasAnyClassFlags(CLASS_Abstract)
		|| !FMath::IsFinite(FlightDuration) || FlightDuration < 0.01f || !RouteGroup.IsValid())
	{
		UE_LOG(LogNPSantaEvent, Warning, TEXT("산타 이벤트 설정 오류: Event=%s Class=%s Duration=%f Group=%s. 산타 클래스, 0.01초 이상의 Duration, 경로 그룹을 확인하세요."),
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
	double TotalWeight = 0.0;
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
		TotalWeight += Weight;
	}
	if (Candidates.IsEmpty())
	{
		UE_LOG(LogNPSantaEvent, Warning, TEXT("산타 비행 경로 없음: Event=%s Group=%s. 로드된 레벨의 NPSantaFlightRoute 그룹/가중치/높이/거리를 확인하세요."),
			*GetName(), *RouteGroup.ToString());
		return false;
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
	Plan.StartLocation = Selected->Start;
	Plan.EndLocation = Selected->End;
	Plan.Duration = FlightDuration;
	Plan.StartServerTime = GetEventEndServerWorldTime() - FlightDuration;
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
	SpawnedSanta = Santa;
	Santa->OnDestroyed.AddDynamic(this, &ThisClass::OnSantaDestroyed);
	ForceNetUpdate();
	UE_LOG(LogNPSantaEvent, Log, TEXT("산타 비행 시작: Event=%s Route=%s Start=%s End=%s Duration=%.2fs Speed=%.2fcm/s"),
		*GetName(), *GetNameSafe(Selected->Route), *Plan.StartLocation.ToString(), *Plan.EndLocation.ToString(),
		Plan.Duration, FVector::Dist(Plan.StartLocation, Plan.EndLocation) / Plan.Duration);
	StartGiftDrops(Definition);
	return true;
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
	if (!ActiveGiftDrops.IsValid() || !ActiveGiftClass || ActiveGiftClass->HasAnyClassFlags(CLASS_Abstract)
		|| !FMath::IsFinite(ActiveDropHeightOffset) || ActiveDropHeightOffset < 0.0f)
	{
		UE_LOG(LogNPSantaEvent, Warning, TEXT("선물 투하 설정 오류: Event=%s. GiftClass, Count(1~128), 진행 구간(0 이상 1 미만), 높이 오프셋을 확인하세요. 비행만 진행합니다."), *GetName());
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
	if (!HasAuthority() || !IsEventActive() || !IsValid(SpawnedSanta) || !ActiveGiftClass)
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
	DropTransform.AddToTranslation(FVector::DownVector * ActiveDropHeightOffset);
	FActorSpawnParameters Params;
	Params.OverrideLevel = GetWorld()->PersistentLevel;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.bDeferConstruction = true;
	// Owner를 이벤트로 지정하거나 부착하지 않습니다. 이미 투하된 선물은 비행 종료 후에도 남습니다.
	ANPSantaGiftActor* Gift = GetWorld()->SpawnActor<ANPSantaGiftActor>(ActiveGiftClass, DropTransform, Params);
	if (IsValid(Gift) && Gift->InitializeGift(ActiveRelicClasses))
	{
		Gift->SetReplicates(true);
		Gift->SetReplicateMovement(true);
		UGameplayStatics::FinishSpawningActor(Gift, DropTransform);
		if (IsValid(Gift))
		{
			UE_LOG(LogNPSantaEvent, Log, TEXT("산타 선물 투하: Event=%s Gift=%s Slot=%d/%d Progress=%.3f Location=%s"),
				*GetName(), *GetNameSafe(Gift), NextGiftIndex + 1, ActiveGiftDrops.Count, CurrentProgress, *DropTransform.GetLocation().ToString());
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
		World->GetTimerManager().ClearTimer(FailedStartTimer);
		World->GetTimerManager().ClearTimer(GiftDropTimer);
	}
	ActiveGiftClass = nullptr;
	ActiveRelicClasses.Reset();
	NextGiftIndex = 0;
	if (IsValid(SpawnedSanta))
	{
		SpawnedSanta->OnDestroyed.RemoveDynamic(this, &ThisClass::OnSantaDestroyed);
		SpawnedSanta->Destroy();
	}
	SpawnedSanta = nullptr;
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
		GetWorldTimerManager().ClearTimer(GiftDropTimer);
		SpawnedSanta = nullptr;
		ForceNetUpdate();
		if (IsEventActive())
		{
			UE_LOG(LogNPSantaEvent, Warning, TEXT("산타가 비행 중 제거되어 이벤트를 종료합니다: Event=%s"), *GetName());
			ScheduleFailedFinish();
		}
	}
}

void ANPSantaMapEvent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		CleanupFlight();
	}
	Super::EndPlay(EndPlayReason);
}
