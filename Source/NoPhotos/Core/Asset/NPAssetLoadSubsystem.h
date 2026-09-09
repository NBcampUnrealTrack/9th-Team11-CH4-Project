#pragma once

#include "CoreMinimal.h"
#include "Core/Asset/NPAssetLoadTypes.h"
#include "Engine/AssetManagerTypes.h"
#include "Engine/StreamableManager.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "NPAssetLoadSubsystem.generated.h"

struct FWorldContext;

DECLARE_DELEGATE_OneParam(FNPOnAssetLoadComplete, const FNPAssetLoadResult&);

/**
 * Thin asynchronous loading facade.
 * Primary Asset and bundle lifetime stays with UAssetManager. This subsystem only owns
 * direct Soft Path handles until completion/cancellation so callers get a small, safe API.
 */
UCLASS()
class NOPHOTOS_API UNPAssetLoadSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void LoadPrimaryAssetTypeAsync(
		FPrimaryAssetType AssetType,
		FStreamableDelegate OnLoaded = FStreamableDelegate());

	void LoadPrimaryAssetBundlesAsync(
		const TArray<FPrimaryAssetId>& AssetIds,
		const TArray<FName>& BundleNames,
		FStreamableDelegate OnLoaded = FStreamableDelegate());

	void UnloadPrimaryAssetBundlesAsync(
		const TArray<FPrimaryAssetId>& AssetIds,
		const TArray<FName>& BundleNames,
		FStreamableDelegate OnUnloaded = FStreamableDelegate());

	int32 UnloadPrimaryAssets(const TArray<FPrimaryAssetId>& AssetIds);
	int32 UnloadPrimaryAssetType(FPrimaryAssetType AssetType);
	UObject* GetLoadedPrimaryAsset(FPrimaryAssetId AssetId) const;
	bool IsPrimaryAssetLoaded(FPrimaryAssetId AssetId) const;

	/** For small temporary assets that do not need a Primary Asset definition. */
	FNPAssetLoadRequestId LoadSoftPathsAsync(
		UObject* Owner,
		const TArray<FSoftObjectPath>& Paths,
		FNPOnAssetLoadComplete OnLoaded);

	UFUNCTION(BlueprintCallable, Category = "Asset Loading")
	bool CancelSoftPathRequest(FNPAssetLoadRequestId RequestId);

	UFUNCTION(BlueprintCallable, Category = "Asset Loading")
	void CancelSoftPathRequestsForOwner(UObject* Owner);

	void CancelAllSoftPathRequests();

	int32 GetActiveSoftPathRequestCount() const { return ActiveSoftPathRequests.Num(); }
	bool IsSoftPathRequestActive(FNPAssetLoadRequestId RequestId) const;
	float GetSoftPathRequestProgress(FNPAssetLoadRequestId RequestId) const;

	/** Called automatically before map load; public for deterministic automation testing. */
	void AdvanceWorldGeneration();

private:
	struct FSoftPathRequest
	{
		FNPAssetLoadRequestId Id;
		TWeakObjectPtr<UObject> Owner;
		TArray<FSoftObjectPath> Paths;
		TSharedPtr<FStreamableHandle> Handle;
		FNPOnAssetLoadComplete Completion;
		uint32 WorldGeneration = 0;
	};

	void HandleSoftPathsLoaded(FNPAssetLoadRequestId RequestId);
	bool CancelSoftPathRequestInternal(
		FNPAssetLoadRequestId RequestId,
		ENPAssetLoadFailure Failure,
		bool bNotifyOwner);
	void HandlePreLoadMap(const FWorldContext& WorldContext, const FString& MapName);

	TMap<FNPAssetLoadRequestId, FSoftPathRequest> ActiveSoftPathRequests;
	uint32 WorldGeneration = 1;
	FString LastPreLoadMapName;
	FDelegateHandle PreLoadMapHandle;
};
