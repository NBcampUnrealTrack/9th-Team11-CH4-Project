#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NPWallLever.generated.h"

class UGrabbableComponent;
class UPhysicsConstraintComponent;
class UPrimitiveComponent;
class USceneComponent;
class UStaticMeshComponent;
class FLifetimeProperty;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWallLeverActivated);

UCLASS(Blueprintable)
class NOPHOTOS_API ANPWallLever : public AActor
{
	GENERATED_BODY()

public:
	ANPWallLever();

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category="Wall Lever")
	bool IsActivated() const { return bIsActivated; }

	UPROPERTY(BlueprintAssignable, Category="Wall Lever")
	FOnWallLeverActivated OnLeverActivated;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UStaticMeshComponent> BaseMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UStaticMeshComponent> HandleMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UPhysicsConstraintComponent> HandleConstraint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UGrabbableComponent> GrabbableComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Wall Lever|Angle", meta=(Units="deg"))
	float InitialAngle = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Wall Lever|Angle", meta=(Units="deg"))
	float MinimumAngle = -35.0f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category="Wall Lever|Angle",
		meta=(ClampMax="0.0", Units="deg"))
	float ActivationAngle = -30.0f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category="Wall Lever|Physics",
		meta=(ClampMin="0.0"))
	float HandleAngularDamping = 8.0f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category="Wall Lever|Physics",
		meta=(ClampMin="1.0", Units="deg/s"))
	float MaxHandleAngularSpeed = 25.0f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category="Wall Lever|Physics",
		meta=(ClampMin="0.0"))
	float LeverAngularResistance = 25000.0f;

private:
	void ConfigureConstraint();
	void HandleGrabStarted(UPrimitiveComponent* GrabbedComponent);
	float GetLeverAngle() const;
	void ActivateLever(float CurrentAngle);
	void ApplyHandleAngle(float Angle);
	void BroadcastActivationOnce();

	UFUNCTION()
	void OnRep_LeverState();

	UPROPERTY(ReplicatedUsing=OnRep_LeverState)
	float ReplicatedLeverAngle = 35.0f;

	UPROPERTY(ReplicatedUsing=OnRep_LeverState)
	bool bIsActivated = false;

	FVector HandleRelativeLocation = FVector::ZeroVector;
	FQuat HandleZeroRelativeRotation = FQuat::Identity;
	FQuat InitialHandleWorldRotation = FQuat::Identity;
	bool bActivationEventBroadcast = false;
	bool bHasBeenGrabbed = false;
};
