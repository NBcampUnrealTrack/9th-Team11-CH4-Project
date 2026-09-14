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

	virtual void PostInitializeComponents() override;
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

	/** 밑동에서 Actor Local Forward 방향으로 완전히 뻗었을 때의 길이입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Wall Spear Trap|Movement",
		meta=(ClampMin="0.0", Units="cm"))
	float ExtensionDistance = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Wall Spear Trap|Movement",
		meta=(ClampMin="0.01", Units="s"))
	float ExtensionDuration = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Wall Spear Trap|Movement",
		meta=(ClampMin="0.01", Units="s"))
	float RetractionDuration = 0.4f;

	/** 찌르기 초반을 빠르게 만드는 EaseOut 지수입니다. 1이면 선형입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Wall Spear Trap|Movement",
		meta=(ClampMin="1.0"))
	float ExtensionEaseExponent = 3.0f;

	/** 회수 초반을 빠르게 만드는 EaseOut 지수입니다. 1이면 선형입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Wall Spear Trap|Movement",
		meta=(ClampMin="1.0"))
	float RetractionEaseExponent = 3.0f;

private:
	UFUNCTION()
	void HandleSpearHit(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		FVector NormalImpulse,
		const FHitResult& Hit);

	void UpdateSpearPose();
	void CacheInitialSpearConfiguration();
	void ApplySpearLengthAlpha(float LengthAlpha);
	void MoveSpearTo(const FVector& NewRelativeLocation, bool bSweepForPlayers);
	void QuerySpearSweep(
		const FVector& StartWorldLocation,
		const FVector& EndWorldLocation);
	void SetDamageCollisionEnabled(bool bEnabled);

	/** 설정된 Collision 위치를 창이 시작되는 고정 밑동 위치로 사용합니다. */
	FVector SpearBaseRelativeLocation = FVector::ZeroVector;
	FVector OriginalSpearMeshRelativeLocation = FVector::ZeroVector;
	FVector OriginalSpearMeshScale = FVector::OneVector;
	FVector OriginalSpearCollisionExtent = FVector::ZeroVector;
	bool bInitialSpearConfigurationCached = false;
};
