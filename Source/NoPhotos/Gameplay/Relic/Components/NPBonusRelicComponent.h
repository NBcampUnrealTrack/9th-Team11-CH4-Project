#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NPBonusRelicComponent.generated.h"

class ANPBaseRelic;

UCLASS(ClassGroup=(Relic), meta=(BlueprintSpawnableComponent))
class NOPHOTOS_API UNPBonusRelicComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNPBonusRelicComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	//전체 퀘스트 유물 반환
	UFUNCTION(BlueprintPure, Category="Quest Relic")
	TArray<ANPBaseRelic*> GetQuestRelics() const;

	//방에 있는 유물 중 하나를 퀘스트 유물로 선정
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Quest Relic")
	ANPBaseRelic* SelectQuestRelicFromRoom(const TArray<ANPBaseRelic*>& RoomRelics);

	//이미 선정된 유물인지 확인
	UFUNCTION(BlueprintPure, Category="Quest Relic")
	bool IsQuestRelic(const ANPBaseRelic* Relic) const;
	
	//문제 없으면 퀘스트 유물 배열에 추가 + 태그 붙임
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Quest Relic")
	bool AddQuestRelic(ANPBaseRelic* Relic);

	//플레이어에게 설정된 수만큼 유물 배정
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Quest Relic")
	bool DistributeQuestRelicsToPlayers();
	
	//유물 배열 지우기
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Quest Relic")
	void ClearQuestRelics();

private:
	virtual void BeginPlay() override;
		
	//플레이어 한 명에게 배정할 퀘스트 유물 수
	UPROPERTY(EditDefaultsOnly, Category = "Quest Relic", meta = (ClampMin = "1"))
	int32 QuestRelicsPerPlayer = 3;
	
	UPROPERTY(EditDefaultsOnly, Category = "Quest Relic")
	FName QuestRelicTag = TEXT("QuestRelic");

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Quest Relic", meta=(AllowPrivateAccess="true"))
	TArray<TObjectPtr<ANPBaseRelic>> QuestRelics;
	
	//방 생성 완료 대기
	UFUNCTION()
	void HandleRoomGenerationCompleted();
	
	//생성된 모든 방에서 유물 가져오기
	void SelectQuestRelicsFromGeneratedRooms();
	
	bool IsOwnerAuthority() const;
};
