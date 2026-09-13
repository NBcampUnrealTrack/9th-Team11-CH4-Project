#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NPHotPotatoBomb.generated.h"

class UNiagaraSystem;
class USceneComponent;
class USoundBase;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FNPHotPotatoBombExploded,
	FVector,
	ExplosionLocation);

/** 맵 이벤트가 수명과 판정을 관리하는 폭탄 돌리기 전용 표시 액터입니다. */
UCLASS(Blueprintable)
class NOPHOTOS_API ANPHotPotatoBomb : public AActor
{
	GENERATED_BODY()

public:
	ANPHotPotatoBomb();

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 서버가 결정한 폭발 시각을 주입합니다. 폭탄 자체는 타이머를 소유하지 않습니다. */
	void InitializeFuse(float InExplosionServerWorldTime);

	/** 서버에서 한 번 호출하여 상태 복제와 모든 클라이언트의 폭발 연출을 실행합니다. */
	void TriggerExplosion();

	UFUNCTION(BlueprintPure, Category="Hot Potato Bomb")
	float GetRemainingFuseTime() const;

	UFUNCTION(BlueprintPure, Category="Hot Potato Bomb")
	bool HasExploded() const { return bHasExploded; }

	UPROPERTY(BlueprintAssignable, Category="Hot Potato Bomb|Presentation")
	FNPHotPotatoBombExploded OnBombExploded;

protected:
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic,
		Category="Hot Potato Bomb|Presentation", meta=(DisplayName="폭탄 활성화"))
	void BP_OnBombArmed(float InArmedExplosionServerWorldTime);

	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic,
		Category="Hot Potato Bomb|Presentation", meta=(DisplayName="폭탄 폭발"))
	void BP_OnBombExploded(FVector ExplosionLocation);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hot Potato Bomb")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hot Potato Bomb")
	TObjectPtr<UStaticMeshComponent> BombMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hot Potato Bomb|Presentation")
	TObjectPtr<UNiagaraSystem> ExplosionSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hot Potato Bomb|Presentation")
	TObjectPtr<USoundBase> ExplosionSound;

	/** 폭발 RPC 전달을 위해 숨겨진 폭탄 액터를 잠시 유지하는 시간입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hot Potato Bomb|Presentation",
		meta=(ClampMin="0.1", Units="s"))
	float DestroyDelayAfterExplosion = 2.0f;

private:
	void ApplyExplodedState();
	void PlayExplosionPresentation(const FVector& ExplosionLocation);

	UFUNCTION()
	void OnRep_HasExploded();

	UFUNCTION()
	void OnRep_ExplosionServerWorldTime();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastExplosion(FVector_NetQuantize ExplosionLocation);

	UPROPERTY(ReplicatedUsing=OnRep_HasExploded)
	bool bHasExploded = false;

	UPROPERTY(ReplicatedUsing=OnRep_ExplosionServerWorldTime)
	float ExplosionServerWorldTime = 0.0f;

	bool bExplosionPresentationPlayed = false;
};
