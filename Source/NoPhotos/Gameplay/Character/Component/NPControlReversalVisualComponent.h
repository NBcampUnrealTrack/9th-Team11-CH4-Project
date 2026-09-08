#pragma once

#include "CoreMinimal.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "NPControlReversalVisualComponent.generated.h"

/** 조작 반전 태그가 있는 동안 유령 네 개가 캐릭터 주변을 선회합니다. */
UCLASS(ClassGroup=(Effects), meta=(BlueprintSpawnableComponent))
class NOPHOTOS_API UNPControlReversalVisualComponent : public UInstancedStaticMeshComponent
{
	GENERATED_BODY()

public:
	UNPControlReversalVisualComponent();
	void SetVisualActive(bool bActive);
	virtual void OnRegister() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Control Reversal|Visual",
		meta=(DisplayName="캐릭터 중심 거리", ClampMin="1.0", Units="cm",
			ToolTip="유령이 도는 수평 반경입니다. 값을 줄이면 캐릭터에 가까워집니다."))
	float OrbitRadius = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Control Reversal|Visual", meta=(Units="cm"))
	float OrbitHeight = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Control Reversal|Visual", meta=(ClampMin="0.0", Units="deg/s"))
	float OrbitSpeed = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Control Reversal|Visual",
		meta=(DisplayName="상하 진폭", ClampMin="0.0", Units="cm",
			ToolTip="중심 높이에서 위아래로 움직이는 최대 거리입니다. 0이면 높이가 고정됩니다."))
	float BobAmplitude = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Control Reversal|Visual")
	FVector GhostScale = FVector(0.25f);

	/** 메시의 정면이 +X가 아닌 경우 진행 방향에 맞추기 위한 보정입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Control Reversal|Visual",
		meta=(DisplayName="유령 메시 회전 보정",
			ToolTip="진행 방향에 더할 메시 회전입니다. 옆을 바라보면 Yaw를 90도 또는 -90도로 보정하세요."))
	FRotator GhostRotationOffset = FRotator::ZeroRotator;

private:
	void UpdateGhostTransforms();

	float OrbitAngle = 0.0f;
};
