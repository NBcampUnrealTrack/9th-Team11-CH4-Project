#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "NPPhotoReactiveTarget.generated.h"

class APlayerState;

/** 서버 사진 판정에 반응할 수 있는 액터가 구현하는 인터페이스입니다. */
UINTERFACE(BlueprintType)
class NOPHOTOS_API UNPPhotoReactiveTarget : public UInterface
{
	GENERATED_BODY()
};

class NOPHOTOS_API INPPhotoReactiveTarget
{
	GENERATED_BODY()

public:
	/** 이 플레이어가 현재 대상에 대한 촬영 보상을 받을 수 있는지 서버에서 확인합니다. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Photo|Reactive Target")
	bool CanBePhotographed(APlayerState* Photographer) const;

	/** 유효한 사진에 대상으로 잡혔을 때 서버에서 한 번 호출됩니다. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Photo|Reactive Target")
	void OnPhotographed(APlayerState* Photographer, float Visibility, int32 CaptureSequence);

	/** 기존 OnPhotographed 처리 직후 전달되는 추가 알림. 보상/대미지를 중복 처리하지 않고 방향 연출에 사용합니다. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Photo|Reactive Target")
	void OnPhotographedFromCamera(APlayerState* Photographer, float Visibility, int32 CaptureSequence,
		FVector CameraLocation, FVector CameraForward);
};
