#include "Gameplay/Relic/Gimmick/NPThroneLiftCameraShake.h"

#include "Shakes/PerlinNoiseCameraShakePattern.h"

UNPThroneLiftCameraShake::UNPThroneLiftCameraShake(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSingleInstance = true;

	UPerlinNoiseCameraShakePattern* Pattern =
		CastChecked<UPerlinNoiseCameraShakePattern>(GetRootShakePattern());
	Pattern->Duration = 3.0f;
	Pattern->BlendInTime = 0.15f;
	Pattern->BlendOutTime = 0.5f;

	Pattern->X.Amplitude = 1.5f;
	Pattern->X.Frequency = 8.0f;
	Pattern->Y.Amplitude = 1.5f;
	Pattern->Y.Frequency = 9.0f;
	Pattern->Z.Amplitude = 2.0f;
	Pattern->Z.Frequency = 7.0f;

	Pattern->Pitch.Amplitude = 0.5f;
	Pattern->Pitch.Frequency = 7.0f;
	Pattern->Yaw.Amplitude = 0.4f;
	Pattern->Yaw.Frequency = 8.0f;
	Pattern->Roll.Amplitude = 0.3f;
	Pattern->Roll.Frequency = 6.0f;
	Pattern->FOV.Amplitude = 0.0f;
}
