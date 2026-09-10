#pragma once

#include "NativeGameplayTags.h"

namespace NPGameplayTags
{
	NOPHOTOS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_LavaBurning);
	NOPHOTOS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Ranking_Leader);
	NOPHOTOS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Relic_Use);
	NOPHOTOS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Relic_Aim);
	NOPHOTOS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Relic_Fire);
	NOPHOTOS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Photo_Aim);
	NOPHOTOS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Photo_Shot);
	NOPHOTOS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Scan);
	NOPHOTOS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Relic);
	NOPHOTOS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Relic_Aim);
	NOPHOTOS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Relic_Fire);
	NOPHOTOS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Photo);
	NOPHOTOS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Photo_Aim);
	NOPHOTOS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Photo_Shot);
	NOPHOTOS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Scan);
	NOPHOTOS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Relic_Aiming);
	/** 하나 이상의 유물을 현재 운반 중인 상태입니다. */
	NOPHOTOS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Relic_Carrying);
	/** 사용할 수 있는 유물을 현재 운반 중인 상태입니다. */
	NOPHOTOS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Relic_Carrying_Usable);
	NOPHOTOS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Photo_Aiming);
	/** 사진 촬영 Ability를 다시 사용할 수 없는 상태입니다. */
	NOPHOTOS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Photo_Shot);
	/** 이동과 조준을 포함한 플레이 조작이 스턴으로 차단된 상태입니다. */
	NOPHOTOS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_CrowdControl_Stunned);
	NOPHOTOS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Effect_Knockback);
	/** 투명화 상태. 서버 판정과 클라이언트 표현이 함께 조회합니다. */
	NOPHOTOS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Invisible);
	/** 시야 제한 상태. 투명화 여부와 별도로 조회할 수 있습니다. */
	NOPHOTOS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_VisionRestricted);
	/** 수평면 이동(W/S, A/D)을 반전합니다. 시점과 점프 입력은 유지합니다. */
	NOPHOTOS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_ControlsMirrored);
	NOPHOTOS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Impact);
	/** 조준 유물 발사 순간의 총구 연출입니다. */
	NOPHOTOS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Relic_Aimable_Fire);
	/** 마법 지팡이 발사 순간의 전용 연출입니다. */
	NOPHOTOS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Relic_MagicWand_Fire);
	/** 조준 유물 발사체가 Trace 대상에 적중한 순간의 연출입니다. */
	NOPHOTOS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Relic_Aimable_Impact);
	/** 빙의 중 캐릭터 주변에 표시할 지속형 GameplayCue입니다. */
	NOPHOTOS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Status_ControlReversal);
	NOPHOTOS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Status_Leader);
	NOPHOTOS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Status_LavaBurning);
	NOPHOTOS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Status_PhotoStun);
	NOPHOTOS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Photo_WorldFeedback_Photographer);
	NOPHOTOS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Photo_WorldFeedback_Photographed);
	/** 사진 조준 Ability가 성공적으로 시작된 순간의 3D 준비음 연출입니다. */
	NOPHOTOS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Photo_AimStart);
	/** 서버가 승인한 사진 촬영 순간의 3D 셔터음 및 월드 연출입니다. */
	NOPHOTOS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Photo_Shutter);
	/** 스포트라이트 아래에서 유물 가치가 증가한 순간 재생하는 GameplayCue입니다. */
	NOPHOTOS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_MapEvent_Spotlight_PriceBonus);
	NOPHOTOS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Knockback_Magnitude);
}
