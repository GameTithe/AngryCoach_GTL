#include "pch.h"
#include "GameUIManager.h"
#include "Widgets/UIWidget.h"
#include "Widgets/UICanvas.h"
#include "Widgets/ProgressBarWidget.h"
#include "Widgets/TextureWidget.h"

#include <d2d1_1.h>
#include <dwrite.h>
#include <dxgi1_2.h>
#include <algorithm>

#pragma comment(lib, "d2d1")
#pragma comment(lib, "dwrite")

// 안전한 COM 객체 해제
static inline void SafeRelease(IUnknown* p)
{
    if (p) p->Release();
}

UGameUIManager& UGameUIManager::Get()
{
    static UGameUIManager Instance;
    return Instance;
}

void UGameUIManager::Initialize(ID3D11Device* InDevice, ID3D11DeviceContext* InContext, IDXGISwapChain* InSwapChain)
{
    D3DDevice = InDevice;
    D3DContext = InContext;
    SwapChain = InSwapChain;

    if (!D3DDevice || !D3DContext || !SwapChain)
    {
        bInitialized = false;
        return;
    }

    CreateD2DResources();
    bInitialized = true;

    // 초기 상태: HUD 비활성화 (Lua에서 직접 제어)
    bShowHUD = false;
    CurrentState = EGameUIState::None;
}

void UGameUIManager::CreateD2DResources()
{
    // D2D Factory 생성
    D2D1_FACTORY_OPTIONS FactoryOpts{};
#ifdef _DEBUG
    FactoryOpts.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif

    HRESULT hr = D2D1CreateFactory(
        D2D1_FACTORY_TYPE_SINGLE_THREADED,
        __uuidof(ID2D1Factory1),
        &FactoryOpts,
        (void**)&D2DFactory
    );
    if (FAILED(hr))
    {
        return;
    }

    // DXGI Device 얻기
    IDXGIDevice* DxgiDevice = nullptr;
    hr = D3DDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&DxgiDevice);
    if (FAILED(hr))
    {
        return;
    }

    // D2D Device 생성
    hr = D2DFactory->CreateDevice(DxgiDevice, &D2DDevice);
    SafeRelease(DxgiDevice);
    if (FAILED(hr))
    {
        return;
    }

    // D2D Device Context 생성
    hr = D2DDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &D2DContext);
    if (FAILED(hr))
    {
        return;
    }

    // DWrite Factory 생성
    hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        (IUnknown**)&DWriteFactory
    );
    if (FAILED(hr))
    {
        return;
    }

    // 브러시와 텍스트 포맷 생성
    CreateBrushes();
    CreateTextFormats();

    // 화면 크기 얻기
    DXGI_SWAP_CHAIN_DESC SwapChainDesc;
    SwapChain->GetDesc(&SwapChainDesc);
    ScreenWidth = static_cast<float>(SwapChainDesc.BufferDesc.Width);
    ScreenHeight = static_cast<float>(SwapChainDesc.BufferDesc.Height);
}

void UGameUIManager::CreateBrushes()
{
    if (!D2DContext)
        return;

    // HP 바 (초록색)
    D2DContext->CreateSolidColorBrush(D2D1::ColorF(0.2f, 0.8f, 0.2f, 1.0f), &BrushHealth);
    // HP 배경 (어두운 회색)
    D2DContext->CreateSolidColorBrush(D2D1::ColorF(0.2f, 0.2f, 0.2f, 0.8f), &BrushHealthBg);
    // 낮은 HP (빨간색)
    D2DContext->CreateSolidColorBrush(D2D1::ColorF(0.9f, 0.2f, 0.2f, 1.0f), &BrushHealthLow);
    // 스태미나 (노란색)
    D2DContext->CreateSolidColorBrush(D2D1::ColorF(0.9f, 0.8f, 0.2f, 1.0f), &BrushStamina);
    // 기본 색상
    D2DContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &BrushWhite);
    D2DContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &BrushBlack);
    D2DContext->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.7f), &BrushBlackTransparent);
    D2DContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Red), &BrushRed);
    D2DContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Yellow), &BrushYellow);
    D2DContext->CreateSolidColorBrush(D2D1::ColorF(0.3f, 0.5f, 0.9f, 1.0f), &BrushBlue);
    // 버튼 호버/프레스
    D2DContext->CreateSolidColorBrush(D2D1::ColorF(0.4f, 0.6f, 1.0f, 1.0f), &BrushButtonHover);
    D2DContext->CreateSolidColorBrush(D2D1::ColorF(0.2f, 0.4f, 0.8f, 1.0f), &BrushButtonPressed);
}

