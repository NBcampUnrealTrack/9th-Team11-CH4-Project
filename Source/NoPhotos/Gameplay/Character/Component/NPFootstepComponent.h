#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NPFootstepComponent.generated.h"

class UAnimInstance;
class USkeletalMeshComponent;
class USoundAttenuation;
class USoundBase;

UCLASS(ClassGroup=(Audio), meta=(BlueprintSpawnableComponent))
class NOPHOTOS_API UNPFootstepComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNPFootstepComponent();

	/** 일반 AnimBP 업데이트에서 한 번 호출합니다. */
	UFUNCTION(BlueprintCallable, Category="Sound|Footstep")
	void UpdateFootsteps(UAnimInstance* AnimInstance);

protected:
	UPROPERTY(EditAnywhere, Category="Sound|Footstep|Sync")
	FName SyncGroupName;

	UPROPERTY(EditAnywhere, Category="Sound|Footstep|Sync")
	FName LeftFootMarkerName = TEXT("L");

	UPROPERTY(EditAnywhere, Category="Sound|Footstep|Sync")
	FName RightFootMarkerName = TEXT("R");

	UPROPERTY(EditAnywhere, Category="Sound|Footstep")
	FName LeftFootBoneName;

	UPROPERTY(EditAnywhere, Category="Sound|Footstep")
	FName RightFootBoneName;

	UPROPERTY(EditAnywhere, Category="Sound|Footstep")
	TArray<TObjectPtr<USoundBase>> FootstepSounds;

	UPROPERTY(EditAnywhere, Category="Sound|Footstep")
	TObjectPtr<USoundAttenuation> FootstepAttenuation;

	UPROPERTY(EditAnywhere, Category="Sound|Footstep", meta=(ClampMin="0.0"))
	float LocalFootstepVolume = 1.0f;

	UPROPERTY(EditAnywhere, Category="Sound|Footstep", meta=(ClampMin="0.0"))
	float RemoteFootstepVolume = 0.4f;

	UPROPERTY(EditAnywhere, Category="Sound|Footstep", meta=(ClampMin="0.01"))
	float MinPitch = 1.0f;

	UPROPERTY(EditAnywhere, Category="Sound|Footstep", meta=(ClampMin="0.01"))
	float MaxPitch = 1.0f;

private:
	TWeakObjectPtr<USoundBase> LastFootstepSound;

	bool CanPlayFootstep() const;
	void PlayFootstep(USkeletalMeshComponent* Mesh, FName FootBoneName);
};
