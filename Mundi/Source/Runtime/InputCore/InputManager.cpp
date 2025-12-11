#include "pch.h"
#include <windowsx.h> // GET_X_LPARAM / GET_Y_LPARAM

#pragma comment(lib, "xinput.lib")

#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#endif
#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif

IMPLEMENT_CLASS(UInputManager)

UInputManager::UInputManager()
    : WindowHandle(nullptr)
    , MousePosition(0.0f, 0.0f)
    , PreviousMousePosition(0.0f, 0.0f)
    , ScreenSize(1.0f, 1.0f)
    , MouseWheelDelta(0.0f)
{
    // 배열 초기화
    memset(MouseButtons, false, sizeof(MouseButtons));
    memset(PreviousMouseButtons, false, sizeof(PreviousMouseButtons));
    memset(KeyStates, false, sizeof(KeyStates));
    memset(PreviousKeyStates, false, sizeof(PreviousKeyStates));
}

UInputManager::~UInputManager()
{
}

UInputManager& UInputManager::GetInstance()
{
    static UInputManager* Instance = nullptr;
    if (Instance == nullptr)
    {
        Instance = NewObject<UInputManager>();
    }
    return *Instance;
}

void UInputManager::Initialize(HWND hWindow)
{
    WindowHandle = hWindow;
    
    // 현재 마우스 위치 가져오기
    POINT CursorPos;
    GetCursorPos(&CursorPos);
    ScreenToClient(WindowHandle, &CursorPos);
    
    MousePosition.X = static_cast<float>(CursorPos.x);
    MousePosition.Y = static_cast<float>(CursorPos.y);
    PreviousMousePosition = MousePosition;

    // 초기 스크린 사이즈 설정 (클라이언트 영역)
    if (WindowHandle)
    {
        RECT rc{};
        if (GetClientRect(WindowHandle, &rc))
        {
            float w = static_cast<float>(rc.right - rc.left);
            float h = static_cast<float>(rc.bottom - rc.top);
            ScreenSize.X = (w > 0.0f) ? w : 1.0f;
            ScreenSize.Y = (h > 0.0f) ? h : 1.0f;
        }
    }
}

void UInputManager::Update()
{
    // 마우스 휠 델타 초기화 (프레임마다 리셋)
    MouseWheelDelta = 0.0f;

    // 게임패드 상태 업데이트
    UpdateGamepadStates();

    // 매 프레임마다 실시간 마우스 위치 업데이트
    if (WindowHandle)
    {
        POINT CursorPos;
        if (GetCursorPos(&CursorPos))
        {
            ScreenToClient(WindowHandle, &CursorPos);

            // 커서 잠금 모드: 무한 드래그 처리
            // ImGui가 마우스를 사용 중이면 커서 잠금 모드를 비활성화
            bool bImGuiWantsMouse = (ImGui::GetCurrentContext() != nullptr) && ImGui::GetIO().WantCaptureMouse;
            if (bIsCursorLocked && !bImGuiWantsMouse)
            {
                MousePosition.X = static_cast<float>(CursorPos.x);
                MousePosition.Y = static_cast<float>(CursorPos.y);

                POINT lockedPoint = { static_cast<int>(LockedCursorPosition.X), static_cast<int>(LockedCursorPosition.Y) };
                ClientToScreen(WindowHandle, &lockedPoint);
                SetCursorPos(lockedPoint.x, lockedPoint.y);

                PreviousMousePosition = LockedCursorPosition;
            }
            else
            {
                PreviousMousePosition = MousePosition;
                MousePosition.X = static_cast<float>(CursorPos.x);
                MousePosition.Y = static_cast<float>(CursorPos.y);
            }
        }

        // 화면 크기 갱신 (윈도우 리사이즈 대응)
        RECT Rectangle{};
        if (GetClientRect(WindowHandle, &Rectangle))
        {
            float w = static_cast<float>(Rectangle.right - Rectangle.left);
            float h = static_cast<float>(Rectangle.bottom - Rectangle.top);
            ScreenSize.X = (w > 0.0f) ? w : 1.0f;
            ScreenSize.Y = (h > 0.0f) ? h : 1.0f;
        }
    }

    memcpy(PreviousMouseButtons, MouseButtons, sizeof(MouseButtons));
    memcpy(PreviousKeyStates, KeyStates, sizeof(KeyStates));
}

