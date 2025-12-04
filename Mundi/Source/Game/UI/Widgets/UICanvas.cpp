#include "pch.h"
#include "UICanvas.h"
#include "ProgressBarWidget.h"
#include "TextureWidget.h"
#include <algorithm>

UUICanvas::UUICanvas()
{
}

// ============================================
// 위젯 생성
// ============================================

bool UUICanvas::CreateProgressBar(const std::string& WidgetName, float PosX, float PosY, float W, float H)
{
    // 이미 존재하면 실패
    if (Widgets.find(WidgetName) != Widgets.end())
        return false;

    auto Widget = std::make_unique<UProgressBarWidget>();
    Widget->Name = WidgetName;
    Widget->X = PosX;
    Widget->Y = PosY;
    Widget->Width = W;
    Widget->Height = H;

    Widgets[WidgetName] = std::move(Widget);
    bWidgetsSortDirty = true;

    return true;
}

bool UUICanvas::CreateTextureWidget(const std::string& WidgetName, const std::string& TexturePath,
                                    float PosX, float PosY, float W, float H,
                                    ID2D1DeviceContext* D2DContext)
{
    if (!D2DContext)
        return false;

    // 이미 존재하면 실패
    if (Widgets.find(WidgetName) != Widgets.end())
        return false;

    auto Widget = std::make_unique<UTextureWidget>();
    Widget->Name = WidgetName;
    Widget->X = PosX;
    Widget->Y = PosY;
    Widget->Width = W;
    Widget->Height = H;

    // 텍스처 로드
    std::wstring WidePath(TexturePath.begin(), TexturePath.end());
    if (!Widget->LoadFromFile(WidePath.c_str(), D2DContext))
    {
        return false;
    }

    Widgets[WidgetName] = std::move(Widget);
    bWidgetsSortDirty = true;

    return true;
}

bool UUICanvas::CreateRect(const std::string& WidgetName, float PosX, float PosY, float W, float H)
{
    // Rect는 Progress가 1.0인 ProgressBar로 구현
    if (Widgets.find(WidgetName) != Widgets.end())
        return false;

    auto Widget = std::make_unique<UProgressBarWidget>();
    Widget->Name = WidgetName;
    Widget->X = PosX;
    Widget->Y = PosY;
    Widget->Width = W;
    Widget->Height = H;
    Widget->Progress = 1.0f;
    Widget->BorderWidth = 0.0f;  // Rect는 테두리 없음
    Widget->bUseLowWarning = false;

    Widgets[WidgetName] = std::move(Widget);
    bWidgetsSortDirty = true;

    return true;
}

// ============================================
// 위젯 관리
// ============================================

UUIWidget* UUICanvas::FindWidget(const std::string& WidgetName)
{
    auto it = Widgets.find(WidgetName);
    if (it != Widgets.end())
        return it->second.get();
    return nullptr;
}

void UUICanvas::RemoveWidget(const std::string& WidgetName)
{
    Widgets.erase(WidgetName);
    bWidgetsSortDirty = true;
}

void UUICanvas::RemoveAllWidgets()
{
    Widgets.clear();
    SortedWidgets.clear();
    bWidgetsSortDirty = false;
}

// ============================================
// 위젯 속성 설정
// ============================================

void UUICanvas::SetWidgetProgress(const std::string& WidgetName, float Value)
{
    if (auto* Widget = dynamic_cast<UProgressBarWidget*>(FindWidget(WidgetName)))
    {
        Widget->SetProgress(Value);
    }
}

void UUICanvas::SetWidgetPosition(const std::string& WidgetName, float PosX, float PosY)
{
    if (auto* Widget = FindWidget(WidgetName))
    {
        Widget->SetPosition(PosX, PosY);
    }
}

void UUICanvas::SetWidgetSize(const std::string& WidgetName, float W, float H)
{
    if (auto* Widget = FindWidget(WidgetName))
    {
        Widget->SetSize(W, H);
    }
}

void UUICanvas::SetWidgetVisible(const std::string& WidgetName, bool bVis)
{
    if (auto* Widget = FindWidget(WidgetName))
    {
        Widget->SetVisible(bVis);
    }
}

void UUICanvas::SetWidgetZOrder(const std::string& WidgetName, int32_t Z)
{
    if (auto* Widget = FindWidget(WidgetName))
    {
        Widget->SetZOrder(Z);
        bWidgetsSortDirty = true;
    }
}

