#pragma once

#include "CoreMinimal.h"
#include "NPAssetLoadTypes.generated.h"

UENUM(BlueprintType)
enum class ENPAssetLoadStatus : uint8
{
	Loaded,
	Failed,
	Canceled
};

UENUM(BlueprintType)
enum class ENPAssetLoadFailure : uint8
{
	None,
	InvalidRequest,
	OwnerInvalid,
	WorldChanged,
	Canceled,
	LoadFailed
};

/** Identifies one cancelable direct Soft Path request. */
USTRUCT(BlueprintType)
struct NOPHOTOS_API FNPAssetLoadRequestId
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Asset Loading")
	FGuid Value;

	static FNPAssetLoadRequestId NewId()
	{
		FNPAssetLoadRequestId Id;
		Id.Value = FGuid::NewGuid();
		return Id;
	}

	bool IsValid() const { return Value.IsValid(); }
	bool operator==(const FNPAssetLoadRequestId& Other) const { return Value == Other.Value; }
};

FORCEINLINE uint32 GetTypeHash(const FNPAssetLoadRequestId& Id)
{
	return GetTypeHash(Id.Value);
}

USTRUCT(BlueprintType)
struct NOPHOTOS_API FNPAssetLoadResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Asset Loading")
	FNPAssetLoadRequestId RequestId;

	UPROPERTY(BlueprintReadOnly, Category = "Asset Loading")
	ENPAssetLoadStatus Status = ENPAssetLoadStatus::Failed;

	UPROPERTY(BlueprintReadOnly, Category = "Asset Loading")
	ENPAssetLoadFailure Failure = ENPAssetLoadFailure::None;

	UPROPERTY(BlueprintReadOnly, Category = "Asset Loading")
	TArray<TObjectPtr<UObject>> LoadedObjects;

	bool IsSuccess() const
	{
		return Status == ENPAssetLoadStatus::Loaded && Failure == ENPAssetLoadFailure::None;
	}
};
