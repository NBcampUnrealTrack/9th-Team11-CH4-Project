#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "NPMapEventTypes.h"
#include "NPMapEventManager.generated.h"

class ANPMapEvent;
class ANPMapEventLocationCollector;
class ANPMapEventSpawnPoint;
class ANPMapEventSpawnVolume;
class UNPMapEventCatalog;
class ULevelStreamingDynamic;
class UWorld;

/** 클라이언트 UI가 맵 이벤트 액터에 의존하지 않고 표시할 수 있는 복제 정보입니다. */
USTRUCT(BlueprintType)
struct NOPHOTOS_API FNPActiveMapEventPresentation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Map Event|UI")
	FName EventId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Map Event|UI")
	FText Title;

	UPROPERTY(BlueprintReadOnly, Category = "Map Event|UI")
	FText Description;
	
	UPROPERTY(BlueprintReadOnly, Category = "Map Event|UI")
	float EndServerWorldTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Map Event|UI")
	float DurationSeconds = 0.0f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNPActiveMapEventsChangedSignature);

UENUM(BlueprintType)
enum class ENPScheduledMapEventState : uint8
{
	Pending,
	Loading,
	Active,
	Completed,
	Cancelled
};

/** ScheduleIndex로 예정 순서를 구분합니다. 시각 -1은 아직 예측할 수 없음을 뜻합니다. */
USTRUCT(BlueprintType)
struct NOPHOTOS_API FNPScheduledMapEventPresentation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Map Event|Schedule")
	int32 ScheduleIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Map Event|Schedule")
	FName EventId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Map Event|Schedule")
	FText Title;

	UPROPERTY(BlueprintReadOnly, Category = "Map Event|Schedule")
	FText Description;

	UPROPERTY(BlueprintReadOnly, Category = "Map Event|Schedule")
	float DurationSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Map Event|Schedule")
	float DelayAfterSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Map Event|Schedule")
	float ExpectedStartServerWorldTime = -1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Map Event|Schedule")
	float ExpectedEndServerWorldTime = -1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Map Event|Schedule")
	float ActualStartServerWorldTime = -1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Map Event|Schedule")
	float ActualEndServerWorldTime = -1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Map Event|Schedule")
	ENPScheduledMapEventState State = ENPScheduledMapEventState::Pending;
};

/** 순서와 상태를 한 스냅샷으로 복제하여 늦게 접속한 클라이언트도 전체 계획을 조회할 수 있습니다. */
USTRUCT(BlueprintType)
struct NOPHOTOS_API FNPMapEventSchedulePresentation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Map Event|Schedule")
	TArray<FNPScheduledMapEventPresentation> Events;

	/** 매니저 BeginPlay 기준 서버 시각입니다. 프로그레스바의 시간 원점으로 사용합니다. */
	UPROPERTY(BlueprintReadOnly, Category = "Map Event|Schedule")
	float ScheduleOriginServerWorldTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Map Event|Schedule")
	bool bRunning = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNPEventScheduleChangedSignature);

