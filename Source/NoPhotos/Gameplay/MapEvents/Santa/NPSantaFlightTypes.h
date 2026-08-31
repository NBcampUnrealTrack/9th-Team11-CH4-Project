#pragma once

#include "CoreMinimal.h"
#include "NPSantaFlightTypes.generated.h"

namespace NPSantaFlight
{
	/** 거리/높이는 월드 cm. 액터 Scale, Pitch, Roll은 경로 계산에 사용하지 않습니다. */
	inline bool BuildStraightPath(const FTransform& RouteTransform, float Height, float Distance,
		FVector& OutStart, FVector& OutEnd)
	{
		OutStart = OutEnd = FVector::ZeroVector;
		if (RouteTransform.ContainsNaN() || !FMath::IsFinite(Height) || Height < 0.0f
			|| !FMath::IsFinite(Distance) || Distance <= KINDA_SMALL_NUMBER)
		{
			return false;
		}
		const FVector Center = RouteTransform.GetLocation() + FVector::UpVector * Height;
		const FVector Direction = FRotator(0.0f, RouteTransform.Rotator().Yaw, 0.0f).Vector();
		OutStart = Center - Direction * (Distance * 0.5f);
		OutEnd = Center + Direction * (Distance * 0.5f);
		return !OutStart.ContainsNaN() && !OutEnd.ContainsNaN() && !OutStart.Equals(OutEnd);
	}
}

/** 시작 시 한 번 확정해 복제합니다. 클라이언트는 경로 액터/위치 레벨을 참조하지 않습니다. */
USTRUCT(BlueprintType)
struct NOPHOTOS_API FNPSantaFlightPlan
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Santa Flight")
	FVector StartLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category="Santa Flight")
	FVector EndLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category="Santa Flight")
	float StartServerTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Santa Flight")
	float Duration = 0.0f;

	bool IsValid() const
	{
		return !StartLocation.ContainsNaN() && !EndLocation.ContainsNaN()
			&& !StartLocation.Equals(EndLocation) && FMath::IsFinite(StartServerTime)
			&& FMath::IsFinite(Duration) && Duration >= 0.01f;
	}

	float GetProgress(float ServerTime) const
	{
		return IsValid() && FMath::IsFinite(ServerTime)
			? FMath::Clamp((ServerTime - StartServerTime) / Duration, 0.0f, 1.0f) : 0.0f;
	}

	FTransform GetTransform(float Progress) const
	{
		if (!IsValid())
		{
			return FTransform::Identity;
		}
		const float Alpha = FMath::IsFinite(Progress) ? FMath::Clamp(Progress, 0.0f, 1.0f) : 0.0f;
		return FTransform((EndLocation - StartLocation).Rotation(), FMath::Lerp(StartLocation, EndLocation, Alpha));
	}
};
