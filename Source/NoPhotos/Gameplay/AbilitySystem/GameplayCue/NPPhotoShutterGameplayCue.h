#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "NPPhotoShutterGameplayCue.generated.h"

/**
 * 서버가 승인한 사진 촬영 순간에 실행되는 일회성 Cue입니다.
 * Blueprint 자식의 OnExecute에서 Parameters.Location을 사용해 3D 셔터음과 연출을 구성합니다.
 */
UCLASS(Blueprintable)
class NOPHOTOS_API UNPPhotoShutterGameplayCue : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	UNPPhotoShutterGameplayCue();

protected:
	/** Blueprint Override에서 Parent를 호출한 뒤 셔터 사운드/연출을 재생합니다. */
	virtual bool OnExecute_Implementation(
		AActor* Target,
		const FGameplayCueParameters& Parameters) const override;
};
