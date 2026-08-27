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
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNPActiveMapEventsChangedSignature);

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

	/** 동시 이벤트를 포함한 현재 활성 이벤트 표시 정보입니다. */
	UFUNCTION(BlueprintPure, Category = "Map Event|UI")
	TArray<FNPActiveMapEventPresentation> GetActiveEventPresentations() const
	{
		return ActiveEventPresentations;
	}

	/** 가장 최근에 시작된 활성 이벤트를 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "Map Event|UI")
	bool GetPrimaryActiveEventPresentation(FNPActiveMapEventPresentation& OutPresentation) const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Map Event")
	void StartEventScheduling();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Map Event")
	void StopEventScheduling();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Map Event")
	bool TriggerRandomEvent(ENPMapEventType EventType);

	/** 로드된 컬렉터가 자신의 Point/Volume 목록을 이 매니저에 제공하도록 등록합니다. */
	void RegisterLocationCollector(ANPMapEventLocationCollector* Collector);
	void UnregisterLocationCollector(ANPMapEventLocationCollector* Collector);

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

	/** 이 GameState에서 사용할 이벤트 정의와 가중치 목록입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map Event")
	TObjectPtr<UNPMapEventCatalog> EventCatalog;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map Event")
	bool bStartAutomatically = true;

	/** 게임 시작 후 A 타입 이벤트가 발생할 수 있는 최소 시간(분)입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map Event|Schedule|Type A", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "min"))
	float TypeAMinimumTimeMinutes = 2.0f;

	/** 게임 시작 후 A 타입 이벤트가 발생할 수 있는 최대 시간(분)입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map Event|Schedule|Type A", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "min"))
	float TypeAMaximumTimeMinutes = 4.0f;

	/** 게임 시작 후 B 타입 이벤트가 발생할 수 있는 최소 시간(분)입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map Event|Schedule|Type B", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "min"))
	float TypeBMinimumTimeMinutes = 7.0f;

	/** 게임 시작 후 B 타입 이벤트가 발생할 수 있는 최대 시간(분)입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map Event|Schedule|Type B", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "min"))
	float TypeBMaximumTimeMinutes = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map Event")
	bool bAllowConcurrentEvents = false;

private:
	bool HasServerAuthority() const;
	void CollectExistingLocationCollectors(
		ENPMapEventLocationSource LocationSource = ENPMapEventLocationSource::Both);
	void CreateEventInstances();
	void RegisterManagedEvent(ANPMapEvent* EventInstance);

	UFUNCTION()
	void HandleManagedEventStarted(ANPMapEvent* MapEvent);

	UFUNCTION()
	void HandleManagedEventFinished(ANPMapEvent* MapEvent);

	UFUNCTION()
	void OnRep_ActiveEventPresentations();

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
	void ScheduleEvent(ENPMapEventType EventType, float MinimumTimeMinutes, float MaximumTimeMinutes);
	void HandleTypeAEventTimer();
	void HandleTypeBEventTimer();
	bool HasActiveEvent() const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ANPMapEvent>> EventInstances;

	/** GameState의 이 컴포넌트를 통해 모든 클라이언트에 전달되는 UI용 활성 이벤트 정보입니다. */
	UPROPERTY(ReplicatedUsing = OnRep_ActiveEventPresentations)
	TArray<FNPActiveMapEventPresentation> ActiveEventPresentations;

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

	FTimerHandle TypeAEventTimer;
	FTimerHandle TypeBEventTimer;
};
