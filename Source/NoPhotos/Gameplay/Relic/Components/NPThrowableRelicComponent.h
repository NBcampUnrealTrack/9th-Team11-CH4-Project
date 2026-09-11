#pragma once

#include "Gameplay/Relic/Components/NPUsableRelicComponent.h"
#include "GameplayTagContainer.h"
#include "NPThrowableRelicComponent.generated.h"

class ANPStablePhysicsPawn;
class UAbilitySystemComponent;
class UAudioComponent;
class UGameplayEffect;
class UPrimitiveComponent;
class USoundAttenuation;
class USoundBase;

USTRUCT(BlueprintType)
struct NOPHOTOS_API FNPRelicThrowSettings
{
	GENERATED_BODY()

	/** 카메라의 수평 정면 방향으로 적용할 속도입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Throw", meta=(ClampMin="0.0", Units="cm/s"))
	float ForwardSpeed = 1800.0f;

	/** 포물선을 만들기 위해 월드 위쪽으로 더할 속도입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Throw", meta=(ClampMin="0.0", Units="cm/s"))
	float UpwardSpeed = 300.0f;

	/** 던진 플레이어의 현재 이동 속도를 유물에 더합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Throw")
	bool bInheritThrowerVelocity = true;

	/** 유물 로컬 좌표 기준 회전축입니다. 기본 Y축은 무기를 앞뒤로 빙글빙글 돌립니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Throw|Spin")
	FVector LocalSpinAxis = FVector::YAxisVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Throw|Spin", meta=(ClampMin="0.0", Units="deg/s"))
	float SpinSpeed = 1440.0f;

	/** 투척 직후 손/몸에 튕기는 것을 줄이기 위한 충돌 무시 시간입니다. 0이면 사용하지 않습니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Throw|Collision",
		meta=(ClampMin="0.0", Units="s", DisplayName="Collision Grace Time"))
	float PawnCollisionGraceTime = 0.2f;

	/** 유예 시간 동안 Pawn 채널과 충돌하지 않습니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Throw|Collision")
	bool bIgnorePawnDuringCollisionGrace = true;

	/** 유예 시간 동안 캐릭터의 물리 Mesh를 포함한 PhysicsBody 채널과 충돌하지 않습니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Throw|Collision")
	bool bIgnorePhysicsBodyDuringCollisionGrace = true;

	/** 공중에서 다시 잡았을 때 즉시 연속 투척하지 못하게 하는 서버 쿨다운입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Throw", meta=(ClampMin="0.0", Units="s"))
	float Cooldown = 0.5f;

	/** 던진 유물이 충분한 속도로 캐릭터에 충돌하면 넉백 효과를 적용합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Throw|Knockback")
	bool bCanKnockbackCharacters = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Throw|Knockback")
	TSubclassOf<UGameplayEffect> KnockbackEffectClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Throw|Knockback",
		meta=(ClampMin="0.0", Units="cm/s"))
	float KnockbackStrength = 1000.0f;

	/** 충돌 지점에서 측정한 유물의 수평 속도가 이 값 이상일 때만 넉백합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Throw|Knockback",
		meta=(ClampMin="0.0", Units="cm/s"))
	float MinimumKnockbackSpeed = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Throw|Effects",
		meta=(Categories="GameplayCue"))
	FGameplayTag ImpactCueTag;
};

/** 잡고 RelicUseAction을 누르면 유물 자체를 회전시키며 던지는 컴포넌트입니다. */
UCLASS(ClassGroup=(Relic), meta=(BlueprintSpawnableComponent))
class NOPHOTOS_API UNPThrowableRelicComponent : public UNPUsableRelicComponent
{
	GENERATED_BODY()

public:
	UNPThrowableRelicComponent();

	const FNPRelicThrowSettings& GetThrowSettings() const { return ThrowSettings; }
	bool CanThrow(const ANPStablePhysicsPawn* ThrowerPawn) const;

	/** 서버에서 그랩을 해제하고 선속도와 각속도를 적용합니다. */
	bool TryThrow(ANPStablePhysicsPawn* ThrowerPawn);

	static FVector CalculateThrowVelocity(
		const FVector& ForwardDirection,
		const FVector& ThrowerVelocity,
		const FNPRelicThrowSettings& Settings);
	static FVector CalculateAngularVelocityDegrees(
		const FTransform& RelicTransform,
		const FNPRelicThrowSettings& Settings);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPrimitiveComponent* ResolveRelicMesh() const;
	void BeginFlight(UPrimitiveComponent* RelicMesh);
	void EndFlight(bool bPlayImpactSound, const FVector& ImpactLocation = FVector::ZeroVector);
	void HandleFlightTimeout();
	void StopFlyingAudio(bool bFadeOut);
	void TryApplyKnockback(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		const FHitResult& Hit);

	UFUNCTION()
	void HandleThrownRelicHit(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		FVector NormalImpulse,
		const FHitResult& Hit);

	void HandleRelicGrabStarted(UPrimitiveComponent* GrabbedComponent);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastBeginPawnCollisionGrace(float Duration);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastBeginFlyingAudio();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastEndFlyingAudio(bool bPlayImpactSound, FVector_NetQuantize10 ImpactLocation);

	void RestorePawnCollision();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Relic Ability", meta=(AllowPrivateAccess="true"))
	FNPRelicThrowSettings ThrowSettings;

	/** 반복 재생이 활성화된 Sound Cue/MetaSound를 지정합니다. 첫 유효 충돌이나 재잡기까지 유물을 따라갑니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Relic Ability|Throw Audio", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USoundBase> FlyingLoopSound;

	/** 투척 후 첫 유효 충돌 위치에서 한 번 재생할 사운드입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Relic Ability|Throw Audio", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USoundBase> ImpactSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Relic Ability|Throw Audio", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USoundAttenuation> ThrowSoundAttenuation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Relic Ability|Throw Audio",
		meta=(AllowPrivateAccess="true", ClampMin="0.0", Units="s"))
	float FlyingSoundFadeOutDuration = 0.08f;

	/** 충돌 없이 맵 밖으로 떨어질 때 비행음이 영구 재생되지 않게 하는 제한시간입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Relic Ability|Throw Audio",
		meta=(AllowPrivateAccess="true", ClampMin="0.1", Units="s"))
	float MaximumFlyingSoundDuration = 10.0f;

	TWeakObjectPtr<UPrimitiveComponent> CollisionGraceMesh;
	TEnumAsByte<ECollisionResponse> PreviousPawnCollisionResponse = ECR_Block;
	TEnumAsByte<ECollisionResponse> PreviousPhysicsBodyCollisionResponse = ECR_Block;
	bool bPawnCollisionGraceApplied = false;
	bool bPhysicsBodyCollisionGraceApplied = false;
	FTimerHandle CollisionGraceTimer;

	TWeakObjectPtr<UPrimitiveComponent> FlightHitMesh;
	bool bPreviousFlightNotifyRigidBodyCollision = false;
	bool bFlightActive = false;
	FTimerHandle FlightTimeoutTimer;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> ActiveFlyingAudio;

	TWeakObjectPtr<AActor> ThrowInstigator;
	TWeakObjectPtr<UAbilitySystemComponent> ThrowSourceAbilitySystem;

	double NextThrowAllowedTime = 0.0;
};
