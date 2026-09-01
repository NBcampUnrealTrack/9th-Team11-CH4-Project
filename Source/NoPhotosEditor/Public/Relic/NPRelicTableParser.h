#pragma once

#include "CoreMinimal.h"
#include "GoogleSheetParserBase.h"
#include "NPRelicTableParser.generated.h"

class UDataTable;

UCLASS()
class NOPHOTOSEDITOR_API UNPRelicTableParser : public UGoogleSheetParserBase
{
	GENERATED_BODY()

public:
	UNPRelicTableParser();

protected:
	virtual bool OnParseComplete(FString& OutError) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Relic|Sheet")
	TObjectPtr<UDataTable> TargetTable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Relic|Sheet", meta = (ContentDir))
	FDirectoryPath RelicClassFolderPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Relic|Sheet")
	FString RelicClassNameFormat = TEXT("BP_{0}");
};
