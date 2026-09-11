#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Actor.h"
#include "NPCCTVSensor.generated.h"

class ANPStablePhysicsPawn;
class ANPCCTVMapEvent;
class UAbilitySystemComponent;
class UMaterialInstanceDynamic;
class USceneComponent;
class USpotLightComponent;
class UStaticMeshComponent;
class UNPAimableRelicComponent;

UENUM(BlueprintType)
enum class ENPCCTVSensorState : uint8
{
	Safe,
	Warning,
	Armed
};

/** 좌우로 감시하다가 사격 상태에서 움직이는 플레이어를 공격하는 CCTV입니다. */
UCLASS(Blueprintable)
class NOPHOTOS_API ANPCCTVSensor : public AActor, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ANPCCTVSensor();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintPure, Category="CCTV")
	ENPCCTVSensorState GetCurrentState() const { return CurrentState; }

	UFUNCTION(BlueprintPure, Category="CCTV")
	ANPStablePhysicsPawn* GetLockedTarget() const { return LockedTarget; }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="CCTV")
	void SetCCTVActive(bool bNewActive);

	UFUNCTION(BlueprintPure, Category="CCTV")
	bool IsCCTVActive() const { return bCCTVActive; }

	void SetOwningMapEvent(ANPCCTVMapEvent* InOwningMapEvent);

protected:
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintImplementableEvent, DisplayName="On CCTV State Changed")
	void BP_OnStateChanged(ENPCCTVSensorState NewState);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UStaticMeshComponent> MountMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USceneComponent> SensorYawPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UStaticMeshComponent> CameraMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UStaticMeshComponent> StatusIndicatorMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USpotLightComponent> SensorLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UStaticMeshComponent> BeamMesh;

	/** 기존 빔 메시의 빈 중심을 덮는 보조 콘 메시입니다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UStaticMeshComponent> BeamFillMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USceneComponent> GunAimPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UStaticMeshComponent> GunMesh;

	/** 기존 조준 총 유물과 같은 발사 설정, Gameplay Cue, Gameplay Effect를 사용합니다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UNPAimableRelicComponent> GunFireComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UAbilitySystemComponent> AbilitySystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="CCTV|State", meta=(ClampMin="0.1", Units="s"))
	float SafeDuration = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="CCTV|State", meta=(ClampMin="0.1", Units="s"))
	float WarningDuration = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="CCTV|State", meta=(ClampMin="0.1", Units="s"))
	float ArmedDuration = 4.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="CCTV|State", meta=(ClampMin="0.0", Units="s"))
	float StateFadeDuration = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="CCTV|State", meta=(ClampMin="0.05", Units="s"))
	float WarningBlinkInterval = 0.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="CCTV|Sweep", meta=(ClampMin="0.0", ClampMax="80.0", Units="deg"))
	float SweepAngle = 35.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="CCTV|Sweep", meta=(ClampMin="0.1", Units="s"))
	float SweepPeriod = 8.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="CCTV|Sweep", meta=(ClampMin="0.0"))
	float RotationInterpSpeed = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="CCTV|Detection", meta=(ClampMin="0.02", Units="s"))
	float DetectionInterval = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="CCTV|Detection", meta=(ClampMin="0.0", Units="cm/s"))
	float LinearMovementThreshold = 25.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="CCTV|Detection", meta=(ClampMin="0.0", Units="deg/s"))
	float AngularMovementThreshold = 20.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="CCTV|Detection", meta=(ClampMin="0.0", Units="s"))
	float MovementConfirmDuration = 0.15f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="CCTV|Detection", meta=(ClampMin="0.0", Units="s"))
	float ArmedEntryGraceDuration = 0.3f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="CCTV|Detection")
	TEnumAsByte<ECollisionChannel> DetectionTraceChannel = ECC_Visibility;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="CCTV|Fire", meta=(ClampMin="0.0", Units="s"))
	float FireWindupDuration = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="CCTV|Visual")
	FLinearColor SafeColor = FLinearColor(0.05f, 1.0f, 0.12f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="CCTV|Visual")
	FLinearColor WarningColor = FLinearColor(1.0f, 0.22f, 0.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="CCTV|Visual")
	FLinearColor ArmedColor = FLinearColor(1.0f, 0.0f, 0.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="CCTV|Visual")
	FName ColorParameterName = TEXT("Color");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="CCTV|Visual")
	FName OpacityParameterName = TEXT("Opacity");

private:
	void EnterState(ENPCCTVSensorState NewState, float ServerTime);
	void UpdateAuthority(float ServerTime);
	void UpdateDetection(float ServerTime);
	void LockTarget(ANPStablePhysicsPawn* NewTarget, float ServerTime);
	void ClearTarget();
	void FireAtLockedTarget(float ServerTime);
	void UpdateRotation(float DeltaSeconds, float ServerTime);
	void UpdatePresentation(float ServerTime);
	bool IsPawnVisible(const ANPStablePhysicsPawn* Pawn) const;
	bool IsPawnMoving(const ANPStablePhysicsPawn* Pawn) const;
	FVector GetCurrentTargetLocation(const ANPStablePhysicsPawn* Pawn) const;
	float GetServerTime() const;
	FLinearColor GetStateColor() const;

	UFUNCTION()
	void OnRep_CurrentState();

	UFUNCTION()
	void OnRep_CCTVActive();

	void ApplyCCTVActiveState();

	UPROPERTY(ReplicatedUsing=OnRep_CCTVActive)
	bool bCCTVActive = false;

	UPROPERTY(ReplicatedUsing=OnRep_CurrentState)
	ENPCCTVSensorState CurrentState = ENPCCTVSensorState::Safe;

	UPROPERTY(Replicated)
	float StateStartServerTime = 0.0f;

	UPROPERTY(Replicated)
	float StateEndServerTime = 0.0f;

	UPROPERTY(Replicated)
	float SweepStartServerTime = 0.0f;

	UPROPERTY(Replicated)
	TObjectPtr<ANPStablePhysicsPawn> LockedTarget;

	UPROPERTY(Replicated)
	float TargetLockStartServerTime = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BeamMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BeamFillMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> StatusMaterial;

	TWeakObjectPtr<ANPCCTVMapEvent> OwningMapEvent;

	TMap<TWeakObjectPtr<ANPStablePhysicsPawn>, float> MovingDurations;
	FRotator InitialSensorRotation = FRotator::ZeroRotator;
	FRotator InitialGunRotation = FRotator::ZeroRotator;
	float InitialLightIntensity = 0.0f;
	float LastDetectionServerTime = 0.0f;
	float NextTargetScanServerTime = 0.0f;
};
