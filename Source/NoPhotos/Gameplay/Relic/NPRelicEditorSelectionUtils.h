#pragma once

#include "CoreMinimal.h"

class ANPBaseRelic;

namespace NPRelicEditorSelectionUtils
{
	/** Content Browser에서 선택한 유효한 유물 BP 클래스를 배열에 중복 없이 추가합니다. */
	NOPHOTOS_API int32 AppendSelectedRelicClasses(
		UObject* EditedObject,
		TArray<TSubclassOf<ANPBaseRelic>>& InOutRelicClasses);
}
