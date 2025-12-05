#include "pch.h"
#include "ProgressBarWidget.h"
#include <algorithm>

#pragma comment(lib, "windowscodecs.lib")

// 정적 멤버 초기화
IWICImagingFactory* UProgressBarWidget::WICFactory = nullptr;

UProgressBarWidget::UProgressBarWidget()
{
    Type = EUIWidgetType::ProgressBar;

    // 기본 색상 설정
    ForegroundColor = D2D1::ColorF(0.2f, 0.8f, 0.2f, 1.0f);  // 초록색
    BackgroundColor = D2D1::ColorF(0.2f, 0.2f, 0.2f, 0.8f);  // 어두운 회색
    BorderColor = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);      // 흰색
    LowColor = D2D1::ColorF(0.9f, 0.2f, 0.2f, 1.0f);         // 빨간색
}

UProgressBarWidget::~UProgressBarWidget()
{
    UE_LOG("[UI] ~UProgressBarWidget: Destroying '%s', FgBitmap=%p, BgBitmap=%p, LowBitmap=%p\n",
        Name.c_str(), ForegroundBitmap, BackgroundBitmap, LowBitmap);
    ClearTextures();
}

bool UProgressBarWidget::InitWICFactory()
{
    if (WICFactory)
        return true;

    HRESULT hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&WICFactory)
    );

    return SUCCEEDED(hr);
}

void UProgressBarWidget::ShutdownWICFactory()
{
    if (WICFactory)
    {
        WICFactory->Release();
        WICFactory = nullptr;
    }
}

void UProgressBarWidget::ReleaseBitmap(ID2D1Bitmap*& Bitmap)
{
    if (Bitmap)
    {
        Bitmap->Release();
        Bitmap = nullptr;
    }
}

void UProgressBarWidget::ClearTextures()
{
    ReleaseBitmap(ForegroundBitmap);
    ReleaseBitmap(BackgroundBitmap);
    ReleaseBitmap(LowBitmap);
}

ID2D1Bitmap* UProgressBarWidget::LoadBitmapFromFile(const wchar_t* Path, ID2D1DeviceContext* Context)
{
    if (!Path || !Context)
        return nullptr;

    // WIC 팩토리 초기화
    if (!InitWICFactory())
        return nullptr;

    HRESULT hr;

    // 디코더 생성
    IWICBitmapDecoder* Decoder = nullptr;
    hr = WICFactory->CreateDecoderFromFilename(
        Path,
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnLoad,
        &Decoder
    );

    if (FAILED(hr))
        return nullptr;

    // 프레임 가져오기
    IWICBitmapFrameDecode* Frame = nullptr;
    hr = Decoder->GetFrame(0, &Frame);

    if (FAILED(hr))
    {
        Decoder->Release();
        return nullptr;
    }

    // 포맷 컨버터 생성
    IWICFormatConverter* Converter = nullptr;
    hr = WICFactory->CreateFormatConverter(&Converter);

    if (FAILED(hr))
    {
        Frame->Release();
        Decoder->Release();
        return nullptr;
    }

    // 32비트 BGRA로 변환 (D2D와 호환)
    hr = Converter->Initialize(
        Frame,
        GUID_WICPixelFormat32bppPBGRA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0f,
        WICBitmapPaletteTypeMedianCut
    );

    if (FAILED(hr))
    {
        Converter->Release();
        Frame->Release();
        Decoder->Release();
        return nullptr;
    }

    // D2D 비트맵 생성
    ID2D1Bitmap* ResultBitmap = nullptr;
    hr = Context->CreateBitmapFromWicBitmap(Converter, nullptr, &ResultBitmap);

    // 정리
    Converter->Release();
    Frame->Release();
    Decoder->Release();

    return SUCCEEDED(hr) ? ResultBitmap : nullptr;
}

bool UProgressBarWidget::LoadForegroundTexture(const wchar_t* Path, ID2D1DeviceContext* Context)
{
    ReleaseBitmap(ForegroundBitmap);
    ForegroundBitmap = LoadBitmapFromFile(Path, Context);
    return ForegroundBitmap != nullptr;
}

