#pragma once

#include "CoreMinimal.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "NPPhotoStunVisualComponent.generated.h"

/**
 * 증거 사진 스턴 중 캐릭터 머리 위에 표시할 가벼운 로컬 연출입니다.
 * 네트워크 상태를 직접 복제하지 않고, 복제된 스턴 상태에 따라 각 클라이언트에서 표시되고 회전합니다.
 */
UCLASS(ClassGroup=(Photo), meta=(BlueprintSpawnableComponent))
class NOPHOTOS_API UNPPhotoStunVisualComponent
	: public UInstancedStaticMeshComponent
{
	GENERATED_BODY()

public:
	UNPPhotoStunVisualComponent();

	virtual void OnRegister() override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** 스턴 연출의 표시와 로컬 회전을 시작하거나 종료합니다. */
	UFUNCTION(BlueprintCallable, Category="Photo|Stun Visual")
	void SetStunVisualActive(bool bActive);

	UFUNCTION(BlueprintPure, Category="Photo|Stun Visual")
	bool IsStunVisualActive() const { return bStunVisualActive; }

protected:
	/** 중심에서 각 표시물까지의 거리입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Photo|Stun Visual",
		meta=(ClampMin="0.0", Units="cm"))
	float OrbitRadius = 28.0f;

	/** 기본 큐브 메시에 적용할 크기입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Photo|Stun Visual")
	FVector MarkerScale = FVector(0.12f);

	/** 로컬의 초당 회전 각도입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Photo|Stun Visual",
		meta=(Units="deg/s"))
	float RotationSpeedDegrees = 180.0f;

private:
	void RebuildMarkerInstances();

	bool bStunVisualActive = false;
};
