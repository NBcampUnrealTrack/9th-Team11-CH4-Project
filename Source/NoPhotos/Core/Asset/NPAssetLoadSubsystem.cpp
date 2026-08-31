#include "Core/Asset/NPAssetLoadSubsystem.h"

#include "Engine/AssetManager.h"
#include "UObject/UObjectGlobals.h"

void UNPAssetLoadSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	PreLoadMapHandle = FCoreUObjectDelegates::PreLoadMap.AddUObject(
		this,
		&UNPAssetLoadSubsystem::HandlePreLoadMap);
}

void UNPAssetLoadSubsystem::Deinitialize()
{
	if (PreLoadMapHandle.IsValid())
	{
		FCoreUObjectDelegates::PreLoadMap.Remove(PreLoadMapHandle);
		PreLoadMapHandle.Reset();
	}

	CancelAllSoftPathRequests();
	Super::Deinitialize();
}

void UNPAssetLoadSubsystem::LoadPrimaryAssetTypeAsync(
	const FPrimaryAssetType AssetType,
	FStreamableDelegate OnLoaded)
{
	if (!AssetType.IsValid())
	{
		OnLoaded.ExecuteIfBound();
		return;
	}

	UAssetManager::Get().LoadPrimaryAssetsWithType(AssetType, {}, MoveTemp(OnLoaded));
}

void UNPAssetLoadSubsystem::LoadPrimaryAssetBundlesAsync(
	const TArray<FPrimaryAssetId>& AssetIds,
	const TArray<FName>& BundleNames,
	FStreamableDelegate OnLoaded)
{
	if (AssetIds.IsEmpty() || BundleNames.IsEmpty())
	{
		OnLoaded.ExecuteIfBound();
		return;
	}

	UAssetManager::Get().ChangeBundleStateForPrimaryAssets(
		AssetIds,
		BundleNames,
		{},
		false,
		MoveTemp(OnLoaded));
}

void UNPAssetLoadSubsystem::UnloadPrimaryAssetBundlesAsync(
	const TArray<FPrimaryAssetId>& AssetIds,
	const TArray<FName>& BundleNames,
	FStreamableDelegate OnUnloaded)
{
	if (AssetIds.IsEmpty() || BundleNames.IsEmpty())
	{
		OnUnloaded.ExecuteIfBound();
		return;
	}

	UAssetManager::Get().ChangeBundleStateForPrimaryAssets(
		AssetIds,
		{},
		BundleNames,
		false,
		MoveTemp(OnUnloaded));
}

int32 UNPAssetLoadSubsystem::UnloadPrimaryAssets(const TArray<FPrimaryAssetId>& AssetIds)
{
	return UAssetManager::Get().UnloadPrimaryAssets(AssetIds);
}

int32 UNPAssetLoadSubsystem::UnloadPrimaryAssetType(const FPrimaryAssetType AssetType)
{
	return UAssetManager::Get().UnloadPrimaryAssetsWithType(AssetType);
}

UObject* UNPAssetLoadSubsystem::GetLoadedPrimaryAsset(const FPrimaryAssetId AssetId) const
{
	return UAssetManager::Get().GetPrimaryAssetObject(AssetId);
}

bool UNPAssetLoadSubsystem::IsPrimaryAssetLoaded(const FPrimaryAssetId AssetId) const
{
	return GetLoadedPrimaryAsset(AssetId) != nullptr;
}

FNPAssetLoadRequestId UNPAssetLoadSubsystem::LoadSoftPathsAsync(
	UObject* Owner,
	const TArray<FSoftObjectPath>& Paths,
	FNPOnAssetLoadComplete OnLoaded)
{
	TArray<FSoftObjectPath> ValidPaths;
	ValidPaths.Reserve(Paths.Num());
	for (const FSoftObjectPath& Path : Paths)
	{
		if (Path.IsValid())
		{
			ValidPaths.AddUnique(Path);
		}
	}

	if (!IsValid(Owner) || ValidPaths.IsEmpty())
	{
		FNPAssetLoadResult Result;
		Result.Status = ENPAssetLoadStatus::Failed;
		Result.Failure = IsValid(Owner)
			? ENPAssetLoadFailure::InvalidRequest
			: ENPAssetLoadFailure::OwnerInvalid;
		OnLoaded.ExecuteIfBound(Result);
		return {};
	}

	FSoftPathRequest Request;
	Request.Id = FNPAssetLoadRequestId::NewId();
	Request.Owner = Owner;
	Request.Paths = MoveTemp(ValidPaths);
	Request.Completion = MoveTemp(OnLoaded);
	Request.WorldGeneration = WorldGeneration;
	const FNPAssetLoadRequestId RequestId = Request.Id;
	ActiveSoftPathRequests.Add(RequestId, MoveTemp(Request));

	const TArray<FSoftObjectPath> RequestedPaths = ActiveSoftPathRequests.FindChecked(RequestId).Paths;
	TSharedPtr<FStreamableHandle> Handle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		RequestedPaths,
		FStreamableDelegate::CreateUObject(
			this,
			&UNPAssetLoadSubsystem::HandleSoftPathsLoaded,
			RequestId));

	if (FSoftPathRequest* PendingRequest = ActiveSoftPathRequests.Find(RequestId))
	{
		PendingRequest->Handle = Handle;
	}
	else if (Handle.IsValid())
	{
		Handle->ReleaseHandle();
	}

	if (!Handle.IsValid() && ActiveSoftPathRequests.Contains(RequestId))
	{
		HandleSoftPathsLoaded(RequestId);
	}
	return RequestId;
}

