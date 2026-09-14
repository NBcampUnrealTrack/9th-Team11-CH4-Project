#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Map/Trap/NPStairTrapBase.h"
#include "NPWallSpearTrap.generated.h"

class UBoxComponent;
class UNPTrapKnockbackComponent;
class UStaticMeshComponent;

/** 벽에서 Local Forward 방향으로 전진했다가 복귀하는 계단 창 함정입니다. */
UCLASS(Blueprintable)
class NOPHOTOS_API ANPWallSpearTrap : public ANPStairTrapBase
{
	GENERATED_BODY()

public:
	ANPWallSpearTrap();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void HandleTrapStateChanged(
		ENPStairTrapState PreviousState,
		ENPStairTrapState NewState) override;

	/** 이동과 서버 Sweep 충돌 판정의 기준이 되는 단순 충돌체입니다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Wall Spear Trap")
	TObjectPtr<UBoxComponent> SpearCollisionComponent;

	/** 블루프린트에서 실제 창 Static Mesh를 지정합니다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Wall Spear Trap")
	TObjectPtr<UStaticMeshComponent> SpearMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Wall Spear Trap")
	TObjectPtr<UNPTrapKnockbackComponent> KnockbackComponent;

	/** 접힌 위치에서 Actor Local Forward 방향으로 이동할 거리입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Wall Spear Trap|Movement",
		meta=(ClampMin="0.0", Units="cm"))
	float ExtensionDistance = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Wall Spear Trap|Movement",
		meta=(ClampMin="0.01", Units="s"))
	float ExtensionDuration = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Wall Spear Trap|Movement",
		meta=(ClampMin="0.01", Units="s"))
	float RetractionDuration = 0.4f;

private:
	UFUNCTION()
	void HandleSpearHit(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		FVector NormalImpulse,
		const FHitResult& Hit);

	void UpdateSpearPose();
	void MoveSpearTo(const FVector& NewRelativeLocation, bool bSweepForPlayers);
	void SetDamageCollisionEnabled(bool bEnabled);

	FVector RetractedRelativeLocation = FVector::ZeroVector;
	FVector ExtendedRelativeLocation = FVector::ZeroVector;
};
