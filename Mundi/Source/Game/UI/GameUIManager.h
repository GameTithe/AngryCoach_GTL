#pragma once

#include <d2d1_1.h>
#include <dwrite.h>
#include <vector>
#include <memory>
#include <functional>

// Forward declarations
struct ID3D11Device;
struct ID3D11DeviceContext;
struct IDXGISwapChain;
class UGameUIBase;

/**
 * @brief 게임 상태 열거형
 */
enum class EGameUIState : uint8_t
{
    None,
    MainMenu,
    CharacterSelect,
    InGame,
    Paused,
    Result
};

/**
 * @brief 플레이어 데이터 구조체
 */
struct FPlayerUIData
{
    float MaxHealth = 100.0f;
    float CurrentHealth = 100.0f;
    float MaxStamina = 100.0f;
    float CurrentStamina = 100.0f;
    int32_t Wins = 0;
    const wchar_t* PlayerName = L"Player";
};

/**
 * @brief UI 버튼 ID
 */
enum class EUIButtonID : uint8_t
{
    None,
    // Main Menu
    MainMenu_Start,
    MainMenu_Settings,
    MainMenu_Exit,
    // Pause Menu
    Pause_Resume,
    Pause_Restart,
    Pause_MainMenu,
    // Result
    Result_Rematch,
    Result_MainMenu
};

/**
 * @brief UI 버튼 구조체
 */
struct FUIButton
{
    EUIButtonID ID = EUIButtonID::None;
    float X = 0.0f;
    float Y = 0.0f;
    float Width = 0.0f;
    float Height = 0.0f;
    const wchar_t* Text = L"";
    bool bHovered = false;
    bool bPressed = false;

    bool Contains(float PosX, float PosY) const
    {
        return PosX >= X && PosX <= X + Width &&
               PosY >= Y && PosY <= Y + Height;
    }
};

/**
 * @brief 게임 UI 매니저 - Direct2D 기반 게임 UI 시스템
 *
 * StatsOverlayD2D 패턴을 확장하여 게임 전용 UI를 관리합니다.
 * - 인게임 HUD (HP바, 스태미나, 타이머)
 * - 메인 메뉴
 * - 결과 화면
 */
class UGameUIManager
{
public:
    static UGameUIManager& Get();

    // 초기화 및 종료
    void Initialize(ID3D11Device* InDevice, ID3D11DeviceContext* InContext, IDXGISwapChain* InSwapChain);
    void Shutdown();

    // 매 프레임 호출
    void Update(float DeltaTime);
    void Render();

    // 상태 관리
    void SetGameState(EGameUIState NewState);
    EGameUIState GetGameState() const { return CurrentState; }

    // 플레이어 데이터 설정
    void SetPlayer1Data(const FPlayerUIData& Data) { Player1Data = Data; }
    void SetPlayer2Data(const FPlayerUIData& Data) { Player2Data = Data; }
    FPlayerUIData& GetPlayer1Data() { return Player1Data; }
    FPlayerUIData& GetPlayer2Data() { return Player2Data; }

    // 게임 타이머
    void SetGameTimer(float Time) { GameTimer = Time; }
    float GetGameTimer() const { return GameTimer; }
    void SetMaxRounds(int32_t Rounds) { MaxRounds = Rounds; }
    void SetCurrentRound(int32_t Round) { CurrentRound = Round; }

    // UI 가시성
    void SetHUDVisible(bool bVisible) { bShowHUD = bVisible; }
    bool IsHUDVisible() const { return bShowHUD; }

    // Direct2D 리소스 접근 (자식 UI에서 사용)
    ID2D1DeviceContext* GetD2DContext() const { return D2DContext; }
    IDWriteFactory* GetDWriteFactory() const { return DWriteFactory; }

    // 공용 브러시
    ID2D1SolidColorBrush* GetHealthBrush() const { return BrushHealth; }
    ID2D1SolidColorBrush* GetHealthBgBrush() const { return BrushHealthBg; }
    ID2D1SolidColorBrush* GetStaminaBrush() const { return BrushStamina; }
    ID2D1SolidColorBrush* GetWhiteBrush() const { return BrushWhite; }
    ID2D1SolidColorBrush* GetBlackBrush() const { return BrushBlack; }
    ID2D1SolidColorBrush* GetRedBrush() const { return BrushRed; }
    ID2D1SolidColorBrush* GetYellowBrush() const { return BrushYellow; }

    // 텍스트 포맷
    IDWriteTextFormat* GetDefaultTextFormat() const { return TextFormatDefault; }
    IDWriteTextFormat* GetLargeTextFormat() const { return TextFormatLarge; }
    IDWriteTextFormat* GetTimerTextFormat() const { return TextFormatTimer; }

    // 화면 크기
    float GetScreenWidth() const { return ScreenWidth; }
    float GetScreenHeight() const { return ScreenHeight; }

