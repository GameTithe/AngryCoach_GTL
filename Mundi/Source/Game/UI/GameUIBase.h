#pragma once

#include <d2d1_1.h>
#include <dwrite.h>

/**
 * @brief 게임 UI 요소의 베이스 클래스
 *
 * 모든 게임 UI 요소가 상속받는 기본 클래스입니다.
 * GameUIManager와 함께 동작합니다.
 */
class UGameUIBase
{
public:
    UGameUIBase() = default;
    virtual ~UGameUIBase() = default;

    /**
     * @brief UI 초기화
     * @param InContext D2D Device Context
     * @param InDWriteFactory DWrite Factory (텍스트용)
     */
    virtual void Initialize(ID2D1DeviceContext* InContext, IDWriteFactory* InDWriteFactory);

    /**
     * @brief UI 정리
     */
    virtual void Shutdown();

    /**
     * @brief 매 프레임 업데이트
     * @param DeltaTime 프레임 간 시간
     */
    virtual void Update(float DeltaTime);

    /**
     * @brief UI 렌더링
     */
    virtual void Render();

    // 가시성
    void SetVisible(bool bInVisible) { bIsVisible = bInVisible; }
    bool IsVisible() const { return bIsVisible; }

    // 위치와 크기
    void SetPosition(float X, float Y) { PosX = X; PosY = Y; }
    void SetSize(float Width, float Height) { SizeWidth = Width; SizeHeight = Height; }
    float GetPosX() const { return PosX; }
    float GetPosY() const { return PosY; }
    float GetWidth() const { return SizeWidth; }
    float GetHeight() const { return SizeHeight; }

    // 마우스 입력
    virtual void OnMouseMove(float X, float Y) {}
    virtual void OnMouseDown(float X, float Y) {}
    virtual void OnMouseUp(float X, float Y) {}

    // 히트 테스트
    bool IsPointInside(float X, float Y) const
    {
        return X >= PosX && X <= PosX + SizeWidth &&
               Y >= PosY && Y <= PosY + SizeHeight;
    }

protected:
    // 렌더링 헬퍼 함수들
    void DrawRect(float X, float Y, float W, float H, ID2D1Brush* Brush);
    void DrawRectOutline(float X, float Y, float W, float H, ID2D1Brush* Brush, float StrokeWidth = 1.0f);
    void DrawText(const wchar_t* Text, float X, float Y, float W, float H, IDWriteTextFormat* Format, ID2D1Brush* Brush);
    void DrawProgressBar(float X, float Y, float W, float H, float Progress, ID2D1Brush* FgBrush, ID2D1Brush* BgBrush);

protected:
    ID2D1DeviceContext* D2DContext = nullptr;
    IDWriteFactory* DWriteFactory = nullptr;

    bool bIsVisible = true;
    float PosX = 0.0f;
    float PosY = 0.0f;
    float SizeWidth = 100.0f;
    float SizeHeight = 100.0f;
};
