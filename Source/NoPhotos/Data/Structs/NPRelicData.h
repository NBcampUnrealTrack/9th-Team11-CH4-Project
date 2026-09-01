#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "NPRelicData.generated.h"

class ANPBaseRelic;

USTRUCT(BlueprintType)
struct NOPHOTOS_API FNPRelicTableRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Relic")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Relic", meta=(MultiLine="true"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Relic", meta=(ClampMin="0"))
	int32 Price = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Relic")
	TSoftClassPtr<ANPBaseRelic> RelicClass;
};
