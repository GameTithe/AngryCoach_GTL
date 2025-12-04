#include "pch.h"
#include "ProgressBarWidget.h"
#include <algorithm>

UProgressBarWidget::UProgressBarWidget()
{
    Type = EUIWidgetType::ProgressBar;

    // 기본 색상 설정
    ForegroundColor = D2D1::ColorF(0.2f, 0.8f, 0.2f, 1.0f);  // 초록색
    BackgroundColor = D2D1::ColorF(0.2f, 0.2f, 0.2f, 0.8f);  // 어두운 회색
    BorderColor = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);      // 흰색
    LowColor = D2D1::ColorF(0.9f, 0.2f, 0.2f, 1.0f);         // 빨간색
}

void UProgressBarWidget::Render(ID2D1DeviceContext* Context)
{
    if (!Context || !bVisible)
        return;

    // Progress 값 클램프
    float ClampedProgress = (std::max)(0.0f, (std::min)(1.0f, Progress));

    // 브러시 생성 (매 프레임 생성은 비효율적이나, 위젯별 색상 지원을 위해)
    // TODO: 브러시 캐싱 최적화
    ID2D1SolidColorBrush* BrushFg = nullptr;
    ID2D1SolidColorBrush* BrushBg = nullptr;
    ID2D1SolidColorBrush* BrushBorder = nullptr;

    // 전경색 결정 (낮은 값 경고)
    D2D1_COLOR_F CurrentFgColor = ForegroundColor;
    if (bUseLowWarning && ClampedProgress <= LowThreshold)
    {
        CurrentFgColor = LowColor;
    }

    Context->CreateSolidColorBrush(CurrentFgColor, &BrushFg);
    Context->CreateSolidColorBrush(BackgroundColor, &BrushBg);
    Context->CreateSolidColorBrush(BorderColor, &BrushBorder);

    if (!BrushFg || !BrushBg || !BrushBorder)
    {
        if (BrushFg) BrushFg->Release();
        if (BrushBg) BrushBg->Release();
        if (BrushBorder) BrushBorder->Release();
        return;
    }

    // 배경 그리기
    D2D1_RECT_F BgRect = GetRect();
    Context->FillRectangle(BgRect, BrushBg);

    // 프로그레스 바 그리기
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

    Context->FillRectangle(FgRect, BrushFg);

    // 테두리 그리기
    if (BorderWidth > 0.0f)
    {
        Context->DrawRectangle(BgRect, BrushBorder, BorderWidth);
    }

    // 브러시 해제
    BrushFg->Release();
    BrushBg->Release();
    BrushBorder->Release();
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