void UGameUIManager::CreateTextFormats()
{
    if (!DWriteFactory)
        return;

    // 기본 텍스트 (16pt)
    DWriteFactory->CreateTextFormat(
        L"Segoe UI",
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        16.0f,
        L"en-us",
        &TextFormatDefault
    );

    // 큰 텍스트 (32pt)
    DWriteFactory->CreateTextFormat(
        L"Segoe UI",
        nullptr,
        DWRITE_FONT_WEIGHT_BOLD,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        32.0f,
        L"en-us",
        &TextFormatLarge
    );

    // 타이머 텍스트 (48pt, Bold)
    DWriteFactory->CreateTextFormat(
        L"Segoe UI",
        nullptr,
        DWRITE_FONT_WEIGHT_BOLD,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        48.0f,
        L"en-us",
        &TextFormatTimer
    );

    // 텍스트 정렬 설정
    if (TextFormatDefault)
    {
        TextFormatDefault->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        TextFormatDefault->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    }
    if (TextFormatLarge)
    {
        TextFormatLarge->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        TextFormatLarge->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
    if (TextFormatTimer)
    {
        TextFormatTimer->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        TextFormatTimer->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
}

void UGameUIManager::Shutdown()
{
    // 캔버스 정리
    RemoveAllCanvases();

    ReleaseD2DResources();

    D3DDevice = nullptr;
    D3DContext = nullptr;
    SwapChain = nullptr;
    bInitialized = false;
}

void UGameUIManager::SetViewport(float X, float Y, float Width, float Height)
{
    ViewportX = X;
    ViewportY = Y;
    ViewportWidth = Width;
    ViewportHeight = Height;
}

void UGameUIManager::ReleaseD2DResources()
{
    // 브러시 해제
    SafeRelease(BrushHealth);
    SafeRelease(BrushHealthBg);
    SafeRelease(BrushHealthLow);
    SafeRelease(BrushStamina);
    SafeRelease(BrushWhite);
    SafeRelease(BrushBlack);
    SafeRelease(BrushBlackTransparent);
    SafeRelease(BrushRed);
    SafeRelease(BrushYellow);
    SafeRelease(BrushBlue);
    SafeRelease(BrushButtonHover);
    SafeRelease(BrushButtonPressed);

    // 텍스트 포맷 해제
    SafeRelease(TextFormatDefault);
    SafeRelease(TextFormatLarge);
    SafeRelease(TextFormatTimer);

    // D2D/DWrite 해제
    SafeRelease(DWriteFactory);
    SafeRelease(D2DContext);
    SafeRelease(D2DDevice);
    SafeRelease(D2DFactory);

    BrushHealth = nullptr;
    BrushHealthBg = nullptr;
    BrushHealthLow = nullptr;
    BrushStamina = nullptr;
    BrushWhite = nullptr;
    BrushBlack = nullptr;
    BrushBlackTransparent = nullptr;
    BrushRed = nullptr;
    BrushYellow = nullptr;
    BrushBlue = nullptr;
    TextFormatDefault = nullptr;
    TextFormatLarge = nullptr;
    TextFormatTimer = nullptr;
    DWriteFactory = nullptr;
    D2DContext = nullptr;
    D2DDevice = nullptr;
    D2DFactory = nullptr;
}

void UGameUIManager::Update(float DeltaTime)
{
    if (!bInitialized)
        return;

    // 게임 상태에 따른 업데이트 로직
    switch (CurrentState)
    {
    case EGameUIState::InGame:
        // 타이머 업데이트는 외부에서 SetGameTimer로 처리
        break;
    default:
        break;
    }

    // 모든 캔버스 업데이트 (위젯 애니메이션 처리)
    for (auto& Pair : Canvases)
    {
        if (Pair.second)
        {
            Pair.second->Update(DeltaTime);
        }
    }
}

void UGameUIManager::SetGameState(EGameUIState NewState)
{
    if (CurrentState != NewState)
    {
        CurrentState = NewState;

        // 상태 전환 시 필요한 초기화
        switch (NewState)
        {
        case EGameUIState::InGame:
            bShowHUD = true;
            break;
        case EGameUIState::Paused:
            // 게임 일시정지 시 HUD는 유지
            break;
        case EGameUIState::MainMenu:
        case EGameUIState::Result:
            bShowHUD = false;
            break;
        default:
            break;
        }

        // 새로운 상태에 맞는 버튼들 설정
        SetupButtons();
    }
}

void UGameUIManager::BeginD2DDraw()
{
    if (!D2DContext || !SwapChain)
        return;

    // SwapChain에서 Surface 얻기
    HRESULT hr = SwapChain->GetBuffer(0, __uuidof(IDXGISurface), (void**)&CurrentSurface);
    if (FAILED(hr))
        return;

    // Bitmap 생성
    D2D1_BITMAP_PROPERTIES1 BmpProps = {};
    BmpProps.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
    BmpProps.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
    BmpProps.dpiX = 96.0f;
    BmpProps.dpiY = 96.0f;
    BmpProps.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;

    hr = D2DContext->CreateBitmapFromDxgiSurface(CurrentSurface, &BmpProps, &CurrentTargetBitmap);
    if (FAILED(hr))
    {
        SafeRelease(CurrentSurface);
        CurrentSurface = nullptr;
        return;
    }

    D2DContext->SetTarget(CurrentTargetBitmap);
    D2DContext->BeginDraw();
}

void UGameUIManager::EndD2DDraw()
{
    if (!D2DContext)
        return;

    D2DContext->EndDraw();
    D2DContext->SetTarget(nullptr);

    SafeRelease(CurrentTargetBitmap);
    SafeRelease(CurrentSurface);
    CurrentTargetBitmap = nullptr;
    CurrentSurface = nullptr;
}

void UGameUIManager::Render()
{
    if (!bInitialized)
        return;

    BeginD2DDraw();

    // 상태에 따른 렌더링
    switch (CurrentState)
    {
    case EGameUIState::InGame:
        if (bShowHUD)
        {
            RenderHUD();
        }
        break;

    case EGameUIState::Paused:
        if (bShowHUD)
        {
            RenderHUD();
        }
        RenderPauseMenu();
        break;

    case EGameUIState::MainMenu:
        RenderMainMenu();
        break;

    case EGameUIState::Result:
        RenderResult();
        break;

    default:
        break;
    }

    // Lua 캔버스 렌더링 (항상 최상위에 그려짐)
    RenderCanvases();

    EndD2DDraw();
}

void UGameUIManager::RenderHUD()
{
    const float Margin = 20.0f;
    const float BarWidth = 350.0f;
    const float BarHeight = 25.0f;
    const float StaminaBarHeight = 12.0f;

    // === Player 1 (좌측) ===
    float P1HealthX = Margin;
    float P1HealthY = Margin;
    float P1HealthPercent = (Player1Data.MaxHealth > 0)
        ? (Player1Data.CurrentHealth / Player1Data.MaxHealth)
        : 0.0f;
    DrawHealthBar(P1HealthX, P1HealthY, BarWidth, BarHeight, P1HealthPercent, true);

    // Player 1 스태미나
    float P1StaminaPercent = (Player1Data.MaxStamina > 0)
        ? (Player1Data.CurrentStamina / Player1Data.MaxStamina)
        : 0.0f;
    DrawStaminaBar(P1HealthX, P1HealthY + BarHeight + 5.0f, BarWidth * 0.8f, StaminaBarHeight, P1StaminaPercent, true);

    // === Player 2 (우측) ===
    float P2HealthX = ScreenWidth - Margin - BarWidth;
    float P2HealthY = Margin;
    float P2HealthPercent = (Player2Data.MaxHealth > 0)
        ? (Player2Data.CurrentHealth / Player2Data.MaxHealth)
        : 0.0f;
    DrawHealthBar(P2HealthX, P2HealthY, BarWidth, BarHeight, P2HealthPercent, false);

    // Player 2 스태미나
    float P2StaminaPercent = (Player2Data.MaxStamina > 0)
        ? (Player2Data.CurrentStamina / Player2Data.MaxStamina)
        : 0.0f;
    float P2StaminaWidth = BarWidth * 0.8f;
    DrawStaminaBar(P2HealthX + BarWidth - P2StaminaWidth, P2HealthY + BarHeight + 5.0f, P2StaminaWidth, StaminaBarHeight, P2StaminaPercent, false);

    // === 타이머 (중앙 상단) ===
    DrawTimer();

    // === 라운드 정보 ===
    DrawRoundInfo();
}

void UGameUIManager::DrawHealthBar(float X, float Y, float Width, float Height, float HealthPercent, bool bIsPlayer1)
{
    if (!D2DContext)
        return;

    HealthPercent = (std::max)(0.0f, (std::min)(1.0f, HealthPercent));

    // 배경
    D2D1_RECT_F BgRect = D2D1::RectF(X, Y, X + Width, Y + Height);
    D2DContext->FillRectangle(BgRect, BrushHealthBg);

    // HP 바 (체력 비율에 따라)
    float FilledWidth = Width * HealthPercent;

    // Player2는 오른쪽에서 왼쪽으로 채워짐
    D2D1_RECT_F HealthRect;
    if (bIsPlayer1)
    {
        HealthRect = D2D1::RectF(X, Y, X + FilledWidth, Y + Height);
    }
    else
    {
        HealthRect = D2D1::RectF(X + Width - FilledWidth, Y, X + Width, Y + Height);
    }

    // 체력 30% 이하면 빨간색
    ID2D1SolidColorBrush* HealthBrush = (HealthPercent <= 0.3f) ? BrushHealthLow : BrushHealth;
    D2DContext->FillRectangle(HealthRect, HealthBrush);

    // 테두리
    D2DContext->DrawRectangle(BgRect, BrushWhite, 2.0f);

    // 플레이어 이름
    const wchar_t* Name = bIsPlayer1 ? Player1Data.PlayerName : Player2Data.PlayerName;
    D2D1_RECT_F NameRect;
    if (bIsPlayer1)
    {
        NameRect = D2D1::RectF(X, Y + Height + 2.0f, X + Width, Y + Height + 22.0f);
    }
    else
    {
        NameRect = D2D1::RectF(X, Y + Height + 2.0f, X + Width, Y + Height + 22.0f);
    }

    IDWriteTextFormat* NameFormat = TextFormatDefault;
    if (NameFormat)
    {
        // 임시로 정렬 변경
        if (bIsPlayer1)
        {
            NameFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        }
        else
        {
            NameFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
        }
        D2DContext->DrawTextW(Name, (UINT32)wcslen(Name), NameFormat, NameRect, BrushWhite);
    }
}

void UGameUIManager::DrawStaminaBar(float X, float Y, float Width, float Height, float StaminaPercent, bool bIsPlayer1)
{
    if (!D2DContext)
        return;

    StaminaPercent = (std::max)(0.0f, (std::min)(1.0f, StaminaPercent));

    // 배경
    D2D1_RECT_F BgRect = D2D1::RectF(X, Y, X + Width, Y + Height);
    D2DContext->FillRectangle(BgRect, BrushHealthBg);

    // 스태미나 바
    float FilledWidth = Width * StaminaPercent;
    D2D1_RECT_F StaminaRect;
    if (bIsPlayer1)
    {
        StaminaRect = D2D1::RectF(X, Y, X + FilledWidth, Y + Height);
    }
    else
    {
        StaminaRect = D2D1::RectF(X + Width - FilledWidth, Y, X + Width, Y + Height);
    }
    D2DContext->FillRectangle(StaminaRect, BrushStamina);
}

void UGameUIManager::DrawTimer()
{
    if (!D2DContext || !TextFormatTimer)
        return;

    // 타이머를 화면 중앙 상단에 표시
    float TimerX = ScreenWidth / 2.0f - 50.0f;
    float TimerY = 10.0f;
    float TimerWidth = 100.0f;
    float TimerHeight = 60.0f;

    // 배경
    D2D1_RECT_F BgRect = D2D1::RectF(TimerX, TimerY, TimerX + TimerWidth, TimerY + TimerHeight);
    D2DContext->FillRectangle(BgRect, BrushBlackTransparent);
    D2DContext->DrawRectangle(BgRect, BrushWhite, 2.0f);

    // 타이머 숫자
    wchar_t TimerText[16];
    int DisplayTime = static_cast<int>(GameTimer);
    if (DisplayTime < 0) DisplayTime = 0;
    swprintf_s(TimerText, L"%02d", DisplayTime);

    D2DContext->DrawTextW(TimerText, (UINT32)wcslen(TimerText), TextFormatTimer, BgRect, BrushWhite);
}

void UGameUIManager::DrawRoundInfo()
{
    if (!D2DContext || !TextFormatDefault)
        return;

    // 라운드 정보를 타이머 아래에 표시
    float RoundX = ScreenWidth / 2.0f - 60.0f;
    float RoundY = 75.0f;
    float RoundWidth = 120.0f;
    float RoundHeight = 20.0f;

    D2D1_RECT_F RoundRect = D2D1::RectF(RoundX, RoundY, RoundX + RoundWidth, RoundY + RoundHeight);

    wchar_t RoundText[32];
    swprintf_s(RoundText, L"Round %d / %d", CurrentRound, MaxRounds);

    IDWriteTextFormat* Format = TextFormatDefault;
    if (Format)
    {
        Format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        D2DContext->DrawTextW(RoundText, (UINT32)wcslen(RoundText), Format, RoundRect, BrushYellow);
    }
}

void UGameUIManager::RenderMainMenu()
{
    if (!D2DContext)
        return;

    // 배경 어둡게
    D2D1_RECT_F FullScreen = D2D1::RectF(0, 0, ScreenWidth, ScreenHeight);
    D2DContext->FillRectangle(FullScreen, BrushBlackTransparent);

    // 타이틀
    D2D1_RECT_F TitleRect = D2D1::RectF(0, ScreenHeight * 0.2f, ScreenWidth, ScreenHeight * 0.35f);
    if (TextFormatLarge)
    {
        D2DContext->DrawTextW(L"ANGRY COACH", 11, TextFormatLarge, TitleRect, BrushWhite);
    }

    // 버튼들 렌더링
    for (const auto& Button : Buttons)
    {
        DrawButton(Button);
    }
}

void UGameUIManager::RenderPauseMenu()
{
    if (!D2DContext)
        return;

    // 반투명 배경
    D2D1_RECT_F FullScreen = D2D1::RectF(0, 0, ScreenWidth, ScreenHeight);
    D2DContext->FillRectangle(FullScreen, BrushBlackTransparent);

    // PAUSED 텍스트
    D2D1_RECT_F PausedRect = D2D1::RectF(0, ScreenHeight * 0.3f, ScreenWidth, ScreenHeight * 0.4f);
    if (TextFormatLarge)
    {
        D2DContext->DrawTextW(L"PAUSED", 6, TextFormatLarge, PausedRect, BrushYellow);
    }

    // 버튼들 렌더링
    for (const auto& Button : Buttons)
    {
        DrawButton(Button);
    }
}

void UGameUIManager::RenderResult()
{
    if (!D2DContext)
        return;

    // 배경
    D2D1_RECT_F FullScreen = D2D1::RectF(0, 0, ScreenWidth, ScreenHeight);
    D2DContext->FillRectangle(FullScreen, BrushBlackTransparent);

    // 승자 결정 (간단히 HP 비교)
    bool bPlayer1Wins = Player1Data.CurrentHealth > Player2Data.CurrentHealth;
    const wchar_t* WinnerText = bPlayer1Wins ? L"PLAYER 1 WINS!" : L"PLAYER 2 WINS!";

    // VICTORY 텍스트
    D2D1_RECT_F VictoryRect = D2D1::RectF(0, ScreenHeight * 0.25f, ScreenWidth, ScreenHeight * 0.35f);
    if (TextFormatLarge)
    {
        D2DContext->DrawTextW(L"VICTORY", 7, TextFormatLarge, VictoryRect, BrushYellow);
    }

    // 승자 이름
    D2D1_RECT_F WinnerRect = D2D1::RectF(0, ScreenHeight * 0.38f, ScreenWidth, ScreenHeight * 0.48f);
    if (TextFormatLarge)
    {
        D2DContext->DrawTextW(WinnerText, (UINT32)wcslen(WinnerText), TextFormatLarge, WinnerRect, BrushWhite);
    }

    // 버튼들 렌더링
    for (const auto& Button : Buttons)
    {
        DrawButton(Button);
    }
}

void UGameUIManager::OnMouseMove(float X, float Y)
{
    MouseX = X;
    MouseY = Y;
    UpdateButtonStates();
}

void UGameUIManager::OnMouseDown(float X, float Y)
{
    MouseX = X;
    MouseY = Y;
    bMouseDown = true;

    // 버튼 Press 상태 설정
    FUIButton* HitButton = GetButtonAt(X, Y);
    if (HitButton)
    {
        HitButton->bPressed = true;
    }
}

void UGameUIManager::OnMouseUp(float X, float Y)
{
    MouseX = X;
    MouseY = Y;

    // 버튼 클릭 처리 - Press 상태였던 버튼 위에서 릴리즈하면 클릭
    FUIButton* HitButton = GetButtonAt(X, Y);
    if (HitButton && HitButton->bPressed)
    {
        HandleButtonClick(HitButton->ID);
    }

    // 모든 버튼의 Press 상태 해제
    for (auto& Button : Buttons)
    {
        Button.bPressed = false;
    }

    bMouseDown = false;
}

void UGameUIManager::OnKeyDown(uint32_t KeyCode)
{
    // ESC 키 (27)
    if (KeyCode == 27)
    {
        TogglePause();
    }
    // F1 키 (112) - 디버그용 MainMenu로 전환
    else if (KeyCode == 112)
    {
        SetGameState(EGameUIState::MainMenu);
    }
    // F2 키 (113) - 디버그용 InGame으로 전환
    else if (KeyCode == 113)
    {
        SetGameState(EGameUIState::InGame);
    }
    // F3 키 (114) - 디버그용 Result로 전환
    else if (KeyCode == 114)
    {
        SetGameState(EGameUIState::Result);
    }
}

void UGameUIManager::OnKeyUp(uint32_t KeyCode)
{
    // 현재는 키 업에서 특별히 처리할 것 없음
}

void UGameUIManager::TogglePause()
{
    if (CurrentState == EGameUIState::InGame)
    {
        PreviousState = CurrentState;
        SetGameState(EGameUIState::Paused);
    }
    else if (CurrentState == EGameUIState::Paused)
    {
        SetGameState(EGameUIState::InGame);
    }
}

void UGameUIManager::SetupButtons()
{
    Buttons.clear();

    switch (CurrentState)
    {
    case EGameUIState::MainMenu:
    {
        const float ButtonWidth = 200.0f;
        const float ButtonHeight = 50.0f;
        const float ButtonX = (ScreenWidth - ButtonWidth) / 2.0f;
        float ButtonY = ScreenHeight * 0.45f;

        // Start Game
        FUIButton StartBtn;
        StartBtn.ID = EUIButtonID::MainMenu_Start;
        StartBtn.X = ButtonX;
        StartBtn.Y = ButtonY;
        StartBtn.Width = ButtonWidth;
        StartBtn.Height = ButtonHeight;
        StartBtn.Text = L"START GAME";
        Buttons.push_back(StartBtn);

        // Settings
        ButtonY += ButtonHeight + 20.0f;
        FUIButton SettingsBtn;
        SettingsBtn.ID = EUIButtonID::MainMenu_Settings;
        SettingsBtn.X = ButtonX;
        SettingsBtn.Y = ButtonY;
        SettingsBtn.Width = ButtonWidth;
        SettingsBtn.Height = ButtonHeight;
        SettingsBtn.Text = L"SETTINGS";
        Buttons.push_back(SettingsBtn);

        // Exit
        ButtonY += ButtonHeight + 20.0f;
        FUIButton ExitBtn;
        ExitBtn.ID = EUIButtonID::MainMenu_Exit;
        ExitBtn.X = ButtonX;
        ExitBtn.Y = ButtonY;
        ExitBtn.Width = ButtonWidth;
        ExitBtn.Height = ButtonHeight;
        ExitBtn.Text = L"EXIT";
        Buttons.push_back(ExitBtn);
        break;
    }

    case EGameUIState::Paused:
    {
        const float ButtonWidth = 180.0f;
        const float ButtonHeight = 45.0f;
        const float ButtonX = (ScreenWidth - ButtonWidth) / 2.0f;
        float ButtonY = ScreenHeight * 0.45f;

        // Resume
        FUIButton ResumeBtn;
        ResumeBtn.ID = EUIButtonID::Pause_Resume;
        ResumeBtn.X = ButtonX;
        ResumeBtn.Y = ButtonY;
        ResumeBtn.Width = ButtonWidth;
        ResumeBtn.Height = ButtonHeight;
        ResumeBtn.Text = L"RESUME";
        Buttons.push_back(ResumeBtn);

        // Restart
        ButtonY += ButtonHeight + 15.0f;
        FUIButton RestartBtn;
        RestartBtn.ID = EUIButtonID::Pause_Restart;
        RestartBtn.X = ButtonX;
        RestartBtn.Y = ButtonY;
        RestartBtn.Width = ButtonWidth;
        RestartBtn.Height = ButtonHeight;
        RestartBtn.Text = L"RESTART";
        Buttons.push_back(RestartBtn);

        // Main Menu
        ButtonY += ButtonHeight + 15.0f;
        FUIButton MainMenuBtn;
        MainMenuBtn.ID = EUIButtonID::Pause_MainMenu;
        MainMenuBtn.X = ButtonX;
        MainMenuBtn.Y = ButtonY;
        MainMenuBtn.Width = ButtonWidth;
        MainMenuBtn.Height = ButtonHeight;
        MainMenuBtn.Text = L"MAIN MENU";
        Buttons.push_back(MainMenuBtn);
        break;
    }

    case EGameUIState::Result:
    {
        const float ButtonWidth = 180.0f;
        const float ButtonHeight = 45.0f;
        const float ButtonSpacing = 30.0f;
        float TotalWidth = ButtonWidth * 2 + ButtonSpacing;
        float ButtonX = (ScreenWidth - TotalWidth) / 2.0f;
        float ButtonY = ScreenHeight * 0.6f;

        // Rematch
        FUIButton RematchBtn;
        RematchBtn.ID = EUIButtonID::Result_Rematch;
        RematchBtn.X = ButtonX;
        RematchBtn.Y = ButtonY;
        RematchBtn.Width = ButtonWidth;
        RematchBtn.Height = ButtonHeight;
        RematchBtn.Text = L"REMATCH";
        Buttons.push_back(RematchBtn);

        // Main Menu
        FUIButton MainMenuBtn;
        MainMenuBtn.ID = EUIButtonID::Result_MainMenu;
        MainMenuBtn.X = ButtonX + ButtonWidth + ButtonSpacing;
        MainMenuBtn.Y = ButtonY;
        MainMenuBtn.Width = ButtonWidth;
        MainMenuBtn.Height = ButtonHeight;
        MainMenuBtn.Text = L"MAIN MENU";
        Buttons.push_back(MainMenuBtn);
        break;
    }

    default:
        break;
    }
}

void UGameUIManager::UpdateButtonStates()
{
    for (auto& Button : Buttons)
    {
        Button.bHovered = Button.Contains(MouseX, MouseY);
        // Press 상태는 마우스가 버튼 밖으로 나가면 해제
        if (!Button.bHovered)
        {
            Button.bPressed = false;
        }
    }
}

void UGameUIManager::DrawButton(const FUIButton& Button)
{
    if (!D2DContext)
        return;

    D2D1_RECT_F Rect = D2D1::RectF(Button.X, Button.Y, Button.X + Button.Width, Button.Y + Button.Height);

    // 버튼 배경색 결정
    ID2D1SolidColorBrush* FillBrush = BrushBlue;

    // Exit 류 버튼은 빨간색
    if (Button.ID == EUIButtonID::MainMenu_Exit ||
        Button.ID == EUIButtonID::Pause_MainMenu ||
        Button.ID == EUIButtonID::Result_MainMenu)
    {
        FillBrush = BrushRed;
    }

    // 호버/프레스 상태에 따른 색상
    if (Button.bPressed)
    {
        FillBrush = BrushButtonPressed;
    }
    else if (Button.bHovered)
    {
        FillBrush = BrushButtonHover;
    }

    D2DContext->FillRectangle(Rect, FillBrush);
    D2DContext->DrawRectangle(Rect, BrushWhite, 2.0f);

    // 텍스트
    if (TextFormatDefault && Button.Text)
    {
        TextFormatDefault->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        TextFormatDefault->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        D2DContext->DrawTextW(Button.Text, (UINT32)wcslen(Button.Text), TextFormatDefault, Rect, BrushWhite);
    }
}

void UGameUIManager::HandleButtonClick(EUIButtonID ButtonID)
{
    // 외부 콜백이 있으면 호출
    if (OnButtonClicked)
    {
        OnButtonClicked(ButtonID);
    }

    // 내부 기본 처리
    switch (ButtonID)
    {
    case EUIButtonID::MainMenu_Start:
        SetGameState(EGameUIState::InGame);
        break;

    case EUIButtonID::MainMenu_Exit:
        // 게임 종료 (Windows)
        PostQuitMessage(0);
        break;

    case EUIButtonID::Pause_Resume:
        SetGameState(EGameUIState::InGame);
        break;

    case EUIButtonID::Pause_Restart:
        // 게임 재시작 - HP 초기화 후 InGame으로
        Player1Data.CurrentHealth = Player1Data.MaxHealth;
        Player1Data.CurrentStamina = Player1Data.MaxStamina;
        Player2Data.CurrentHealth = Player2Data.MaxHealth;
        Player2Data.CurrentStamina = Player2Data.MaxStamina;
        GameTimer = 99.0f;
        SetGameState(EGameUIState::InGame);
        break;

    case EUIButtonID::Pause_MainMenu:
    case EUIButtonID::Result_MainMenu:
        SetGameState(EGameUIState::MainMenu);
        break;

    case EUIButtonID::Result_Rematch:
        // 재대결 - HP 초기화 후 InGame으로
        Player1Data.CurrentHealth = Player1Data.MaxHealth;
        Player1Data.CurrentStamina = Player1Data.MaxStamina;
        Player2Data.CurrentHealth = Player2Data.MaxHealth;
        Player2Data.CurrentStamina = Player2Data.MaxStamina;
        GameTimer = 99.0f;
        SetGameState(EGameUIState::InGame);
        break;

    default:
        break;
    }
}

FUIButton* UGameUIManager::GetButtonAt(float X, float Y)
{
    for (auto& Button : Buttons)
    {
        if (Button.Contains(X, Y))
        {
            return &Button;
        }
    }
    return nullptr;
}

// ============================================
// Canvas 시스템 구현
// ============================================

UUICanvas* UGameUIManager::CreateCanvas(const std::string& Name, int32_t ZOrder)
{
    // 이미 존재하면 실패
    if (Canvases.find(Name) != Canvases.end())
        return nullptr;

    auto Canvas = std::make_unique<UUICanvas>();
    Canvas->Name = Name;
    Canvas->ZOrder = ZOrder;

    UUICanvas* Ptr = Canvas.get();
    Canvases[Name] = std::move(Canvas);
    bCanvasesSortDirty = true;

    return Ptr;
}

UUICanvas* UGameUIManager::FindCanvas(const std::string& Name)
{
    auto it = Canvases.find(Name);
    if (it != Canvases.end())
        return it->second.get();
    return nullptr;
}

void UGameUIManager::RemoveCanvas(const std::string& Name)
{
    Canvases.erase(Name);
    bCanvasesSortDirty = true;
}

void UGameUIManager::RemoveAllCanvases()
{
    Canvases.clear();
    SortedCanvases.clear();
    bCanvasesSortDirty = false;
}

void UGameUIManager::SetCanvasVisible(const std::string& Name, bool bVisible)
{
    if (auto* Canvas = FindCanvas(Name))
    {
        Canvas->SetVisible(bVisible);
    }
}

void UGameUIManager::SetCanvasZOrder(const std::string& Name, int32_t Z)
{
    if (auto* Canvas = FindCanvas(Name))
    {
        Canvas->SetZOrder(Z);
        bCanvasesSortDirty = true;
    }
}

void UGameUIManager::UpdateCanvasSortOrder()
{
    SortedCanvases.clear();
    SortedCanvases.reserve(Canvases.size());

    for (auto& Pair : Canvases)
    {
        SortedCanvases.push_back(Pair.second.get());
    }

    // ZOrder 오름차순 정렬 (낮은 것이 먼저 그려짐)
    std::sort(SortedCanvases.begin(), SortedCanvases.end(),
        [](const UUICanvas* A, const UUICanvas* B)
        {
            return A->ZOrder < B->ZOrder;
        });

    bCanvasesSortDirty = false;
}

void UGameUIManager::RenderCanvases()
{
    if (!D2DContext || Canvases.empty())
        return;

    // 정렬이 필요하면 갱신
    if (bCanvasesSortDirty)
    {
        UpdateCanvasSortOrder();
    }

    // ZOrder 순서대로 캔버스 렌더링
    for (UUICanvas* Canvas : SortedCanvases)
    {
        if (Canvas && Canvas->bVisible)
        {
            Canvas->Render(D2DContext, ViewportX, ViewportY);
        }
    }
}
