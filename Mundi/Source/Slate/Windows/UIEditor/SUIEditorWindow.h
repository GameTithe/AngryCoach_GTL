#pragma once

#include "../SWindow.h"
#include <vector>
#include <memory>
#include <string>
#include <filesystem>

// 전방 선언
struct ID3D11Device;
struct ID3D11ShaderResourceView;

/**
 * @brief UI 에디터에서 편집할 위젯 데이터
 */
struct FUIEditorWidget
{
    std::string Name;
    std::string Type;       // "ProgressBar", "Texture", "Rect", "Text"

    // Transform
    float X = 0.0f;
    float Y = 0.0f;
    float Width = 100.0f;
    float Height = 30.0f;
    int32_t ZOrder = 0;

    // ProgressBar
    float Progress = 1.0f;
    float ForegroundColor[4] = { 0.2f, 0.8f, 0.2f, 1.0f };
    float BackgroundColor[4] = { 0.2f, 0.2f, 0.2f, 0.8f };
    float BorderColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    float BorderWidth = 2.0f;
    bool bRightToLeft = false;
    float LowThreshold = 0.3f;
    float LowColor[4] = { 0.9f, 0.2f, 0.2f, 1.0f };

    // Texture
    std::string TexturePath;
    int32_t SubUV_NX = 1;
    int32_t SubUV_NY = 1;
    int32_t SubUV_Frame = 0;
    bool bAdditive = false;

    // Binding
    std::string BindingKey;  // e.g., "Player.Health"

    // 에디터 상태
    bool bSelected = false;
};

/**
 * @brief UI 에셋 데이터 (Canvas 단위)
 */
struct FUIAsset
{
    std::string Name;
    std::vector<FUIEditorWidget> Widgets;

    // 직렬화
    bool SaveToFile(const std::string& Path) const;
    bool LoadFromFile(const std::string& Path);
};

/**
 * @brief UI 에디터 윈도우
 *
 * 2D UI 레이아웃을 시각적으로 편집하고 에셋으로 저장합니다.
 * ImGui DrawList를 사용하여 캔버스를 렌더링합니다.
 */
class SUIEditorWindow : public SWindow
{
public:
    SUIEditorWindow();
    ~SUIEditorWindow() override;

    bool Initialize(float StartX, float StartY, float Width, float Height, ID3D11Device* InDevice);

    // SWindow overrides
    virtual void OnRender() override;
    virtual void OnUpdate(float DeltaSeconds) override;
    virtual void OnMouseMove(FVector2D MousePos) override;
    virtual void OnMouseDown(FVector2D MousePos, uint32 Button) override;
    virtual void OnMouseUp(FVector2D MousePos, uint32 Button) override;

    // 에셋 관리
    void NewAsset();
    void LoadAsset();
    void LoadAsset(const std::string& Path);
    void SaveAsset();
    void SaveAssetAs();

    // Accessors
    bool IsOpen() const { return bIsOpen; }
    void Close() { bIsOpen = false; }

private:
    // 렌더링 함수
    void RenderMenuBar();
    void RenderToolbar();
    void RenderWidgetPalette(float Width, float Height);
    void RenderCanvas(float Width, float Height);
    void RenderPropertyPanel(float Width, float Height);
    void RenderHierarchy(float Width, float Height);

    // 캔버스 렌더링 헬퍼
    void DrawWidget(ImDrawList* DrawList, const FUIEditorWidget& Widget, ImVec2 CanvasOrigin, float Zoom);
    void DrawSelectionHandles(ImDrawList* DrawList, const FUIEditorWidget& Widget, ImVec2 CanvasOrigin, float Zoom);
    void DrawGrid(ImDrawList* DrawList, ImVec2 CanvasOrigin, ImVec2 CanvasSize, float Zoom);

    // 위젯 조작
    void SelectWidget(int32_t Index);
    void DeselectAll();
    void DeleteSelectedWidget();
    void DuplicateSelectedWidget();
    FUIEditorWidget* GetSelectedWidget();

    // 히트 테스트
    int32_t HitTest(ImVec2 CanvasPos);
    int32_t HitTestHandle(ImVec2 CanvasPos);  // 0-7: resize handles, -1: none

    // 위젯 생성
    void CreateWidget(const std::string& Type);

private:
    ID3D11Device* Device = nullptr;

    // 현재 편집 중인 에셋
    FUIAsset CurrentAsset;
    std::string CurrentAssetPath;
    bool bModified = false;

    // 선택된 위젯 인덱스
    int32_t SelectedWidgetIndex = -1;

    // 캔버스 상태
    float CanvasZoom = 1.0f;
    ImVec2 CanvasPan = ImVec2(0, 0);
    ImVec2 CanvasSize = ImVec2(1920, 1080);  // 기본 캔버스 크기

    // 드래그 상태
    bool bDraggingWidget = false;
    bool bDraggingHandle = false;
    bool bDraggingCanvas = false;
    int32_t DragHandleIndex = -1;
    ImVec2 DragStartPos;
    ImVec2 DragWidgetStartPos;
    ImVec2 DragWidgetStartSize;

    // 팔레트에서 드래그 중
    bool bDraggingFromPalette = false;
    std::string DragWidgetType;

    // 그리드 설정
    bool bShowGrid = true;
    float GridSize = 10.0f;
    bool bSnapToGrid = true;

    // 레이아웃 비율
    float LeftPanelWidth = 150.0f;
    float RightPanelWidth = 250.0f;
    float BottomPanelHeight = 150.0f;

    // 윈도우 상태
    bool bIsOpen = true;
    bool bInitialPlacementDone = false;

    // 에셋 경로
    std::filesystem::path UIAssetPath;
};
