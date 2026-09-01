#include "Relic/NPRelicTableParser.h"

#include "Data/Structs/NPRelicData.h"
#include "DataTableEditorUtils.h"
#include "Engine/DataTable.h"
#include "Gameplay/Relic/NPBaseRelic.h"
#include "Parser/SheetParserUtils.h"

UNPRelicTableParser::UNPRelicTableParser()
{
	RelicClassFolderPath.Path = TEXT("/Game/NoPhotos/Blueprints/Relic/Objects");
}

bool UNPRelicTableParser::OnParseComplete(FString& OutError)
{
	if (!IsValid(TargetTable))
	{
		OutError = TEXT("Target Table이 지정되지 않았습니다.");
		return false;
	}

	if (TargetTable->GetRowStruct() != FNPRelicTableRow::StaticStruct())
	{
		OutError = TEXT("Target Table의 Row Struct가 FNPRelicTableRow가 아닙니다.");
		return false;
	}

	const TArray<FString> RequiredHeaders = {
		TEXT("RowName"),
		TEXT("DisplayName"),
		TEXT("Description"),
		TEXT("Price")
	};
	for (const FString& Header : RequiredHeaders)
	{
		if (!GetHeaders().Contains(Header))
		{
			OutError = FString::Printf(
				TEXT("필수 헤더가 없습니다: %s"),
				*Header);
			return false;
		}
	}

	TArray<TPair<FName, FNPRelicTableRow>> NewRows;
	for (int32 RowIndex = 0; RowIndex < GetRowCount(); ++RowIndex)
	{
		TMap<FString, FString> RowData;
		if (!GetRowAt(RowIndex, RowData))
		{
			continue;
		}

		const FString RowNameString = SheetParserUtils::TrimCell(
			RowData.FindRef(TEXT("RowName")));
		if (SheetParserUtils::IsUnsetValue(RowNameString))
		{
			continue;
		}

		const FName RowName(*RowNameString);
		FNPRelicTableRow NewRow;
		NewRow.DisplayName = FText::FromString(
			RowData.FindRef(TEXT("DisplayName")));
		NewRow.Description = FText::FromString(
			RowData.FindRef(TEXT("Description")));
		NewRow.Price = FMath::Max(
			SheetParserUtils::ParseIntValue(
				RowData.FindRef(TEXT("Price")),
				0),
			0);
		NewRow.RelicClass = SheetParserUtils::FindBlueprintClass<ANPBaseRelic>(
			RelicClassFolderPath.Path,
			RelicClassNameFormat,
			RowName);

		NewRows.Emplace(RowName, MoveTemp(NewRow));
	}

	FDataTableEditorUtils::BroadcastPreChange(
		TargetTable,
		FDataTableEditorUtils::EDataTableChangeInfo::RowList);
	TargetTable->Modify();
	TargetTable->EmptyTable();

	for (const TPair<FName, FNPRelicTableRow>& Row : NewRows)
	{
		TargetTable->AddRow(Row.Key, Row.Value);
	}

	TargetTable->HandleDataTableChanged();
	TargetTable->MarkPackageDirty();
	FDataTableEditorUtils::BroadcastPostChange(
		TargetTable,
		FDataTableEditorUtils::EDataTableChangeInfo::RowList);

	return true;
}
