#pragma once

#include <d2d1_1.h>
#include <string>

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

    // 렌더링 순서 (높을수록 위에 그려짐)
    int32_t ZOrder = 0;

    // 가시성
    bool bVisible = true;

    // 위젯 타입
    EUIWidgetType Type = EUIWidgetType::None;

    /**
     * @brief 위젯 렌더링 (파생 클래스에서 구현)
     */
    virtual void Render(ID2D1DeviceContext* Context) = 0;

    /**
     * @brief 위젯 업데이트 (애니메이션 등)
     */
    virtual void Update(float DeltaTime) {}

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
};
