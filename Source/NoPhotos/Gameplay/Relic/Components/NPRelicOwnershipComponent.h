#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NPRelicOwnershipComponent.generated.h"

class ANPPlayerState;
class UNPStablePhysicsGrabComponent;

/** 서버에서 Relic을 현재 잡고 있는 Grab 주체와 고유 소유자를 관리합니다. */
UCLASS(ClassGroup=(Relic), meta=(BlueprintSpawnableComponent))
class NOPHOTOS_API UNPRelicOwnershipComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNPRelicOwnershipComponent();

	void RegisterGrabber(
		UNPStablePhysicsGrabComponent* GrabComponent,
		ANPPlayerState* PlayerState);
	void UnregisterGrabber(UNPStablePhysicsGrabComponent* GrabComponent);

	/** 같은 플레이어가 여러 Grab Component로 잡고 있어도 한 번만 반환합니다. */
	void GetCurrentOwners(TArray<ANPPlayerState*>& OutOwners) const;
	int32 GetCurrentOwnerCount() const;

	/** 등록된 모든 Grab Component에 서버 권한으로 놓기 요청을 전달합니다. */
	void ReleaseAllGrabbers();
	void ClearOwnership();

private:
	bool HasServerAuthority() const;
	void UpdateReplicationOwner();

	TMap<
		TWeakObjectPtr<UNPStablePhysicsGrabComponent>,
		TWeakObjectPtr<ANPPlayerState>> ActiveGrabbers;
};
