#pragma once

#include <d2d1_1.h>
#include <string>
#include <cmath>

/**
 * @brief UI 위젯 타입 열거형
 */
enum class EUIWidgetType : uint8_t
{
    None,
    Rect,
    ProgressBar,
    Texture,
    Text
};

/**
 * @brief 이징 함수 타입
 */
enum class EEasingType : uint8_t
{
    Linear,
    EaseIn,
    EaseOut,
    EaseInOut
};

/**
 * @brief 블렌드 모드
 */
enum class EUIBlendMode : uint8_t
{
    Normal,     // 기본 (Source Over) - 알파 채널 사용
    Additive,   // 가산 블렌딩 - 검은 배경이 투명해짐
};

/**
 * @brief 위젯 Enter/Exit 애니메이션 타입
 */
enum class EWidgetAnimType : int32_t
{
    None,           // 애니메이션 없음
    Fade,           // 페이드 (Opacity)
    SlideLeft,      // 왼쪽에서 슬라이드
    SlideRight,     // 오른쪽에서 슬라이드
    SlideTop,       // 위에서 슬라이드
    SlideBottom,    // 아래에서 슬라이드
    Scale,          // 스케일 (0 -> 1)
    FadeSlideLeft,  // 페이드 + 왼쪽 슬라이드
    FadeSlideRight, // 페이드 + 오른쪽 슬라이드
    FadeSlideTop,   // 페이드 + 위 슬라이드
    FadeSlideBottom,// 페이드 + 아래 슬라이드
    FadeScale,      // 페이드 + 스케일
};

/**
 * @brief Enter/Exit 애니메이션 설정
 */
struct FWidgetAnimConfig
{
    EWidgetAnimType Type = EWidgetAnimType::None;
    float Duration = 0.3f;
    EEasingType Easing = EEasingType::EaseOut;
    float Delay = 0.0f;
    float Offset = 100.0f;  // 슬라이드 거리 (픽셀)
};

/**
 * @brief 애니메이션 속성 타입
 */
enum class EAnimProperty : uint8_t
{
    None     = 0,
    Position = 1 << 0,
    Size     = 1 << 1,
    Rotation = 1 << 2,
    Opacity  = 1 << 3
};

/**
 * @brief 위젯 애니메이션 상태
 */
struct FWidgetAnimation
{
    // 이동 애니메이션
    float StartX = 0.0f, StartY = 0.0f;
    float TargetX = 0.0f, TargetY = 0.0f;
    bool bAnimatingPosition = false;

    // 크기 애니메이션
    float StartWidth = 0.0f, StartHeight = 0.0f;
    float TargetWidth = 0.0f, TargetHeight = 0.0f;
    bool bAnimatingSize = false;

    // 회전 애니메이션
    float StartRotation = 0.0f;
    float TargetRotation = 0.0f;
    bool bAnimatingRotation = false;

    // 투명도 애니메이션
    float StartOpacity = 1.0f;
    float TargetOpacity = 1.0f;
    bool bAnimatingOpacity = false;

    // 공통
    float Duration = 0.0f;
    float Elapsed = 0.0f;
    EEasingType Easing = EEasingType::Linear;

    bool IsAnimating() const
    {
        return bAnimatingPosition || bAnimatingSize ||
               bAnimatingRotation || bAnimatingOpacity;
    }

    void Reset()
    {
        bAnimatingPosition = false;
        bAnimatingSize = false;
        bAnimatingRotation = false;
        bAnimatingOpacity = false;
        Elapsed = 0.0f;
    }
};

/**
 * @brief UI 위젯 베이스 클래스
 *
 * Lua에서 동적으로 생성/관리할 수 있는 UI 위젯의 기본 클래스입니다.
 */
class UUIWidget
{
public:
    UUIWidget() = default;
    virtual ~UUIWidget() = default;

    // 위젯 식별자 (Lua에서 참조용)
    std::string Name;

    // 위치 및 크기
    float X = 0.0f;
    float Y = 0.0f;
    float Width = 100.0f;
    float Height = 30.0f;

    // 회전 (도 단위, 중심점 기준)
    float Rotation = 0.0f;

    // 투명도 (0.0 ~ 1.0)
    float Opacity = 1.0f;

    // 렌더링 순서 (높을수록 위에 그려짐)
    int32_t ZOrder = 0;

    // 가시성
    bool bVisible = true;

    // 위젯 타입
    EUIWidgetType Type = EUIWidgetType::None;

    // 애니메이션 상태
    FWidgetAnimation Animation;

    // Enter/Exit 애니메이션 설정
    FWidgetAnimConfig EnterAnimConfig;
    FWidgetAnimConfig ExitAnimConfig;

    // 애니메이션 기준값 (원본 위치/크기/투명도)
    float OriginalX = 0.0f;
    float OriginalY = 0.0f;
    float OriginalWidth = 100.0f;
    float OriginalHeight = 30.0f;
    float OriginalOpacity = 1.0f;
    bool bOriginalCaptured = false;

    /**
     * @brief 위젯 렌더링 (파생 클래스에서 구현)
     */
    virtual void Render(ID2D1DeviceContext* Context) = 0;

    /**
     * @brief 위젯 업데이트 (애니메이션 처리)
     */
    virtual void Update(float DeltaTime);

    /**
     * @brief 애니메이션 중인지 확인
     */
    bool IsAnimating() const { return Animation.IsAnimating(); }

    // === 애니메이션 시작 함수 ===

    void MoveTo(float TargetX, float TargetY, float Duration, EEasingType Easing = EEasingType::Linear);
    void SizeTo(float TargetW, float TargetH, float Duration, EEasingType Easing = EEasingType::Linear);
    void RotateTo(float TargetAngle, float Duration, EEasingType Easing = EEasingType::Linear);
    void FadeTo(float TargetOpacity, float Duration, EEasingType Easing = EEasingType::Linear);

    /**
     * @brief 모든 애니메이션 즉시 중지
     */
    void StopAnimation();

    /**
     * @brief Enter 애니메이션 재생 (위젯 등장 시)
     */
    void PlayEnterAnimation();

    /**
     * @brief Exit 애니메이션 재생 (위젯 퇴장 시)
     */
    void PlayExitAnimation();

    /**
     * @brief 원본 값 캡처 (애니메이션 기준점)
     */
    void CaptureOriginalValues();

    /**
     * @brief 원본 값으로 복원
     */
    void RestoreOriginalValues();

    // === Lua에서 사용할 공통 setter ===

    void SetPosition(float InX, float InY)
    {
        X = InX;
        Y = InY;
    }

    void SetSize(float W, float H)
    {
        Width = W;
        Height = H;
    }

    void SetVisible(bool bVis)
    {
        bVisible = bVis;
    }

    void SetZOrder(int32_t Z)
    {
        ZOrder = Z;
    }

    // === 유틸리티 ===

    /**
     * @brief 포인트가 위젯 영역 안에 있는지 확인
     */
    bool Contains(float PosX, float PosY) const
    {
        return PosX >= X && PosX <= X + Width &&
               PosY >= Y && PosY <= Y + Height;
    }

    /**
     * @brief 위젯 영역을 D2D1_RECT_F로 반환
     */
    D2D1_RECT_F GetRect() const
    {
        return D2D1::RectF(X, Y, X + Width, Y + Height);
    }

private:
    /**
     * @brief 이징 함수 적용
     */
    static float ApplyEasing(float t, EEasingType Easing);
};
