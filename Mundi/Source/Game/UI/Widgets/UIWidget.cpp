#include "pch.h"
#include "UIWidget.h"
#include <algorithm>
#include <random>

// PI 상수
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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

    // 디버그: 애니메이션 진행 상황 (첫 몇 프레임만)
    static int debugFrameCount = 0;
    if (debugFrameCount < 10)
    {
        UE_LOG("[UI] Update %s: t=%.3f, easedT=%.3f, elapsed=%.3f, duration=%.3f\n",
            Name.c_str(), t, easedT, Animation.Elapsed, Animation.Duration);
        debugFrameCount++;
    }

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
        float oldOpacity = Opacity;
        Opacity = Animation.StartOpacity + (Animation.TargetOpacity - Animation.StartOpacity) * easedT;
        if (debugFrameCount <= 10)
        {
            UE_LOG("[UI] Opacity update %s: %.3f -> %.3f (start=%.2f, target=%.2f)\n",
                Name.c_str(), oldOpacity, Opacity, Animation.StartOpacity, Animation.TargetOpacity);
        }
    }

    // 애니메이션 완료 체크
    if (t >= 1.0f)
    {
        Animation.Reset();
    }
}

// Shake 애니메이션 업데이트 (Update와 별도로 호출)
void UUIWidget::UpdateShake(float DeltaTime)
{
    if (!Animation.bShaking)
        return;

    Animation.ShakeElapsed += DeltaTime;

    // 지속 시간 체크 (0이면 무한)
    if (Animation.ShakeDuration > 0.0f && Animation.ShakeElapsed >= Animation.ShakeDuration)
    {
        // 진동 종료, 원래 위치로 복원
        X = Animation.ShakeBaseX;
        Y = Animation.ShakeBaseY;
        Animation.ResetShake();
        return;
    }

    // 감쇠 계산 (시간이 지나면서 약해짐)
    float decay = 1.0f;
    if (Animation.bShakeDecay && Animation.ShakeDuration > 0.0f)
    {
        // 시간이 지남에 따라 1.0 -> 0.0
        decay = 1.0f - (Animation.ShakeElapsed / Animation.ShakeDuration);
        decay = decay * decay;  // Quadratic decay for smoother falloff
    }

    float intensity = Animation.ShakeIntensity * decay;
    float time = Animation.ShakeElapsed;
    float freq = Animation.ShakeFrequency;

    // 불규칙한 진동을 위해 여러 사인파 합성
    // 기본 주파수 + 배음 + 서로 다른 위상
    float offsetX = 0.0f;
    float offsetY = 0.0f;

    // X축: 여러 주파수 합성
    offsetX += sinf(time * freq * 2.0f * (float)M_PI + Animation.ShakePhaseX) * 0.5f;
    offsetX += sinf(time * freq * 1.3f * 2.0f * (float)M_PI + Animation.ShakePhaseX * 2.1f) * 0.3f;
    offsetX += sinf(time * freq * 2.7f * 2.0f * (float)M_PI + Animation.ShakePhaseX * 0.7f) * 0.2f;

    // Y축: 다른 주파수/위상
    offsetY += sinf(time * freq * 1.1f * 2.0f * (float)M_PI + Animation.ShakePhaseY) * 0.5f;
    offsetY += sinf(time * freq * 1.7f * 2.0f * (float)M_PI + Animation.ShakePhaseY * 1.8f) * 0.3f;
    offsetY += sinf(time * freq * 2.3f * 2.0f * (float)M_PI + Animation.ShakePhaseY * 0.5f) * 0.2f;

    // 강도 적용
    offsetX *= intensity;
    offsetY *= intensity;

    // 위치 업데이트
    X = Animation.ShakeBaseX + offsetX;
    Y = Animation.ShakeBaseY + offsetY;
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
    UE_LOG("[UI] FadeTo called for %s: start=%.2f, target=%.2f, duration=%.2f\n",
        Name.c_str(), Animation.StartOpacity, Animation.TargetOpacity, Duration);
}

void UUIWidget::StopAnimation()
{
    Animation.Reset();
}

