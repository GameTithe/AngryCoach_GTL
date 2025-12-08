#pragma once
#include "PlayerController.h"
#include "AAngryCoachPlayerController.generated.h"

class AAngryCoachCharacter;
class ACameraActor;

class AAngryCoachPlayerController : public APlayerController
{
public:
	GENERATED_REFLECTION_BODY()

	AAngryCoachPlayerController();
	virtual ~AAngryCoachPlayerController() override;

	virtual void Tick(float DeltaSeconds) override;

	// 두 캐릭터 등록
	void SetControlledCharacters(AAngryCoachCharacter* InPlayer1, AAngryCoachCharacter* InPlayer2);
	void SetGameCamera(ACameraActor* InCamera);

protected:
	// 각 캐릭터별 이동 입력 처리
	void ProcessPlayer1Input(float DeltaTime);  // WASD + R + TYU
	void ProcessPlayer2Input(float DeltaTime);  // 화살표 + Numpad1 + Numpad234

	// 카메라 위치 업데이트 (두 캐릭터 중심)
	void UpdateCameraPosition(float DeltaTime);

private:
	// 헬퍼 함수
	void ProcessPlayer1Attack(float DeltaTime);
	void ProcessPlayer2Attack(float DeltaTime);
	void ResetInputBuffer(bool& bIsInputBuffering, char& PendingKey, float& InputBufferTime);

protected:
	AAngryCoachCharacter* Player1 = nullptr;
	AAngryCoachCharacter* Player2 = nullptr;
	ACameraActor* GameCamera = nullptr;

	// 카메라 오프셋 (두 캐릭터 중심에서 얼마나 떨어질지) - 미터 단위
	FVector CameraOffset = FVector(-6.0f, 0.0f, 1.5f);
	float CameraLerpSpeed = 5.0f;

	// Player 1
	bool bIsP1InputBuffering = false;
	char P1PendingKey = 0;
	float P1InputBufferTime = 0.0f;
	// Player2
	bool bIsP2InputBuffering = false;
	char P2PendingKey = 0;
	float P2InputBufferTime = 0.0f;
	
	float InputBufferLimit = 0.1f;

};
