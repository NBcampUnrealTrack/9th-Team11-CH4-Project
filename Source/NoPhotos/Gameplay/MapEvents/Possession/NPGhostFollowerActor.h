#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NPGhostFollowerActor.generated.h"

class ANPStablePhysicsPawn;
class USceneComponent;
class UStaticMeshComponent;
class USkeletalMeshComponent;
class UMaterialInstanceDynamic;

/** 화면별로 생성하는 충돌 없는 유령 외형. 서버 위치 복제나 캐릭터 입력에는 관여하지 않습니다. */
UCLASS(Blueprintable)
class NOPHOTOS_API ANPGhostFollowerActor : public AActor
{
	GENERATED_BODY()

public:
	ANPGhostFollowerActor();
	virtual void Tick(float DeltaSeconds) override;

	/** 이벤트의 deferred spawn 중 호출합니다. 캐릭터 자체에는 컴포넌트를 추가하지 않습니다. */
	bool InitializeFollower(ANPStablePhysicsPawn* InTarget);

	/** 현재 알파부터 퇴장한 뒤 자동 파괴합니다. 중복 요청은 무시합니다. */
	UFUNCTION(BlueprintCallable, Category="Ghost Follower|Fade")
	void RequestFadeOut();

	UFUNCTION(BlueprintPure, Category="Ghost Follower")
	ANPStablePhysicsPawn* GetFollowTarget() const { return FollowTarget.Get(); }

	/** 수평 정면을 기준으로 뒤쪽/위쪽 오프셋을 계산합니다. 외형 Scale은 포함하지 않습니다. */
	static FTransform CalculateFollowTransform(const FVector& TargetLocation, const FVector& Forward,
		float Distance, float Height);

	static float CalculateFadeOpacity(float StartOpacity, float TargetOpacity, float Elapsed, float Duration);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ghost Follower")
	TObjectPtr<USceneComponent> FollowRoot;

	/** 메시 방향/피벗/크기는 이 컴포넌트 또는 개별 메시의 상대 Transform으로 보정합니다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ghost Follower|Visual")
	TObjectPtr<USceneComponent> VisualRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ghost Follower|Visual")
	TObjectPtr<UStaticMeshComponent> GhostMesh;

	/** 애니메이션이 필요한 유령일 때 사용합니다. 기본 Static Mesh와 둘 중 하나만 써도 됩니다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ghost Follower|Visual")
	TObjectPtr<USkeletalMeshComponent> AnimatedGhostMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ghost Follower|Follow", meta=(ClampMin="0.0", Units="cm"))
	float FollowDistance = 150.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ghost Follower|Follow", meta=(Units="cm"))
	float HeightOffset = 70.0f;

	/** 0이면 보간 없이 바로 등 뒤에 붙습니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ghost Follower|Follow", meta=(ClampMin="0.0"))
	float FollowInterpSpeed = 8.0f;

	/** 순간이동 등으로 멀어지면 긴 거리를 보간하지 않고 즉시 따라갑니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ghost Follower|Follow", meta=(ClampMin="1.0", Units="cm"))
	float SnapDistance = 600.0f;

	/** 유령 머티리얼의 Opacity에 연결한 Scalar Parameter 이름입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ghost Follower|Fade")
	FName GhostOpacityParameterName = TEXT("GhostOpacity");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ghost Follower|Fade", meta=(ClampMin="0.0", ClampMax="1.0"))
	float GhostMaxOpacity = 0.35f;

	/** 0이면 즉시 표시합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ghost Follower|Fade", meta=(ClampMin="0.0", Units="s"))
	float GhostFadeInDuration = 0.5f;

	/** 0이면 퇴장 요청 시 즉시 제거합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ghost Follower|Fade", meta=(ClampMin="0.0", Units="s"))
	float GhostFadeOutDuration = 0.5f;

private:
	void UpdateFollow(float DeltaSeconds, bool bSnap);
	void InitializeFadeMaterials();
	void ApplyGhostOpacity(float Opacity);
	void UpdateFade(float DeltaSeconds);
	void StopFollowing();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> FadeMaterials;
	float CurrentGhostOpacity = 0.0f;
	float FadeStartOpacity = 0.0f;
	float FadeTargetOpacity = 0.0f;
	float FadeElapsed = 0.0f;
	float FadeDuration = 0.0f;
	bool bGhostFadingOut = false;
	bool bGhostFadeRunning = false;

	TWeakObjectPtr<ANPStablePhysicsPawn> FollowTarget;
	FVector LastHorizontalForward = FVector::ForwardVector;
};