bool UProgressBarWidget::LoadBackgroundTexture(const wchar_t* Path, ID2D1DeviceContext* Context)
{
    ReleaseBitmap(BackgroundBitmap);
    BackgroundBitmap = LoadBitmapFromFile(Path, Context);
    return BackgroundBitmap != nullptr;
}

bool UProgressBarWidget::LoadLowTexture(const wchar_t* Path, ID2D1DeviceContext* Context)
{
    ReleaseBitmap(LowBitmap);
    LowBitmap = LoadBitmapFromFile(Path, Context);
    return LowBitmap != nullptr;
}

void UProgressBarWidget::CalculateSubUVRect(int32_t FrameIndex, int32_t NX, int32_t NY,
                                            float BitmapWidth, float BitmapHeight,
                                            D2D1_RECT_F& OutSrcRect) const
{
    int32_t TotalFrames = NX * NY;
    if (TotalFrames <= 1)
    {
        // SubUV 비활성화
        OutSrcRect = D2D1::RectF(0, 0, BitmapWidth, BitmapHeight);
        return;
    }

    FrameIndex = (std::max)(0, (std::min)(FrameIndex, TotalFrames - 1));

    int32_t tileX = FrameIndex % NX;
    int32_t tileY = FrameIndex / NX;

    float tileWidth = BitmapWidth / static_cast<float>(NX);
    float tileHeight = BitmapHeight / static_cast<float>(NY);

    OutSrcRect = D2D1::RectF(
        tileX * tileWidth,
        tileY * tileHeight,
        (tileX + 1) * tileWidth,
        (tileY + 1) * tileHeight
    );
}

void UProgressBarWidget::Render(ID2D1DeviceContext* Context)
{
    if (!Context || !bVisible)
        return;

    // Progress 값 클램프
    float ClampedProgress = (std::max)(0.0f, (std::min)(1.0f, Progress));

    // 전체 영역
    D2D1_RECT_F BgRect = GetRect();

    // 프로그레스 바 영역 계산
    float FilledWidth = Width * ClampedProgress;
    D2D1_RECT_F FgRect;

    if (bRightToLeft)
    {
        // 오른쪽에서 왼쪽으로 (P2용)
        FgRect = D2D1::RectF(X + Width - FilledWidth, Y, X + Width, Y + Height);
    }
    else
    {
        // 왼쪽에서 오른쪽으로 (기본)
        FgRect = D2D1::RectF(X, Y, X + FilledWidth, Y + Height);
    }

    // 낮은 값 여부 확인
    bool bIsLow = bUseLowWarning && ClampedProgress <= LowThreshold;

    // === 배경 그리기 ===
    if (BackgroundBitmap)
    {
        D2D1_SIZE_F BitmapSize = BackgroundBitmap->GetSize();
        D2D1_RECT_F SrcRect;

        // SubUV 적용
        CalculateSubUVRect(BgSubImageFrame, BgSubImages_Horizontal, BgSubImages_Vertical,
                           BitmapSize.width, BitmapSize.height, SrcRect);

        Context->DrawBitmap(
            BackgroundBitmap,
            BgRect,
            TextureOpacity,
            D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
            SrcRect
        );
    }
    else
    {
        // 색상 모드
        ID2D1SolidColorBrush* BrushBg = nullptr;
        Context->CreateSolidColorBrush(BackgroundColor, &BrushBg);
        if (BrushBg)
        {
            Context->FillRectangle(BgRect, BrushBg);
            BrushBg->Release();
        }
    }

    // === 전경 (진행도) 그리기 ===
    ID2D1Bitmap* CurrentFgBitmap = ForegroundBitmap;
    if (bIsLow && LowBitmap)
    {
        CurrentFgBitmap = LowBitmap;
    }

    if (CurrentFgBitmap && ClampedProgress > 0.0f)
    {
        D2D1_SIZE_F BitmapSize = CurrentFgBitmap->GetSize();

        // SubUV로 타일 영역 계산
        D2D1_RECT_F TileSrcRect;
        CalculateSubUVRect(FgSubImageFrame, FgSubImages_Horizontal, FgSubImages_Vertical,
                           BitmapSize.width, BitmapSize.height, TileSrcRect);

        float TileWidth = TileSrcRect.right - TileSrcRect.left;
        float TileHeight = TileSrcRect.bottom - TileSrcRect.top;

        // 진행도에 따라 클리핑
        D2D1_RECT_F SrcRect;
        if (bRightToLeft)
        {
            float SrcLeft = TileSrcRect.left + TileWidth * (1.0f - ClampedProgress);
            SrcRect = D2D1::RectF(SrcLeft, TileSrcRect.top, TileSrcRect.right, TileSrcRect.bottom);
        }
        else
        {
            float SrcRight = TileSrcRect.left + TileWidth * ClampedProgress;
            SrcRect = D2D1::RectF(TileSrcRect.left, TileSrcRect.top, SrcRight, TileSrcRect.bottom);
        }

        Context->DrawBitmap(
            CurrentFgBitmap,
            FgRect,
            TextureOpacity,
            D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
            SrcRect
        );
    }
    else if (!CurrentFgBitmap && ClampedProgress > 0.0f)
    {
        // 색상 모드
        D2D1_COLOR_F CurrentFgColor = bIsLow ? LowColor : ForegroundColor;

        ID2D1SolidColorBrush* BrushFg = nullptr;
        Context->CreateSolidColorBrush(CurrentFgColor, &BrushFg);
        if (BrushFg)
        {
            Context->FillRectangle(FgRect, BrushFg);
            BrushFg->Release();
        }
    }

    // === 테두리 그리기 ===
    if (BorderWidth > 0.0f)
    {
        ID2D1SolidColorBrush* BrushBorder = nullptr;
        Context->CreateSolidColorBrush(BorderColor, &BrushBorder);
        if (BrushBorder)
        {
            Context->DrawRectangle(BgRect, BrushBorder, BorderWidth);
            BrushBorder->Release();
        }
    }
}

