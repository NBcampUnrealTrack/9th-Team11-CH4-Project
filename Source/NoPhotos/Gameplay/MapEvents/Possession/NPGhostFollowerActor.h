#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NPGhostFollowerActor.generated.h"

class ANPStablePhysicsPawn;
class ANPGhostPatrolRoute;
class USceneComponent;
class USphereComponent;
class UStaticMeshComponent;
class USkeletalMeshComponent;
class UMaterialInstanceDynamic;
class UPrimitiveComponent;

/**

순찰 고스트와 빙의 후 화면별 등 뒤 외형에 함께 사용하는 충돌 없는 유령 액터입니다. */
/** 서버에서 플레이어를 추격하고 접촉 시 빙의를 요청하는 복제 유령입니다. */
UCLASS(Blueprintable)
class NOPHOTOS_API ANPGhostFollowerActor : public AActor
{
	GENERATED_BODY()

public:
	ANPGhostFollowerActor();
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 이벤트의 deferred spawn 중 호출합니다. 캐릭터 자체에는 컴포넌트를 추가하지 않습니다. */
	bool InitializeFollower(ANPStablePhysicsPawn* InTarget);

	/** 서버의 deferred spawn 중 호출합니다. 지정한 Spline을 왕복하는 복제 유령으로 초기화합니다. */
	bool InitializeRoamingGhost(ANPGhostPatrolRoute* InPatrolRoute, float StartDistance,
		bool bStartForward = true);
	/** 서버의 deferred spawn 중 호출합니다. 추적 대상 없이 Point에서 대기하는 복제 유령으로 초기화합니다. */
	bool InitializeRoamingGhost();

	/** 서버 Roaming 유령이 추격할 플레이어를 지정합니다. 이동 결과는 Replicate Movement로 전달됩니다. */
	bool SetRoamingChaseTarget(ANPStablePhysicsPawn* InTarget);

	/** 지정 시간 동안 플레이어 접촉 판정만 무시합니다. 순찰/추격 이동은 계속합니다. */
	void SetRoamingContactDelay(float DelaySeconds);

	/** 모든 클라이언트에서 즉시 숨긴 뒤 잠시 후 서버 액터를 제거합니다. */
	void ConsumeRoamingGhost(float DestroyDelay = 0.25f);

	/** 현재 알파부터 퇴장한 뒤 자동 파괴합니다. 중복 요청은 무시합니다. */
	UFUNCTION(BlueprintCallable, Category="Ghost Follower|Fade")
	void RequestFadeOut();

	UFUNCTION(BlueprintPure, Category="Ghost Follower")
	ANPStablePhysicsPawn* GetFollowTarget() const { return FollowTarget.Get(); }

	/** 서버에서 이 순찰 고스트에 배정한 루트입니다. */
	ANPGhostPatrolRoute* GetRoamingPatrolRoute() const { return RoamingPatrolRoute.Get(); }

	/** 수평 정면을 기준으로 뒤쪽/위쪽 오프셋을 계산합니다. 외형 Scale은 포함하지 않습니다. */
	static FTransform CalculateFollowTransform(const FVector& TargetLocation, const FVector& Forward,
		float Distance, float Height);

	static float CalculateFadeOpacity(float StartOpacity, float TargetOpacity, float Elapsed, float Duration);

	/** 이동량을 반영하고 끝점에서 방향을 뒤집습니다. 큰 프레임에서도 여러 번 왕복할 수 있습니다. */
	static float CalculatePingPongDistance(float Distance, float TravelDistance, float RouteLength,
		float& OutDirection);

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