    // 마우스 입력 (UI 상호작용용)
    void OnMouseMove(float X, float Y);
    void OnMouseDown(float X, float Y);
    void OnMouseUp(float X, float Y);

    // 키보드 입력
    void OnKeyDown(uint32_t KeyCode);
    void OnKeyUp(uint32_t KeyCode);

    // 버튼 콜백 설정
    using ButtonCallback = std::function<void(EUIButtonID)>;
    void SetButtonCallback(ButtonCallback Callback) { OnButtonClicked = Callback; }

    // 일시정지 토글
    void TogglePause();

    // 디버그용: 강제 상태 전환
    void ForceSetState(EGameUIState State) { SetGameState(State); }

private:
    UGameUIManager() = default;
    ~UGameUIManager() = default;
    UGameUIManager(const UGameUIManager&) = delete;
    UGameUIManager& operator=(const UGameUIManager&) = delete;

    // D2D 리소스 생성/해제
    void CreateD2DResources();
    void ReleaseD2DResources();
    void CreateBrushes();
    void CreateTextFormats();

    // 렌더링 헬퍼
    void BeginD2DDraw();
    void EndD2DDraw();

    // 각 UI 상태별 렌더링
    void RenderHUD();
    void RenderMainMenu();
    void RenderPauseMenu();
    void RenderResult();

    // HUD 요소 렌더링
    void DrawHealthBar(float X, float Y, float Width, float Height, float HealthPercent, bool bIsPlayer1);
    void DrawStaminaBar(float X, float Y, float Width, float Height, float StaminaPercent, bool bIsPlayer1);
    void DrawTimer();
    void DrawRoundInfo();

    // 버튼 관련
    void SetupButtons();
    void UpdateButtonStates();
    void DrawButton(const FUIButton& Button);
    void HandleButtonClick(EUIButtonID ButtonID);
    FUIButton* GetButtonAt(float X, float Y);

private:
    bool bInitialized = false;
    bool bShowHUD = true;

    // D3D 참조
    ID3D11Device* D3DDevice = nullptr;
    ID3D11DeviceContext* D3DContext = nullptr;
    IDXGISwapChain* SwapChain = nullptr;

    // D2D 리소스
    ID2D1Factory1* D2DFactory = nullptr;
    ID2D1Device* D2DDevice = nullptr;
    ID2D1DeviceContext* D2DContext = nullptr;
    IDWriteFactory* DWriteFactory = nullptr;

    // 현재 렌더 타겟 (Draw 호출마다 갱신)
    ID2D1Bitmap1* CurrentTargetBitmap = nullptr;
    IDXGISurface* CurrentSurface = nullptr;

    // 브러시
    ID2D1SolidColorBrush* BrushHealth = nullptr;      // 초록색 (HP)
    ID2D1SolidColorBrush* BrushHealthBg = nullptr;    // 어두운 회색 (HP 배경)
    ID2D1SolidColorBrush* BrushHealthLow = nullptr;   // 빨간색 (낮은 HP)
    ID2D1SolidColorBrush* BrushStamina = nullptr;     // 노란색 (스태미나)
    ID2D1SolidColorBrush* BrushWhite = nullptr;
    ID2D1SolidColorBrush* BrushBlack = nullptr;
    ID2D1SolidColorBrush* BrushBlackTransparent = nullptr;
    ID2D1SolidColorBrush* BrushRed = nullptr;
    ID2D1SolidColorBrush* BrushYellow = nullptr;
    ID2D1SolidColorBrush* BrushBlue = nullptr;

    // 텍스트 포맷
    IDWriteTextFormat* TextFormatDefault = nullptr;   // 기본 폰트 (16pt)
    IDWriteTextFormat* TextFormatLarge = nullptr;     // 큰 폰트 (32pt)
    IDWriteTextFormat* TextFormatTimer = nullptr;     // 타이머용 (48pt, Bold)

    // 게임 상태
    EGameUIState CurrentState = EGameUIState::None;
    FPlayerUIData Player1Data;
    FPlayerUIData Player2Data;

    // 게임 정보
    float GameTimer = 99.0f;
    int32_t MaxRounds = 3;
    int32_t CurrentRound = 1;

    // 화면 크기
    float ScreenWidth = 1920.0f;
    float ScreenHeight = 1080.0f;

    // 마우스 상태
    float MouseX = 0.0f;
    float MouseY = 0.0f;
    bool bMouseDown = false;

    // 버튼 시스템
    std::vector<FUIButton> Buttons;
    ButtonCallback OnButtonClicked = nullptr;
    ID2D1SolidColorBrush* BrushButtonHover = nullptr;
    ID2D1SolidColorBrush* BrushButtonPressed = nullptr;

    // 이전 게임 상태 (일시정지 해제용)
    EGameUIState PreviousState = EGameUIState::None;
};
