#pragma once

#include "Gameplay/Relic/Components/NPUsableRelicComponent.h"
#include "NPThrowableRelicComponent.generated.h"

class ANPStablePhysicsPawn;
class UPrimitiveComponent;

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

	/** 투척 직후 손/몸에 튕기는 것을 줄이기 위해 모든 Pawn 충돌을 잠시 무시하는 시간입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Throw|Collision", meta=(ClampMin="0.0", Units="s"))
	float PawnCollisionGraceTime = 0.2f;

	/** 공중에서 다시 잡았을 때 즉시 연속 투척하지 못하게 하는 서버 쿨다운입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Throw", meta=(ClampMin="0.0", Units="s"))
	float Cooldown = 0.5f;
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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPrimitiveComponent* ResolveRelicMesh() const;

	UFUNCTION(NetMulticast, Reliable)
	void MulticastBeginPawnCollisionGrace(float Duration);

	void RestorePawnCollision();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Relic Ability", meta=(AllowPrivateAccess="true"))
	FNPRelicThrowSettings ThrowSettings;

	TWeakObjectPtr<UPrimitiveComponent> CollisionGraceMesh;
	TEnumAsByte<ECollisionResponse> PreviousPawnCollisionResponse = ECR_Block;
	FTimerHandle CollisionGraceTimer;
	double NextThrowAllowedTime = 0.0;
};
