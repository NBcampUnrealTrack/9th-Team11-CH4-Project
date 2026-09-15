#include "Gameplay/Photo/NPMatchScorePolicy.h"

#include "Gameplay/Photo/NPPhotoEvidenceTypes.h"

int32 UNPMatchScorePolicy::CalculateEvidenceScore_Implementation(
	const FNPPhotoEvidenceResult& Evidence) const
{
	if (!Evidence.bSuccess)
	{
		return 0;
	}

	return 10 * Evidence.RelicEvidenceGroups.Num();
}
