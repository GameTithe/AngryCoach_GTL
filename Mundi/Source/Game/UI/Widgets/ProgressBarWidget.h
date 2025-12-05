#pragma once

#include "UIWidget.h"
#include <wincodec.h>

/**
 * @brief 프로그레스 바 위젯 (체력바, 스태미나 바 등)
 *
 * D2D로 직접 그리는 게이지 형태의 UI 위젯입니다.
 * Lua에서 Progress 값을 설정하여 동적으로 업데이트할 수 있습니다.
 *
 * 텍스처 모드: 전경/배경에 텍스처를 사용하여 진행도를 표시할 수 있습니다.
 * 텍스처가 설정되면 색상 대신 텍스처가 사용됩니다.
 */
class UProgressBarWidget : public UUIWidget
{
public:
    UProgressBarWidget();
    virtual ~UProgressBarWidget();

    // === 프로그레스 값 ===
    float Progress = 1.0f;  // 0.0 ~ 1.0

    // === 색상 (텍스처 미사용 시) ===
    D2D1_COLOR_F ForegroundColor;   // 채워진 부분
    D2D1_COLOR_F BackgroundColor;   // 빈 부분 (배경)
    D2D1_COLOR_F BorderColor;       // 테두리
    float BorderWidth = 2.0f;

    // === 텍스처 모드 ===
    ID2D1Bitmap* ForegroundBitmap = nullptr;  // 전경 텍스처 (채워진 부분)
    ID2D1Bitmap* BackgroundBitmap = nullptr;  // 배경 텍스처
    ID2D1Bitmap* LowBitmap = nullptr;         // 낮은 값일 때 전경 텍스처 (선택)
    float TextureOpacity = 1.0f;              // 텍스처 투명도

    // === SubUV (스프라이트 시트용) ===
    // 전경 텍스처용 SubUV
    int32_t FgSubImages_Horizontal = 1;
    int32_t FgSubImages_Vertical = 1;
    int32_t FgSubImageFrame = 0;
    // 배경 텍스처용 SubUV
    int32_t BgSubImages_Horizontal = 1;
    int32_t BgSubImages_Vertical = 1;
    int32_t BgSubImageFrame = 0;

    // === 방향 ===
    bool bRightToLeft = false;      // true면 오른쪽에서 왼쪽으로 채움 (P2용)

    // === 낮은 값 경고 ===
    bool bUseLowWarning = true;
    float LowThreshold = 0.3f;      // 이 값 이하면 LowColor/LowBitmap 사용
    D2D1_COLOR_F LowColor;          // 낮을 때 색상 (빨강)

    // === 렌더링 ===
    void Render(ID2D1DeviceContext* Context) override;

    // === 텍스처 로딩 ===
    bool LoadForegroundTexture(const wchar_t* Path, ID2D1DeviceContext* Context);
    bool LoadBackgroundTexture(const wchar_t* Path, ID2D1DeviceContext* Context);
    bool LoadLowTexture(const wchar_t* Path, ID2D1DeviceContext* Context);
    void ClearTextures();

    // === Lua용 setter ===
    void SetProgress(float Value);
    void SetForegroundColor(float R, float G, float B, float A);
    void SetBackgroundColor(float R, float G, float B, float A);
    void SetBorderColor(float R, float G, float B, float A);
    void SetLowColor(float R, float G, float B, float A);
    void SetRightToLeft(bool bRTL) { bRightToLeft = bRTL; }
    void SetLowThreshold(float Threshold) { LowThreshold = Threshold; }
    void SetUseLowWarning(bool bUse) { bUseLowWarning = bUse; }
    void SetTextureOpacity(float Opacity);

    // === SubUV 함수 ===
    void SetForegroundSubUVGrid(int32_t NX, int32_t NY);
    void SetForegroundSubUVFrame(int32_t FrameIndex);
    void SetForegroundSubUV(int32_t FrameIndex, int32_t NX, int32_t NY);
    void SetBackgroundSubUVGrid(int32_t NX, int32_t NY);
    void SetBackgroundSubUVFrame(int32_t FrameIndex);
    void SetBackgroundSubUV(int32_t FrameIndex, int32_t NX, int32_t NY);

private:
    // SubUV UV 계산 헬퍼
    void CalculateSubUVRect(int32_t FrameIndex, int32_t NX, int32_t NY,
                            float BitmapWidth, float BitmapHeight,
                            D2D1_RECT_F& OutSrcRect) const;
    // WIC 팩토리 (이미지 로딩용) - TextureWidget과 공유
    static IWICImagingFactory* WICFactory;
    static bool InitWICFactory();

    // 텍스처 로딩 헬퍼
    ID2D1Bitmap* LoadBitmapFromFile(const wchar_t* Path, ID2D1DeviceContext* Context);

    // 텍스처 해제
    void ReleaseBitmap(ID2D1Bitmap*& Bitmap);
};