void UProgressBarWidget::SetProgress(float Value)
{
    Progress = (std::max)(0.0f, (std::min)(1.0f, Value));
}

void UProgressBarWidget::SetForegroundColor(float R, float G, float B, float A)
{
    ForegroundColor = D2D1::ColorF(R, G, B, A);
}

void UProgressBarWidget::SetBackgroundColor(float R, float G, float B, float A)
{
    BackgroundColor = D2D1::ColorF(R, G, B, A);
}

void UProgressBarWidget::SetBorderColor(float R, float G, float B, float A)
{
    BorderColor = D2D1::ColorF(R, G, B, A);
}

void UProgressBarWidget::SetLowColor(float R, float G, float B, float A)
{
    LowColor = D2D1::ColorF(R, G, B, A);
}

void UProgressBarWidget::SetTextureOpacity(float Opacity)
{
    TextureOpacity = (std::max)(0.0f, (std::min)(1.0f, Opacity));
}

// === SubUV 함수 ===

void UProgressBarWidget::SetForegroundSubUVGrid(int32_t NX, int32_t NY)
{
    FgSubImages_Horizontal = (std::max)(1, NX);
    FgSubImages_Vertical = (std::max)(1, NY);
}

void UProgressBarWidget::SetForegroundSubUVFrame(int32_t FrameIndex)
{
    int32_t TotalFrames = FgSubImages_Horizontal * FgSubImages_Vertical;
    FgSubImageFrame = (std::max)(0, (std::min)(FrameIndex, TotalFrames - 1));
}

void UProgressBarWidget::SetForegroundSubUV(int32_t FrameIndex, int32_t NX, int32_t NY)
{
    SetForegroundSubUVGrid(NX, NY);
    SetForegroundSubUVFrame(FrameIndex);
}

void UProgressBarWidget::SetBackgroundSubUVGrid(int32_t NX, int32_t NY)
{
    BgSubImages_Horizontal = (std::max)(1, NX);
    BgSubImages_Vertical = (std::max)(1, NY);
}

void UProgressBarWidget::SetBackgroundSubUVFrame(int32_t FrameIndex)
{
    int32_t TotalFrames = BgSubImages_Horizontal * BgSubImages_Vertical;
    BgSubImageFrame = (std::max)(0, (std::min)(FrameIndex, TotalFrames - 1));
}

void UProgressBarWidget::SetBackgroundSubUV(int32_t FrameIndex, int32_t NX, int32_t NY)
{
    SetBackgroundSubUVGrid(NX, NY);
    SetBackgroundSubUVFrame(FrameIndex);
}