void UUIWidget::StartShake(float Intensity, float Duration, float Frequency, bool bDecay)
{
    // 현재 위치를 기준점으로 저장
    Animation.ShakeBaseX = X;
    Animation.ShakeBaseY = Y;

    Animation.ShakeIntensity = Intensity;
    Animation.ShakeDuration = Duration;
    Animation.ShakeFrequency = Frequency;
    Animation.bShakeDecay = bDecay;
    Animation.ShakeElapsed = 0.0f;

    // 불규칙성을 위한 랜덤 위상 생성
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<float> dist(0.0f, 2.0f * (float)M_PI);

    Animation.ShakePhaseX = dist(gen);
    Animation.ShakePhaseY = dist(gen);

    Animation.bShaking = true;
}

void UUIWidget::StopShake()
{
    if (Animation.bShaking)
    {
        // 원래 위치로 복원
        X = Animation.ShakeBaseX;
        Y = Animation.ShakeBaseY;
        Animation.ResetShake();
    }
}

void UUIWidget::CaptureOriginalValues()
{
    OriginalX = X;
    OriginalY = Y;
    OriginalWidth = Width;
    OriginalHeight = Height;
    OriginalOpacity = Opacity;
    bOriginalCaptured = true;
}

void UUIWidget::RestoreOriginalValues()
{
    if (bOriginalCaptured)
    {
        X = OriginalX;
        Y = OriginalY;
        Width = OriginalWidth;
        Height = OriginalHeight;
        Opacity = OriginalOpacity;
    }
}

void UUIWidget::PlayEnterAnimation()
{
    UE_LOG("[UI] PlayEnterAnimation called for %s, type=%d, duration=%.2f, originalOpacity=%.2f\n",
        Name.c_str(), (int)EnterAnimConfig.Type, EnterAnimConfig.Duration, OriginalOpacity);

    if (EnterAnimConfig.Type == EWidgetAnimType::None)
    {
        UE_LOG("[UI] EnterAnimConfig.Type is None, returning\n");
        return;
    }

    // 원본 값 캡처 (처음 한 번만)
    if (!bOriginalCaptured)
    {
        CaptureOriginalValues();
        UE_LOG("[UI] CaptureOriginalValues called, OriginalOpacity=%.2f\n", OriginalOpacity);
    }

    // 시작 상태 설정 (애니메이션 타입에 따라)
    switch (EnterAnimConfig.Type)
    {
    case EWidgetAnimType::Fade:
        UE_LOG("[UI] Starting Fade animation: setting Opacity to 0, will fade to %.2f\n", OriginalOpacity);
        Opacity = 0.0f;
        FadeTo(OriginalOpacity, EnterAnimConfig.Duration, EnterAnimConfig.Easing);
        break;

    case EWidgetAnimType::SlideLeft:
        X = OriginalX - EnterAnimConfig.Offset;
        MoveTo(OriginalX, OriginalY, EnterAnimConfig.Duration, EnterAnimConfig.Easing);
        break;

    case EWidgetAnimType::SlideRight:
        X = OriginalX + EnterAnimConfig.Offset;
        MoveTo(OriginalX, OriginalY, EnterAnimConfig.Duration, EnterAnimConfig.Easing);
        break;

    case EWidgetAnimType::SlideTop:
        Y = OriginalY - EnterAnimConfig.Offset;
        MoveTo(OriginalX, OriginalY, EnterAnimConfig.Duration, EnterAnimConfig.Easing);
        break;

    case EWidgetAnimType::SlideBottom:
        Y = OriginalY + EnterAnimConfig.Offset;
        MoveTo(OriginalX, OriginalY, EnterAnimConfig.Duration, EnterAnimConfig.Easing);
        break;

    case EWidgetAnimType::Scale:
        Width = 0.0f;
        Height = 0.0f;
        SizeTo(OriginalWidth, OriginalHeight, EnterAnimConfig.Duration, EnterAnimConfig.Easing);
        break;

    case EWidgetAnimType::FadeSlideLeft:
        Opacity = 0.0f;
        X = OriginalX - EnterAnimConfig.Offset;
        FadeTo(OriginalOpacity, EnterAnimConfig.Duration, EnterAnimConfig.Easing);
        MoveTo(OriginalX, OriginalY, EnterAnimConfig.Duration, EnterAnimConfig.Easing);
        break;

    case EWidgetAnimType::FadeSlideRight:
        Opacity = 0.0f;
        X = OriginalX + EnterAnimConfig.Offset;
        FadeTo(OriginalOpacity, EnterAnimConfig.Duration, EnterAnimConfig.Easing);
        MoveTo(OriginalX, OriginalY, EnterAnimConfig.Duration, EnterAnimConfig.Easing);
        break;

    case EWidgetAnimType::FadeSlideTop:
        Opacity = 0.0f;
        Y = OriginalY - EnterAnimConfig.Offset;
        FadeTo(OriginalOpacity, EnterAnimConfig.Duration, EnterAnimConfig.Easing);
        MoveTo(OriginalX, OriginalY, EnterAnimConfig.Duration, EnterAnimConfig.Easing);
        break;

    case EWidgetAnimType::FadeSlideBottom:
        Opacity = 0.0f;
        Y = OriginalY + EnterAnimConfig.Offset;
        FadeTo(OriginalOpacity, EnterAnimConfig.Duration, EnterAnimConfig.Easing);
        MoveTo(OriginalX, OriginalY, EnterAnimConfig.Duration, EnterAnimConfig.Easing);
        break;

    case EWidgetAnimType::FadeScale:
        Opacity = 0.0f;
        Width = 0.0f;
        Height = 0.0f;
        FadeTo(OriginalOpacity, EnterAnimConfig.Duration, EnterAnimConfig.Easing);
        SizeTo(OriginalWidth, OriginalHeight, EnterAnimConfig.Duration, EnterAnimConfig.Easing);
        break;

    default:
        break;
    }
}

