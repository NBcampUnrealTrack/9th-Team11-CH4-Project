#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Map/Trap/NPStairTrapBase.h"
#include "NPPendulumBladeTrap.generated.h"

class UArrowComponent;
class UBoxComponent;
class UNPTrapKnockbackComponent;
class USceneComponent;
class UStaticMeshComponent;

/** 서버 시간 기반으로 왕복 회전하며 고정 화살표 방향으로 Pawn을 밀어내는 칼날 함정입니다. */
UCLASS(Blueprintable)
class NOPHOTOS_API ANPPendulumBladeTrap : public ANPStairTrapBase
{
	GENERATED_BODY()

public:
	ANPPendulumBladeTrap();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void HandleTrapStateChanged(
		ENPStairTrapState PreviousState,
		ENPStairTrapState NewState) override;

	/** 진자 회전만 담당하며 넉백 방향에는 영향을 주지 않습니다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pendulum Blade Trap")
	TObjectPtr<USceneComponent> PendulumPivotComponent;

	/** 서버에서 Pawn 겹침을 판정하는 단순 충돌체입니다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pendulum Blade Trap")
	TObjectPtr<UBoxComponent> BladeCollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pendulum Blade Trap")
	TObjectPtr<UStaticMeshComponent> BladeMeshComponent;

	/** 회전 Pivot과 무관하게 고정되며 에디터에서 넉백 방향을 지정합니다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pendulum Blade Trap")
	TObjectPtr<UArrowComponent> KnockbackDirectionArrow;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pendulum Blade Trap")
	TObjectPtr<UNPTrapKnockbackComponent> KnockbackComponent;

	/** Pivot의 로컬 공간에서 회전할 축입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pendulum Blade Trap|Movement")
	FVector LocalSwingAxis = FVector::ForwardVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pendulum Blade Trap|Movement",
		meta=(ClampMin="0.0", ClampMax="179.0", Units="deg"))
	float MaximumSwingAngle = 60.0f;

	/** 한쪽 끝에서 반대쪽 끝을 거쳐 원래 위치로 돌아오는 시간입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pendulum Blade Trap|Movement",
		meta=(ClampMin="0.05", Units="s"))
	float SwingPeriod = 2.0f;

	/** 여러 칼날의 움직임을 서로 어긋나게 만드는 시작 위상입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pendulum Blade Trap|Movement",
		meta=(Units="deg"))
	float PhaseOffsetDegrees = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pendulum Blade Trap|Movement",
		meta=(ClampMin="0.01", Units="s"))
	float ReturnToRestDuration = 0.5f;

private:
	void UpdateBladePose();
	void QueryBladeSweep(
		const FVector& StartWorldLocation,
		const FVector& EndWorldLocation);
	void TryKnockbackActor(AActor* OtherActor, const FHitResult& Hit);
	void SetDamageCollisionEnabled(bool bEnabled);

	FRotator RestRelativeRotation = FRotator::ZeroRotator;
	FRotator ReturnStartRelativeRotation = FRotator::ZeroRotator;
};
