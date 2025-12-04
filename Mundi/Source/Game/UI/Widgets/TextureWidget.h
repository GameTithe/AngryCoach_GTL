#pragma once

#include "UIWidget.h"
#include <wincodec.h>

/**
 * @brief 텍스처 위젯 (이미지 기반 UI)
 *
 * PNG, JPG 등의 이미지 파일을 로드하여 표시합니다.
 * 스프라이트 시트를 사용할 경우 UV 좌표로 일부분만 표시할 수 있습니다.
 */
class UTextureWidget : public UUIWidget
{
public:
    UTextureWidget();
    virtual ~UTextureWidget();

    // === 텍스처 ===
    ID2D1Bitmap* Bitmap = nullptr;

    // === UV 좌표 (스프라이트 시트용) ===
    // 0.0 ~ 1.0 범위, 기본값은 전체 이미지
    float U0 = 0.0f;
    float V0 = 0.0f;
    float U1 = 1.0f;
    float V1 = 1.0f;

    // === 색상 틴트 ===
    // 기본 흰색 (원본 색상 유지)
    D2D1_COLOR_F Tint;

    // === 투명도 ===
    float Opacity = 1.0f;

    /**
     * @brief 파일에서 텍스처 로드
     * @param Path 이미지 파일 경로 (PNG, JPG 등)
     * @param Context D2D 디바이스 컨텍스트
     * @return 성공 여부
     */
    bool LoadFromFile(const wchar_t* Path, ID2D1DeviceContext* Context);

    /**
     * @brief 렌더링
     */
    void Render(ID2D1DeviceContext* Context) override;

    // === Lua용 setter ===
    void SetUV(float u0, float v0, float u1, float v1);
    void SetTint(float R, float G, float B, float A);
    void SetOpacity(float Alpha);

private:
    // WIC 팩토리 (이미지 로딩용)
    static IWICImagingFactory* WICFactory;
    static bool InitWICFactory();

    // 텍스처 해제
    void ReleaseTexture();
};
