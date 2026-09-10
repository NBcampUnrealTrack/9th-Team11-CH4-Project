#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "NPPhotoAimStartGameplayCue.generated.h"

/**
 * 사진 조준 Ability가 성공적으로 시작될 때 한 번 실행되는 Cue입니다.
 * Blueprint 자식의 OnExecute에서 Parameters.Location을 사용해 3D 준비음을 재생합니다.
 */
UCLASS(Blueprintable)
class NOPHOTOS_API UNPPhotoAimStartGameplayCue : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	UNPPhotoAimStartGameplayCue();

protected:
	/** Blueprint Override에서 Parent를 호출한 뒤 조준 준비음/연출을 재생합니다. */
	virtual bool OnExecute_Implementation(
		AActor* Target,
		const FGameplayCueParameters& Parameters) const override;
};