void UInputManager::ProcessMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    bool IsUIHover = false;
    bool IsKeyBoardCapture = false;

    if (ImGui::GetCurrentContext() != nullptr)
    {
        // ImGui가 입력을 사용 중인지 확인
        ImGuiIO& io = ImGui::GetIO();
        static bool once = false;
        if (!once) { io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; once = true; }

        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        // skeletal mesh 뷰어와 파티클 뷰어도 ImGui로 만들어져서 뷰포트에 인풋이 안먹히는 현상이 일어남.
        // 콜백을 통해 마우스 위치가 뷰포트 윈도우 안에 있는지 체크
        bool bIsViewportWindow = false;
        if (ViewportChecker)
        {
            bIsViewportWindow = ViewportChecker(MousePosition);
        }

        // 뷰포트 윈도우가 아닐 때만 UI hover 체크
        if (!bIsViewportWindow)
        {
            bool bAnyItemHovered = ImGui::IsAnyItemHovered();
            // WantCaptureMouse는 마우스가 UI 위에 있거나 UI가 마우스를 사용 중일 때 true
            IsUIHover = io.WantCaptureMouse || io.WantCaptureMouseUnlessPopupClose || ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow);
        }

        IsKeyBoardCapture = io.WantTextInput;
        
        // 디버그 출력 (마우스 클릭 시만)
        if (bEnableDebugLogging && message == WM_LBUTTONDOWN)
        {
            char debugMsg[128];
            sprintf_s(debugMsg, "ImGui State - WantMouse: %d, WantKeyboard: %d\n", 
                      IsUIHover, IsKeyBoardCapture);
            UE_LOG(debugMsg);
        }
    }
    else
    {
        // ImGui 컨텍스트가 없으면 게임 입력을 허용
        if (bEnableDebugLogging && message == WM_LBUTTONDOWN)
        {
            UE_LOG("ImGui context not initialized - allowing game input\n");
        }
    }

    switch (message)
    {
    case WM_MOUSEMOVE:
        {
            // 항상 마우스 위치는 업데이트 (ImGui 상관없이)
            // 좌표는 16-bit signed. 반드시 GET_X/Y_LPARAM으로 부호 확장해야 함.
            int MouseX = GET_X_LPARAM(lParam);
            int MouseY = GET_Y_LPARAM(lParam);
            UpdateMousePosition(MouseX, MouseY);
        }
        break;
    case WM_SIZE:
        {
            // 클라이언트 영역 크기 업데이트
            int w = LOWORD(lParam);
            int h = HIWORD(lParam);
            if (w <= 0) w = 1;
            if (h <= 0) h = 1;
            ScreenSize.X = static_cast<float>(w);
            ScreenSize.Y = static_cast<float>(h);
        }
        break;
        
    case WM_LBUTTONDOWN:
        if (!IsUIHover)  // ImGui가 마우스를 사용하지 않을 때만
        {
            UpdateMouseButton(LeftButton, true);
            if (bEnableDebugLogging) UE_LOG("InputManager: Left Mouse Down\n");
        }
        break;
        
    case WM_LBUTTONUP:
        if (!IsUIHover)  // ImGui가 마우스를 사용하지 않을 때만
        {
            UpdateMouseButton(LeftButton, false);
            if (bEnableDebugLogging) UE_LOG("InputManager: Left Mouse UP\n");
        }
        break;
        
    case WM_RBUTTONDOWN:
        if (!IsUIHover)
        {
            UpdateMouseButton(RightButton, true);
            if (bEnableDebugLogging) UE_LOG("InputManager: Right Mouse DOWN");
        }
        else
        {
            if (bEnableDebugLogging) UE_LOG("InputManager: Right Mouse DOWN blocked by ImGui");
        }
        break;
        
    case WM_RBUTTONUP:
        // 마우스 버튼 해제는 항상 처리 (드래그 중 UI 위에서 놓아도 해제되어야 함)
        UpdateMouseButton(RightButton, false);
        if (bEnableDebugLogging) UE_LOG("InputManager: Right Mouse UP");
        break;
        
    case WM_MBUTTONDOWN:
        if (!IsUIHover)
        {
            UpdateMouseButton(MiddleButton, true);
        }
        break;
        
    case WM_MBUTTONUP:
        if (!IsUIHover)
        {
            UpdateMouseButton(MiddleButton, false);
        }
        break;
        
    case WM_XBUTTONDOWN:
        if (!IsUIHover)
        {
            // X버튼 구분 (X1, X2)
            WORD XButton = GET_XBUTTON_WPARAM(wParam);
            if (XButton == XBUTTON1)
                UpdateMouseButton(XButton1, true);
            else if (XButton == XBUTTON2)
                UpdateMouseButton(XButton2, true);
        }
        break;
        
    case WM_XBUTTONUP:
        if (!IsUIHover)
        {
            // X버튼 구분 (X1, X2)
            WORD XButton = GET_XBUTTON_WPARAM(wParam);
            if (XButton == XBUTTON1)
                UpdateMouseButton(XButton1, false);
            else if (XButton == XBUTTON2)
                UpdateMouseButton(XButton2, false);
        }
        break;

    case WM_MOUSEWHEEL:
        if (!IsUIHover)
        {
            // 휠 델타값 추출 (HIWORD에서 signed short로 캐스팅)
            short wheelDelta = GET_WHEEL_DELTA_WPARAM(wParam);
            MouseWheelDelta = static_cast<float>(wheelDelta) / WHEEL_DELTA; // 정규화 (-1.0 ~ 1.0)

            if (bEnableDebugLogging)
            {
                char debugMsg[64];
                sprintf_s(debugMsg, "InputManager: Mouse Wheel - Delta: %.2f\n", MouseWheelDelta);
                UE_LOG(debugMsg);
            }
        }
        break;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (!IsKeyBoardCapture && !IsUIHover)  // ImGui가 키보드를 사용하지 않을 때만
        {
            // Virtual Key Code 추출
            int KeyCode = static_cast<int>(wParam);
            UpdateKeyState(KeyCode, true);
            
            // 디버그 출력
            if (bEnableDebugLogging)
            {
                char debugMsg[64];
                sprintf_s(debugMsg, "InputManager: Key Down - %d\n", KeyCode);
                UE_LOG(debugMsg);
            }
        }
        break;
        
    case WM_KEYUP:
    case WM_SYSKEYUP:
        if (!IsKeyBoardCapture)  // ImGui가 키보드를 사용하지 않을 때만
        {
            // Virtual Key Code 추출
            int KeyCode = static_cast<int>(wParam);
            UpdateKeyState(KeyCode, false);
            
            // 디버그 출력
            if (bEnableDebugLogging)
            {
                char debugMsg[64];
                sprintf_s(debugMsg, "InputManager: Key UP - %d\n", KeyCode);
                UE_LOG(debugMsg);
            }
        }
        break;
    }

}

