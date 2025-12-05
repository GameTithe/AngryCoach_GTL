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

    // 모든 캔버스 업데이트 (위젯 애니메이션 처리)
    for (auto& Pair : Canvases)
    {
        if (Pair.second)
        {
            Pair.second->Update(DeltaTime);
        }
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

    // 캔버스 렌더링
    RenderCanvases();

    EndD2DDraw();
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
