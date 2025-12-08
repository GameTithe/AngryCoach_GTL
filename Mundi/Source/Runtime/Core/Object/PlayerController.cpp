#include "pch.h"
#include "PlayerController.h"
#include "Pawn.h"
#include "CameraComponent.h"
#include "SpringArmComponent.h"
#include <windows.h>
#include <cmath>
#include "Character.h"

// 액션 인덱스 정의
enum EActionIndex
{
	Action_Attack = 0,    // 공격 (마우스 왼쪽 / 게임패드 X)
	Action_Jump = 1,      // 점프 (스페이스 / 게임패드 A)
	Action_Special = 2,   // 스페셜 (마우스 오른쪽 / 게임패드 Y)
	Action_Interact = 3,  // 상호작용 (E / 게임패드 B)
};

APlayerController::APlayerController()
{
}

APlayerController::~APlayerController()
{
}

int32 APlayerController::GetGamepadIndex() const
{
	switch (AssignedDevice)
	{
	case EInputDeviceType::Gamepad0: return 0;
	case EInputDeviceType::Gamepad1: return 1;
	case EInputDeviceType::Gamepad2: return 2;
	case EInputDeviceType::Gamepad3: return 3;
	default: return -1;
	}
}

FVector2D APlayerController::GetMovementInput() const
{
	UInputManager& InputManager = UInputManager::GetInstance();

	if (AssignedDevice == EInputDeviceType::KeyboardMouse)
	{
		// 키보드 WASD
		FVector2D Input(0.0f, 0.0f);
		if (InputManager.IsKeyDown('W')) Input.X += 1.0f;
		if (InputManager.IsKeyDown('S')) Input.X -= 1.0f;
		if (InputManager.IsKeyDown('D')) Input.Y += 1.0f;
		if (InputManager.IsKeyDown('A')) Input.Y -= 1.0f;
		return Input;
	}
	else
	{
		// 게임패드 왼쪽 스틱
		int32 PadIndex = GetGamepadIndex();
		if (PadIndex >= 0 && InputManager.IsGamepadConnected(PadIndex))
		{
			float X = InputManager.GetGamepadLeftStickY(PadIndex);  // 전후 (스틱 Y = 앞뒤)
			float Y = InputManager.GetGamepadLeftStickX(PadIndex);  // 좌우 (스틱 X = 좌우)
			return FVector2D(X, Y);
		}
	}
	return FVector2D(0.0f, 0.0f);
}

FVector2D APlayerController::GetLookInput() const
{
	UInputManager& InputManager = UInputManager::GetInstance();

	if (AssignedDevice == EInputDeviceType::KeyboardMouse)
	{
		// 마우스 델타
		return InputManager.GetMouseDelta();
	}
	else
	{
		// 게임패드 오른쪽 스틱
		int32 PadIndex = GetGamepadIndex();
		if (PadIndex >= 0 && InputManager.IsGamepadConnected(PadIndex))
		{
			float X = InputManager.GetGamepadRightStickX(PadIndex);
			float Y = InputManager.GetGamepadRightStickY(PadIndex);
			// 스틱은 프레임당 값이므로 감도 적용
			const float StickSensitivity = 150.0f;
			return FVector2D(X * StickSensitivity, -Y * StickSensitivity);
		}
	}
	return FVector2D(0.0f, 0.0f);
}

bool APlayerController::IsActionPressed(int32 ActionIndex) const
{
	UInputManager& InputManager = UInputManager::GetInstance();

	if (AssignedDevice == EInputDeviceType::KeyboardMouse)
	{
		switch (ActionIndex)
		{
		case Action_Attack:   return InputManager.IsMouseButtonPressed(EMouseButton::LeftButton);
		case Action_Jump:     return InputManager.IsKeyPressed(VK_SPACE);
		case Action_Special:  return InputManager.IsMouseButtonPressed(EMouseButton::RightButton);
		case Action_Interact: return InputManager.IsKeyPressed('E');
		}
	}
	else
	{
		int32 PadIndex = GetGamepadIndex();
		if (PadIndex >= 0 && InputManager.IsGamepadConnected(PadIndex))
		{
			switch (ActionIndex)
			{
			case Action_Attack:   return InputManager.IsGamepadButtonPressed(PadIndex, EGamepadButton::X);
			case Action_Jump:     return InputManager.IsGamepadButtonPressed(PadIndex, EGamepadButton::A);
			case Action_Special:  return InputManager.IsGamepadButtonPressed(PadIndex, EGamepadButton::Y);
			case Action_Interact: return InputManager.IsGamepadButtonPressed(PadIndex, EGamepadButton::B);
			}
		}
	}
	return false;
}