bool UInputManager::IsMouseButtonDown(EMouseButton Button) const
{
    if (Button >= MaxMouseButtons) return false;
    return MouseButtons[Button];
}

bool UInputManager::IsMouseButtonPressed(EMouseButton Button) const
{
    if (Button >= MaxMouseButtons) return false;
    
    bool currentState = MouseButtons[Button];
    bool previousState = PreviousMouseButtons[Button];
    bool isPressed = currentState && !previousState;
    
    // 디버그 출력 추가
    if (bEnableDebugLogging && Button == LeftButton && (currentState || previousState))
    {
        char debugMsg[128];
        sprintf_s(debugMsg, "IsPressed: Current=%d, Previous=%d, Result=%d\n", 
                  currentState, previousState, isPressed);
        UE_LOG(debugMsg);
    }
    
    return isPressed;
}

bool UInputManager::IsMouseButtonReleased(EMouseButton Button) const
{
    if (Button >= MaxMouseButtons) return false;
    return !MouseButtons[Button] && PreviousMouseButtons[Button];
}

bool UInputManager::IsKeyDown(int KeyCode) const
{
    if (KeyCode < 0 || KeyCode >= 256) return false;

    // 키보드 상태 체크
    if (KeyStates[KeyCode]) return true;

    // 게임패드 → 키보드 매핑 체크
    if (bGamepadToKeyboardMapping && IsGamepadMappedToKey(KeyCode, false, false))
    {
        return true;
    }

    return false;
}

bool UInputManager::IsKeyPressed(int KeyCode) const
{
    if (KeyCode < 0 || KeyCode >= 256) return false;

    // 키보드 상태 체크
    if (KeyStates[KeyCode] && !PreviousKeyStates[KeyCode]) return true;

    // 게임패드 → 키보드 매핑 체크 (Pressed)
    if (bGamepadToKeyboardMapping && IsGamepadMappedToKey(KeyCode, true, false))
    {
        return true;
    }

    return false;
}

bool UInputManager::IsKeyReleased(int KeyCode) const
{
    if (KeyCode < 0 || KeyCode >= 256) return false;

    // 키보드 상태 체크
    if (!KeyStates[KeyCode] && PreviousKeyStates[KeyCode]) return true;

    // 게임패드 → 키보드 매핑 체크 (Released)
    if (bGamepadToKeyboardMapping && IsGamepadMappedToKey(KeyCode, false, true))
    {
        return true;
    }

    return false;
}

