#pragma once

#include "UIWidget.h"

/**
 * @brief 프로그레스 바 위젯 (체력바, 스태미나 바 등)
 *
 * D2D로 직접 그리는 게이지 형태의 UI 위젯입니다.
 * Lua에서 Progress 값을 설정하여 동적으로 업데이트할 수 있습니다.
 */
class UProgressBarWidget : public UUIWidget
{
public:
    UProgressBarWidget();
    virtual ~UProgressBarWidget() = default;

    // === 프로그레스 값 ===
    float Progress = 1.0f;  // 0.0 ~ 1.0

    // === 색상 ===
    D2D1_COLOR_F ForegroundColor;   // 채워진 부분
    D2D1_COLOR_F BackgroundColor;   // 빈 부분 (배경)
    D2D1_COLOR_F BorderColor;       // 테두리
    float BorderWidth = 2.0f;

    // === 방향 ===
    bool bRightToLeft = false;      // true면 오른쪽에서 왼쪽으로 채움 (P2용)

    // === 낮은 값 경고 ===
    bool bUseLowWarning = true;
    float LowThreshold = 0.3f;      // 이 값 이하면 LowColor 사용
    D2D1_COLOR_F LowColor;          // 낮을 때 색상 (빨강)

    // === 렌더링 ===
    void Render(ID2D1DeviceContext* Context) override;

    // === Lua용 setter ===
    void SetProgress(float Value);
    void SetForegroundColor(float R, float G, float B, float A);
    void SetBackgroundColor(float R, float G, float B, float A);
    void SetBorderColor(float R, float G, float B, float A);
    void SetLowColor(float R, float G, float B, float A);
    void SetRightToLeft(bool bRTL) { bRightToLeft = bRTL; }
    void SetLowThreshold(float Threshold) { LowThreshold = Threshold; }
    void SetUseLowWarning(bool bUse) { bUseLowWarning = bUse; }

private:
    // 내부에서 사용하는 브러시들 (렌더링 시 Context에서 생성)
    // Note: 브러시는 매 프레임 생성하지 않고 캐싱하는 것이 좋으나,
    //       GameUIManager의 브러시를 공유하는 방식으로 최적화 가능
};