void UUIWidget::PlayExitAnimation()
{
    if (ExitAnimConfig.Type == EWidgetAnimType::None)
        return;

    // 원본 값 캡처 (처음 한 번만)
    if (!bOriginalCaptured)
        CaptureOriginalValues();

    // 현재 상태에서 Exit 타겟으로 애니메이션
    switch (ExitAnimConfig.Type)
    {
    case EWidgetAnimType::Fade:
        FadeTo(0.0f, ExitAnimConfig.Duration, ExitAnimConfig.Easing);
        break;

    case EWidgetAnimType::SlideLeft:
        MoveTo(OriginalX - ExitAnimConfig.Offset, OriginalY, ExitAnimConfig.Duration, ExitAnimConfig.Easing);
        break;

    case EWidgetAnimType::SlideRight:
        MoveTo(OriginalX + ExitAnimConfig.Offset, OriginalY, ExitAnimConfig.Duration, ExitAnimConfig.Easing);
        break;

    case EWidgetAnimType::SlideTop:
        MoveTo(OriginalX, OriginalY - ExitAnimConfig.Offset, ExitAnimConfig.Duration, ExitAnimConfig.Easing);
        break;

    case EWidgetAnimType::SlideBottom:
        MoveTo(OriginalX, OriginalY + ExitAnimConfig.Offset, ExitAnimConfig.Duration, ExitAnimConfig.Easing);
        break;

    case EWidgetAnimType::Scale:
        SizeTo(0.0f, 0.0f, ExitAnimConfig.Duration, ExitAnimConfig.Easing);
        break;

    case EWidgetAnimType::FadeSlideLeft:
        FadeTo(0.0f, ExitAnimConfig.Duration, ExitAnimConfig.Easing);
        MoveTo(OriginalX - ExitAnimConfig.Offset, OriginalY, ExitAnimConfig.Duration, ExitAnimConfig.Easing);
        break;

    case EWidgetAnimType::FadeSlideRight:
        FadeTo(0.0f, ExitAnimConfig.Duration, ExitAnimConfig.Easing);
        MoveTo(OriginalX + ExitAnimConfig.Offset, OriginalY, ExitAnimConfig.Duration, ExitAnimConfig.Easing);
        break;

    case EWidgetAnimType::FadeSlideTop:
        FadeTo(0.0f, ExitAnimConfig.Duration, ExitAnimConfig.Easing);
        MoveTo(OriginalX, OriginalY - ExitAnimConfig.Offset, ExitAnimConfig.Duration, ExitAnimConfig.Easing);
        break;

    case EWidgetAnimType::FadeSlideBottom:
        FadeTo(0.0f, ExitAnimConfig.Duration, ExitAnimConfig.Easing);
        MoveTo(OriginalX, OriginalY + ExitAnimConfig.Offset, ExitAnimConfig.Duration, ExitAnimConfig.Easing);
        break;

    case EWidgetAnimType::FadeScale:
        FadeTo(0.0f, ExitAnimConfig.Duration, ExitAnimConfig.Easing);
        SizeTo(0.0f, 0.0f, ExitAnimConfig.Duration, ExitAnimConfig.Easing);
        break;

    default:
        break;
    }
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
