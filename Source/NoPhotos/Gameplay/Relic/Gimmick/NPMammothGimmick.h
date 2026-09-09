#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NPMammothGimmick.generated.h"

class UGeometryCollectionComponent;

UCLASS(Blueprintable)
class NOPHOTOS_API ANPMammothGimmick : public AActor
{
	GENERATED_BODY()

public:
	ANPMammothGimmick();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mammoth")
	void Collapse();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(NetMulticast, Reliable)
	void MulticastCollapse();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UGeometryCollectionComponent> BoneGeometry;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Mammoth")
	TArray<TObjectPtr<AActor>> PullGimmickActors;

	UFUNCTION(BlueprintImplementableEvent, Category="Mammoth")
	void OnCollapsed();

private:
	void HandlePullCompleted();
	void DisablePullActor(AActor* GimmickActor);

	UFUNCTION()
	void OnRep_DisabledPullActors();

	UPROPERTY(ReplicatedUsing=OnRep_DisabledPullActors)
	TArray<TObjectPtr<AActor>> DisabledPullActors;

	UFUNCTION()
	void OnRep_Collapsed();

	UPROPERTY(ReplicatedUsing=OnRep_Collapsed, VisibleInstanceOnly, Category="Mammoth")
	bool bCollapsed = false;

	bool bCollapseApplied = false;
	bool bGeometryInitialized = false;
};
