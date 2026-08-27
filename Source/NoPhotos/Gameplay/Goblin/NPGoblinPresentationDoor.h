#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NPGoblinPresentationDoor.generated.h"

class USceneComponent;
class UStaticMeshComponent;

/** 바닥 중앙이 원점이고 +X가 문 바깥 방향인, 충돌 없는 고블린 연출용 문입니다. */
UCLASS(Blueprintable)
class NOPHOTOS_API ANPGoblinPresentationDoor : public AActor
{
	GENERATED_BODY()

public:
	ANPGoblinPresentationDoor();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 서버 전용. 닫기 연출 후 문만 독립적으로 제거합니다. 여러 번 호출해도 닫기를 다시 시작하지 않습니다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Goblin|Door")
	void CloseAndDestroy();
	float GetOpenDuration() const { return FMath::Max(0.01f, OpenDuration); }
	float GetCloseDuration() const { return FMath::Max(0.01f, CloseDuration); }
	FVector GetClearanceHalfExtent() const
	{
		const float Thickness = FMath::Max(1.0f, FrameThickness);
		return FVector(Thickness + 2.0f, FMath::Max(50.0f, OpeningWidth) * 0.5f + Thickness,
			(FMath::Max(50.0f, OpeningHeight) + Thickness) * 0.5f);
	}

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Goblin|Door")
	TObjectPtr<USceneComponent> DoorRoot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Goblin|Door")
	TObjectPtr<USceneComponent> Hinge;
	/** 기본 문짝은 프로젝트의 LevelPrototyping/Interactable/Door/Meshes/SM_Door입니다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Goblin|Door")
	TObjectPtr<UStaticMeshComponent> DoorMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Goblin|Door")
	TObjectPtr<UStaticMeshComponent> Interior;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Goblin|Door")
	TObjectPtr<UStaticMeshComponent> LeftFrame;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Goblin|Door")
	TObjectPtr<UStaticMeshComponent> RightFrame;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Goblin|Door")
	TObjectPtr<UStaticMeshComponent> TopFrame;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Goblin|Door", meta = (ClampMin = "50", Units = "cm"))
	float OpeningWidth = 140.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Goblin|Door", meta = (ClampMin = "50", Units = "cm"))
	float OpeningHeight = 220.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Goblin|Door", meta = (ClampMin = "1", Units = "cm"))
	float FrameThickness = 12.0f;
	/** 문짝을 지정된 폭/높이에 맞추고 얇은 XY축을 문 앞쪽으로 정렬합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Goblin|Door")
	bool bAutoFitDoorMesh = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Goblin|Door", meta = (Units = "deg"))
	float OpenAngle = -110.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Goblin|Door", meta = (ClampMin = "0.01", Units = "s"))
	float OpenDuration = 0.6f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Goblin|Door", meta = (ClampMin = "0.01", Units = "s"))
	float CloseDuration = 0.5f;

private:
	double GetPresentationTime() const;
	float GetOpenAlpha() const;
	// Replicate the animation clock, not a stepped rotation, so each client animates smoothly.
	UPROPERTY(Replicated)
	double AnimationStartTime = 0.0;
	UPROPERTY(Replicated)
	float StartOpenAlpha = 0.0f;
	UPROPERTY(Replicated)
	bool bClosing = false;
};