bool UInputManager::IsAnyKeyPressed() const
{
    // 키보드 체크
    for (int i = 0; i < 256; ++i)
    {
        if (KeyStates[i] && !PreviousKeyStates[i]) return true;
    }

    // 게임패드 버튼 체크
    static const EGamepadButton GamepadButtons[] = {
        EGamepadButton::A, EGamepadButton::B, EGamepadButton::X, EGamepadButton::Y,
        EGamepadButton::Start, EGamepadButton::Back,
        EGamepadButton::LeftShoulder, EGamepadButton::RightShoulder,
        EGamepadButton::LeftThumb, EGamepadButton::RightThumb,
        EGamepadButton::DPadUp, EGamepadButton::DPadDown,
        EGamepadButton::DPadLeft, EGamepadButton::DPadRight
    };
    for (int g = 0; g < MaxGamepads; ++g)
    {
        if (!IsGamepadConnected(g)) continue;
        for (EGamepadButton btn : GamepadButtons)
        {
            if (IsGamepadButtonPressed(g, btn)) return true;
        }
    }

    // 마우스 버튼 체크
    for (int i = 0; i < MaxMouseButtons; ++i)
    {
        if (MouseButtons[i] && !PreviousMouseButtons[i]) return true;
    }
    return false;
}

void UInputManager::UpdateMousePosition(int X, int Y)
{
    MousePosition.X = static_cast<float>(X);
    MousePosition.Y = static_cast<float>(Y);
}

void UInputManager::UpdateMouseButton(EMouseButton Button, bool bPressed)
{
    if (Button < MaxMouseButtons)
    {
        MouseButtons[Button] = bPressed;
    }
}

void UInputManager::UpdateKeyState(int KeyCode, bool bPressed)
{
    if (KeyCode >= 0 && KeyCode < 256)
    {
        KeyStates[KeyCode] = bPressed;
    }
}

FVector2D UInputManager::GetScreenSize() const
{
    // 우선 ImGui 컨텍스트가 있으면 IO의 DisplaySize 사용
    if (ImGui::GetCurrentContext() != nullptr)
    {
        const ImGuiIO& io = ImGui::GetIO();
        float w = io.DisplaySize.x;
        float h = io.DisplaySize.y;
        if (w > 1.0f && h > 1.0f)
        {
            return FVector2D(w, h);
        }
    }

    // 윈도우 클라이언트 영역 쿼리
    HWND hwnd = WindowHandle ? WindowHandle : GetActiveWindow();
    if (hwnd)
    {
        RECT rc{};
        if (GetClientRect(hwnd, &rc))
        {
            float w = static_cast<float>(rc.right - rc.left);
            float h = static_cast<float>(rc.bottom - rc.top);
            if (w <= 0.0f) w = 1.0f;
            if (h <= 0.0f) h = 1.0f;
            return FVector2D(w, h);
        }
    }
    return FVector2D(1.0f, 1.0f);
}

void UInputManager::SetCursorVisible(bool bVisible)
{
    if (bVisible)
    {
        while (ShowCursor(TRUE) < 0);
    }
    else
    {
        while (ShowCursor(FALSE) >= 0);
    }
}

void UInputManager::LockCursor()
{
    if (!WindowHandle) return;

    // 마우스 커서를 화면의 중점으로 고정
    RECT rc{};
    if (GetClientRect(WindowHandle, &rc))
    {
        const int centerX = rc.left + (rc.right - rc.left) / 2;
        const int centerY = rc.top + (rc.bottom - rc.top) / 2;

        LockedCursorPosition = FVector2D(static_cast<float>(centerX), static_cast<float>(centerY)); 
    }

    // 현재 커서 위치를 기준점으로 저장
   /* POINT currentCursor;
    if (GetCursorPos(&currentCursor))
    {
        ScreenToClient(WindowHandle, &currentCursor);
        LockedCursorPosition = FVector2D(static_cast<float>(currentCursor.x), static_cast<float>(currentCursor.y));
    }*/

    // 잠금 상태 설정
    bIsCursorLocked = true;

    // 마우스 위치 동기화 (델타 계산을 위해)
    MousePosition = LockedCursorPosition;
    PreviousMousePosition = LockedCursorPosition;
}

void UInputManager::ReleaseCursor()
{
    if (!WindowHandle) return;

    // 잠금 해제
    bIsCursorLocked = false;

    // 원래 커서 위치로 복원
    POINT lockedPoint = { static_cast<int>(LockedCursorPosition.X), static_cast<int>(LockedCursorPosition.Y) };
    ClientToScreen(WindowHandle, &lockedPoint);
    SetCursorPos(lockedPoint.x, lockedPoint.y);

    // 마우스 위치 동기화
    MousePosition = LockedCursorPosition;
    PreviousMousePosition = LockedCursorPosition;
}

