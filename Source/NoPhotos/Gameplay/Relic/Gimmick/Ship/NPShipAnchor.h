#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Relic/Gimmick/Ship/NPShipGimmickBase.h"
#include "NPShipAnchor.generated.h"

class UCableComponent;

USTRUCT()
struct FNPShipAnchorState
{
	GENERATED_BODY()

	UPROPERTY()
	bool bPlaced = false;

	UPROPERTY()
	FTransform Transform = FTransform::Identity;

	UPROPERTY()
	uint32 Revision = 0;
};

UCLASS(Blueprintable)
class NOPHOTOS_API ANPShipAnchor : public ANPShipGimmickBase
{
	GENERATED_BODY()

public:
	ANPShipAnchor();

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Ship Anchor")
	TObjectPtr<AActor> AnchorZone;
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Ship Anchor")
	TObjectPtr<AActor> RopeStart;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ship Anchor")
	FName SnapPointComponentName = TEXT("SnapPoint");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ship Anchor")
	FName TargetVisualComponentName = TEXT("StaticMesh");

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ship Anchor")
	TObjectPtr<UCableComponent> AnchorRope;

	UFUNCTION(BlueprintPure, Category="Ship Anchor")
	bool IsAnchorPlaced() const { return AnchorState.bPlaced; }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Ship Anchor")
	void PlaceAnchor(const FTransform& SnapTransform);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	void HandleAnchorGrabChanged(bool bIsGrabbed);
	UFUNCTION()
	void HandleZoneOverlap(AActor* OverlappedActor, AActor* OtherActor);

	UFUNCTION()
	void OnRep_AnchorState();
	UFUNCTION()
	void OnRep_ShowTarget();

	void ApplyAnchorState();
	void SetTargetVisible(bool bVisible);
	void ConfigureRope();
	USceneComponent* FindZoneComponent(FName ComponentName) const;

	UPROPERTY(ReplicatedUsing=OnRep_AnchorState)
	FNPShipAnchorState AnchorState;

	UPROPERTY(ReplicatedUsing=OnRep_ShowTarget)
	bool bShowTarget = false;
	FTransform InitialMeshTransform = FTransform::Identity;
	bool bChangingState = false;
};
