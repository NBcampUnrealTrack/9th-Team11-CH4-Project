#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "NPMapEvent.h"
#include "NPMapEventDefinition.generated.h"

/**
 * 맵 이벤트 하나의 고유한 정의입니다.
 * 출현 가중치는 이벤트 자체의 속성이 아니므로 이벤트 카탈로그에서 별도로 설정합니다.
 */
UCLASS(BlueprintType)
class NOPHOTOS_API UNPMapEventDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	TSubclassOf<ANPMapEvent> GetEventClass() const { return EventClass; }
	FName GetEventId() const { return EventId; }
	const FText& GetDisplayName() const { return DisplayName; }
	const FText& GetDescription() const { return Description; }
	ENPMapEventType GetEventType() const { return EventType; }
	ENPMapEventScale GetEventScale() const { return EventScale; }
	float GetDuration() const { return FMath::Max(0.0f, Duration); }
	float GetDelay() const { return FMath::Max(0.0f, Delay); }

private:
	/** 이 정의를 실행할 실제 이벤트 액터 클래스입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map Event", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<ANPMapEvent> EventClass;

	/** 로그와 저장 데이터에서 이벤트를 구분할 안정적인 식별자입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map Event|Metadata", meta = (AllowPrivateAccess = "true"))
	FName EventId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map Event|Metadata", meta = (AllowPrivateAccess = "true"))
	FText DisplayName;

	/** UI에 표시할 이벤트 설명입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map Event|Metadata", meta = (AllowPrivateAccess = "true", MultiLine = "true"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map Event|Metadata", meta = (AllowPrivateAccess = "true"))
	ENPMapEventType EventType = ENPMapEventType::TypeA;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map Event|Metadata", meta = (AllowPrivateAccess = "true"))
	ENPMapEventScale EventScale = ENPMapEventScale::Small;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map Event|Timing", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float Duration = 8.0f;

	/** 자동 스케줄에서 이 이벤트 종료 후 다음 이벤트 시작까지의 대기 시간(초)입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map Event|Timing", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float Delay = 0.0f;
};
