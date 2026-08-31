#include "NPArtifactSpawnMapEvent.h"

#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

DEFINE_LOG_CATEGORY_STATIC(LogNPArtifactEvent, Log, All);

ANPArtifactSpawnMapEvent::ANPArtifactSpawnMapEvent()
{
	Duration = 1.0f;
	ArtifactClass = AActor::StaticClass();
}

void ANPArtifactSpawnMapEvent::ApplyEventState_Implementation(const bool bNewActive)
{
	if (!bNewActive)
	{
		return;
	}

	UE_LOG(LogNPArtifactEvent, Display, TEXT("특수 유물 생성 이벤트가 발생했습니다."));
	if (!HasAuthority())
	{
		return;
	}

	AActor* SpawnedArtifact = SpawnArtifact();
	if (!IsValid(SpawnedArtifact))
	{
		UE_LOG(LogNPArtifactEvent, Error, TEXT("특수 유물 생성에 실패했습니다."));
		return;
	}

	UE_LOG(
		LogNPArtifactEvent,
		Display,
		TEXT("특수 유물이 생성되었습니다: Actor=%s, Location=%s"),
		*SpawnedArtifact->GetName(),
		*SpawnedArtifact->GetActorLocation().ToCompactString());
}

AActor* ANPArtifactSpawnMapEvent::SpawnArtifact()
{
	UWorld* World = GetWorld();
	if (!World || !ArtifactClass)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AActor* SpawnedArtifact = World->SpawnActor<AActor>(ArtifactClass, SelectSpawnTransform(), SpawnParameters);
	if (SpawnedArtifact)
	{
		SpawnedArtifact->SetReplicates(true);
		SpawnedArtifact->SetReplicateMovement(true);
	}

	return SpawnedArtifact;
}

FTransform ANPArtifactSpawnMapEvent::SelectSpawnTransform() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return GetActorTransform();
	}

	TArray<AActor*> SpawnPoints;
	for (TActorIterator<AActor> ActorIterator(World); ActorIterator; ++ActorIterator)
	{
		AActor* Actor = *ActorIterator;
		if (IsValid(Actor) && Actor->ActorHasTag(SpawnPointTag))
		{
			SpawnPoints.Add(Actor);
		}
	}

	if (SpawnPoints.IsEmpty())
	{
		UE_LOG(
			LogNPArtifactEvent,
			Warning,
			TEXT("'%s' 태그가 지정된 생성 지점이 없어 이벤트 매니저 위치를 사용합니다."),
			*SpawnPointTag.ToString());
		return GetActorTransform();
	}

	return SpawnPoints[FMath::RandRange(0, SpawnPoints.Num() - 1)]->GetActorTransform();
}
