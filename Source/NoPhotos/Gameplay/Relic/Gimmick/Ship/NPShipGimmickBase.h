#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NPShipGimmickBase.generated.h"

class UGrabbableComponent;
class UNPShipGimmickComponent;
class UPhysicsConstraintComponent;
class UPrimitiveComponent;
class USceneComponent;
class UStaticMeshComponent;
class ANPShipGimmickBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnShipGimmickGrabStateChanged,
	bool,
	bIsGrabbed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnShipGimmickActivated, ANPShipGimmickBase*, Gimmick);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnShipGimmickCompleted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnShipGimmickReset);

UCLASS(Abstract, Blueprintable)
class NOPHOTOS_API ANPShipGimmickBase : public AActor
{
	GENERATED_BODY()

public:
	ANPShipGimmickBase();

	UFUNCTION(BlueprintPure, Category="Ship Gimmick")
	bool IsGimmickGrabbed() const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Ship Gimmick")
	void CompleteShipGimmick();

	UFUNCTION(BlueprintCallable, Category="Ship Gimmick")
	void NotifyShipGimmickActivated();

	/** Called by a sequence controller after the player uses this device out of order. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Ship Gimmick")
	void ResetShipGimmick();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Ship Gimmick|Physics")
	void SetGimmickPhysicsEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category="Ship Gimmick|Components")
	UStaticMeshComponent* GetGimmickMesh() const { return GimmickMesh; }

	UFUNCTION(BlueprintPure, Category="Ship Gimmick|Components")
	UPhysicsConstraintComponent* GetGimmickConstraint() const { return PhysicsConstraint; }

	UFUNCTION(BlueprintPure, Category="Ship Gimmick|Components")
	UGrabbableComponent* GetGrabbableComponent() const { return GrabbableComponent; }

	UFUNCTION(BlueprintPure, Category="Ship Gimmick|Components")
	UNPShipGimmickComponent* GetGimmickComponent() const { return GimmickComponent; }

	UPROPERTY(BlueprintAssignable, Category="Ship Gimmick")
	FOnShipGimmickGrabStateChanged OnGrabStateChanged;

	UPROPERTY(BlueprintAssignable, Category="Ship Gimmick")
	FOnShipGimmickActivated OnActivated;

	UPROPERTY(BlueprintAssignable, Category="Ship Gimmick")
	FOnShipGimmickCompleted OnCompleted;

	UPROPERTY(BlueprintAssignable, Category="Ship Gimmick")
	FOnShipGimmickReset OnReset;

protected:
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintImplementableEvent, Category="Ship Gimmick")
	void ReceiveGrabStateChanged(bool bIsGrabbed);

	UFUNCTION(BlueprintImplementableEvent, Category="Ship Gimmick")
	void ReceiveShipGimmickActivated();

	UFUNCTION(BlueprintImplementableEvent, Category="Ship Gimmick")
	void ReceiveShipGimmickCompleted();

	/** Override in a device Blueprint to restore its local state (wheel flag, pull handle, anchor, etc.). */
	UFUNCTION(BlueprintImplementableEvent, Category="Ship Gimmick")
	void ReceiveShipGimmickReset();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UStaticMeshComponent> GimmickMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UPhysicsConstraintComponent> PhysicsConstraint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UGrabbableComponent> GrabbableComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UNPShipGimmickComponent> GimmickComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ship Gimmick|Physics")
	bool bStartWithPhysicsEnabled = true;

private:
	void HandleGrabStarted(UPrimitiveComponent* GrabbedComponent);
	void HandleGrabEnded();
	void HandleGimmickCompleted();
	void ConfigurePhysicsConstraint();
};
