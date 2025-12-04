#include "pch.h"
#include "TextureWidget.h"

#pragma comment(lib, "windowscodecs.lib")

// 정적 멤버 초기화
IWICImagingFactory* UTextureWidget::WICFactory = nullptr;

UTextureWidget::UTextureWidget()
{
    Type = EUIWidgetType::Texture;
    Tint = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);  // 흰색 (원본 유지)
}

UTextureWidget::~UTextureWidget()
{
    ReleaseTexture();
}

bool UTextureWidget::InitWICFactory()
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

void UTextureWidget::ReleaseTexture()
{
    if (Bitmap)
    {
        Bitmap->Release();
        Bitmap = nullptr;
    }
}

bool UTextureWidget::LoadFromFile(const wchar_t* Path, ID2D1DeviceContext* Context)
{
    if (!Path || !Context)
        return false;

    // WIC 팩토리 초기화
    if (!InitWICFactory())
        return false;

    // 기존 텍스처 해제
    ReleaseTexture();

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
        return false;

    // 프레임 가져오기
    IWICBitmapFrameDecode* Frame = nullptr;
    hr = Decoder->GetFrame(0, &Frame);

    if (FAILED(hr))
    {
        Decoder->Release();
        return false;
    }

    // 포맷 컨버터 생성
    IWICFormatConverter* Converter = nullptr;
    hr = WICFactory->CreateFormatConverter(&Converter);

    if (FAILED(hr))
    {
        Frame->Release();
        Decoder->Release();
        return false;
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
        return false;
    }

    // D2D 비트맵 생성
    hr = Context->CreateBitmapFromWicBitmap(Converter, nullptr, &Bitmap);

    // 정리
    Converter->Release();
    Frame->Release();
    Decoder->Release();

    return SUCCEEDED(hr);
}

void UTextureWidget::Render(ID2D1DeviceContext* Context)
{
    if (!Context || !Bitmap || !bVisible)
        return;

    // 대상 사각형
    D2D1_RECT_F DestRect = GetRect();

    // 소스 사각형 (UV 좌표 기반)
    D2D1_SIZE_F BitmapSize = Bitmap->GetSize();
    D2D1_RECT_F SrcRect = D2D1::RectF(
        U0 * BitmapSize.width,
        V0 * BitmapSize.height,
        U1 * BitmapSize.width,
        V1 * BitmapSize.height
    );

    // 실제 투명도 계산 (Tint의 A와 Opacity 조합)
    float FinalOpacity = Tint.a * Opacity;

    // 비트맵 그리기
    Context->DrawBitmap(
        Bitmap,
        DestRect,
        FinalOpacity,
        D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
        SrcRect
    );
}

void UTextureWidget::SetUV(float u0, float v0, float u1, float v1)
{
    U0 = u0;
    V0 = v0;
    U1 = u1;
    V1 = v1;
}

void UTextureWidget::SetTint(float R, float G, float B, float A)
{
    Tint = D2D1::ColorF(R, G, B, A);
}

void UTextureWidget::SetOpacity(float Alpha)
{
    Opacity = (std::max)(0.0f, (std::min)(1.0f, Alpha));
}
