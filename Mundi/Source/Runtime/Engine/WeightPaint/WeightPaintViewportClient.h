#pragma once

#include "Source/Runtime/Renderer/FViewportClient.h"
#include "Vector.h"

class FWeightPaintState;
class URenderer;
class FViewport;

/**
 * @brief Weight Paint 에디터용 뷰포트 클라이언트
 *
 * Cloth Weight Paint 기능을 위한 입력 처리와 렌더링을 담당합니다.
 */
class FWeightPaintViewportClient : public FViewportClient
{
public:
    FWeightPaintViewportClient();
    virtual ~FWeightPaintViewportClient() override;

    // ============================================
    // FViewportClient 오버라이드
    // ============================================

    virtual void Draw(FViewport* Viewport) override;
    virtual void Tick(float DeltaTime) override;
    virtual void MouseMove(FViewport* Viewport, int32 X, int32 Y) override;
    virtual void MouseButtonDown(FViewport* Viewport, int32 X, int32 Y, int32 Button) override;
    virtual void MouseButtonUp(FViewport* Viewport, int32 X, int32 Y, int32 Button) override;
    virtual void MouseWheel(float DeltaSeconds) override;

    // ============================================
    // Weight Paint 전용
    // ============================================

    /**
     * @brief 페인트 상태 설정
     */
    void SetPaintState(FWeightPaintState* InState) { PaintState = InState; }
    FWeightPaintState* GetPaintState() const { return PaintState; }

    /**
     * @brief 레이 생성 (스크린 좌표 -> 월드 좌표)
     */
    bool MakeRayFromViewport(FViewport* Viewport, int32 ScreenX, int32 ScreenY,
                              FVector& OutRayOrigin, FVector& OutRayDirection);

    /**
     * @brief 가중치 시각화 렌더링
     */
    void DrawWeightVisualization(URenderer* Renderer);

    /**
     * @brief 브러시 미리보기 렌더링
     */
    void DrawBrushPreview(URenderer* Renderer);

private:
    // Weight Paint 상태 참조
    FWeightPaintState* PaintState = nullptr;

    // 페인팅 중인지 여부
    bool bIsPainting = false;

    // 캐시된 레이 정보
    FVector CachedRayOrigin;
    FVector CachedRayDirection;
    bool bRayValid = false;

    // 마지막 유효한 브러시 위치 (끊김 방지용)
    FVector LastValidBrushPosition;
    bool bLastBrushValid = false;

    /**
     * @brief 현재 마우스 위치에서 버텍스 피킹 수행
     */
    void PerformVertexPicking(FViewport* Viewport, int32 X, int32 Y);

    /**
     * @brief 브러시 적용
     */
    void ApplyBrushAtPosition(const FVector& WorldPosition);
};
