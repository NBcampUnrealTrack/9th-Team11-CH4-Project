#include "UI/GameScreen/NPPhotoPenaltyWidget.h"

#include "Components/TextBlock.h"
#include "Gameplay/Photo/NPPhotoLog.h"

void UNPPhotoPenaltyWidget::SetPenaltyAmount(const int32 AppliedPhotoPenalty)
{
	if (!IsValid(PhotoPenaltyText) || AppliedPhotoPenalty <= 0)
	{
		UE_LOG(
			LogNPPhoto,
			Warning,
			TEXT("[PhotoPenaltyUI] Cannot update penalty text. Widget=%s TextBlock=%s Amount=%d"),
			*GetNameSafe(this),
			*GetNameSafe(PhotoPenaltyText),
			AppliedPhotoPenalty);
		return;
	}

	PhotoPenaltyText->SetText(FText::Format(
		NSLOCTEXT(
			"NPPhoto",
			"RelicValueReducedByAmount",
			" -{0}$$$$"),
		FText::AsNumber(AppliedPhotoPenalty)));
}
