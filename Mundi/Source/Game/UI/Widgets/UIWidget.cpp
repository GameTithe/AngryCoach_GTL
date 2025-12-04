#include "pch.h"
#include "UIWidget.h"
#include <algorithm>

void UUIWidget::Update(float DeltaTime)
{
    if (!Animation.IsAnimating())
        return;

    Animation.Elapsed += DeltaTime;
    float t = (Animation.Duration > 0.0f)
        ? std::min(Animation.Elapsed / Animation.Duration, 1.0f)
        : 1.0f;

    // 이징 적용
    float easedT = ApplyEasing(t, Animation.Easing);

    // 위치 애니메이션
    if (Animation.bAnimatingPosition)
    {
        X = Animation.StartX + (Animation.TargetX - Animation.StartX) * easedT;
        Y = Animation.StartY + (Animation.TargetY - Animation.StartY) * easedT;
    }

    // 크기 애니메이션
    if (Animation.bAnimatingSize)
    {
        Width = Animation.StartWidth + (Animation.TargetWidth - Animation.StartWidth) * easedT;
        Height = Animation.StartHeight + (Animation.TargetHeight - Animation.StartHeight) * easedT;
    }

    // 회전 애니메이션
    if (Animation.bAnimatingRotation)
    {
        Rotation = Animation.StartRotation + (Animation.TargetRotation - Animation.StartRotation) * easedT;
    }

    // 투명도 애니메이션
    if (Animation.bAnimatingOpacity)
    {
        Opacity = Animation.StartOpacity + (Animation.TargetOpacity - Animation.StartOpacity) * easedT;
    }

    // 애니메이션 완료 체크
    if (t >= 1.0f)
    {
        Animation.Reset();
    }
}

void UUIWidget::MoveTo(float TargetX, float TargetY, float Duration, EEasingType Easing)
{
    Animation.StartX = X;
    Animation.StartY = Y;
    Animation.TargetX = TargetX;
    Animation.TargetY = TargetY;
    Animation.Duration = Duration;
    Animation.Elapsed = 0.0f;
    Animation.Easing = Easing;
    Animation.bAnimatingPosition = true;
}

void UUIWidget::SizeTo(float TargetW, float TargetH, float Duration, EEasingType Easing)
{
    Animation.StartWidth = Width;
    Animation.StartHeight = Height;
    Animation.TargetWidth = TargetW;
    Animation.TargetHeight = TargetH;
    Animation.Duration = Duration;
    Animation.Elapsed = 0.0f;
    Animation.Easing = Easing;
    Animation.bAnimatingSize = true;
}

void UUIWidget::RotateTo(float TargetAngle, float Duration, EEasingType Easing)
{
    Animation.StartRotation = Rotation;
    Animation.TargetRotation = TargetAngle;
    Animation.Duration = Duration;
    Animation.Elapsed = 0.0f;
    Animation.Easing = Easing;
    Animation.bAnimatingRotation = true;
}

void UUIWidget::FadeTo(float TargetOpacity, float Duration, EEasingType Easing)
{
    Animation.StartOpacity = Opacity;
    Animation.TargetOpacity = std::clamp(TargetOpacity, 0.0f, 1.0f);
    Animation.Duration = Duration;
    Animation.Elapsed = 0.0f;
    Animation.Easing = Easing;
    Animation.bAnimatingOpacity = true;
}

void UUIWidget::StopAnimation()
{
    Animation.Reset();
}

float UUIWidget::ApplyEasing(float t, EEasingType Easing)
{
    switch (Easing)
    {
    case EEasingType::Linear:
        return t;

    case EEasingType::EaseIn:
        // Quadratic ease in: t^2
        return t * t;

    case EEasingType::EaseOut:
        // Quadratic ease out: 1 - (1-t)^2
        return 1.0f - (1.0f - t) * (1.0f - t);

    case EEasingType::EaseInOut:
        // Quadratic ease in-out
        if (t < 0.5f)
            return 2.0f * t * t;
        else
            return 1.0f - 2.0f * (1.0f - t) * (1.0f - t);

    default:
        return t;
    }
}
