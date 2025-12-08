#pragma once

#include <windows.h>
#include <cmath>
#include <Xinput.h>

#include "Object.h"
#include "Vector.h"
#include "ImGui/imgui.h"

// 마우스 버튼 상수
enum EMouseButton
{
    LeftButton = 0,
    RightButton = 1,
    MiddleButton = 2,
    XButton1 = 3,
    XButton2 = 4,
    MaxMouseButtons = 5
};

// 게임패드 버튼 상수
enum class EGamepadButton : uint16
{
    DPadUp       = XINPUT_GAMEPAD_DPAD_UP,
    DPadDown     = XINPUT_GAMEPAD_DPAD_DOWN,
    DPadLeft     = XINPUT_GAMEPAD_DPAD_LEFT,
    DPadRight    = XINPUT_GAMEPAD_DPAD_RIGHT,
    Start        = XINPUT_GAMEPAD_START,
    Back         = XINPUT_GAMEPAD_BACK,
    LeftThumb    = XINPUT_GAMEPAD_LEFT_THUMB,
    RightThumb   = XINPUT_GAMEPAD_RIGHT_THUMB,
    LeftShoulder = XINPUT_GAMEPAD_LEFT_SHOULDER,
    RightShoulder= XINPUT_GAMEPAD_RIGHT_SHOULDER,
    A            = XINPUT_GAMEPAD_A,
    B            = XINPUT_GAMEPAD_B,
    X            = XINPUT_GAMEPAD_X,
    Y            = XINPUT_GAMEPAD_Y
};

// 입력 장치 타입
enum class EInputDeviceType : uint8
{
    None,
    KeyboardMouse,
    Gamepad0,
    Gamepad1,
    Gamepad2,
    Gamepad3
};

// 게임패드 상태 구조체
struct FGamepadState
{
    bool bConnected = false;
    XINPUT_STATE CurrentState = {};
    XINPUT_STATE PreviousState = {};

    // 스틱 데드존 적용된 값 (-1.0 ~ 1.0)
    float LeftStickX = 0.0f;
    float LeftStickY = 0.0f;
    float RightStickX = 0.0f;
    float RightStickY = 0.0f;

    // 트리거 값 (0.0 ~ 1.0)
    float LeftTrigger = 0.0f;
    float RightTrigger = 0.0f;
};

class UInputManager : public UObject
{
public:
    DECLARE_CLASS(UInputManager, UObject)

    // 생성자/소멸자 (싱글톤)
    UInputManager();
protected:
    ~UInputManager() override;

    // 복사 방지
    UInputManager(const UInputManager&) = delete;
    UInputManager& operator=(const UInputManager&) = delete;

public:
    // 싱글톤 접근자
    static UInputManager& GetInstance();

    // 생명주기
    void Initialize(HWND hWindow);
    void Update(); // 매 프레임 호출
    void ProcessMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    // 마우스 함수들
    FVector2D GetMousePosition() const { return MousePosition; }
    FVector2D GetMouseDelta() const { return MousePosition - PreviousMousePosition; }
    // 화면 크기 (픽셀) - 매 호출 시 동적 조회
    FVector2D GetScreenSize() const;

	void SetLastMousePosition(const FVector2D& Pos) { PreviousMousePosition = Pos; }

    bool IsMouseButtonDown(EMouseButton Button) const;
    bool IsMouseButtonPressed(EMouseButton Button) const; // 이번 프레임에 눌림
    bool IsMouseButtonReleased(EMouseButton Button) const; // 이번 프레임에 떼짐

    // 키보드 함수들
    bool IsKeyDown(int KeyCode) const;
    bool IsKeyPressed(int KeyCode) const; // 이번 프레임에 눌림
    bool IsKeyReleased(int KeyCode) const; // 이번 프레임에 떼짐

    // 마우스 휠 함수들
    float GetMouseWheelDelta() const { return MouseWheelDelta; }

    // === 게임패드 함수들 ===

    // 게임패드 연결 상태
    bool IsGamepadConnected(int GamepadIndex) const;
    int GetConnectedGamepadCount() const;

    // 어떤 게임패드에서 입력이 왔는지 확인 (-1이면 입력 없음)
    int32 GetGamepadWithAnyButtonPressed() const;
    int32 GetGamepadWithAnyInput() const;  // 버튼 + 스틱 + 트리거

    // 게임패드 버튼 (특정 게임패드)
    bool IsGamepadButtonDown(int GamepadIndex, EGamepadButton Button) const;
    bool IsGamepadButtonPressed(int GamepadIndex, EGamepadButton Button) const;
    bool IsGamepadButtonReleased(int GamepadIndex, EGamepadButton Button) const;

    // 게임패드 스틱 (-1.0 ~ 1.0, 데드존 적용됨)
    float GetGamepadLeftStickX(int GamepadIndex) const;
    float GetGamepadLeftStickY(int GamepadIndex) const;
    float GetGamepadRightStickX(int GamepadIndex) const;
    float GetGamepadRightStickY(int GamepadIndex) const;

    // 게임패드 트리거 (0.0 ~ 1.0)
    float GetGamepadLeftTrigger(int GamepadIndex) const;
    float GetGamepadRightTrigger(int GamepadIndex) const;

    // 진동 설정 (0.0 ~ 1.0)
    void SetGamepadVibration(int GamepadIndex, float LeftMotor, float RightMotor);
    void StopGamepadVibration(int GamepadIndex);

    // 데드존 설정
    void SetStickDeadzone(float Deadzone) { StickDeadzone = Deadzone; }
    float GetStickDeadzone() const { return StickDeadzone; }
    void SetTriggerThreshold(float Threshold) { TriggerThreshold = Threshold; }
    float GetTriggerThreshold() const { return TriggerThreshold; }

