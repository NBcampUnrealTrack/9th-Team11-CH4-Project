#pragma once

#include "CoreMinimal.h"
#include "Gameplay/MapEvents/NPMapEvent.h"
#include "NPArtifactSpawnMapEvent.generated.h"

UCLASS(Blueprintable)
class NOPHOTOS_API ANPArtifactSpawnMapEvent : public ANPMapEvent
{
	GENERATED_BODY()

public:
	ANPArtifactSpawnMapEvent();

protected:
	virtual void ApplyEventState_Implementation(bool bNewActive) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Artifact")
	TSubclassOf<AActor> ArtifactClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Artifact")
	FName SpawnPointTag = TEXT("ArtifactSpawnPoint");

private:
	AActor* SpawnArtifact();
	FTransform SelectSpawnTransform() const;
};
