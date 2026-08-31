#pragma once

#include "CoreMinimal.h"
#include "NPSantaGiftTypes.generated.h"

/** 전체 비행 중 선물을 떨어뜨릴 구간과 수량입니다. */
USTRUCT(BlueprintType)
struct NOPHOTOS_API FNPSantaGiftDropSchedule
{
	GENERATED_BODY()

	/** 0이면 기존처럼 비행만 합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gift Drops", meta=(ClampMin="0", ClampMax="128"))
	int32 Count = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gift Drops", meta=(ClampMin="0.0", ClampMax="0.99"))
	float StartProgress = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gift Drops", meta=(ClampMin="0.0", ClampMax="0.99"))
	float EndProgress = 0.9f;

	bool IsValid() const
	{
		return Count >= 0 && Count <= 128 && FMath::IsFinite(StartProgress) && FMath::IsFinite(EndProgress)
			&& StartProgress >= 0.0f && EndProgress < 1.0f && StartProgress <= EndProgress
			&& (Count <= 1 || StartProgress < EndProgress);
	}

	bool GetDropProgress(int32 Index, float& OutProgress) const
	{
		OutProgress = 0.0f;
		if (!IsValid() || Index < 0 || Index >= Count)
		{
			return false;
		}
		OutProgress = Count == 1 ? StartProgress
			: FMath::Lerp(StartProgress, EndProgress, static_cast<float>(Index) / (Count - 1));
		return true;
	}
};

/** 착지 상태와 시각/위치를 묶어서 복제해 중도 접속에도 개봉 진행률을 복원합니다. */
USTRUCT()
struct FNPSantaGiftLandingState
{
	GENERATED_BODY()

	UPROPERTY()
	bool bLanded = false;
	UPROPERTY()
	float ServerTime = 0.0f;
	UPROPERTY()
	FVector Location = FVector::ZeroVector;
	UPROPERTY()
	FRotator Rotation = FRotator::ZeroRotator;
};
