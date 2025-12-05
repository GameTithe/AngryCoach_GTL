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

    // === SubUV (스프라이트 시트 애니메이션용) ===
    int32_t SubImages_Horizontal = 1;  // NX (가로 타일 수)
    int32_t SubImages_Vertical = 1;    // NY (세로 타일 수)

    // === 색상 틴트 ===
    // 기본 흰색 (원본 색상 유지)
    D2D1_COLOR_F Tint;

    // === 투명도 ===
    float Opacity = 1.0f;

    // === 블렌드 모드 ===
    EUIBlendMode BlendMode = EUIBlendMode::Normal;

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
    void SetBlendMode(EUIBlendMode Mode) { BlendMode = Mode; }
    void SetAdditive(bool bAdditive) { BlendMode = bAdditive ? EUIBlendMode::Additive : EUIBlendMode::Normal; }

    // === SubUV 함수 ===
    /**
     * @brief SubUV 그리드 설정
     * @param NX 가로 타일 수
     * @param NY 세로 타일 수
     */
    void SetSubUVGrid(int32_t NX, int32_t NY);

    /**
     * @brief SubUV 프레임 설정 (UV 자동 계산)
     * @param FrameIndex 프레임 인덱스 (0부터 시작, 왼쪽→오른쪽, 위→아래 순서)
     */
    void SetSubUVFrame(int32_t FrameIndex);

    /**
     * @brief SubUV 프레임 설정 (그리드도 함께 설정)
     * @param FrameIndex 프레임 인덱스
     * @param NX 가로 타일 수
     * @param NY 세로 타일 수
     */
    void SetSubUVFrameWithGrid(int32_t FrameIndex, int32_t NX, int32_t NY);

    /**
     * @brief 전체 프레임 수 반환
     */
    int32_t GetTotalFrames() const { return SubImages_Horizontal * SubImages_Vertical; }

    /**
     * @brief WIC 팩토리 정리 (프로그램 종료 시 호출)
     */
    static void ShutdownWICFactory();

private:
    // WIC 팩토리 (이미지 로딩용)
    static IWICImagingFactory* WICFactory;
    static bool InitWICFactory();

    // 텍스처 해제
    void ReleaseTexture();
};