    // === 게임패드 → 키보드 자동 매핑 ===
    // 활성화하면 게임패드 입력이 자동으로 키보드 입력으로 인식됨
    // 등록된 게임패드 인덱스 기반으로 매핑
    void SetGamepadToKeyboardMapping(bool bEnable) { bGamepadToKeyboardMapping = bEnable; }
    bool IsGamepadToKeyboardMappingEnabled() const { return bGamepadToKeyboardMapping; }

    // === 플레이어-게임패드 동적 등록 ===
    // "Press A to join" 방식으로 플레이어 등록
    // 반환값: 등록 성공 여부
    bool RegisterGamepadForPlayer(int PlayerIndex, int GamepadIndex);
    void UnregisterGamepadForPlayer(int PlayerIndex);
    void UnregisterAllGamepads();

    // 등록된 게임패드 인덱스 조회 (-1이면 미등록)
    int GetRegisteredGamepadForPlayer(int PlayerIndex) const;

    // 해당 게임패드가 이미 등록되어 있는지 확인 (-1이면 미등록)
    int GetPlayerForGamepad(int GamepadIndex) const;

    // 버튼을 누른 게임패드를 자동으로 플레이어에 등록 (미등록 플레이어 우선)
    // 반환값: 등록된 플레이어 인덱스 (-1이면 등록 실패 또는 입력 없음)
    int TryRegisterGamepadFromInput();

    // 스틱 → 키 매핑 임계값 (기본 0.5)
    void SetStickToKeyThreshold(float Threshold) { StickToKeyThreshold = Threshold; }
    float GetStickToKeyThreshold() const { return StickToKeyThreshold; }

    // 디버그 로그 토글
    void SetDebugLoggingEnabled(bool bEnabled) { bEnableDebugLogging = bEnabled; }
    bool IsDebugLoggingEnabled() const { return bEnableDebugLogging; }

    bool GetIsGizmoDragging() const { return bIsGizmoDragging; }
    void SetIsGizmoDragging(bool bInGizmoDragging) { bIsGizmoDragging = bInGizmoDragging; }

    uint32 GetDraggingAxis() const { return DraggingAxis; }
    void SetDraggingAxis(uint32 Axis) { DraggingAxis = Axis; }

    // 커서 제어 함수
    void SetCursorVisible(bool bVisible);
    void LockCursor();
    void ReleaseCursor();
    // Lock position helper: move lock to client center and warp cursor there
    void LockCursorToCenter();
    bool IsCursorLocked() const { return bIsCursorLocked; }

    // 뷰포트 윈도우 체크 콜백 등록 (순환 참조 방지)
    using ViewportCheckCallback = bool(*)(const FVector2D&);
    void SetViewportCheckCallback(ViewportCheckCallback Callback) { ViewportChecker = Callback; }

    // 게임플레이 입력 활성화/비활성화 (WASD, 점프 등)
    // 시스템 입력(F11 등)은 항상 작동
    void SetGameplayInputEnabled(bool bEnabled) { bGameplayInputEnabled = bEnabled; }
    bool IsGameplayInputEnabled() const { return bGameplayInputEnabled; }

private:
    // 내부 헬퍼 함수들
    void UpdateMousePosition(int X, int Y);
    void UpdateMouseButton(EMouseButton Button, bool bPressed);
    void UpdateKeyState(int KeyCode, bool bPressed);
    void UpdateGamepadStates();
    float ApplyStickDeadzone(SHORT Value, float Deadzone) const;

    // 윈도우 핸들
    HWND WindowHandle;

    // 마우스 상태
    FVector2D MousePosition;
    FVector2D PreviousMousePosition;
    // 스크린/뷰포트 사이즈 (클라이언트 영역 픽셀)
    FVector2D ScreenSize;
    bool MouseButtons[MaxMouseButtons];
    bool PreviousMouseButtons[MaxMouseButtons];

    // 마우스 휠 상태
    float MouseWheelDelta;

    // 키보드 상태 (Virtual Key Code 기준)
    bool KeyStates[256];
    bool PreviousKeyStates[256];

    // 마스터 디버그 로그 온/오프
    bool bEnableDebugLogging = false;

    bool bIsGizmoDragging = false;
    uint32 DraggingAxis = 0;

    // 커서 잠금 상태
    bool bIsCursorLocked = false;
    FVector2D LockedCursorPosition; // 우클릭한 위치 (기준점)

    // 뷰포트 윈도우 체크 콜백
    ViewportCheckCallback ViewportChecker = nullptr;

    // 게임플레이 입력 활성화 여부 (WASD, 점프 등)
    bool bGameplayInputEnabled = true;

    // === 게임패드 상태 ===
    static constexpr int MaxGamepads = 4;
    FGamepadState GamepadStates[MaxGamepads];

    // 데드존 설정
    float StickDeadzone = 0.24f;   // Xbox 권장 데드존 (XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE / 32767)
    float TriggerThreshold = 0.12f; // 트리거 임계값 (XINPUT_GAMEPAD_TRIGGER_THRESHOLD / 255)

    // 게임패드 → 키보드 매핑 설정
    bool bGamepadToKeyboardMapping = true;  // 기본 활성화
    float StickToKeyThreshold = 0.5f;       // 스틱이 이 값 이상이면 키 입력으로 인식

    // 플레이어별 등록된 게임패드 인덱스 (-1이면 미등록)
    static constexpr int MaxPlayers = 2;
    int RegisteredGamepads[MaxPlayers] = { -1, -1 };

    // 게임패드 입력이 특정 키에 매핑되는지 확인 (내부 헬퍼)
    bool IsGamepadMappedToKey(int KeyCode, bool bCheckPressed = false, bool bCheckReleased = false) const;
};
