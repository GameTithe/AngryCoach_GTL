#include "pch.h"
#include "UICanvas.h"
#include "ProgressBarWidget.h"
#include "TextureWidget.h"
#include "ButtonWidget.h"
#include "Source/Game/UI/GameUIManager.h"
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

bool UUICanvas::CreateButton(const std::string& WidgetName, const std::string& TexturePath,
                             float PosX, float PosY, float W, float H,
                             ID2D1DeviceContext* D2DContext)
{
    if (!D2DContext)
        return false;

    // 이미 존재하면 실패
    if (Widgets.find(WidgetName) != Widgets.end())
        return false;

    auto Widget = std::make_unique<UButtonWidget>();
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

void UUICanvas::SetWidgetOpacity(const std::string& WidgetName, float Opacity)
{
    if (auto* Widget = FindWidget(WidgetName))
    {
        Widget->Opacity = std::clamp(Opacity, 0.0f, 1.0f);
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
// ProgressBar 텍스처 설정
// ============================================

bool UUICanvas::SetProgressBarForegroundTexture(const std::string& WidgetName, const std::string& Path, ID2D1DeviceContext* Context)
{
    if (!Context)
        return false;

    if (auto* Widget = dynamic_cast<UProgressBarWidget*>(FindWidget(WidgetName)))
    {
        std::wstring WidePath(Path.begin(), Path.end());
        return Widget->LoadForegroundTexture(WidePath.c_str(), Context);
    }
    return false;
}

bool UUICanvas::SetProgressBarBackgroundTexture(const std::string& WidgetName, const std::string& Path, ID2D1DeviceContext* Context)
{
    if (!Context)
        return false;

    if (auto* Widget = dynamic_cast<UProgressBarWidget*>(FindWidget(WidgetName)))
    {
        std::wstring WidePath(Path.begin(), Path.end());
        return Widget->LoadBackgroundTexture(WidePath.c_str(), Context);
    }
    return false;
}

bool UUICanvas::SetProgressBarLowTexture(const std::string& WidgetName, const std::string& Path, ID2D1DeviceContext* Context)
{
    if (!Context)
        return false;

    if (auto* Widget = dynamic_cast<UProgressBarWidget*>(FindWidget(WidgetName)))
    {
        std::wstring WidePath(Path.begin(), Path.end());
        return Widget->LoadLowTexture(WidePath.c_str(), Context);
    }
    return false;
}

void UUICanvas::SetProgressBarTextureOpacity(const std::string& WidgetName, float Opacity)
{
    if (auto* Widget = dynamic_cast<UProgressBarWidget*>(FindWidget(WidgetName)))
    {
        Widget->SetTextureOpacity(Opacity);
    }
}

void UUICanvas::ClearProgressBarTextures(const std::string& WidgetName)
{
    if (auto* Widget = dynamic_cast<UProgressBarWidget*>(FindWidget(WidgetName)))
    {
        Widget->ClearTextures();
    }
}

// ============================================
// SubUV 설정
// ============================================

void UUICanvas::SetTextureSubUVGrid(const std::string& WidgetName, int32_t NX, int32_t NY)
{
    if (auto* Widget = dynamic_cast<UTextureWidget*>(FindWidget(WidgetName)))
    {
        Widget->SetSubUVGrid(NX, NY);
    }
}

void UUICanvas::SetTextureSubUVFrame(const std::string& WidgetName, int32_t FrameIndex)
{
    if (auto* Widget = dynamic_cast<UTextureWidget*>(FindWidget(WidgetName)))
    {
        Widget->SetSubUVFrame(FrameIndex);
    }
}

void UUICanvas::SetTextureSubUV(const std::string& WidgetName, int32_t FrameIndex, int32_t NX, int32_t NY)
{
    if (auto* Widget = dynamic_cast<UTextureWidget*>(FindWidget(WidgetName)))
    {
        Widget->SetSubUVFrameWithGrid(FrameIndex, NX, NY);
    }
}

void UUICanvas::SetProgressBarForegroundSubUV(const std::string& WidgetName, int32_t FrameIndex, int32_t NX, int32_t NY)
{
    if (auto* Widget = dynamic_cast<UProgressBarWidget*>(FindWidget(WidgetName)))
    {
        Widget->SetForegroundSubUV(FrameIndex, NX, NY);
    }
}

void UUICanvas::SetProgressBarBackgroundSubUV(const std::string& WidgetName, int32_t FrameIndex, int32_t NX, int32_t NY)
{
    if (auto* Widget = dynamic_cast<UProgressBarWidget*>(FindWidget(WidgetName)))
    {
        Widget->SetBackgroundSubUV(FrameIndex, NX, NY);
    }
}

void UUICanvas::SetTextureBlendMode(const std::string& WidgetName, EUIBlendMode Mode)
{
    if (auto* Widget = dynamic_cast<UTextureWidget*>(FindWidget(WidgetName)))
    {
        Widget->SetBlendMode(Mode);
    }
}

void UUICanvas::SetTextureAdditive(const std::string& WidgetName, bool bAdditive)
{
    if (auto* Widget = dynamic_cast<UTextureWidget*>(FindWidget(WidgetName)))
    {
        Widget->SetAdditive(bAdditive);
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

void UUICanvas::ShakeWidget(const std::string& WidgetName, float Intensity, float Duration,
                            float Frequency, bool bDecay)
{
    if (auto* Widget = FindWidget(WidgetName))
    {
        Widget->StartShake(Intensity, Duration, Frequency, bDecay);
    }
}

void UUICanvas::StopWidgetShake(const std::string& WidgetName)
{
    if (auto* Widget = FindWidget(WidgetName))
    {
        Widget->StopShake();
    }
}

void UUICanvas::PlayAllEnterAnimations()
{
    UE_LOG("[UI] PlayAllEnterAnimations called for canvas %s, widget count=%d\n", Name.c_str(), (int)Widgets.size());
    for (auto& Pair : Widgets)
    {
        if (Pair.second)
        {
            Pair.second->PlayEnterAnimation();
        }
    }
}

void UUICanvas::PlayAllExitAnimations()
{
    for (auto& Pair : Widgets)
    {
        if (Pair.second)
        {
            Pair.second->PlayExitAnimation();
        }
    }
}

// ============================================
// 버튼 설정
// ============================================

void UUICanvas::SetButtonInteractable(const std::string& WidgetName, bool bInteractable)
{
    if (auto* Button = dynamic_cast<UButtonWidget*>(FindWidget(WidgetName)))
    {
        Button->SetInteractable(bInteractable);
    }
}

void UUICanvas::SetButtonHoverScale(const std::string& WidgetName, float Scale, float Duration)
{
    if (auto* Button = dynamic_cast<UButtonWidget*>(FindWidget(WidgetName)))
    {
        Button->HoverScale = Scale;
        Button->HoverScaleDuration = Duration;
    }
}

bool UUICanvas::SetButtonHoveredTexture(const std::string& WidgetName, const std::string& Path, ID2D1DeviceContext* Context)
{
    if (!Context)
        return false;

    if (auto* Button = dynamic_cast<UButtonWidget*>(FindWidget(WidgetName)))
    {
        std::wstring WidePath(Path.begin(), Path.end());
        return Button->LoadHoveredTexture(WidePath.c_str(), Context);
    }
    return false;
}

bool UUICanvas::SetButtonPressedTexture(const std::string& WidgetName, const std::string& Path, ID2D1DeviceContext* Context)
{
    if (!Context)
        return false;

    if (auto* Button = dynamic_cast<UButtonWidget*>(FindWidget(WidgetName)))
    {
        std::wstring WidePath(Path.begin(), Path.end());
        return Button->LoadPressedTexture(WidePath.c_str(), Context);
    }
    return false;
}

bool UUICanvas::SetButtonDisabledTexture(const std::string& WidgetName, const std::string& Path, ID2D1DeviceContext* Context)
{
    if (!Context)
        return false;

    if (auto* Button = dynamic_cast<UButtonWidget*>(FindWidget(WidgetName)))
    {
        std::wstring WidePath(Path.begin(), Path.end());
        return Button->LoadDisabledTexture(WidePath.c_str(), Context);
    }
    return false;
}

void UUICanvas::SetButtonNormalTint(const std::string& WidgetName, float R, float G, float B, float A)
{
    if (auto* Button = dynamic_cast<UButtonWidget*>(FindWidget(WidgetName)))
    {
        Button->SetNormalTint(R, G, B, A);
    }
}

void UUICanvas::SetButtonHoveredTint(const std::string& WidgetName, float R, float G, float B, float A)
{
    if (auto* Button = dynamic_cast<UButtonWidget*>(FindWidget(WidgetName)))
    {
        Button->SetHoveredTint(R, G, B, A);
    }
}

void UUICanvas::SetButtonPressedTint(const std::string& WidgetName, float R, float G, float B, float A)
{
    if (auto* Button = dynamic_cast<UButtonWidget*>(FindWidget(WidgetName)))
    {
        Button->SetPressedTint(R, G, B, A);
    }
}

void UUICanvas::SetButtonDisabledTint(const std::string& WidgetName, float R, float G, float B, float A)
{
    if (auto* Button = dynamic_cast<UButtonWidget*>(FindWidget(WidgetName)))
    {
        Button->SetDisabledTint(R, G, B, A);
    }
}

// ============================================
// 마우스 입력 처리
// ============================================

bool UUICanvas::ContainsPoint(float PosX, float PosY) const
{
    // Width/Height가 0이면 전체 화면 취급
    if (Width <= 0.0f || Height <= 0.0f)
        return true;

    return PosX >= X && PosX <= X + Width &&
           PosY >= Y && PosY <= Y + Height;
}

UButtonWidget* UUICanvas::FindButtonAtPosition(float LocalX, float LocalY)
{
    // 정렬이 필요하면 갱신
    if (bWidgetsSortDirty)
    {
        UpdateWidgetSortOrder();
    }

    // Z-order 역순으로 검색 (가장 위에 있는 것부터)
    for (auto it = SortedWidgets.rbegin(); it != SortedWidgets.rend(); ++it)
    {
        UUIWidget* Widget = *it;
        if (!Widget || !Widget->bVisible)
            continue;

        // 버튼 위젯인지 확인
        if (auto* Button = dynamic_cast<UButtonWidget*>(Widget))
        {
            if (Button->IsInteractable() && Button->Contains(LocalX, LocalY))
            {
                return Button;
            }
        }
    }

    return nullptr;
}

bool UUICanvas::ProcessMouseInput(float MouseX, float MouseY, bool bLeftDown, bool bLeftPressed, bool bLeftReleased)
{
    if (!bVisible)
        return false;

    // 마우스 좌표를 캔버스 로컬 좌표로 변환
    // Note: 위젯 좌표는 LoadUIAsset에서 이미 뷰포트 크기에 맞게 스케일링됨
    float LocalX = MouseX - X;
    float LocalY = MouseY - Y;

    // 현재 마우스 위치에서 버튼 찾기
    UButtonWidget* CurrentHovered = FindButtonAtPosition(LocalX, LocalY);

    // 호버 상태 변경 처리
    if (CurrentHovered != HoveredButton)
    {
        // 이전 호버 버튼에서 나감
        if (HoveredButton)
        {
            HoveredButton->OnMouseLeave();
        }

        // 새 호버 버튼에 진입
        if (CurrentHovered)
        {
            CurrentHovered->OnMouseEnter();
        }

        HoveredButton = CurrentHovered;
    }

    // 마우스 버튼 눌림 처리
    if (bLeftPressed && CurrentHovered)
    {
        UE_LOG("[UICanvas] '%s': Mouse pressed on button '%s'\n", Name.c_str(), CurrentHovered->Name.c_str());
        CurrentHovered->OnMouseDown();
        PressedButton = CurrentHovered;
        return true;  // 입력 소비
    }

    // 마우스 버튼 뗌 처리
    if (bLeftReleased && PressedButton)
    {
        // OnMouseUp 콜백에서 캔버스가 삭제될 수 있으므로 미리 복사
        std::string canvasName = Name;
        std::string buttonName = PressedButton->Name;
        UE_LOG("[UICanvas] '%s': Mouse released on button '%s'\n", canvasName.c_str(), buttonName.c_str());
        bool bClicked = PressedButton->OnMouseUp();
        UE_LOG("[UICanvas] '%s': Button click result: %s\n", canvasName.c_str(), bClicked ? "true" : "false");
        PressedButton = nullptr;
        return bClicked;  // 클릭 성공하면 입력 소비
    }

    // 버튼 위에 있으면 입력 소비 (다른 UI가 처리하지 않도록)
    return CurrentHovered != nullptr;
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
            Pair.second->UpdateShake(DeltaTime);
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
    // Note: 위젯 좌표/크기는 LoadUIAsset에서 이미 뷰포트 크기에 맞게 스케일링됨
    D2D1_MATRIX_3X2_F OriginalTransform;
    D2DContext->GetTransform(&OriginalTransform);

    // 변환: 뷰포트 오프셋 + 캔버스 위치만 적용 (스케일은 LoadUIAsset에서 처리됨)
    D2DContext->SetTransform(
        D2D1::Matrix3x2F::Translation(ViewportOffsetX + X, ViewportOffsetY + Y) *
        OriginalTransform
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