#include "pch.h"
#include "GameUIManager.h"
#include "Widgets/UIWidget.h"
#include "Widgets/UICanvas.h"
#include "Widgets/ProgressBarWidget.h"
#include "Widgets/TextureWidget.h"
#include "Widgets/ButtonWidget.h"
#include "Source/Runtime/Core/Misc/JsonSerializer.h"
#include "Source/Slate/GlobalConsole.h"
#include "Source/Runtime/InputCore/InputManager.h"

#include <d2d1_1.h>
#include <dwrite.h>
#include <dxgi1_2.h>
#include <algorithm>

#ifdef _EDITOR
#include "Source/Runtime/Engine/GameFramework/EditorEngine.h"
extern UEditorEngine GEngine;
#endif

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

    // D2DContext가 제대로 생성되었을 때만 초기화 완료
    bInitialized = (D2DContext != nullptr);
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

    // 기본 색상 브러시
    D2DContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &BrushWhite);
    D2DContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &BrushBlack);
    D2DContext->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.7f), &BrushBlackTransparent);
    D2DContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Red), &BrushRed);
    D2DContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Yellow), &BrushYellow);
    D2DContext->CreateSolidColorBrush(D2D1::ColorF(0.3f, 0.5f, 0.9f, 1.0f), &BrushBlue);
    D2DContext->CreateSolidColorBrush(D2D1::ColorF(0.2f, 0.8f, 0.2f, 1.0f), &BrushGreen);
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
}

void UGameUIManager::Shutdown()
{
    // 캔버스 정리 (위젯들의 D2D 리소스도 함께 해제됨)
    RemoveAllCanvases();

    // WIC 팩토리 정리 (DXGI 리소스 해제 전에 호출해야 함)
    UTextureWidget::ShutdownWICFactory();
    UProgressBarWidget::ShutdownWICFactory();

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
    SafeRelease(BrushWhite);
    SafeRelease(BrushBlack);
    SafeRelease(BrushBlackTransparent);
    SafeRelease(BrushRed);
    SafeRelease(BrushYellow);
    SafeRelease(BrushBlue);
    SafeRelease(BrushGreen);

    // 텍스트 포맷 해제
    SafeRelease(TextFormatDefault);
    SafeRelease(TextFormatLarge);

    // D2D/DWrite 해제
    SafeRelease(DWriteFactory);
    SafeRelease(D2DContext);
    SafeRelease(D2DDevice);
    SafeRelease(D2DFactory);

    BrushWhite = nullptr;
    BrushBlack = nullptr;
    BrushBlackTransparent = nullptr;
    BrushRed = nullptr;
    BrushYellow = nullptr;
    BrushBlue = nullptr;
    BrushGreen = nullptr;
    TextFormatDefault = nullptr;
    TextFormatLarge = nullptr;
    DWriteFactory = nullptr;
    D2DContext = nullptr;
    D2DDevice = nullptr;
    D2DFactory = nullptr;
}

void UGameUIManager::Update(float DeltaTime)
{
    if (!bInitialized)
        return;

    // 마우스 입력 처리
    ProcessMouseInput();

    // 키보드/게임패드 입력 처리 (UI 포커스 네비게이션)
    ProcessKeyboardInput();

    // 모든 캔버스 업데이트 (위젯 애니메이션 처리)
    static int updateLogCount = 0;
    for (auto& Pair : Canvases)
    {
        if (Pair.second)
        {
            if (updateLogCount < 5)
            {
                UE_LOG("[UI] GameUIManager::Update calling canvas %s update, dt=%.4f\n",
                    Pair.first.c_str(), DeltaTime);
            }
            Pair.second->Update(DeltaTime);
        }
    }
    if (updateLogCount < 5 && !Canvases.empty())
        updateLogCount++;
}

