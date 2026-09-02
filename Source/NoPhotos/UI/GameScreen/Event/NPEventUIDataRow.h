#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "NPEventUIDataRow.generated.h"

USTRUCT(BlueprintType)
struct NOPHOTOS_API FNPEventUIDataRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event UI")
	FText EventName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event UI")
	TSoftObjectPtr<UTexture2D> LogoImage;
};