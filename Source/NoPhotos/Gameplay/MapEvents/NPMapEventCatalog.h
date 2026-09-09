#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "NPMapEventCatalog.generated.h"

class ANPMapEvent;
class UNPMapEventDefinition;
class UWorld;

/** 한 이벤트 목록 안에서 사용할 이벤트 정의와 활성 여부입니다. */
USTRUCT(BlueprintType)
struct FNPMapEventCatalogEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map Event")
	TObjectPtr<UNPMapEventDefinition> EventDefinition;

	/** false이면 인스턴스를 생성하지 않으며 이번 게임의 이벤트 후보에서 제외합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map Event")
	bool bEnabled = true;

	/**
	 * 이 이벤트의 Point/Volume 세트를 담은 선택적 위치 레벨 원본(.umap)입니다.
	 * 이벤트가 선택되면 서버가 동적으로 로드하며, 이벤트 종료 후 다시 언로드합니다.
	 * NavMesh는 이 위치 레벨이 아니라 메인 Persistent Level에서 상시 제공합니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map Event|Locations")
	TSoftObjectPtr<UWorld> LocationLevelInstance;

	/**
	 * 위치 레벨을 메인 월드에 적용할 Transform입니다.
	 * 월드 원점을 기준으로 제작한 위치 레벨은 Identity를 사용하면 됩니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map Event|Locations",
		meta = (EditCondition = "LocationLevelInstance != nullptr", EditConditionHides))
	FTransform LocationLevelTransform = FTransform::Identity;
};

/**
 * 한 게임에서 사용할 맵 이벤트 목록을 관리합니다.
 * 이벤트 추가/제거는 레벨의 매니저가 아니라 이 데이터 에셋에서 수행합니다.
 */
UCLASS(BlueprintType)
class NOPHOTOS_API UNPMapEventCatalog : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	const TArray<FNPMapEventCatalogEntry>& GetEventEntries() const { return EventEntries; }

	/** 기존 카탈로그 에셋을 새 정의 방식으로 옮기는 동안만 사용하는 호환 목록입니다. */
	const TArray<TSubclassOf<ANPMapEvent>>& GetEventClasses() const { return EventClasses; }

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map Event", meta = (AllowPrivateAccess = "true"))
	TArray<FNPMapEventCatalogEntry> EventEntries;

	/** EventEntries로 이전한 뒤 제거할 기존 클래스 목록입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = "Map Event|Legacy", meta = (AllowPrivateAccess = "true"))
	TArray<TSubclassOf<ANPMapEvent>> EventClasses;
};
