#include "pch.h"
#include "GameUIBase.h"

void UGameUIBase::Initialize(ID2D1DeviceContext* InContext, IDWriteFactory* InDWriteFactory)
{
    D2DContext = InContext;
    DWriteFactory = InDWriteFactory;
}

void UGameUIBase::Shutdown()
{
    D2DContext = nullptr;
    DWriteFactory = nullptr;
}

void UGameUIBase::Update(float DeltaTime)
{
    // 기본 구현은 비어있음 - 파생 클래스에서 오버라이드
}

void UGameUIBase::Render()
{
    // 기본 구현은 비어있음 - 파생 클래스에서 오버라이드
}

void UGameUIBase::DrawRect(float X, float Y, float W, float H, ID2D1Brush* Brush)
{
    if (!D2DContext || !Brush)
        return;

    D2D1_RECT_F Rect = D2D1::RectF(X, Y, X + W, Y + H);
    D2DContext->FillRectangle(Rect, Brush);
}

void UGameUIBase::DrawRectOutline(float X, float Y, float W, float H, ID2D1Brush* Brush, float StrokeWidth)
{
    if (!D2DContext || !Brush)
        return;

    D2D1_RECT_F Rect = D2D1::RectF(X, Y, X + W, Y + H);
    D2DContext->DrawRectangle(Rect, Brush, StrokeWidth);
}

void UGameUIBase::DrawText(const wchar_t* Text, float X, float Y, float W, float H, IDWriteTextFormat* Format, ID2D1Brush* Brush)
{
    if (!D2DContext || !Text || !Format || !Brush)
        return;

    D2D1_RECT_F Rect = D2D1::RectF(X, Y, X + W, Y + H);
    D2DContext->DrawTextW(Text, static_cast<UINT32>(wcslen(Text)), Format, Rect, Brush);
}

void UGameUIBase::DrawProgressBar(float X, float Y, float W, float H, float Progress, ID2D1Brush* FgBrush, ID2D1Brush* BgBrush)
{
    if (!D2DContext)
        return;

    Progress = (std::max)(0.0f, (std::min)(1.0f, Progress));

    // 배경
    if (BgBrush)
    {
        D2D1_RECT_F BgRect = D2D1::RectF(X, Y, X + W, Y + H);
        D2DContext->FillRectangle(BgRect, BgBrush);
    }

    // 전경 (진행률)
    if (FgBrush)
    {
        float FilledWidth = W * Progress;
        D2D1_RECT_F FgRect = D2D1::RectF(X, Y, X + FilledWidth, Y + H);
        D2DContext->FillRectangle(FgRect, FgBrush);
    }
}