void UGameUIManager::BeginD2DDraw()
{
    if (!D2DContext || !SwapChain)
        return;

    // 안전 장치: 기존 리소스가 있으면 먼저 해제 (EndD2DDraw가 호출되지 않은 경우 대비)
    if (CurrentTargetBitmap || CurrentSurface)
    {
        SafeRelease(CurrentTargetBitmap);
        SafeRelease(CurrentSurface);
        CurrentTargetBitmap = nullptr;
        CurrentSurface = nullptr;
    }

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

#ifdef _EDITOR
    // 에디터 모드에서는 PIE 실행 중일 때만 UI 렌더링
    if (!GEngine.IsPIEActive())
        return;
#endif

    BeginD2DDraw();

    // 캔버스 렌더링
    RenderCanvases();

    EndD2DDraw();
}

// ============================================
// Canvas 시스템 구현
// ============================================

UUICanvas* UGameUIManager::CreateCanvas(const std::string& Name, int32_t ZOrder)
{
    // 초기화되지 않았으면 실패
    if (!bInitialized)
    {
        UE_LOG("[UI] CreateCanvas failed: GameUIManager not initialized!\n");
        return nullptr;
    }

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

bool UGameUIManager::IsValidCanvas(UUICanvas* Canvas) const
{
    if (!Canvas)
        return false;

    // Canvases 맵에서 해당 포인터가 존재하는지 확인
    for (const auto& Pair : Canvases)
    {
        if (Pair.second.get() == Canvas)
            return true;
    }
    return false;
}

void UGameUIManager::RemoveCanvas(const std::string& Name)
{
    Canvases.erase(Name);
    bCanvasesSortDirty = true;
}

void UGameUIManager::RemoveAllCanvases()
{
    UE_LOG("[UI] RemoveAllCanvases: Removing %d canvases\n", (int)Canvases.size());
    Canvases.clear();
    SortedCanvases.clear();
    bCanvasesSortDirty = false;
    UE_LOG("[UI] RemoveAllCanvases: Done\n");
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

UUICanvas* UGameUIManager::LoadUIAsset(const std::string& FilePath)
{
    // 초기화되지 않았으면 실패
    if (!bInitialized)
    {
        UE_LOG("[UI] LoadUIAsset failed: GameUIManager not initialized! File: %s\n", FilePath.c_str());
        return nullptr;
    }

    // JSON 파일 로드
    std::wstring widePath(FilePath.begin(), FilePath.end());
    JSON doc;
    if (!FJsonSerializer::LoadJsonFromFile(doc, widePath))
    {
        UE_LOG("[UI] LoadUIAsset failed: Could not load JSON file: %s\n", FilePath.c_str());
        return nullptr;
    }

    // 에셋 이름 읽기
    FString assetName;
    FJsonSerializer::ReadString(doc, "name", assetName, "UIAsset", false);

    // 디자인 해상도 읽기
    float designWidth = 1920.0f, designHeight = 1080.0f;
    FJsonSerializer::ReadFloat(doc, "canvasWidth", designWidth, 1920.0f, false);
    FJsonSerializer::ReadFloat(doc, "canvasHeight", designHeight, 1080.0f, false);

    // 실제 뷰포트 크기와의 스케일 비율 계산
    float scaleX = (designWidth > 0) ? ViewportWidth / designWidth : 1.0f;
    float scaleY = (designHeight > 0) ? ViewportHeight / designHeight : 1.0f;

    // 같은 이름의 캔버스가 있으면 삭제
    RemoveCanvas(assetName.c_str());

    // 캔버스 생성
    UUICanvas* canvas = CreateCanvas(assetName.c_str(), 0);
    if (!canvas)
    {
        UE_LOG("[UI] LoadUIAsset failed: Could not create canvas '%s'\n", assetName.c_str());
        return nullptr;
    }

    // 위젯 배열 읽기
    JSON widgetsArray;
    if (!FJsonSerializer::ReadArray(doc, "widgets", widgetsArray, JSON(), false))
    {
        return canvas;
    }

    for (int i = 0; i < widgetsArray.length(); i++)
    {
        const JSON& widgetObj = widgetsArray[i];

        FString widgetName, widgetType;
        FJsonSerializer::ReadString(widgetObj, "name", widgetName, "", false);
        FJsonSerializer::ReadString(widgetObj, "type", widgetType, "", false);

        // 디자인 해상도 기준 좌표/크기 읽기
        float rawX = 0, rawY = 0, rawW = 100, rawH = 30;
        FJsonSerializer::ReadFloat(widgetObj, "x", rawX, 0.0f, false);
        FJsonSerializer::ReadFloat(widgetObj, "y", rawY, 0.0f, false);
        FJsonSerializer::ReadFloat(widgetObj, "width", rawW, 100.0f, false);
        FJsonSerializer::ReadFloat(widgetObj, "height", rawH, 30.0f, false);

        // 실제 뷰포트에 맞게 스케일링
        float x = rawX * scaleX;
        float y = rawY * scaleY;
        float w = rawW * scaleX;
        float h = rawH * scaleY;

        int32 zOrder = 0;
        FJsonSerializer::ReadInt32(widgetObj, "zOrder", zOrder, 0, false);

        if (widgetType == "ProgressBar")
        {
            canvas->CreateProgressBar(widgetName.c_str(), x, y, w, h);

            float progress = 1.0f;
            FJsonSerializer::ReadFloat(widgetObj, "progress", progress, 1.0f, false);
            canvas->SetWidgetProgress(widgetName.c_str(), progress);

            // 전경/배경 모드 읽기
            int32 fgMode = 0, bgMode = 0;
            FJsonSerializer::ReadInt32(widgetObj, "foregroundMode", fgMode, 0, false);
            FJsonSerializer::ReadInt32(widgetObj, "backgroundMode", bgMode, 0, false);

            // 배경 설정
            if (bgMode == 1) // Texture
            {
                FString bgTexPath;
                if (FJsonSerializer::ReadString(widgetObj, "backgroundTexturePath", bgTexPath, "", false) && !bgTexPath.empty())
                {
                    canvas->SetProgressBarBackgroundTexture(widgetName.c_str(), bgTexPath.c_str(), D2DContext);
                }
            }
            else // Color
            {
                FLinearColor bgColor;
                if (FJsonSerializer::ReadLinearColor(widgetObj, "backgroundColor", bgColor, FLinearColor(0.2f, 0.2f, 0.2f, 0.8f), false))
                {
                    canvas->SetWidgetBackgroundColor(widgetName.c_str(), bgColor.R, bgColor.G, bgColor.B, bgColor.A);
                }
            }

            // 전경 설정
            if (fgMode == 1) // Texture
            {
                FString fgTexPath;
                if (FJsonSerializer::ReadString(widgetObj, "foregroundTexturePath", fgTexPath, "", false) && !fgTexPath.empty())
                {
                    canvas->SetProgressBarForegroundTexture(widgetName.c_str(), fgTexPath.c_str(), D2DContext);
                }
            }
            else // Color
            {
                FLinearColor fgColor;
                if (FJsonSerializer::ReadLinearColor(widgetObj, "foregroundColor", fgColor, FLinearColor(0.2f, 0.8f, 0.2f, 1.0f), false))
                {
                    canvas->SetWidgetForegroundColor(widgetName.c_str(), fgColor.R, fgColor.G, fgColor.B, fgColor.A);
                }
                // Note: LowColor/LowThreshold는 현재 UICanvas에서 미지원
            }

            bool bRTL = false;
            FJsonSerializer::ReadBool(widgetObj, "rightToLeft", bRTL, false, false);
            canvas->SetWidgetRightToLeft(widgetName.c_str(), bRTL);
        }
        else if (widgetType == "Texture")
        {
            FString texPath;
            FJsonSerializer::ReadString(widgetObj, "texturePath", texPath, "", false);

            canvas->CreateTextureWidget(widgetName.c_str(), texPath.c_str(), x, y, w, h, D2DContext);

            // SubUV 설정
            int32 nx = 1, ny = 1, frame = 0;
            FJsonSerializer::ReadInt32(widgetObj, "subUV_NX", nx, 1, false);
            FJsonSerializer::ReadInt32(widgetObj, "subUV_NY", ny, 1, false);
            FJsonSerializer::ReadInt32(widgetObj, "subUV_Frame", frame, 0, false);
            if (nx > 1 || ny > 1)
            {
                canvas->SetTextureSubUV(widgetName.c_str(), frame, nx, ny);
            }

            bool bAdditive = false;
            FJsonSerializer::ReadBool(widgetObj, "additive", bAdditive, false, false);
            canvas->SetTextureAdditive(widgetName.c_str(), bAdditive);
        }
        else if (widgetType == "Rect")
        {
            canvas->CreateRect(widgetName.c_str(), x, y, w, h);

            FLinearColor color;
            if (FJsonSerializer::ReadLinearColor(widgetObj, "color", color, FLinearColor(1, 1, 1, 1), false))
            {
                canvas->SetWidgetForegroundColor(widgetName.c_str(), color.R, color.G, color.B, color.A);
            }
        }
        else if (widgetType == "Button")
        {
            FString texPath;
            FJsonSerializer::ReadString(widgetObj, "texturePath", texPath, "", false);

            if (canvas->CreateButton(widgetName.c_str(), texPath.c_str(), x, y, w, h, D2DContext))
            {
                // 상태별 텍스처 로드
                FString hoveredPath, pressedPath, disabledPath;
                if (FJsonSerializer::ReadString(widgetObj, "hoveredTexturePath", hoveredPath, "", false) && !hoveredPath.empty())
                {
                    canvas->SetButtonHoveredTexture(widgetName.c_str(), hoveredPath.c_str(), D2DContext);
                }
                if (FJsonSerializer::ReadString(widgetObj, "pressedTexturePath", pressedPath, "", false) && !pressedPath.empty())
                {
                    canvas->SetButtonPressedTexture(widgetName.c_str(), pressedPath.c_str(), D2DContext);
                }
                if (FJsonSerializer::ReadString(widgetObj, "disabledTexturePath", disabledPath, "", false) && !disabledPath.empty())
                {
                    canvas->SetButtonDisabledTexture(widgetName.c_str(), disabledPath.c_str(), D2DContext);
                }

                // 상태별 Alpha 값 (새 형식)
                float normalAlpha = 1.0f, hoveredAlpha = 1.0f, pressedAlpha = 1.0f, disabledAlpha = 0.5f;
                FJsonSerializer::ReadFloat(widgetObj, "normalAlpha", normalAlpha, 1.0f, false);
                FJsonSerializer::ReadFloat(widgetObj, "hoveredAlpha", hoveredAlpha, 1.0f, false);
                FJsonSerializer::ReadFloat(widgetObj, "pressedAlpha", pressedAlpha, 1.0f, false);
                FJsonSerializer::ReadFloat(widgetObj, "disabledAlpha", disabledAlpha, 0.5f, false);

                // Legacy tint 형식 호환 (alpha만 사용)
                FLinearColor tempTint;
                if (FJsonSerializer::ReadLinearColor(widgetObj, "normalTint", tempTint, FLinearColor(1, 1, 1, 1), false))
                    normalAlpha = tempTint.A;
                if (FJsonSerializer::ReadLinearColor(widgetObj, "hoveredTint", tempTint, FLinearColor(1, 1, 1, 1), false))
                    hoveredAlpha = tempTint.A;
                if (FJsonSerializer::ReadLinearColor(widgetObj, "pressedTint", tempTint, FLinearColor(1, 1, 1, 1), false))
                    pressedAlpha = tempTint.A;
                if (FJsonSerializer::ReadLinearColor(widgetObj, "disabledTint", tempTint, FLinearColor(0.5f, 0.5f, 0.5f, 0.5f), false))
                    disabledAlpha = tempTint.A;

                // Alpha 설정 (RGB는 1.0으로 고정)
                canvas->SetButtonNormalTint(widgetName.c_str(), 1.0f, 1.0f, 1.0f, normalAlpha);
                canvas->SetButtonHoveredTint(widgetName.c_str(), 1.0f, 1.0f, 1.0f, hoveredAlpha);
                canvas->SetButtonPressedTint(widgetName.c_str(), 1.0f, 1.0f, 1.0f, pressedAlpha);
                canvas->SetButtonDisabledTint(widgetName.c_str(), 1.0f, 1.0f, 1.0f, disabledAlpha);

                // 활성화 상태
                bool bInteractable = true;
                FJsonSerializer::ReadBool(widgetObj, "interactable", bInteractable, true, false);
                canvas->SetButtonInteractable(widgetName.c_str(), bInteractable);

                // 호버 스케일 효과
                float hoverScale = 1.0f, hoverScaleDuration = 0.1f;
                FJsonSerializer::ReadFloat(widgetObj, "hoverScale", hoverScale, 1.0f, false);
                FJsonSerializer::ReadFloat(widgetObj, "hoverScaleDuration", hoverScaleDuration, 0.1f, false);
                canvas->SetButtonHoverScale(widgetName.c_str(), hoverScale, hoverScaleDuration);

                // SubUV 설정 (Button도 TextureWidget을 상속받으므로 지원)
                int32 nx = 1, ny = 1, frame = 0;
                FJsonSerializer::ReadInt32(widgetObj, "subUV_NX", nx, 1, false);
                FJsonSerializer::ReadInt32(widgetObj, "subUV_NY", ny, 1, false);
                FJsonSerializer::ReadInt32(widgetObj, "subUV_Frame", frame, 0, false);
                if (nx > 1 || ny > 1)
                {
                    canvas->SetTextureSubUV(widgetName.c_str(), frame, nx, ny);
                }
            }
        }

        // Z-Order 설정
        canvas->SetWidgetZOrder(widgetName.c_str(), zOrder);

        // Animation 설정
        if (UUIWidget* widget = canvas->FindWidget(widgetName.c_str()))
        {
            // Enter Animation
            JSON enterAnimObj;
            if (FJsonSerializer::ReadObject(widgetObj, "enterAnim", enterAnimObj, JSON(), false))
            {
                int32 animType = 0;
                FJsonSerializer::ReadInt32(enterAnimObj, "type", animType, 0, false);
                widget->EnterAnimConfig.Type = static_cast<EWidgetAnimType>(animType);
                FJsonSerializer::ReadFloat(enterAnimObj, "duration", widget->EnterAnimConfig.Duration, 0.3f, false);
                int32 easing = 2;
                FJsonSerializer::ReadInt32(enterAnimObj, "easing", easing, 2, false);
                widget->EnterAnimConfig.Easing = static_cast<EEasingType>(easing);
                FJsonSerializer::ReadFloat(enterAnimObj, "delay", widget->EnterAnimConfig.Delay, 0.0f, false);
                FJsonSerializer::ReadFloat(enterAnimObj, "offset", widget->EnterAnimConfig.Offset, 100.0f, false);
                UE_LOG("[UI] Loaded enterAnim for %s: type=%d, duration=%.2f\n",
                    widgetName.c_str(), animType, widget->EnterAnimConfig.Duration);
            }
            else
            {
                UE_LOG("[UI] No enterAnim found for %s\n", widgetName.c_str());
            }

            // Exit Animation
            JSON exitAnimObj;
            if (FJsonSerializer::ReadObject(widgetObj, "exitAnim", exitAnimObj, JSON(), false))
            {
                int32 animType = 0;
                FJsonSerializer::ReadInt32(exitAnimObj, "type", animType, 0, false);
                widget->ExitAnimConfig.Type = static_cast<EWidgetAnimType>(animType);
                FJsonSerializer::ReadFloat(exitAnimObj, "duration", widget->ExitAnimConfig.Duration, 0.3f, false);
                int32 easing = 2;
                FJsonSerializer::ReadInt32(exitAnimObj, "easing", easing, 2, false);
                widget->ExitAnimConfig.Easing = static_cast<EEasingType>(easing);
                FJsonSerializer::ReadFloat(exitAnimObj, "delay", widget->ExitAnimConfig.Delay, 0.0f, false);
                FJsonSerializer::ReadFloat(exitAnimObj, "offset", widget->ExitAnimConfig.Offset, 100.0f, false);
            }

            // 원본 값 캡처 (애니메이션 기준점 저장)
            widget->CaptureOriginalValues();
            // Enter 애니메이션은 Lua에서 직접 제어
        }
    }

    return canvas;
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

void UGameUIManager::ProcessMouseInput()
{
    if (Canvases.empty())
        return;

    // 정렬이 필요하면 갱신
    if (bCanvasesSortDirty)
    {
        UpdateCanvasSortOrder();
    }

    // InputManager에서 마우스 상태 가져오기
    UInputManager& Input = UInputManager::GetInstance();
    FVector2D MousePos = Input.GetMousePosition();

    // 뷰포트 로컬 좌표로 변환
    float LocalMouseX = MousePos.X - ViewportX;
    float LocalMouseY = MousePos.Y - ViewportY;

    bool bLeftDown = Input.IsMouseButtonDown(EMouseButton::LeftButton);
    bool bLeftPressed = Input.IsMouseButtonPressed(EMouseButton::LeftButton);
    bool bLeftReleased = Input.IsMouseButtonReleased(EMouseButton::LeftButton);

    // Z-order 역순으로 처리 (가장 위에 있는 캔버스부터)
    for (auto it = SortedCanvases.rbegin(); it != SortedCanvases.rend(); ++it)
    {
        UUICanvas* Canvas = *it;
        if (!Canvas || !Canvas->bVisible)
            continue;

        // 캔버스가 입력을 처리했으면 종료 (다른 캔버스로 전파하지 않음)
        if (Canvas->ProcessMouseInput(LocalMouseX, LocalMouseY, bLeftDown, bLeftPressed, bLeftReleased))
        {
            bUIConsumedInput = true;
            return;
        }
    }

    bUIConsumedInput = false;
}

void UGameUIManager::ProcessKeyboardInput()
{
    if (Canvases.empty())
        return;

    // 정렬이 필요하면 갱신
    if (bCanvasesSortDirty)
    {
        UpdateCanvasSortOrder();
    }

    // InputManager에서 키보드/게임패드 상태 가져오기
    UInputManager& Input = UInputManager::GetInstance();

    // 방향키 입력 체크 (W/S, 위/아래 화살표, 게임패드 D-pad)
    bool bUpKey = Input.IsKeyDown(VK_UP) || Input.IsKeyDown('W');
    bool bDownKey = Input.IsKeyDown(VK_DOWN) || Input.IsKeyDown('S');
    bool bConfirmKey = Input.IsKeyDown(VK_RETURN) || Input.IsKeyDown(VK_SPACE);

    // 게임패드 입력 체크 (연결된 모든 게임패드)
    for (int i = 0; i < 4; ++i)
    {
        if (Input.IsGamepadConnected(i))
        {
            if (Input.IsGamepadButtonDown(i, EGamepadButton::DPadUp))
                bUpKey = true;
            if (Input.IsGamepadButtonDown(i, EGamepadButton::DPadDown))
                bDownKey = true;
            if (Input.IsGamepadButtonDown(i, EGamepadButton::A))
                bConfirmKey = true;
        }
    }

    // Edge detection (이번 프레임에 눌렸는지)
    bool bUpPressed = bUpKey && !bPrevUpKey;
    bool bDownPressed = bDownKey && !bPrevDownKey;
    bool bConfirmPressed = bConfirmKey && !bPrevConfirmKey;

    // 이전 상태 저장
    bPrevUpKey = bUpKey;
    bPrevDownKey = bDownKey;
    bPrevConfirmKey = bConfirmKey;

    // 입력이 없으면 리턴
    if (!bUpPressed && !bDownPressed && !bConfirmPressed)
        return;

    // Z-order 역순으로 처리 (가장 위에 있는 캔버스부터)
    for (auto it = SortedCanvases.rbegin(); it != SortedCanvases.rend(); ++it)
    {
        UUICanvas* Canvas = *it;
        if (!Canvas || !Canvas->bVisible)
            continue;

        // 캔버스에 포커스 가능한 버튼이 있으면 입력 처리
        if (Canvas->HasFocusableButtons())
        {
            if (Canvas->ProcessKeyboardInput(bUpPressed, bDownPressed, bConfirmPressed))
            {
                bUIConsumedInput = true;
                return;
            }
        }
    }
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