void UInputManager::LockCursorToCenter()
{
    if (!WindowHandle) return;

    RECT rc{};
    if (GetClientRect(WindowHandle, &rc))
    {
        const int centerX = rc.left + (rc.right - rc.left) / 2;
        const int centerY = rc.top + (rc.bottom - rc.top) / 2;

        LockedCursorPosition = FVector2D(static_cast<float>(centerX), static_cast<float>(centerY));

        POINT screenPt{ centerX, centerY };
        ClientToScreen(WindowHandle, &screenPt);
        SetCursorPos(screenPt.x, screenPt.y);

        // 동기화 (첫 프레임 델타 0)
        MousePosition = LockedCursorPosition;
        PreviousMousePosition = LockedCursorPosition;
    }
}

// === 게임패드 구현 ===

void UInputManager::UpdateGamepadStates()
{
    for (int i = 0; i < MaxGamepads; ++i)
    {
        FGamepadState& State = GamepadStates[i];

        // 이전 상태 저장
        State.PreviousState = State.CurrentState;

        // XInput 상태 조회
        ZeroMemory(&State.CurrentState, sizeof(XINPUT_STATE));
        DWORD Result = XInputGetState(i, &State.CurrentState);

        State.bConnected = (Result == ERROR_SUCCESS);

        if (State.bConnected)
        {
            const XINPUT_GAMEPAD& Gamepad = State.CurrentState.Gamepad;

            // 스틱 값 계산 (데드존 적용)
            State.LeftStickX = ApplyStickDeadzone(Gamepad.sThumbLX, StickDeadzone);
            State.LeftStickY = ApplyStickDeadzone(Gamepad.sThumbLY, StickDeadzone);
            State.RightStickX = ApplyStickDeadzone(Gamepad.sThumbRX, StickDeadzone);
            State.RightStickY = ApplyStickDeadzone(Gamepad.sThumbRY, StickDeadzone);

            // 트리거 값 계산 (0.0 ~ 1.0)
            float LeftTriggerNorm = static_cast<float>(Gamepad.bLeftTrigger) / 255.0f;
            float RightTriggerNorm = static_cast<float>(Gamepad.bRightTrigger) / 255.0f;

            State.LeftTrigger = (LeftTriggerNorm > TriggerThreshold) ? LeftTriggerNorm : 0.0f;
            State.RightTrigger = (RightTriggerNorm > TriggerThreshold) ? RightTriggerNorm : 0.0f;
        }
        else
        {
            // 연결 안됨 - 값 초기화
            State.LeftStickX = State.LeftStickY = 0.0f;
            State.RightStickX = State.RightStickY = 0.0f;
            State.LeftTrigger = State.RightTrigger = 0.0f;
        }
    }
}

float UInputManager::ApplyStickDeadzone(SHORT Value, float Deadzone) const
{
    // -32768 ~ 32767 범위를 -1.0 ~ 1.0으로 정규화
    float Normalized = static_cast<float>(Value) / 32767.0f;

    // 데드존 적용
    float AbsValue = std::abs(Normalized);
    if (AbsValue < Deadzone)
    {
        return 0.0f;
    }

    // 데드존 이후 영역을 0~1로 리매핑
    float Sign = (Normalized > 0.0f) ? 1.0f : -1.0f;
    float Remapped = (AbsValue - Deadzone) / (1.0f - Deadzone);
    return Sign * Remapped;
}

bool UInputManager::IsGamepadConnected(int GamepadIndex) const
{
    if (GamepadIndex < 0 || GamepadIndex >= MaxGamepads) return false;
    return GamepadStates[GamepadIndex].bConnected;
}

int UInputManager::GetConnectedGamepadCount() const
{
    int Count = 0;
    for (int i = 0; i < MaxGamepads; ++i)
    {
        if (GamepadStates[i].bConnected) ++Count;
    }
    return Count;
}

int32 UInputManager::GetGamepadWithAnyButtonPressed() const
{
    for (int i = 0; i < MaxGamepads; ++i)
    {
        if (!GamepadStates[i].bConnected) continue;

        // 이번 프레임에 새로 눌린 버튼이 있는지 확인
        WORD CurrentButtons = GamepadStates[i].CurrentState.Gamepad.wButtons;
        WORD PreviousButtons = GamepadStates[i].PreviousState.Gamepad.wButtons;

        // 새로 눌린 버튼 = 현재 눌려있고 이전엔 안 눌렸던 것
        if ((CurrentButtons & ~PreviousButtons) != 0)
        {
            return i;
        }
    }
    return -1;
}