bool APlayerController::IsActionDown(int32 ActionIndex) const
{
	UInputManager& InputManager = UInputManager::GetInstance();

	if (AssignedDevice == EInputDeviceType::KeyboardMouse)
	{
		switch (ActionIndex)
		{
		case Action_Attack:   return InputManager.IsMouseButtonDown(EMouseButton::LeftButton);
		case Action_Jump:     return InputManager.IsKeyDown(VK_SPACE);
		case Action_Special:  return InputManager.IsMouseButtonDown(EMouseButton::RightButton);
		case Action_Interact: return InputManager.IsKeyDown('E');
		}
	}
	else
	{
		int32 PadIndex = GetGamepadIndex();
		if (PadIndex >= 0 && InputManager.IsGamepadConnected(PadIndex))
		{
			switch (ActionIndex)
			{
			case Action_Attack:   return InputManager.IsGamepadButtonDown(PadIndex, EGamepadButton::X);
			case Action_Jump:     return InputManager.IsGamepadButtonDown(PadIndex, EGamepadButton::A);
			case Action_Special:  return InputManager.IsGamepadButtonDown(PadIndex, EGamepadButton::Y);
			case Action_Interact: return InputManager.IsGamepadButtonDown(PadIndex, EGamepadButton::B);
			}
		}
	}
	return false;
}

bool APlayerController::IsActionReleased(int32 ActionIndex) const
{
	UInputManager& InputManager = UInputManager::GetInstance();

	if (AssignedDevice == EInputDeviceType::KeyboardMouse)
	{
		switch (ActionIndex)
		{
		case Action_Attack:   return InputManager.IsMouseButtonReleased(EMouseButton::LeftButton);
		case Action_Jump:     return InputManager.IsKeyReleased(VK_SPACE);
		case Action_Special:  return InputManager.IsMouseButtonReleased(EMouseButton::RightButton);
		case Action_Interact: return InputManager.IsKeyReleased('E');
		}
	}
	else
	{
		int32 PadIndex = GetGamepadIndex();
		if (PadIndex >= 0 && InputManager.IsGamepadConnected(PadIndex))
		{
			switch (ActionIndex)
			{
			case Action_Attack:   return InputManager.IsGamepadButtonReleased(PadIndex, EGamepadButton::X);
			case Action_Jump:     return InputManager.IsGamepadButtonReleased(PadIndex, EGamepadButton::A);
			case Action_Special:  return InputManager.IsGamepadButtonReleased(PadIndex, EGamepadButton::Y);
			case Action_Interact: return InputManager.IsGamepadButtonReleased(PadIndex, EGamepadButton::B);
			}
		}
	}
	return false;
}

void APlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

    if (Pawn == nullptr) return;

	UInputManager& InputManager = UInputManager::GetInstance();

	// F11을 통해서, Detail Panel 클릭이 가능해짐 (키보드 사용자만)
	if (AssignedDevice == EInputDeviceType::KeyboardMouse)
	{
        if (InputManager.IsKeyPressed(VK_F11))
        {
            bMouseLookEnabled = !bMouseLookEnabled;
            if (bMouseLookEnabled)
            {
                InputManager.SetCursorVisible(false);
                InputManager.LockCursor();
                InputManager.LockCursorToCenter();
            }
            else
            {
                InputManager.SetCursorVisible(true);
                InputManager.ReleaseCursor();
            }
        }
    }

	// 입력 처리 (Move)
	ProcessMovementInput(DeltaSeconds);

	// 입력 처리 (Look/Turn)
	ProcessRotationInput(DeltaSeconds);
}

void APlayerController::SetupInput()
{
	// InputManager에 키 바인딩
}