void UNPAssetLoadSubsystem::HandleSoftPathsLoaded(const FNPAssetLoadRequestId RequestId)
{
	FSoftPathRequest Request;
	if (!ActiveSoftPathRequests.RemoveAndCopyValue(RequestId, Request))
	{
		return;
	}

	if (!Request.Owner.IsValid() || Request.WorldGeneration != WorldGeneration)
	{
		return;
	}

	FNPAssetLoadResult Result;
	Result.RequestId = RequestId;
	for (const FSoftObjectPath& Path : Request.Paths)
	{
		if (UObject* LoadedObject = Path.ResolveObject())
		{
			Result.LoadedObjects.Add(LoadedObject);
		}
	}

	const bool bAllLoaded = Result.LoadedObjects.Num() == Request.Paths.Num();
	Result.Status = bAllLoaded ? ENPAssetLoadStatus::Loaded : ENPAssetLoadStatus::Failed;
	Result.Failure = bAllLoaded ? ENPAssetLoadFailure::None : ENPAssetLoadFailure::LoadFailed;
	Request.Completion.ExecuteIfBound(Result);

	if (Request.Handle.IsValid())
	{
		Request.Handle->ReleaseHandle();
	}
}

bool UNPAssetLoadSubsystem::CancelSoftPathRequest(const FNPAssetLoadRequestId RequestId)
{
	return CancelSoftPathRequestInternal(RequestId, ENPAssetLoadFailure::Canceled, true);
}

bool UNPAssetLoadSubsystem::CancelSoftPathRequestInternal(
	const FNPAssetLoadRequestId RequestId,
	const ENPAssetLoadFailure Failure,
	const bool bNotifyOwner)
{
	FSoftPathRequest Request;
	if (!ActiveSoftPathRequests.RemoveAndCopyValue(RequestId, Request))
	{
		return false;
	}

	if (Request.Handle.IsValid())
	{
		Request.Handle->CancelHandle();
		Request.Handle->ReleaseHandle();
	}

	if (bNotifyOwner && Request.Owner.IsValid())
	{
		FNPAssetLoadResult Result;
		Result.RequestId = RequestId;
		Result.Status = ENPAssetLoadStatus::Canceled;
		Result.Failure = Failure;
		Request.Completion.ExecuteIfBound(Result);
	}
	return true;
}

void UNPAssetLoadSubsystem::CancelSoftPathRequestsForOwner(UObject* Owner)
{
	if (!Owner)
	{
		return;
	}

	TArray<FNPAssetLoadRequestId> RequestsToCancel;
	for (const TPair<FNPAssetLoadRequestId, FSoftPathRequest>& Pair : ActiveSoftPathRequests)
	{
		if (Pair.Value.Owner.Get() == Owner)
		{
			RequestsToCancel.Add(Pair.Key);
		}
	}
	for (const FNPAssetLoadRequestId& RequestId : RequestsToCancel)
	{
		CancelSoftPathRequestInternal(RequestId, ENPAssetLoadFailure::Canceled, false);
	}
}

void UNPAssetLoadSubsystem::CancelAllSoftPathRequests()
{
	TArray<FNPAssetLoadRequestId> RequestIds;
	ActiveSoftPathRequests.GetKeys(RequestIds);
	for (const FNPAssetLoadRequestId& RequestId : RequestIds)
	{
		CancelSoftPathRequestInternal(RequestId, ENPAssetLoadFailure::Canceled, false);
	}
}

bool UNPAssetLoadSubsystem::IsSoftPathRequestActive(const FNPAssetLoadRequestId RequestId) const
{
	return ActiveSoftPathRequests.Contains(RequestId);
}

void UNPAssetLoadSubsystem::AdvanceWorldGeneration()
{
	++WorldGeneration;
	if (WorldGeneration == 0)
	{
		WorldGeneration = 1;
	}

	TArray<FNPAssetLoadRequestId> RequestIds;
	ActiveSoftPathRequests.GetKeys(RequestIds);
	for (const FNPAssetLoadRequestId& RequestId : RequestIds)
	{
		CancelSoftPathRequestInternal(RequestId, ENPAssetLoadFailure::WorldChanged, false);
	}
}

void UNPAssetLoadSubsystem::HandlePreLoadMap(const FString& MapName)
{
	AdvanceWorldGeneration();
}
