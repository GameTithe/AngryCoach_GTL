#pragma once
#include "Controller.h"
#include "InputManager.h"
#include "APlayerController.generated.h"

class APlayerController : public AController
{
public:
	GENERATED_REFLECTION_BODY()

	APlayerController();
	virtual ~APlayerController() override;

	virtual void Tick(float DeltaSeconds) override;

	virtual void SetupInput();

	// === 플레이어 입력 장치 설정 ===

	// 플레이어 인덱스 (0 또는 1)
	void SetPlayerIndex(int32 InPlayerIndex) { PlayerIndex = InPlayerIndex; }
	int32 GetPlayerIndex() const { return PlayerIndex; }

	// 입력 장치 할당
	void SetInputDevice(EInputDeviceType DeviceType) { AssignedDevice = DeviceType; }
	EInputDeviceType GetInputDevice() const { return AssignedDevice; }

	// 게임패드 인덱스 반환 (Gamepad0 -> 0, Gamepad1 -> 1, 그 외 -1)
	int32 GetGamepadIndex() const;

	// === 입력 조회 (할당된 장치 기준) ===

	// 이동 입력 (키보드 WASD 또는 게임패드 왼쪽 스틱)
	FVector2D GetMovementInput() const;

	// 카메라/시선 입력 (마우스 또는 게임패드 오른쪽 스틱)
	FVector2D GetLookInput() const;

	// 버튼 입력
	bool IsActionPressed(int32 ActionIndex) const;  // 공격, 점프 등
	bool IsActionDown(int32 ActionIndex) const;
	bool IsActionReleased(int32 ActionIndex) const;

protected:
    void ProcessMovementInput(float DeltaTime);
    void ProcessRotationInput(float DeltaTime);

protected:
    bool bMouseLookEnabled = false;

	// 플레이어 인덱스 (0 = Player1, 1 = Player2)
	int32 PlayerIndex = 0;

	// 할당된 입력 장치
	EInputDeviceType AssignedDevice = EInputDeviceType::KeyboardMouse;

private:
	float Sensitivity = 0.1f;
};