void APlayerController::ProcessMovementInput(float DeltaTime)
{
	UInputManager& InputManager = UInputManager::GetInstance();

	// 게임플레이 입력이 비활성화되어 있으면 이동 입력 무시
	if (!InputManager.IsGameplayInputEnabled())
	{
		return;
	}

	// 할당된 장치에서 이동 입력 가져오기
	FVector2D MoveInput = GetMovementInput();
	FVector InputDir(MoveInput.X, MoveInput.Y, 0.0f);

	if (!InputDir.IsZero())
	{
		InputDir.Normalize();

		// 카메라(ControlRotation) 기준으로 월드 이동 방향 계산
		FVector ControlEuler = GetControlRotation().ToEulerZYXDeg();
		FQuat YawOnlyRotation = FQuat::MakeFromEulerZYX(FVector(0.0f, 0.0f, ControlEuler.Z));
		FVector WorldDir = YawOnlyRotation.RotateVector(InputDir);
		WorldDir.Z = 0.0f; // 수평 이동만
		WorldDir.Normalize();

		// 이동 방향으로 캐릭터 회전 (목표 방향까지만)
		float TargetYaw = std::atan2(WorldDir.Y, WorldDir.X) * (180.0f / PI);
		FQuat TargetRotation = FQuat::MakeFromEulerZYX(FVector(0.0f, 0.0f, TargetYaw));

		// 부드러운 회전 (보간) - 목표에 도달하면 멈춤
		FQuat CurrentRotation = Pawn->GetActorRotation();
		FQuat NewRotation = FQuat::Slerp(CurrentRotation, TargetRotation, FMath::Clamp(DeltaTime * 3.0f, 0.0f, 1.0f));
		Pawn->SetActorRotation(NewRotation);

		// 이동 적용
		Pawn->AddMovementInput(WorldDir * (Pawn->GetVelocity() * DeltaTime));
	}

	// 점프 처리 (할당된 장치 사용)
	if (IsActionPressed(Action_Jump))
	{
		if (auto* Character = Cast<ACharacter>(Pawn))
		{
			Character->Jump();
		}
	}
	if (IsActionReleased(Action_Jump))
	{
		if (auto* Character = Cast<ACharacter>(Pawn))
		{
			Character->StopJumping();
		}
	}
}

void APlayerController::ProcessRotationInput(float DeltaTime)
{
    UInputManager& InputManager = UInputManager::GetInstance();

    // 키보드+마우스: bMouseLookEnabled가 true일 때만 카메라 회전
    // 게임패드: 항상 오른쪽 스틱으로 카메라 회전 가능
    bool bCanLook = (AssignedDevice == EInputDeviceType::KeyboardMouse) ? bMouseLookEnabled : true;
    if (!bCanLook)
        return;

    // 할당된 장치에서 시선 입력 가져오기
    FVector2D LookDelta = GetLookInput();

    // 입력이 있을 때만 ControlRotation 업데이트
    if (LookDelta.X != 0.0f || LookDelta.Y != 0.0f)
    {
        FVector Euler = GetControlRotation().ToEulerZYXDeg();

        // Yaw (좌우 회전)
        Euler.Z += LookDelta.X * Sensitivity;

        // Pitch (상하 회전)
        Euler.Y += LookDelta.Y * Sensitivity;
        Euler.Y = FMath::Clamp(Euler.Y, -89.0f, 89.0f);

        // Roll 방지
        Euler.X = 0.0f;

        FQuat NewControlRotation = FQuat::MakeFromEulerZYX(Euler);
        SetControlRotation(NewControlRotation);
    }

    // 매 프레임 SpringArm 월드 회전을 ControlRotation으로 동기화 (캐릭터 회전과 독립)
    if (UActorComponent* C = Pawn->GetComponent(USpringArmComponent::StaticClass()))
    {
        if (USpringArmComponent* SpringArm = Cast<USpringArmComponent>(C))
        {
            FVector Euler = GetControlRotation().ToEulerZYXDeg();
            FQuat SpringArmRot = FQuat::MakeFromEulerZYX(FVector(0.0f, Euler.Y, Euler.Z));
            SpringArm->SetWorldRotation(SpringArmRot);
        }
    }
}
