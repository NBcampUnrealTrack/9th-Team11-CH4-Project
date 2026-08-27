#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "NPMapEventSpawnVolume.generated.h"

class UBoxComponent;

/**
 * 디자이너가 레벨에 배치하는 맵 이벤트용 생성 허용 영역입니다.
 * 박스 내부와 NavMesh가 겹치는 지점 중 지면/공간/접근성 검사를 통과한 위치를 반환합니다.
 * Navigation Data는 메인 Persistent Level에 상시 존재하는 NavMesh를 사용합니다.
 */
UCLASS(Blueprintable)
class NOPHOTOS_API ANPMapEventSpawnVolume : public AActor
{
	GENERATED_BODY()

public:
	ANPMapEventSpawnVolume();

	UFUNCTION(BlueprintPure, Category = "Map Event|Spawn Volume")
	bool SupportsSpawnGroup(FGameplayTag SpawnGroup) const;

	UFUNCTION(BlueprintPure, Category = "Map Event|Spawn Volume")
	float GetSelectionWeight() const { return SelectionWeight; }

	/** true이면 자신이 속한 Collector의 기본 SpawnGroup도 함께 사용합니다. */
	UFUNCTION(BlueprintPure, Category = "Map Event|Spawn Volume")
	bool UsesCollectorSpawnGroups() const { return bUseCollectorSpawnGroups; }

	/** 회전/스케일을 반영한 박스 내부 판정입니다. 충돌, Overlap, NavMesh는 필요하지 않습니다. */
	UFUNCTION(BlueprintPure, Category = "Map Event|Spawn Volume")
	bool IsInsideSpawnBounds(const FVector& WorldLocation) const;

	/**
	 * RequiredHalfExtent는 생성할 Actor가 차지할 공간의 반지름입니다.
	 * 반환 Transform의 위치는 Actor의 바닥 Pivot을 기준으로 합니다.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Map Event|Spawn Volume")
	bool FindRandomGroundTransform(
		FVector RequiredHalfExtent,
		FTransform& OutTransform) const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> SpawnBounds;

	/** 이 Volume을 사용할 수 있는 이벤트 생성 그룹입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map Event|Spawn Volume")
	FGameplayTagContainer SupportedSpawnGroups;

	/** Level Instance Collector의 기본 그룹을 상속합니다. 개별 태그만 쓰려면 끕니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map Event|Spawn Volume")
	bool bUseCollectorSpawnGroups = true;

	/** 같은 SpawnGroup의 영역이 여러 개일 때 사용할 상대 가중치입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map Event|Spawn Volume", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SelectionWeight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map Event|Spawn Volume|Sampling", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaximumSamplingAttempts = 20;

	/** 임의 지점을 NavMesh로 투영할 때 탐색할 수평/수직 범위입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map Event|Spawn Volume|Navigation", meta = (ClampMin = "0.0"))
	FVector NavigationProjectionExtent = FVector(100.0f, 100.0f, 300.0f);

	/** 현재 플레이어 중 한 명이라도 후보 지점까지 NavMesh 경로가 있어야 하는지 여부입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map Event|Spawn Volume|Navigation")
	bool bRequireReachableFromPlayer = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map Event|Spawn Volume|Ground", meta = (ClampMin = "0.0", Units = "cm"))
	float GroundTraceUpDistance = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map Event|Spawn Volume|Ground", meta = (ClampMin = "0.0", Units = "cm"))
	float GroundTraceDownDistance = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map Event|Spawn Volume|Ground", meta = (ClampMin = "0.0", ClampMax = "89.0", Units = "deg"))
	float MaximumGroundSlopeDegrees = 20.0f;

	/** 생성 영역의 중앙과 네 모서리 사이에 허용할 최대 지면 높이 차이입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map Event|Spawn Volume|Ground", meta = (ClampMin = "0.0", Units = "cm"))
	float MaximumGroundHeightDifference = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map Event|Spawn Volume|Sampling")
	bool bRandomizeYaw = false;

private:
	bool TraceGround(const FVector& TestLocation, FHitResult& OutHit) const;
	bool TryResolveGroundLocation(
		const FVector& NavigationLocation,
		const FVector& RequiredHalfExtent,
		const FRotator& Rotation,
		FVector& OutGroundLocation) const;
	bool HasRequiredClearance(
		const FVector& GroundLocation,
		const FVector& RequiredHalfExtent,
		const FRotator& Rotation) const;
	bool IsReachableFromAnyPlayer(const FVector& GroundLocation) const;
};
