#pragma once
#include "PlayerController.h"
#include "AAngryCoachPlayerController.generated.h"

class ACharacter;
class ACameraActor;

class AAngryCoachPlayerController : public APlayerController
{
public:
	GENERATED_REFLECTION_BODY()

	AAngryCoachPlayerController();
	virtual ~AAngryCoachPlayerController() override;

	virtual void Tick(float DeltaSeconds) override;

	// 두 캐릭터 등록
	void SetControlledCharacters(ACharacter* InPlayer1, ACharacter* InPlayer2);
	void SetGameCamera(ACameraActor* InCamera);

protected:
	// 각 캐릭터별 이동 입력 처리
	void ProcessPlayer1Input(float DeltaTime);  // WASD + Space
	void ProcessPlayer2Input(float DeltaTime);  // 화살표 + RCtrl

	// 카메라 위치 업데이트 (두 캐릭터 중심)
	void UpdateCameraPosition(float DeltaTime);

protected:
	ACharacter* Player1 = nullptr;
	ACharacter* Player2 = nullptr;
	ACameraActor* GameCamera = nullptr;

	// 카메라 오프셋 (두 캐릭터 중심에서 얼마나 떨어질지) - 미터 단위
	FVector CameraOffset = FVector(-2.0f, 0.0f, 0.5f);
	float CameraLerpSpeed = 5.0f;
};
