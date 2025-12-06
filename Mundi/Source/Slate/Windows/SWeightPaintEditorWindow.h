#pragma once

#include "SWindow.h"
#include "Source/Runtime/Engine/WeightPaint/WeightPaintState.h"

class FViewport;
class FWeightPaintViewportClient;
class UWorld;
class USkeletalMesh;
class URenderer;
struct ID3D11Device;

/**
 * @brief Weight Paint 에디터 창
 *
 * Cloth Weight Paint 기능을 위한 독립적인 에디터 창입니다.
 * 스켈레탈 메쉬를 로드하고 Cloth 버텍스의 가중치를 페인팅할 수 있습니다.
 */
class SWeightPaintEditorWindow : public SWindow
{
public:
    SWeightPaintEditorWindow();
    virtual ~SWeightPaintEditorWindow();

    /**
     * @brief 초기화
     */
    bool Initialize(float StartX, float StartY, float Width, float Height, UWorld* InWorld, ID3D11Device* InDevice);

    // ============================================
    // SWindow 오버라이드
    // ============================================

    virtual void OnRender() override;
    virtual void OnUpdate(float DeltaSeconds) override;
    virtual void OnMouseMove(FVector2D MousePos) override;
    virtual void OnMouseDown(FVector2D MousePos, uint32 Button) override;
    virtual void OnMouseUp(FVector2D MousePos, uint32 Button) override;

    /**
     * @brief 뷰포트 렌더링 (ImGui 이전에 호출)
     */
    void OnRenderViewport();

    // ============================================
    // 접근자
    // ============================================

    FViewport* GetViewport() const;
    FWeightPaintViewportClient* GetViewportClient() const;
    FWeightPaintState* GetPaintState() { return &PaintState; }

    bool IsOpen() const { return bIsOpen; }
    void Close() { bIsOpen = false; }
    const FRect& GetViewportRect() const { return ViewportRect; }

    // ============================================
    // 메쉬 로드
    // ============================================

    void LoadSkeletalMesh(const FString& Path);
    void UnloadMesh();

private:
    // ============================================
    // UI 렌더링
    // ============================================

    void RenderViewportPanel(float Width, float Height);
    void RenderToolbar();
    void RenderBrushPanel();
    void RenderWeightInfoPanel();
    void RenderMeshBrowserPanel();

    // ============================================
    // 유틸리티
    // ============================================

    void ApplyWeightsToCloth();
    void ResetWeights(float DefaultValue);
    void SaveWeightsToAsset();
    void LoadWeightsFromAsset();

private:
    // Weight Paint 상태
    FWeightPaintState PaintState;

    // Preview World (별도의 독립 월드)
    UWorld* PreviewWorld = nullptr;

    // 디바이스
    ID3D11Device* Device = nullptr;

    // 뷰포트 영역 (OnRenderViewport에서 사용)
    FRect ViewportRect;

    // 레이아웃 비율
    float LeftPanelWidth = 250.0f;   // 브러시/정보 패널 너비

    // 창 상태
    bool bIsOpen = true;
    bool bInitialPlacementDone = false;
    bool bRequestFocus = false;

    // 메쉬 경로 버퍼
    char MeshPathBuffer[260] = {0};
};
