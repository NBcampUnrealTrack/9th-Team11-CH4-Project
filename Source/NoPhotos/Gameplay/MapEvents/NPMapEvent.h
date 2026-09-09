#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NPMapEventTypes.h"
#include "NPMapEvent.generated.h"

class ANPMapEvent;
class UNPMapEventDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FNPMapEventLifecycleSignature,
	ANPMapEvent*,
	MapEvent);

UCLASS(Abstract, Blueprintable)
class NOPHOTOS_API ANPMapEvent : public AActor
{
	GENERATED_BODY()

public:
	ANPMapEvent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Map Event")
	bool StartEvent();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Map Event")
	void FinishEvent();

	UFUNCTION(BlueprintPure, Category = "Map Event")
	bool IsEventActive() const { return bIsActive; }

	/** 카탈로그가 이벤트 액터를 생성할 때 정의를 주입합니다. */
	void InitializeEvent(UNPMapEventDefinition* InEventDefinition);

	UFUNCTION(BlueprintPure, Category = "Map Event|Metadata")
	UNPMapEventDefinition* GetEventDefinition() const { return EventDefinition; }

	UFUNCTION(BlueprintPure, Category = "Map Event|Metadata")
	FName GetEventId() const;

	UFUNCTION(BlueprintPure, Category = "Map Event|Metadata")
	FText GetEventDisplayName() const;

	UFUNCTION(BlueprintPure, Category = "Map Event|Metadata")
	FText GetEventDescription() const;

	UFUNCTION(BlueprintPure, Category = "Map Event|Metadata")
	ENPMapEventType GetEventType() const;

	UFUNCTION(BlueprintPure, Category = "Map Event|Metadata")
	ENPMapEventScale GetEventScale() const;

	UFUNCTION(BlueprintPure, Category = "Map Event|Metadata")
	float GetEventDuration() const;

	/** 서버 동기화 시간을 기준으로 계산한 현재 이벤트의 남은 시간입니다. */
	UFUNCTION(BlueprintPure, Category = "Map Event|Timing")
	float GetRemainingEventTime() const;

	/** 월드 UI처럼 동일한 종료 시각을 공유해야 하는 객체에서 사용하는 서버 절대 시간입니다. */
	UFUNCTION(BlueprintPure, Category = "Map Event|Timing")
	float GetEventEndServerWorldTime() const { return EventEndServerWorldTime; }

	/** 이 이벤트 BP가 위치 후보로 사용할 Point/Volume 종류입니다. */
	UFUNCTION(BlueprintPure, Category = "Map Event|Locations")
	ENPMapEventLocationSource GetLocationSource() const { return LocationSource; }

	/** 이 이벤트가 지정된 A/B 발생 시간의 후보로 들어갈 수 있는지 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "Map Event|Metadata")
	bool CanRunAtEventTime(ENPMapEventType EventTimeType) const;

	/** 초대형 이벤트는 일반 시간표와 분리해 단독으로 실행해야 합니다. */
	UFUNCTION(BlueprintPure, Category = "Map Event|Metadata")
	bool RequiresStandaloneExecution() const { return GetEventScale() == ENPMapEventScale::Colossal; }

	bool CanStartEvent() const;

	/** 서버와 클라이언트 각각에서 이벤트 상태가 적용된 직후 호출됩니다. */
	UPROPERTY(BlueprintAssignable, Category = "Map Event|Lifecycle")
	FNPMapEventLifecycleSignature OnEventStarted;

	/** 서버와 클라이언트 각각에서 이벤트 상태가 해제된 직후 호출됩니다. */
	UPROPERTY(BlueprintAssignable, Category = "Map Event|Lifecycle")
	FNPMapEventLifecycleSignature OnEventFinished;

protected:
	UFUNCTION(BlueprintNativeEvent, Category = "Map Event")
	void ApplyEventState(bool bNewActive);
	virtual void ApplyEventState_Implementation(bool bNewActive);

	/** 새 이벤트 정의 에셋으로 이전하기 전 네이티브 이벤트를 위한 임시 호환값입니다. */
	UPROPERTY()
	FName EventId = NAME_None;

	UPROPERTY()
	FText EventDisplayName;

	UPROPERTY()
	FText EventDescription;

	UPROPERTY()
	ENPMapEventType EventType = ENPMapEventType::TypeA;

	UPROPERTY()
	ENPMapEventScale EventScale = ENPMapEventScale::Small;

	UPROPERTY()
	float Duration = 8.0f;

	/** Point는 지정 위치, Volume은 영역 안 임의 위치, Both는 두 후보를 가중치로 함께 사용합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map Event|Locations")
	ENPMapEventLocationSource LocationSource = ENPMapEventLocationSource::Both;

private:
	UFUNCTION()
	void OnRep_IsActive();

	/** 서버가 선택한 정의이며 초기 액터 복제를 통해 클라이언트에도 전달됩니다. */
	UPROPERTY(Replicated)
	TObjectPtr<UNPMapEventDefinition> EventDefinition;

	UPROPERTY(ReplicatedUsing = OnRep_IsActive)
	bool bIsActive = false;

	/** GameState의 서버 동기화 시간 기준 이벤트 종료 시각입니다. */
	UPROPERTY(Replicated)
	float EventEndServerWorldTime = 0.0f;

	FTimerHandle DurationTimer;
};
