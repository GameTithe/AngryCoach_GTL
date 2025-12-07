#include "pch.h"
#include "TextureWidget.h"
#include "Source/Slate/GlobalConsole.h"

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
    UE_LOG("[UI] ~UTextureWidget: Destroying '%s', Bitmap=%p\n", Name.c_str(), Bitmap);
    ReleaseTexture();
}

bool UTextureWidget::InitWICFactory()
{
    if (WICFactory)
        return true;

    // COM 초기화 (WIC는 COM 기반)
    // COINIT_MULTITHREADED는 D3D11과 호환됨
    // 이미 초기화된 경우 S_FALSE 또는 RPC_E_CHANGED_MODE 반환하므로 안전
    HRESULT hrCom = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hrCom) && hrCom != RPC_E_CHANGED_MODE)
    {
        // RPC_E_CHANGED_MODE는 다른 모드로 이미 초기화된 경우 - 무시 가능
        return false;
    }

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

void UTextureWidget::ShutdownWICFactory()
{
    if (WICFactory)
    {
        WICFactory->Release();
        WICFactory = nullptr;
    }
}

bool UTextureWidget::LoadFromFile(const wchar_t* Path, ID2D1DeviceContext* Context)
{
    if (!Path || !Context)
    {
        UE_LOG("[UI] TextureWidget::LoadFromFile failed: Path=%p, Context=%p\n", Path, Context);
        return false;
    }

    // WIC 팩토리 초기화
    if (!InitWICFactory())
    {
        UE_LOG("[UI] TextureWidget::LoadFromFile failed: WIC Factory init failed\n");
        return false;
    }

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
    {
        UE_LOG("[UI] TextureWidget::LoadFromFile failed: CreateDecoderFromFilename hr=0x%08X, Path=%ls\n", hr, Path);
        return false;
    }

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

    // 디버그: 렌더링 시 실제 사용되는 Opacity 확인
    static int renderLogCount = 0;
    if (renderLogCount < 20)
    {
        UE_LOG("[UI] TextureWidget::Render %s: Opacity=%.3f, Tint.a=%.3f, FinalOpacity=%.3f\n",
            Name.c_str(), Opacity, Tint.a, FinalOpacity);
        renderLogCount++;
    }

    // 블렌드 모드 설정
    D2D1_PRIMITIVE_BLEND PrevBlend = Context->GetPrimitiveBlend();
    if (BlendMode == EUIBlendMode::Additive)
    {
        Context->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_ADD);
    }

    // 비트맵 그리기
    Context->DrawBitmap(
        Bitmap,
        DestRect,
        FinalOpacity,
        D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
        SrcRect
    );

    // 블렌드 모드 복원
    if (BlendMode == EUIBlendMode::Additive)
    {
        Context->SetPrimitiveBlend(PrevBlend);
    }
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

void UTextureWidget::SetSubUVGrid(int32_t NX, int32_t NY)
{
    SubImages_Horizontal = (std::max)(1, NX);
    SubImages_Vertical = (std::max)(1, NY);
}

void UTextureWidget::SetSubUVFrame(int32_t FrameIndex)
{
    int32_t TotalFrames = SubImages_Horizontal * SubImages_Vertical;

    if (TotalFrames <= 1)
    {
        // SubUV 비활성화 상태, 전체 이미지 사용
        U0 = 0.0f; V0 = 0.0f;
        U1 = 1.0f; V1 = 1.0f;
        return;
    }

    // 프레임 인덱스 클램프
    FrameIndex = (std::max)(0, (std::min)(FrameIndex, TotalFrames - 1));

    // 타일 좌표 계산
    int32_t tileX = FrameIndex % SubImages_Horizontal;
    int32_t tileY = FrameIndex / SubImages_Horizontal;

    // UV 스케일 및 오프셋 계산
    float scaleX = 1.0f / static_cast<float>(SubImages_Horizontal);
    float scaleY = 1.0f / static_cast<float>(SubImages_Vertical);

    U0 = tileX * scaleX;
    V0 = tileY * scaleY;
    U1 = U0 + scaleX;
    V1 = V0 + scaleY;
}

void UTextureWidget::SetSubUVFrameWithGrid(int32_t FrameIndex, int32_t NX, int32_t NY)
{
    SetSubUVGrid(NX, NY);
    SetSubUVFrame(FrameIndex);
}

void UTextureWidget::Update(float DeltaTime)
{
    // 부모 클래스 Update 호출 (애니메이션 처리)
    UUIWidget::Update(DeltaTime);

    // SubUV 자동 애니메이션 처리
    if (bSubUVAnimating && SubUVFrameRate > 0.0f)
    {
        int32_t TotalFrames = GetTotalFrames();
        if (TotalFrames <= 1)
            return;

        SubUVElapsedTime += DeltaTime;
        float FrameDuration = 1.0f / SubUVFrameRate;

        while (SubUVElapsedTime >= FrameDuration)
        {
            SubUVElapsedTime -= FrameDuration;
            CurrentSubUVFrame++;

            if (CurrentSubUVFrame >= TotalFrames)
            {
                if (bSubUVLoop)
                {
                    CurrentSubUVFrame = 0;
                }
                else
                {
                    CurrentSubUVFrame = TotalFrames - 1;
                    bSubUVAnimating = false;
                    break;
                }
            }

            SetSubUVFrame(CurrentSubUVFrame);
        }
    }
}

void UTextureWidget::StartSubUVAnimation(float FrameRate, bool bLoop)
{
    SubUVFrameRate = (std::max)(0.1f, FrameRate);  // 최소 0.1 FPS
    bSubUVLoop = bLoop;
    bSubUVAnimating = true;
    SubUVElapsedTime = 0.0f;
    CurrentSubUVFrame = 0;
    SetSubUVFrame(0);

    UE_LOG("[UI] TextureWidget '%s': SubUV animation started (FPS=%.1f, Loop=%s)\n",
        Name.c_str(), SubUVFrameRate, bSubUVLoop ? "true" : "false");
}

void UTextureWidget::StopSubUVAnimation()
{
    bSubUVAnimating = false;
    SubUVElapsedTime = 0.0f;

    UE_LOG("[UI] TextureWidget '%s': SubUV animation stopped\n", Name.c_str());
}
