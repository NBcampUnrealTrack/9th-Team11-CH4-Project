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

/** 전체 이벤트 시간과 별개인 1회 비행 시간 및 비행 종료 후 재등장 대기 범위입니다. */
USTRUCT(BlueprintType)
struct NOPHOTOS_API FNPSantaFlightSchedule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Santa Flight", meta=(ClampMin="0.01", Units="s"))
	float FlightDuration = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Santa Flight", meta=(ClampMin="0.0", Units="s"))
	float RespawnDelayMin = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Santa Flight", meta=(ClampMin="0.0", Units="s"))
	float RespawnDelayMax = 7.0f;

	bool IsValid() const
	{
		return FMath::IsFinite(FlightDuration) && FlightDuration >= 0.01f
			&& FMath::IsFinite(RespawnDelayMin) && RespawnDelayMin >= 0.0f
			&& FMath::IsFinite(RespawnDelayMax) && RespawnDelayMax >= RespawnDelayMin;
	}

	/** 서버가 매번 새로 추첨한 0~1 값을 사용합니다. 0초 대기는 다음 틱에 처리합니다. */
	float GetRespawnDelay(float RandomFraction) const
	{
		return IsValid() && FMath::IsFinite(RandomFraction)
			? FMath::Lerp(RespawnDelayMin, RespawnDelayMax, FMath::Clamp(RandomFraction, 0.0f, 1.0f)) : 0.0f;
	}
};

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

	void SetRouteEndpoints(const FVector& RouteStart, const FVector& RouteEnd, bool bReverse)
	{
		StartLocation = bReverse ? RouteEnd : RouteStart;
		EndLocation = bReverse ? RouteStart : RouteEnd;
	}

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