int32 UInputManager::GetGamepadWithAnyInput() const
{
    // 먼저 버튼 입력 확인
    int32 ButtonPad = GetGamepadWithAnyButtonPressed();
    if (ButtonPad >= 0) return ButtonPad;

    // 스틱이나 트리거 입력 확인
    for (int i = 0; i < MaxGamepads; ++i)
    {
        if (!GamepadStates[i].bConnected) continue;

        const FGamepadState& State = GamepadStates[i];

        // 스틱 입력 (데드존 적용된 값이 0이 아니면)
        if (std::abs(State.LeftStickX) > 0.5f || std::abs(State.LeftStickY) > 0.5f ||
            std::abs(State.RightStickX) > 0.5f || std::abs(State.RightStickY) > 0.5f)
        {
            return i;
        }

        // 트리거 입력
        if (State.LeftTrigger > 0.5f || State.RightTrigger > 0.5f)
        {
            return i;
        }
    }
    return -1;
}

bool UInputManager::IsGamepadButtonDown(int GamepadIndex, EGamepadButton Button) const
{
    if (GamepadIndex < 0 || GamepadIndex >= MaxGamepads) return false;
    if (!GamepadStates[GamepadIndex].bConnected) return false;

    WORD ButtonMask = static_cast<WORD>(Button);
    return (GamepadStates[GamepadIndex].CurrentState.Gamepad.wButtons & ButtonMask) != 0;
}

bool UInputManager::IsGamepadButtonPressed(int GamepadIndex, EGamepadButton Button) const
{
    if (GamepadIndex < 0 || GamepadIndex >= MaxGamepads) return false;
    if (!GamepadStates[GamepadIndex].bConnected) return false;

    WORD ButtonMask = static_cast<WORD>(Button);
    bool Current = (GamepadStates[GamepadIndex].CurrentState.Gamepad.wButtons & ButtonMask) != 0;
    bool Previous = (GamepadStates[GamepadIndex].PreviousState.Gamepad.wButtons & ButtonMask) != 0;
    return Current && !Previous;
}

bool UInputManager::IsGamepadButtonReleased(int GamepadIndex, EGamepadButton Button) const
{
    if (GamepadIndex < 0 || GamepadIndex >= MaxGamepads) return false;
    if (!GamepadStates[GamepadIndex].bConnected) return false;

    WORD ButtonMask = static_cast<WORD>(Button);
    bool Current = (GamepadStates[GamepadIndex].CurrentState.Gamepad.wButtons & ButtonMask) != 0;
    bool Previous = (GamepadStates[GamepadIndex].PreviousState.Gamepad.wButtons & ButtonMask) != 0;
    return !Current && Previous;
}

float UInputManager::GetGamepadLeftStickX(int GamepadIndex) const
{
    if (GamepadIndex < 0 || GamepadIndex >= MaxGamepads) return 0.0f;
    return GamepadStates[GamepadIndex].LeftStickX;
}

float UInputManager::GetGamepadLeftStickY(int GamepadIndex) const
{
    if (GamepadIndex < 0 || GamepadIndex >= MaxGamepads) return 0.0f;
    return GamepadStates[GamepadIndex].LeftStickY;
}

float UInputManager::GetGamepadRightStickX(int GamepadIndex) const
{
    if (GamepadIndex < 0 || GamepadIndex >= MaxGamepads) return 0.0f;
    return GamepadStates[GamepadIndex].RightStickX;
}

float UInputManager::GetGamepadRightStickY(int GamepadIndex) const
{
    if (GamepadIndex < 0 || GamepadIndex >= MaxGamepads) return 0.0f;
    return GamepadStates[GamepadIndex].RightStickY;
}

float UInputManager::GetGamepadLeftTrigger(int GamepadIndex) const
{
    if (GamepadIndex < 0 || GamepadIndex >= MaxGamepads) return 0.0f;
    return GamepadStates[GamepadIndex].LeftTrigger;
}

float UInputManager::GetGamepadRightTrigger(int GamepadIndex) const
{
    if (GamepadIndex < 0 || GamepadIndex >= MaxGamepads) return 0.0f;
    return GamepadStates[GamepadIndex].RightTrigger;
}

void UInputManager::SetGamepadVibration(int GamepadIndex, float LeftMotor, float RightMotor)
{
    if (GamepadIndex < 0 || GamepadIndex >= MaxGamepads) return;
    if (!GamepadStates[GamepadIndex].bConnected) return;

    XINPUT_VIBRATION Vibration;
    Vibration.wLeftMotorSpeed = static_cast<WORD>(std::clamp(LeftMotor, 0.0f, 1.0f) * 65535.0f);
    Vibration.wRightMotorSpeed = static_cast<WORD>(std::clamp(RightMotor, 0.0f, 1.0f) * 65535.0f);
    XInputSetState(GamepadIndex, &Vibration);
}

