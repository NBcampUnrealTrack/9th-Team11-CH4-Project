#include "Core/Asset/NPAssetLoadSubsystem.h"

#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogNPAssetLoad, Log, All);

void UNPAssetLoadSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	PreLoadMapHandle = FCoreUObjectDelegates::PreLoadMapWithContext.AddUObject(
		this,
		&UNPAssetLoadSubsystem::HandlePreLoadMap);
}

void UNPAssetLoadSubsystem::Deinitialize()
{
	if (PreLoadMapHandle.IsValid())
	{
		FCoreUObjectDelegates::PreLoadMapWithContext.Remove(PreLoadMapHandle);
		PreLoadMapHandle.Reset();
	}

	CancelAllSoftPathRequests();
	LastPreLoadMapName.Reset();
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
		UE_LOG(LogNPAssetLoad, Error,
			TEXT("Soft path request rejected. Owner=%s OwnerValid=%s InputPaths=%d ValidPaths=%d WorldGeneration=%u"),
			*GetNameSafe(Owner),
			IsValid(Owner) ? TEXT("true") : TEXT("false"),
			Paths.Num(),
			ValidPaths.Num(),
			WorldGeneration);
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
	UE_LOG(LogNPAssetLoad, Log,
		TEXT("Soft path request started. RequestId=%s Owner=%s Paths=%d WorldGeneration=%u"),
		*RequestId.Value.ToString(), *GetNameSafe(Owner), RequestedPaths.Num(), WorldGeneration);
	for (const FSoftObjectPath& RequestedPath : RequestedPaths)
	{
		UE_LOG(LogNPAssetLoad, Verbose,
			TEXT("Soft path request item. RequestId=%s Path=%s"),
			*RequestId.Value.ToString(), *RequestedPath.ToString());
	}
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
		UE_LOG(LogNPAssetLoad, Warning,
			TEXT("Streamable handle was not created; validating immediately. RequestId=%s"),
			*RequestId.Value.ToString());
		HandleSoftPathsLoaded(RequestId);
	}
	return RequestId;
}

void UNPAssetLoadSubsystem::HandleSoftPathsLoaded(const FNPAssetLoadRequestId RequestId)
{
	FSoftPathRequest Request;
	if (!ActiveSoftPathRequests.RemoveAndCopyValue(RequestId, Request))
	{
		UE_LOG(LogNPAssetLoad, Verbose,
			TEXT("Ignoring completion for inactive request. RequestId=%s"),
			*RequestId.Value.ToString());
		return;
	}

	if (!Request.Owner.IsValid() || Request.WorldGeneration != WorldGeneration)
	{
		UE_LOG(LogNPAssetLoad, Warning,
			TEXT("Discarding stale completion. RequestId=%s OwnerValid=%s RequestGeneration=%u CurrentGeneration=%u"),
			*RequestId.Value.ToString(),
			Request.Owner.IsValid() ? TEXT("true") : TEXT("false"),
			Request.WorldGeneration, WorldGeneration);
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
		else
		{
			UE_LOG(LogNPAssetLoad, Error,
				TEXT("Soft path failed to resolve after load. RequestId=%s Path=%s"),
				*RequestId.Value.ToString(), *Path.ToString());
		}
	}

	const bool bAllLoaded = Result.LoadedObjects.Num() == Request.Paths.Num();
	Result.Status = bAllLoaded ? ENPAssetLoadStatus::Loaded : ENPAssetLoadStatus::Failed;
	Result.Failure = bAllLoaded ? ENPAssetLoadFailure::None : ENPAssetLoadFailure::LoadFailed;
	UE_LOG(LogNPAssetLoad, Log,
		TEXT("Soft path request completed. RequestId=%s Success=%s Loaded=%d Requested=%d"),
		*RequestId.Value.ToString(), bAllLoaded ? TEXT("true") : TEXT("false"),
		Result.LoadedObjects.Num(), Request.Paths.Num());
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
		UE_LOG(LogNPAssetLoad, Verbose,
			TEXT("Cancel ignored for inactive request. RequestId=%s Failure=%d"),
			*RequestId.Value.ToString(), static_cast<int32>(Failure));
		return false;
	}

	UE_LOG(LogNPAssetLoad, Log,
		TEXT("Soft path request canceled. RequestId=%s Owner=%s Failure=%d NotifyOwner=%s"),
		*RequestId.Value.ToString(), *GetNameSafe(Request.Owner.Get()),
		static_cast<int32>(Failure), bNotifyOwner ? TEXT("true") : TEXT("false"));

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
	const uint32 PreviousGeneration = WorldGeneration;
	++WorldGeneration;
	if (WorldGeneration == 0)
	{
		WorldGeneration = 1;
	}

	TArray<FNPAssetLoadRequestId> RequestIds;
	ActiveSoftPathRequests.GetKeys(RequestIds);
	UE_LOG(LogNPAssetLoad, Log,
		TEXT("World generation advanced. Previous=%u Current=%u RequestsToCancel=%d"),
		PreviousGeneration, WorldGeneration, RequestIds.Num());
	for (const FNPAssetLoadRequestId& RequestId : RequestIds)
	{
		CancelSoftPathRequestInternal(
			RequestId,
			ENPAssetLoadFailure::WorldChanged,
			true);
	}
}

void UNPAssetLoadSubsystem::HandlePreLoadMap(
	const FWorldContext& WorldContext,
	const FString& MapName)
{
	if (WorldContext.OwningGameInstance != GetGameInstance())
	{
		UE_LOG(LogNPAssetLoad, Verbose,
			TEXT("Ignoring PreLoadMap from another GameInstance. Map=%s Context=%s"),
			*MapName,
			*WorldContext.ContextHandle.ToString());
		return;
	}

	if (LastPreLoadMapName == MapName)
	{
		UE_LOG(LogNPAssetLoad, Log,
			TEXT("Duplicate PreLoadMap ignored. Map=%s WorldGeneration=%u ActiveRequests=%d Context=%s"),
			*MapName,
			WorldGeneration,
			ActiveSoftPathRequests.Num(),
			*WorldContext.ContextHandle.ToString());
		return;
	}

	LastPreLoadMapName = MapName;
	UE_LOG(LogNPAssetLoad, Log,
		TEXT("PreLoadMap received. Map=%s Context=%s GameInstance=%s"),
		*MapName,
		*WorldContext.ContextHandle.ToString(),
		*GetNameSafe(GetGameInstance()));
	AdvanceWorldGeneration();
}
