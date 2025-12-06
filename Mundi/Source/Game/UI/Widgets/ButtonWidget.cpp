#include "pch.h"
#include "ButtonWidget.h"

UButtonWidget::UButtonWidget()
{
    Type = EUIWidgetType::Button;

    // 기본 틴트 색상 설정
    NormalTint = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);      // 흰색 (원본)
    HoveredTint = D2D1::ColorF(1.2f, 1.2f, 1.2f, 1.0f);     // 약간 밝게
    PressedTint = D2D1::ColorF(0.8f, 0.8f, 0.8f, 1.0f);     // 약간 어둡게
    DisabledTint = D2D1::ColorF(0.5f, 0.5f, 0.5f, 0.5f);    // 반투명 회색
}

void UButtonWidget::Render(ID2D1DeviceContext* Context)
{
    if (!Context || !bVisible)
        return;

    // 현재 상태에 맞는 비트맵과 틴트 가져오기
    ID2D1Bitmap* CurrentBitmap = GetCurrentBitmap();
    D2D1_COLOR_F CurrentTint = GetCurrentTint();

    if (!CurrentBitmap)
        return;

    // 틴트 적용
    Tint = CurrentTint;

    // 부모 클래스의 렌더링 사용 (UV, 블렌드 모드 등 처리)
    // 단, 비트맵을 임시로 교체
    ID2D1Bitmap* OriginalBitmap = Bitmap;
    Bitmap = CurrentBitmap;

    UTextureWidget::Render(Context);

    Bitmap = OriginalBitmap;
}

bool UButtonWidget::LoadHoveredTexture(const wchar_t* Path, ID2D1DeviceContext* Context)
{
    if (HoveredBitmap)
    {
        HoveredBitmap->Release();
        HoveredBitmap = nullptr;
    }

    // 임시로 Bitmap 포인터 사용
    ID2D1Bitmap* OriginalBitmap = Bitmap;
    Bitmap = nullptr;

    bool bSuccess = LoadFromFile(Path, Context);
    if (bSuccess)
    {
        HoveredBitmap = Bitmap;
    }

    Bitmap = OriginalBitmap;
    return bSuccess;
}

bool UButtonWidget::LoadPressedTexture(const wchar_t* Path, ID2D1DeviceContext* Context)
{
    if (PressedBitmap)
    {
        PressedBitmap->Release();
        PressedBitmap = nullptr;
    }

    ID2D1Bitmap* OriginalBitmap = Bitmap;
    Bitmap = nullptr;

    bool bSuccess = LoadFromFile(Path, Context);
    if (bSuccess)
    {
        PressedBitmap = Bitmap;
    }

    Bitmap = OriginalBitmap;
    return bSuccess;
}

bool UButtonWidget::LoadDisabledTexture(const wchar_t* Path, ID2D1DeviceContext* Context)
{
    if (DisabledBitmap)
    {
        DisabledBitmap->Release();
        DisabledBitmap = nullptr;
    }

    ID2D1Bitmap* OriginalBitmap = Bitmap;
    Bitmap = nullptr;

    bool bSuccess = LoadFromFile(Path, Context);
    if (bSuccess)
    {
        DisabledBitmap = Bitmap;
    }

    Bitmap = OriginalBitmap;
    return bSuccess;
}

void UButtonWidget::SetNormalTint(float R, float G, float B, float A)
{
    NormalTint = D2D1::ColorF(R, G, B, A);
}

void UButtonWidget::SetHoveredTint(float R, float G, float B, float A)
{
    HoveredTint = D2D1::ColorF(R, G, B, A);
}

void UButtonWidget::SetPressedTint(float R, float G, float B, float A)
{
    PressedTint = D2D1::ColorF(R, G, B, A);
}

void UButtonWidget::SetDisabledTint(float R, float G, float B, float A)
{
    DisabledTint = D2D1::ColorF(R, G, B, A);
}

void UButtonWidget::SetState(EButtonState NewState)
{
    State = NewState;
}

void UButtonWidget::SetInteractable(bool bEnabled)
{
    bInteractable = bEnabled;
    if (!bEnabled)
    {
        State = EButtonState::Disabled;
    }
    else if (State == EButtonState::Disabled)
    {
        State = EButtonState::Normal;
    }
}

void UButtonWidget::OnMouseEnter()
{
    if (!bInteractable || State == EButtonState::Disabled)
        return;

    bMouseOver = true;

    if (!bMouseDown)
    {
        State = EButtonState::Hovered;
    }

    // 콜백 호출
    if (OnHoverStart)
    {
        OnHoverStart();
    }
}

void UButtonWidget::OnMouseLeave()
{
    if (!bInteractable || State == EButtonState::Disabled)
        return;

    bMouseOver = false;
    bMouseDown = false;  // 버튼 밖으로 나가면 눌림 취소

    State = EButtonState::Normal;

    // 콜백 호출
    if (OnHoverEnd)
    {
        OnHoverEnd();
    }
}

void UButtonWidget::OnMouseDown()
{
    if (!bInteractable || State == EButtonState::Disabled)
        return;

    bMouseDown = true;
    State = EButtonState::Pressed;

    if (OnPressStart)
    {
        OnPressStart();
    }
}

bool UButtonWidget::OnMouseUp()
{
    if (!bInteractable || State == EButtonState::Disabled)
        return false;

    bool bWasPressed = bMouseDown;
    bMouseDown = false;

    if (bMouseOver)
    {
        State = EButtonState::Hovered;

        // 버튼 위에서 뗐으면 클릭 완료
        if (bWasPressed)
        {
            // OnClick 콜백 호출 전에 this 포인터 백업
            // (콜백에서 버튼이 삭제될 수 있음)
            auto OnClickCopy = OnClick;
            auto OnPressEndCopy = OnPressEnd;

            if (OnClickCopy)
            {
                try
                {
                    OnClickCopy();
                }
                catch (...)
                {
                    UE_LOG("[Button] Exception in OnClick callback!\n");
                }
            }

            if (OnPressEndCopy)
            {
                try
                {
                    OnPressEndCopy();
                }
                catch (...)
                {
                    UE_LOG("[Button] Exception in OnPressEnd callback!\n");
                }
            }

            return true;  // 클릭 성공
        }
    }
    else
    {
        State = EButtonState::Normal;
    }

    return false;
}

void UButtonWidget::ReleaseAllTextures()
{
    if (HoveredBitmap)
    {
        HoveredBitmap->Release();
        HoveredBitmap = nullptr;
    }
    if (PressedBitmap)
    {
        PressedBitmap->Release();
        PressedBitmap = nullptr;
    }
    if (DisabledBitmap)
    {
        DisabledBitmap->Release();
        DisabledBitmap = nullptr;
    }
}

ID2D1Bitmap* UButtonWidget::GetCurrentBitmap() const
{
    switch (State)
    {
    case EButtonState::Hovered:
        return HoveredBitmap ? HoveredBitmap : Bitmap;
    case EButtonState::Pressed:
        return PressedBitmap ? PressedBitmap : Bitmap;
    case EButtonState::Disabled:
        return DisabledBitmap ? DisabledBitmap : Bitmap;
    case EButtonState::Normal:
    default:
        return Bitmap;
    }
}

D2D1_COLOR_F UButtonWidget::GetCurrentTint() const
{
    switch (State)
    {
    case EButtonState::Hovered:
        return HoveredTint;
    case EButtonState::Pressed:
        return PressedTint;
    case EButtonState::Disabled:
        return DisabledTint;
    case EButtonState::Normal:
    default:
        return NormalTint;
    }
}