void UInputManager::StopGamepadVibration(int GamepadIndex)
{
    SetGamepadVibration(GamepadIndex, 0.0f, 0.0f);
}

// === 플레이어-게임패드 동적 등록 구현 ===

bool UInputManager::RegisterGamepadForPlayer(int PlayerIndex, int GamepadIndex)
{
    if (PlayerIndex < 0 || PlayerIndex >= MaxPlayers) return false;
    if (GamepadIndex < 0 || GamepadIndex >= MaxGamepads) return false;
    if (!GamepadStates[GamepadIndex].bConnected) return false;

    // 이미 다른 플레이어가 사용 중인지 확인
    for (int i = 0; i < MaxPlayers; ++i)
    {
        if (RegisteredGamepads[i] == GamepadIndex)
        {
            return false; // 이미 등록됨
        }
    }

    RegisteredGamepads[PlayerIndex] = GamepadIndex;
    return true;
}

void UInputManager::UnregisterGamepadForPlayer(int PlayerIndex)
{
    if (PlayerIndex < 0 || PlayerIndex >= MaxPlayers) return;
    RegisteredGamepads[PlayerIndex] = -1;
}

void UInputManager::UnregisterAllGamepads()
{
    for (int i = 0; i < MaxPlayers; ++i)
    {
        RegisteredGamepads[i] = -1;
    }
}

int UInputManager::GetRegisteredGamepadForPlayer(int PlayerIndex) const
{
    if (PlayerIndex < 0 || PlayerIndex >= MaxPlayers) return -1;
    return RegisteredGamepads[PlayerIndex];
}

int UInputManager::GetPlayerForGamepad(int GamepadIndex) const
{
    if (GamepadIndex < 0 || GamepadIndex >= MaxGamepads) return -1;
    for (int i = 0; i < MaxPlayers; ++i)
    {
        if (RegisteredGamepads[i] == GamepadIndex)
        {
            return i;
        }
    }
    return -1;
}

int UInputManager::TryRegisterGamepadFromInput()
{
    int32 PadIndex = GetGamepadWithAnyButtonPressed();
    if (PadIndex < 0) return -1;

    // 이미 등록된 게임패드인지 확인
    int ExistingPlayer = GetPlayerForGamepad(PadIndex);
    if (ExistingPlayer >= 0) return -1; // 이미 등록됨

    // 미등록 플레이어 찾기 (0번부터)
    for (int i = 0; i < MaxPlayers; ++i)
    {
        if (RegisteredGamepads[i] < 0)
        {
            if (RegisterGamepadForPlayer(i, PadIndex))
            {
                UE_LOG("Gamepad %d registered to Player %d (Connected: %d)\n", PadIndex, i, GetConnectedGamepadCount());
                return i; // 등록 성공, 플레이어 인덱스 반환
            }
        }
    }

    return -1; // 빈 슬롯 없음
}