	/** Roaming 상태에서 플레이어 접촉을 서버 권한으로 감지합니다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ghost Follower|Roaming")
	TObjectPtr<USphereComponent> RoamingContactSphere;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ghost Follower|Roaming", meta=(ClampMin="1.0", Units="cm"))
	float RoamingContactRadius = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ghost Follower|Roaming", meta=(ClampMin="0.0", Units="cm/s"))
	float RoamingChaseSpeed = 350.0f;

	/** 이 거리 안에 들어온 가장 가까운 플레이어를 추격합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ghost Follower|Roaming", meta=(ClampMin="0.0", Units="cm"))
	float RoamingPlayerDetectionRadius = 600.0f;

	/** 추격 중인 플레이어가 이 거리 밖으로 나가면 루트로 복귀합니다. 감지 거리보다 크게 설정합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ghost Follower|Roaming", meta=(ClampMin="0.0", Units="cm"))
	float RoamingChaseReleaseRadius = 900.0f;

	/** 플레이어 감지와 추격 해제를 다시 판단하는 주기입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ghost Follower|Roaming", meta=(ClampMin="0.05", Units="s"))
	float RoamingChaseDecisionInterval = 0.2f;
	/** 플레이어를 추격하지 않을 때 Spline을 왕복하는 속도입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ghost Follower|Roaming", meta=(ClampMin="0.0", Units="cm/s"))
	float RoamingPatrolSpeed = 200.0f;

	/** 추격 종료 후 가장 가까운 루트 지점으로 돌아가는 속도입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ghost Follower|Roaming", meta=(ClampMin="0.0", Units="cm/s"))
	float RoamingRouteReturnSpeed = 250.0f;

	/** 이 거리까지 루트에 접근하면 순찰을 재개합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ghost Follower|Roaming", meta=(ClampMin="1.0", Units="cm"))
	float RoamingRouteReturnAcceptanceRadius = 25.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ghost Follower|Roaming", meta=(ClampMin="0.0"))
	float RoamingRotationInterpSpeed = 8.0f;

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
	UFUNCTION()
	void HandleRoamingContactBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void OnRep_RoamingGhostConsumed();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastConsumeRoamingGhost();

	void ApplyRoamingGhostConsumedState();
	void CheckRoamingPlayerContacts();
	void TryHandleRoamingPlayerContact(ANPStablePhysicsPawn* PlayerPawn);
	void EvaluateRoamingChaseTarget();
	ANPStablePhysicsPawn* FindNearestChaseTarget(float DetectionRadius) const;
	void BeginReturnToPatrolRoute();
	void UpdateFollow(float DeltaSeconds, bool bSnap);
	void UpdateRoamingChase(float DeltaSeconds);
	void UpdateRoamingPatrol(float DeltaSeconds);
	void UpdateRoamingRouteReturn(float DeltaSeconds);
	void InitializeFadeMaterials();
	void ApplyGhostOpacity(float Opacity);
	void UpdateFade(float DeltaSeconds);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> FadeMaterials;
	float CurrentGhostOpacity = 0.0f;
	float FadeStartOpacity = 0.0f;
	float FadeTargetOpacity = 0.0f;
	float FadeElapsed = 0.0f;
	float FadeDuration = 0.0f;
	bool bGhostFadingOut = false;
	bool bGhostFadeRunning = false;

	TWeakObjectPtr<ANPStablePhysicsPawn> RoamingChaseTarget;
	TWeakObjectPtr<ANPGhostPatrolRoute> RoamingPatrolRoute;
	float RoamingPatrolDistance = 0.0f;
	float RoamingPatrolDirection = 1.0f;
	float RoamingRouteReturnDistance = 0.0f;
	double NextRoamingChaseDecisionTime = 0.0;
	bool bRoamingChaseActive = false;
	bool bReturningToPatrolRoute = false;
	FVector LastHorizontalForward = FVector::ForwardVector;

	UPROPERTY(Replicated)
	bool bRoamingGhost = false;

	UPROPERTY(ReplicatedUsing=OnRep_RoamingGhostConsumed)
	bool bRoamingGhostConsumed = false;

	/** Deferred Blueprint Construction에서 UPROPERTY 기본값이 복원되어도 서버 생성 의도를 유지합니다. */
	bool bRoamingInitializationRequested = false;
	float RoamingContactEnableWorldTime = 0.0f;
};
