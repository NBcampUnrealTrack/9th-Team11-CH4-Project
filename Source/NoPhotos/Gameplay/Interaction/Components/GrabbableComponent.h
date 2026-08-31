#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GrabbableComponent.generated.h"

class UPrimitiveComponent;
class FLifetimeProperty;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnGrabStarted, UPrimitiveComponent*);
DECLARE_MULTICAST_DELEGATE_ThreeParams(
	FOnGrabForceUpdated,
	const FVector&,
	const FVector&,
	float);
DECLARE_MULTICAST_DELEGATE(FOnGrabEnded);
DECLARE_MULTICAST_DELEGATE(FOnForceReleaseAllGrabs);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnActiveGrabCountChanged, int32);

UCLASS(ClassGroup=(Interaction), meta=(BlueprintSpawnableComponent))
class NOPHOTOS_API UGrabbableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGrabbableComponent();
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category="Interaction")
	bool CanBeGrabbed() const
	{
		return bGrabEnabled && !bAdditionalGrabLocked;
	}
	bool CanApplyReplicatedGrab() const { return bGrabEnabled; }

	UFUNCTION(BlueprintPure, Category="Interaction")
	bool IsGrabbed() const { return bIsGrabbed; }

	int32 GetActiveGrabCount() const { return ActiveGrabCount; }

	UFUNCTION(BlueprintPure, Category="Interaction")
	FVector GetCurrentLinearGrabForce() const { return CurrentLinearGrabForce; }

	UFUNCTION(BlueprintPure, Category="Interaction")
	FVector GetCurrentAngularGrabForce() const { return CurrentAngularGrabForce; }

	UFUNCTION(BlueprintCallable, Category="Interaction")
	void SetGrabEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category="Interaction")
	void ForceReleaseAllGrabs();

	bool AcquireAdditionalGrabLock();
	void ReleaseAdditionalGrabLock();

	UPrimitiveComponent* ResolveGrabTarget(UPrimitiveComponent* DetectedComponent) const;

	void NotifyGrabStarted(UPrimitiveComponent* GrabbedComponent);
	void NotifyGrabForce(
		const FVector& LinearForce,
		const FVector& AngularForce,
		float IntentForceAlignment);
	void NotifyGrabEnded();

	FOnGrabStarted OnGrabStarted;
	FOnGrabForceUpdated OnGrabForceUpdated;
	FOnGrabEnded OnGrabEnded;
	FOnForceReleaseAllGrabs OnForceReleaseAllGrabs;
	FOnActiveGrabCountChanged OnActiveGrabCountChanged;

private:
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Interaction", meta=(AllowPrivateAccess="true"))
	bool bGrabEnabled = true;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Interaction", meta=(AllowPrivateAccess="true"))
	bool bAdditionalGrabLocked = false;

	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="Interaction", meta=(AllowPrivateAccess="true"))
	bool bIsGrabbed = false;

	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="Interaction", meta=(AllowPrivateAccess="true"))
	FVector CurrentLinearGrabForce = FVector::ZeroVector;

	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="Interaction", meta=(AllowPrivateAccess="true"))
	FVector CurrentAngularGrabForce = FVector::ZeroVector;

	int32 ActiveGrabCount = 0;
	int32 AdditionalGrabLockCount = 0;
};