bool UInputManager::IsGamepadMappedToKey(int KeyCode, bool bCheckPressed, bool bCheckReleased) const
{
    // ===== Player 1 (등록된 게임패드) → WASD + 액션키 =====
    int Pad0Index = RegisteredGamepads[0];
    if (Pad0Index >= 0 && Pad0Index < MaxGamepads && GamepadStates[Pad0Index].bConnected)
    {
        const FGamepadState& Pad0 = GamepadStates[Pad0Index];

        // 현재/이전 스틱 값
        float CurrLX = Pad0.LeftStickX;
        float CurrLY = Pad0.LeftStickY;

        // 이전 프레임 스틱 값 계산 (PreviousState에서)
        float PrevLX = ApplyStickDeadzone(Pad0.PreviousState.Gamepad.sThumbLX, StickDeadzone);
        float PrevLY = ApplyStickDeadzone(Pad0.PreviousState.Gamepad.sThumbLY, StickDeadzone);

        // 스틱 → WASD 매핑
        bool bCurrW = (CurrLY > StickToKeyThreshold);
        bool bCurrS = (CurrLY < -StickToKeyThreshold);
        bool bCurrA = (CurrLX < -StickToKeyThreshold);
        bool bCurrD = (CurrLX > StickToKeyThreshold);

        bool bPrevW = (PrevLY > StickToKeyThreshold);
        bool bPrevS = (PrevLY < -StickToKeyThreshold);
        bool bPrevA = (PrevLX < -StickToKeyThreshold);
        bool bPrevD = (PrevLX > StickToKeyThreshold);

        // 버튼 상태
        WORD CurrButtons = Pad0.CurrentState.Gamepad.wButtons;
        WORD PrevButtons = Pad0.PreviousState.Gamepad.wButtons;

        auto CheckKey = [&](bool bCurr, bool bPrev) -> bool {
            if (bCheckPressed) return bCurr && !bPrev;
            if (bCheckReleased) return !bCurr && bPrev;
            return bCurr; // Down
        };

        auto CheckButton = [&](WORD ButtonMask) -> bool {
            bool bCurr = (CurrButtons & ButtonMask) != 0;
            bool bPrev = (PrevButtons & ButtonMask) != 0;
            if (bCheckPressed) return bCurr && !bPrev;
            if (bCheckReleased) return !bCurr && bPrev;
            return bCurr;
        };

        switch (KeyCode)
        {
        case 'W': if (CheckKey(bCurrW, bPrevW)) return true; break;
        case 'S': if (CheckKey(bCurrS, bPrevS)) return true; break;
        case 'A': if (CheckKey(bCurrA, bPrevA)) return true; break;
        case 'D': if (CheckKey(bCurrD, bPrevD)) return true; break;
        case 'I': if (CheckButton(XINPUT_GAMEPAD_B)) return true; break;  // 점프 (B)
        case 'T': if (CheckButton(XINPUT_GAMEPAD_X)) return true; break;  // 약공 (X)
        case 'Y': if (CheckButton(XINPUT_GAMEPAD_A)) return true; break;  // 강공 (A)
        case 'U': if (CheckButton(XINPUT_GAMEPAD_Y)) return true; break;  // 스킬 (Y)
        }
    }

    // ===== Player 2 (등록된 게임패드) → 화살표 + 액션키 =====
    int Pad1Index = RegisteredGamepads[1];
    if (Pad1Index >= 0 && Pad1Index < MaxGamepads && GamepadStates[Pad1Index].bConnected)
    {
        const FGamepadState& Pad1 = GamepadStates[Pad1Index];

        float CurrLX = Pad1.LeftStickX;
        float CurrLY = Pad1.LeftStickY;

        float PrevLX = ApplyStickDeadzone(Pad1.PreviousState.Gamepad.sThumbLX, StickDeadzone);
        float PrevLY = ApplyStickDeadzone(Pad1.PreviousState.Gamepad.sThumbLY, StickDeadzone);

        bool bCurrUp = (CurrLY > StickToKeyThreshold);
        bool bCurrDown = (CurrLY < -StickToKeyThreshold);
        bool bCurrLeft = (CurrLX < -StickToKeyThreshold);
        bool bCurrRight = (CurrLX > StickToKeyThreshold);

        bool bPrevUp = (PrevLY > StickToKeyThreshold);
        bool bPrevDown = (PrevLY < -StickToKeyThreshold);
        bool bPrevLeft = (PrevLX < -StickToKeyThreshold);
        bool bPrevRight = (PrevLX > StickToKeyThreshold);

        WORD CurrButtons = Pad1.CurrentState.Gamepad.wButtons;
        WORD PrevButtons = Pad1.PreviousState.Gamepad.wButtons;

        auto CheckKey = [&](bool bCurr, bool bPrev) -> bool {
            if (bCheckPressed) return bCurr && !bPrev;
            if (bCheckReleased) return !bCurr && bPrev;
            return bCurr;
        };

        auto CheckButton = [&](WORD ButtonMask) -> bool {
            bool bCurr = (CurrButtons & ButtonMask) != 0;
            bool bPrev = (PrevButtons & ButtonMask) != 0;
            if (bCheckPressed) return bCurr && !bPrev;
            if (bCheckReleased) return !bCurr && bPrev;
            return bCurr;
        };

        switch (KeyCode)
        {
        case VK_UP:      if (CheckKey(bCurrUp, bPrevUp)) return true; break;
        case VK_DOWN:    if (CheckKey(bCurrDown, bPrevDown)) return true; break;
        case VK_LEFT:    if (CheckKey(bCurrLeft, bPrevLeft)) return true; break;
        case VK_RIGHT:   if (CheckKey(bCurrRight, bPrevRight)) return true; break;
        case VK_NUMPAD6: if (CheckButton(XINPUT_GAMEPAD_B)) return true; break;  // 점프 (B)
        case VK_NUMPAD1: if (CheckButton(XINPUT_GAMEPAD_X)) return true; break;  // 약공 (X)
        case VK_NUMPAD2: if (CheckButton(XINPUT_GAMEPAD_A)) return true; break;  // 강공 (A)
        case VK_NUMPAD3: if (CheckButton(XINPUT_GAMEPAD_Y)) return true; break;  // 스킬 (Y)
        }
    }

    return false;
}