/** GameState에 부착되어 서버의 맵 이벤트 스케줄과 실행 상태를 관리합니다. */
UCLASS(Blueprintable, ClassGroup = (MapEvent), meta = (BlueprintSpawnableComponent))
class NOPHOTOS_API UNPMapEventManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNPMapEventManagerComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 활성 이벤트가 시작되거나 종료되어 UI 표시 정보가 바뀌면 서버와 각 클라이언트에서 호출됩니다. */
	UPROPERTY(BlueprintAssignable, Category = "Map Event|UI")
	FNPActiveMapEventsChangedSignature OnActiveMapEventsChanged;

	/** 계획 생성, 시작, 종료, 취소 시 호출됩니다. 바인딩 직후에도 GetEventSchedule로 현재 값을 읽으세요. */
	UPROPERTY(BlueprintAssignable, Category = "Map Event|Schedule")
	FNPEventScheduleChangedSignature OnEventScheduleChanged;

	UFUNCTION(BlueprintPure, Category = "Map Event|Schedule")
	FNPMapEventSchedulePresentation GetEventSchedule() const { return EventSchedule; }

	UFUNCTION(BlueprintPure, Category = "Map Event|Schedule")
	TArray<FNPScheduledMapEventPresentation> GetScheduledEventPresentations() const { return EventSchedule.Events; }

	UFUNCTION(BlueprintPure, Category = "Map Event|Schedule")
	bool GetNextScheduledEventPresentation(FNPScheduledMapEventPresentation& OutPresentation) const;

	/** 다음 이벤트의 예상 시작까지 남은 게임 시간입니다. 다음 이벤트가 없거나 시각을 모르면 -1입니다. */
	UFUNCTION(BlueprintPure, Category = "Map Event|Schedule")
	float GetNextEventStartRemainingSeconds() const;

	UFUNCTION(BlueprintPure, Category = "Map Event|Schedule")
	float GetScheduleElapsedSeconds() const;

	/** 동시 이벤트를 포함한 현재 활성 이벤트 표시 정보입니다. */
	UFUNCTION(BlueprintPure, Category = "Map Event|UI")
	TArray<FNPActiveMapEventPresentation> GetActiveEventPresentations() const
	{
		return ActiveEventPresentations;
	}

	/** 가장 최근에 시작된 활성 이벤트를 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "Map Event|UI")
	bool GetPrimaryActiveEventPresentation(FNPActiveMapEventPresentation& OutPresentation) const;

	/** 중복 없이 개수와 전체 순서를 미리 추첨합니다. 실행 중 다시 호출해도 재추첨하지 않습니다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Map Event")
	void StartEventScheduling();

	/** 다음 자동 실행과 로드 대기를 취소합니다. 이미 시작된 이벤트는 정상 종료까지 유지합니다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Map Event")
	void StopEventScheduling();

	bool ShouldStartAutomatically() const { return bStartAutomatically; }

	/** 게임 종료용입니다. 모든 자동/수동 이벤트를 끝내고 이 매니저의 이후 실행을 차단합니다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Map Event")
	void ShutdownEventsForGameEnd();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Map Event")
	bool TriggerRandomEvent(ENPMapEventType EventType);

	/** 로드된 컬렉터가 자신의 Point/Volume 목록을 이 매니저에 제공하도록 등록합니다. */
	void RegisterLocationCollector(ANPMapEventLocationCollector* Collector);
	void UnregisterLocationCollector(ANPMapEventLocationCollector* Collector);

	/** 등록된 Collector에서 그룹에 속한 유효 Point 전체를 중복 없이 조회합니다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Map Event|Locations")
	void GetSpawnPointsForGroup(
		FGameplayTag SpawnGroup,
		TArray<ANPMapEventSpawnPoint*>& OutSpawnPoints) const;

	/** 등록된 Collector에서 그룹에 속한 유효 Volume 전체를 중복 없이 조회합니다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Map Event|Locations")
	void GetSpawnVolumesForGroup(
		FGameplayTag SpawnGroup,
		TArray<ANPMapEventSpawnVolume*>& OutSpawnVolumes) const;

	/** SpawnGroup에 속한 Point 중 하나를 가중치로 선택합니다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Map Event|Locations")
	ANPMapEventSpawnPoint* FindRandomSpawnPoint(FGameplayTag SpawnGroup) const;

	/** SpawnGroup에 속한 Volume 중 실제 생성 가능한 임의의 지면 Transform을 찾습니다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Map Event|Locations")
	bool FindRandomSpawnTransform(
		FGameplayTag SpawnGroup,
		FVector RequiredHalfExtent,
		FTransform& OutTransform) const;

	/** 이벤트 BP에서 선택한 Point/Volume/Both 방식으로 생성 Transform을 찾습니다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Map Event|Locations")
	bool FindRandomSpawnTransformBySource(
		FGameplayTag SpawnGroup,
		FVector RequiredHalfExtent,
		ENPMapEventLocationSource LocationSource,
		FTransform& OutTransform) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 이 GameState에서 사용할 이벤트 정의와 활성 여부 목록입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map Event")
	TObjectPtr<UNPMapEventCatalog> EventCatalog;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map Event")
	bool bStartAutomatically = true;

	/** 매니저 BeginPlay(게임 시작)부터 첫 이벤트 시작 요청까지의 고정 시간(초). 위치 레벨 로드는 추가로 걸릴 수 있습니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map Event|Schedule", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float FirstEventStartTimeSeconds = 0.0f;

	/** 한 번의 스케줄에서 자동 실행할 최소 이벤트 개수입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map Event|Schedule", meta = (ClampMin = "0", UIMin = "0", ClampMax = "256", UIMax = "256"))
	int32 MinimumEventCount = 2;

	/** 최대 개수까지 포함해 추첨합니다. 최소/최대가 모두 0이면 자동 실행하지 않습니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map Event|Schedule", meta = (ClampMin = "0", UIMin = "0", ClampMax = "256", UIMax = "256"))
	int32 MaximumEventCount = 3;

	/** 수동 TriggerRandomEvent 호출에 적용합니다. 자동 스케줄은 항상 순차 실행합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map Event")
	bool bAllowConcurrentEvents = false;

private:
	friend class FNPMapEventSchedulingTest;

	bool HasServerAuthority() const;
	bool IsGameOver() const;
	static FString GetEventIdentityKey(const ANPMapEvent* MapEvent);
	void CollectExistingLocationCollectors(
		ENPMapEventLocationSource LocationSource = ENPMapEventLocationSource::Both);
	void CreateEventInstances();
	void RegisterManagedEvent(ANPMapEvent* EventInstance);

	UFUNCTION()
	void HandleManagedEventStarted(ANPMapEvent* MapEvent);

	UFUNCTION()
	void HandleManagedEventFinished(ANPMapEvent* MapEvent);

	UFUNCTION()
	void HandleManagedEventDestroyed(AActor* DestroyedActor);

	UFUNCTION()
	void OnRep_ActiveEventPresentations();

	UFUNCTION()
	void OnRep_EventSchedule();

	void NotifyEventScheduleChanged();
	float GetServerWorldTimeSeconds() const;
	void UpdateExpectedEventTimes(int32 FirstIndex, float FirstStartServerWorldTime);

	void NotifyActiveEventPresentationsChanged();
	bool RequestEventStart(ANPMapEvent* EventInstance);
	bool LoadEventLocationLevel(
		ANPMapEvent* EventInstance,
		const TSoftObjectPtr<UWorld>& LocationLevelInstance,
		const FTransform& LocationLevelTransform);

	UFUNCTION()
	void HandleLocationLevelShown();

	UFUNCTION()
	void HandleLocationLevelUnloaded();

	UFUNCTION()
	void HandleEventWithLocationLevelFinished(ANPMapEvent* MapEvent);

	void UnloadActiveLocationLevel();
	ANPMapEvent* SelectRandomEvent(const ENPMapEventType* EventType = nullptr, bool bRequireReady = true, bool bExcludePlanned = false) const;
	void ScheduleNextEvent(float DelaySeconds);
	void HandleNextEventTimer();
	bool HasActiveEvent() const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ANPMapEvent>> EventInstances;

	/** GameState의 이 컴포넌트를 통해 모든 클라이언트에 전달되는 UI용 활성 이벤트 정보입니다. */
	UPROPERTY(ReplicatedUsing = OnRep_ActiveEventPresentations)
	TArray<FNPActiveMapEventPresentation> ActiveEventPresentations;

	UPROPERTY(ReplicatedUsing = OnRep_EventSchedule)
	FNPMapEventSchedulePresentation EventSchedule;

	/** 서버에서 확정한 실행 순서입니다. 클라이언트에는 액터 참조 대신 표시용 스냅샷을 보냅니다. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<ANPMapEvent>> PlannedEvents;

	/** 같은 판에서 이미 시작한 종류입니다. 수동 실행 및 스케줄 재시작으로도 재추첨하지 않습니다. */
	TSet<FString> PlayedEventKeys;
	bool bEventsShutdown = false;

	/** 이벤트 인스턴스가 실행될 때 함께 스트리밍할 위치 Point/Volume 레벨입니다. */
	UPROPERTY(Transient)
	TMap<TObjectPtr<ANPMapEvent>, TSoftObjectPtr<UWorld>> EventLocationLevels;

	/** Level Instance로 변환하면서 생긴 원점 오프셋을 복원할 이벤트별 Transform입니다. */
	UPROPERTY(Transient)
	TMap<TObjectPtr<ANPMapEvent>, FTransform> EventLocationLevelTransforms;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ANPMapEventLocationCollector>> LocationCollectors;

	/** 위치 레벨이 준비된 뒤 시작할 이벤트입니다. */
	UPROPERTY(Transient)
	TObjectPtr<ANPMapEvent> PendingEvent;

	/** 현재 이벤트를 위해 로드 중이거나 로드된 위치 레벨입니다. */
	UPROPERTY(Transient)
	TObjectPtr<ULevelStreamingDynamic> ActiveLocationLevel;

	bool bLocationLevelTransitionInProgress = false;
	int32 LocationLevelInstanceSerial = 0;

	/** 로드 중이거나 실행 중인 자동 이벤트입니다. 수동 실행은 개수에 포함하지 않습니다. */
	UPROPERTY(Transient)
	TObjectPtr<ANPMapEvent> ScheduledEvent;

	bool bScheduling = false;
	bool bScheduledEventStarted = false;
	int32 TargetEventCount = 0;
	int32 StartedEventCount = 0;
	int32 ScheduledPlanIndex = INDEX_NONE;
	float ScheduleOriginServerWorldTime = -1.0f;
	FTimerHandle NextEventTimer;
};
