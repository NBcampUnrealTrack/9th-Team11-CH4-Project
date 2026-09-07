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

	/** 각 투하 지점을 중심으로 선물이 흩어지는 수평 원형 반경입니다. 0이면 경로 바로 아래에 투하합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gift Drops", meta=(ClampMin="0.0", Units="cm"))
	float RandomDropRadius = 500.0f;

	bool IsValid() const
	{
		return Count >= 0 && Count <= 128 && FMath::IsFinite(StartProgress) && FMath::IsFinite(EndProgress)
			&& FMath::IsFinite(RandomDropRadius) && RandomDropRadius >= 0.0f
			&& StartProgress >= 0.0f && EndProgress < 1.0f && StartProgress <= EndProgress
			&& (Count <= 1 || StartProgress < EndProgress);
	}

	/** 서버에서 전달한 두 난수(0~1)로 원 내부에 균일한 수평 오프셋을 계산합니다. */
	FVector GetRandomDropOffset(float AngleSample, float RadiusSample) const
	{
		if (!FMath::IsFinite(RandomDropRadius) || RandomDropRadius <= 0.0f)
		{
			return FVector::ZeroVector;
		}
		const float Angle = FMath::Clamp(AngleSample, 0.0f, 1.0f) * UE_TWO_PI;
		const float Radius = FMath::Sqrt(FMath::Clamp(RadiusSample, 0.0f, 1.0f)) * RandomDropRadius;
		return FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.0f);
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

/** 착지와 잡기에 의한 개봉 시작을 함께 복제해 중도 접속에도 대기/개봉 상태를 복원합니다. */
USTRUCT()
struct FNPSantaGiftLandingState
{
	GENERATED_BODY()

	UPROPERTY()
	bool bLanded = false;
	UPROPERTY()
	bool bOpening = false;
	UPROPERTY()
	float OpeningServerTime = 0.0f;
	UPROPERTY()
	float ServerTime = 0.0f;
	UPROPERTY()
	FVector Location = FVector::ZeroVector;
	UPROPERTY()
	FRotator Rotation = FRotator::ZeroRotator;
};
