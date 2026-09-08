#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NPGhostFollowerActor.generated.h"

class ANPStablePhysicsPawn;
class USceneComponent;
class USphereComponent;
class UStaticMeshComponent;
class USkeletalMeshComponent;
class UMaterialInstanceDynamic;
class UPrimitiveComponent;

/** 서버에서 플레이어를 추격하고 접촉 시 빙의를 요청하는 복제 유령입니다. */
UCLASS(Blueprintable)
class NOPHOTOS_API ANPGhostFollowerActor : public AActor
{
	GENERATED_BODY()

public:
	ANPGhostFollowerActor();
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 서버의 deferred spawn 중 호출합니다. 추적 대상 없이 Point에서 대기하는 복제 유령으로 초기화합니다. */
	bool InitializeRoamingGhost();

	/** 서버 Roaming 유령이 추격할 플레이어를 지정합니다. 이동 결과는 Replicate Movement로 전달됩니다. */
	bool SetRoamingChaseTarget(ANPStablePhysicsPawn* InTarget);

	/** 지정 시간 동안 플레이어 접촉 판정만 무시합니다. 추격 이동은 계속합니다. */
	void SetRoamingContactDelay(float DelaySeconds);

	/** 모든 클라이언트에서 즉시 숨긴 뒤 잠시 후 서버 액터를 제거합니다. */
	void ConsumeRoamingGhost(float DestroyDelay = 0.25f);

	/** 현재 알파부터 퇴장한 뒤 자동 파괴합니다. 중복 요청은 무시합니다. */
	UFUNCTION(BlueprintCallable, Category="Ghost Follower|Fade")
	void RequestFadeOut();

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

	/** Roaming 상태에서 플레이어 접촉을 서버 권한으로 감지합니다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ghost Follower|Roaming")
	TObjectPtr<USphereComponent> RoamingContactSphere;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ghost Follower|Roaming", meta=(ClampMin="1.0", Units="cm"))
	float RoamingContactRadius = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ghost Follower|Roaming", meta=(ClampMin="0.0", Units="cm/s"))
	float RoamingChaseSpeed = 350.0f;

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
	void UpdateRoamingChase(float DeltaSeconds);
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

	UPROPERTY(Replicated)
	bool bRoamingGhost = false;

	UPROPERTY(ReplicatedUsing=OnRep_RoamingGhostConsumed)
	bool bRoamingGhostConsumed = false;

	/** Deferred Blueprint Construction에서 UPROPERTY 기본값이 복원되어도 서버 생성 의도를 유지합니다. */
	bool bRoamingInitializationRequested = false;
	float RoamingContactEnableWorldTime = 0.0f;
};