void UUICanvas::SetWidgetForegroundColor(const std::string& WidgetName, float R, float G, float B, float A)
{
    if (auto* Widget = dynamic_cast<UProgressBarWidget*>(FindWidget(WidgetName)))
    {
        Widget->SetForegroundColor(R, G, B, A);
    }
}

void UUICanvas::SetWidgetBackgroundColor(const std::string& WidgetName, float R, float G, float B, float A)
{
    if (auto* Widget = dynamic_cast<UProgressBarWidget*>(FindWidget(WidgetName)))
    {
        Widget->SetBackgroundColor(R, G, B, A);
    }
}

void UUICanvas::SetWidgetRightToLeft(const std::string& WidgetName, bool bRTL)
{
    if (auto* Widget = dynamic_cast<UProgressBarWidget*>(FindWidget(WidgetName)))
    {
        Widget->SetRightToLeft(bRTL);
    }
}

// ============================================
// 위젯 애니메이션
// ============================================

void UUICanvas::MoveWidget(const std::string& WidgetName, float TargetX, float TargetY,
                           float Duration, EEasingType Easing)
{
    if (auto* Widget = FindWidget(WidgetName))
    {
        Widget->MoveTo(TargetX, TargetY, Duration, Easing);
    }
}

void UUICanvas::ResizeWidget(const std::string& WidgetName, float TargetW, float TargetH,
                             float Duration, EEasingType Easing)
{
    if (auto* Widget = FindWidget(WidgetName))
    {
        Widget->SizeTo(TargetW, TargetH, Duration, Easing);
    }
}

void UUICanvas::RotateWidget(const std::string& WidgetName, float TargetAngle,
                             float Duration, EEasingType Easing)
{
    if (auto* Widget = FindWidget(WidgetName))
    {
        Widget->RotateTo(TargetAngle, Duration, Easing);
    }
}

void UUICanvas::FadeWidget(const std::string& WidgetName, float TargetOpacity,
                           float Duration, EEasingType Easing)
{
    if (auto* Widget = FindWidget(WidgetName))
    {
        Widget->FadeTo(TargetOpacity, Duration, Easing);
    }
}

void UUICanvas::StopWidgetAnimation(const std::string& WidgetName)
{
    if (auto* Widget = FindWidget(WidgetName))
    {
        Widget->StopAnimation();
    }
}

// ============================================
// 업데이트 및 렌더링
// ============================================

void UUICanvas::Update(float DeltaTime)
{
    if (!bVisible)
        return;

    // 모든 위젯 업데이트 (애니메이션 처리)
    for (auto& Pair : Widgets)
    {
        if (Pair.second)
        {
            Pair.second->Update(DeltaTime);
        }
    }
}

void UUICanvas::UpdateWidgetSortOrder()
{
    SortedWidgets.clear();
    SortedWidgets.reserve(Widgets.size());

    for (auto& Pair : Widgets)
    {
        SortedWidgets.push_back(Pair.second.get());
    }

    // ZOrder 오름차순 정렬 (낮은 것이 먼저 그려짐)
    std::sort(SortedWidgets.begin(), SortedWidgets.end(),
        [](const UUIWidget* A, const UUIWidget* B)
        {
            return A->ZOrder < B->ZOrder;
        });

    bWidgetsSortDirty = false;
}

void UUICanvas::Render(ID2D1DeviceContext* D2DContext, float ViewportOffsetX, float ViewportOffsetY)
{
    if (!D2DContext || !bVisible || Widgets.empty())
        return;

    // 정렬이 필요하면 갱신
    if (bWidgetsSortDirty)
    {
        UpdateWidgetSortOrder();
    }

    // 캔버스 오프셋 + 뷰포트 오프셋 적용
    D2D1_MATRIX_3X2_F OriginalTransform;
    D2DContext->GetTransform(&OriginalTransform);

    // 변환: 뷰포트 오프셋 + 캔버스 위치
    D2DContext->SetTransform(
        D2D1::Matrix3x2F::Translation(ViewportOffsetX + X, ViewportOffsetY + Y) * OriginalTransform
    );

    // ZOrder 순서대로 렌더링
    for (UUIWidget* Widget : SortedWidgets)
    {
        if (Widget && Widget->bVisible)
        {
            Widget->Render(D2DContext);
        }
    }

    // Transform 복원
    D2DContext->SetTransform(OriginalTransform);
}
