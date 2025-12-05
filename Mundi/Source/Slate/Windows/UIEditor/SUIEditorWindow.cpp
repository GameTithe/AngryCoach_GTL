#include "pch.h"
#include "SUIEditorWindow.h"
#include "imgui.h"
#include "PlatformProcess.h"
#include "Source/Runtime/Core/Misc/JsonSerializer.h"
#include "Source/Game/UI/GameUIManager.h"
#include "Source/Runtime/AssetManagement/ResourceManager.h"
#include "Source/Runtime/AssetManagement/Texture.h"

// ============================================
// FUIAsset 직렬화
// ============================================

bool FUIAsset::SaveToFile(const std::string& Path) const
{
    JSON doc = JSON::Make(JSON::Class::Object);

    // Name
    doc["name"] = Name;

    // Widgets array
    JSON widgetsArray = JSON::Make(JSON::Class::Array);
    for (const auto& widget : Widgets)
    {
        JSON widgetObj = JSON::Make(JSON::Class::Object);

        widgetObj["name"] = widget.Name;
        widgetObj["type"] = widget.Type;
        widgetObj["x"] = widget.X;
        widgetObj["y"] = widget.Y;
        widgetObj["width"] = widget.Width;
        widgetObj["height"] = widget.Height;
        widgetObj["zOrder"] = widget.ZOrder;

        // ProgressBar specific
        if (widget.Type == "ProgressBar")
        {
            widgetObj["progress"] = widget.Progress;
            widgetObj["foregroundMode"] = static_cast<int>(widget.ForegroundMode);
            widgetObj["backgroundMode"] = static_cast<int>(widget.BackgroundMode);

            JSON fgColor = JSON::Make(JSON::Class::Array);
            fgColor.append(widget.ForegroundColor[0], widget.ForegroundColor[1],
                          widget.ForegroundColor[2], widget.ForegroundColor[3]);
            widgetObj["foregroundColor"] = fgColor;

            JSON bgColor = JSON::Make(JSON::Class::Array);
            bgColor.append(widget.BackgroundColor[0], widget.BackgroundColor[1],
                          widget.BackgroundColor[2], widget.BackgroundColor[3]);
            widgetObj["backgroundColor"] = bgColor;

            widgetObj["borderWidth"] = widget.BorderWidth;
            widgetObj["rightToLeft"] = widget.bRightToLeft;
            widgetObj["lowThreshold"] = widget.LowThreshold;

            JSON lowColor = JSON::Make(JSON::Class::Array);
            lowColor.append(widget.LowColor[0], widget.LowColor[1],
                           widget.LowColor[2], widget.LowColor[3]);
            widgetObj["lowColor"] = lowColor;

            // 텍스처 모드용 경로
            if (!widget.ForegroundTexturePath.empty())
                widgetObj["foregroundTexturePath"] = widget.ForegroundTexturePath;
            if (!widget.BackgroundTexturePath.empty())
                widgetObj["backgroundTexturePath"] = widget.BackgroundTexturePath;
        }

        // Texture specific
        if (widget.Type == "Texture")
        {
            widgetObj["texturePath"] = widget.TexturePath;
            widgetObj["subUV_NX"] = widget.SubUV_NX;
            widgetObj["subUV_NY"] = widget.SubUV_NY;
            widgetObj["subUV_Frame"] = widget.SubUV_Frame;
            widgetObj["additive"] = widget.bAdditive;
        }

        // Binding
        if (!widget.BindingKey.empty())
        {
            widgetObj["bind"] = widget.BindingKey;
        }

        widgetsArray.append(widgetObj);
    }
    doc["widgets"] = widgetsArray;

    // Write to file
    std::wstring widePath(Path.begin(), Path.end());
    return FJsonSerializer::SaveJsonToFile(doc, widePath);
}

bool FUIAsset::LoadFromFile(const std::string& Path)
{
    std::wstring widePath(Path.begin(), Path.end());
    JSON doc;
    if (!FJsonSerializer::LoadJsonFromFile(doc, widePath))
        return false;

    // Name
    FString tempName;
    if (FJsonSerializer::ReadString(doc, "name", tempName, "", false))
        Name = tempName;

    // Widgets
    Widgets.clear();
    JSON widgetsArray;
    if (FJsonSerializer::ReadArray(doc, "widgets", widgetsArray, JSON(), false))
    {
        for (int i = 0; i < widgetsArray.length(); i++)
        {
            const JSON& widgetObj = widgetsArray[i];
            FUIEditorWidget widget;

            FString tempStr;
            if (FJsonSerializer::ReadString(widgetObj, "name", tempStr, "", false))
                widget.Name = tempStr;
            if (FJsonSerializer::ReadString(widgetObj, "type", tempStr, "", false))
                widget.Type = tempStr;

            FJsonSerializer::ReadFloat(widgetObj, "x", widget.X, 0.0f, false);
            FJsonSerializer::ReadFloat(widgetObj, "y", widget.Y, 0.0f, false);
            FJsonSerializer::ReadFloat(widgetObj, "width", widget.Width, 100.0f, false);
            FJsonSerializer::ReadFloat(widgetObj, "height", widget.Height, 30.0f, false);

            int32 tempZOrder = 0;
            if (FJsonSerializer::ReadInt32(widgetObj, "zOrder", tempZOrder, 0, false))
                widget.ZOrder = tempZOrder;

            // ProgressBar
            FJsonSerializer::ReadFloat(widgetObj, "progress", widget.Progress, 1.0f, false);

            int32 tempMode = 0;
            if (FJsonSerializer::ReadInt32(widgetObj, "foregroundMode", tempMode, 0, false))
                widget.ForegroundMode = static_cast<EProgressBarMode>(tempMode);
            tempMode = 0;
            if (FJsonSerializer::ReadInt32(widgetObj, "backgroundMode", tempMode, 0, false))
                widget.BackgroundMode = static_cast<EProgressBarMode>(tempMode);

            // Color arrays (foregroundColor, backgroundColor, lowColor)
            FLinearColor tempColor;
            if (FJsonSerializer::ReadLinearColor(widgetObj, "foregroundColor", tempColor, FLinearColor(0.2f, 0.8f, 0.2f, 1.0f), false))
            {
                widget.ForegroundColor[0] = tempColor.R;
                widget.ForegroundColor[1] = tempColor.G;
                widget.ForegroundColor[2] = tempColor.B;
                widget.ForegroundColor[3] = tempColor.A;
            }
            if (FJsonSerializer::ReadLinearColor(widgetObj, "backgroundColor", tempColor, FLinearColor(0.2f, 0.2f, 0.2f, 0.8f), false))
            {
                widget.BackgroundColor[0] = tempColor.R;
                widget.BackgroundColor[1] = tempColor.G;
                widget.BackgroundColor[2] = tempColor.B;
                widget.BackgroundColor[3] = tempColor.A;
            }
            if (FJsonSerializer::ReadLinearColor(widgetObj, "lowColor", tempColor, FLinearColor(0.9f, 0.2f, 0.2f, 1.0f), false))
            {
                widget.LowColor[0] = tempColor.R;
                widget.LowColor[1] = tempColor.G;
                widget.LowColor[2] = tempColor.B;
                widget.LowColor[3] = tempColor.A;
            }

            FJsonSerializer::ReadFloat(widgetObj, "borderWidth", widget.BorderWidth, 2.0f, false);
            FJsonSerializer::ReadBool(widgetObj, "rightToLeft", widget.bRightToLeft, false, false);
            FJsonSerializer::ReadFloat(widgetObj, "lowThreshold", widget.LowThreshold, 0.3f, false);

            // ProgressBar 텍스처 모드용 경로
            if (FJsonSerializer::ReadString(widgetObj, "foregroundTexturePath", tempStr, "", false))
                widget.ForegroundTexturePath = tempStr;
            if (FJsonSerializer::ReadString(widgetObj, "backgroundTexturePath", tempStr, "", false))
                widget.BackgroundTexturePath = tempStr;

            // Texture
            if (FJsonSerializer::ReadString(widgetObj, "texturePath", tempStr, "", false))
                widget.TexturePath = tempStr;

            int32 tempInt = 1;
            if (FJsonSerializer::ReadInt32(widgetObj, "subUV_NX", tempInt, 1, false))
                widget.SubUV_NX = tempInt;
            if (FJsonSerializer::ReadInt32(widgetObj, "subUV_NY", tempInt, 1, false))
                widget.SubUV_NY = tempInt;
            tempInt = 0;
            if (FJsonSerializer::ReadInt32(widgetObj, "subUV_Frame", tempInt, 0, false))
                widget.SubUV_Frame = tempInt;

            FJsonSerializer::ReadBool(widgetObj, "additive", widget.bAdditive, false, false);

            // Binding
            if (FJsonSerializer::ReadString(widgetObj, "bind", tempStr, "", false))
                widget.BindingKey = tempStr;

            Widgets.push_back(widget);
        }
    }

    return true;
}

// ============================================
// SUIEditorWindow
// ============================================

SUIEditorWindow::SUIEditorWindow()
{
    UIAssetPath = std::filesystem::path(GDataDir) / "UI";
    if (!std::filesystem::exists(UIAssetPath))
    {
        std::filesystem::create_directories(UIAssetPath);
    }

    NewAsset();
}

SUIEditorWindow::~SUIEditorWindow()
{
}

bool SUIEditorWindow::Initialize(float StartX, float StartY, float Width, float Height, ID3D11Device* InDevice)
{
    Device = InDevice;
    SetRect(StartX, StartY, StartX + Width, StartY + Height);

    // 실제 게임 뷰포트 크기로 캔버스 크기 설정
    float vpWidth = UGameUIManager::Get().GetViewportWidth();
    float vpHeight = UGameUIManager::Get().GetViewportHeight();
    if (vpWidth > 0 && vpHeight > 0)
    {
        CanvasSize = ImVec2(vpWidth, vpHeight);
    }

    return true;
}

void SUIEditorWindow::OnRender()
{
    if (!bIsOpen)
        return;

    ImGui::SetNextWindowSize(ImVec2(1400, 900), ImGuiCond_FirstUseEver);
    if (!bInitialPlacementDone)
    {
        ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);
        bInitialPlacementDone = true;
    }

    std::string title = "UI Editor";
    if (!CurrentAsset.Name.empty())
    {
        title += " - " + CurrentAsset.Name;
    }
    if (bModified)
    {
        title += "*";
    }

    ImGuiWindowFlags flags = ImGuiWindowFlags_MenuBar;
    if (!ImGui::Begin(title.c_str(), &bIsOpen, flags))
    {
        ImGui::End();
        return;
    }

    // 1. 메뉴바
    RenderMenuBar();

    // 2. 툴바
    RenderToolbar();

    // 3. 메인 레이아웃
    ImVec2 contentSize = ImGui::GetContentRegionAvail();

    // 좌측 패널 (팔레트)
    ImGui::BeginChild("LeftPanel", ImVec2(LeftPanelWidth, contentSize.y - BottomPanelHeight), true);
    RenderWidgetPalette(LeftPanelWidth, contentSize.y - BottomPanelHeight);
    ImGui::EndChild();

    ImGui::SameLine();

    // 중앙 캔버스
    float centerWidth = contentSize.x - LeftPanelWidth - RightPanelWidth - 20;
    ImGui::BeginChild("CenterPanel", ImVec2(centerWidth, contentSize.y - BottomPanelHeight), true,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    RenderCanvas(centerWidth, contentSize.y - BottomPanelHeight);
    ImGui::EndChild();

    ImGui::SameLine();

    // 우측 패널 (프로퍼티)
    ImGui::BeginChild("RightPanel", ImVec2(RightPanelWidth, contentSize.y - BottomPanelHeight), true);
    RenderPropertyPanel(RightPanelWidth, contentSize.y - BottomPanelHeight);
    ImGui::EndChild();

    // 하단 패널 (계층 구조)
    ImGui::BeginChild("BottomPanel", ImVec2(contentSize.x, BottomPanelHeight), true);
    RenderHierarchy(contentSize.x, BottomPanelHeight);
    ImGui::EndChild();

    ImGui::End();
}

void SUIEditorWindow::OnUpdate(float DeltaSeconds)
{
}

void SUIEditorWindow::OnMouseMove(FVector2D MousePos)
{
}

void SUIEditorWindow::OnMouseDown(FVector2D MousePos, uint32 Button)
{
}

void SUIEditorWindow::OnMouseUp(FVector2D MousePos, uint32 Button)
{
}

// ============================================
// 렌더링 함수
// ============================================

void SUIEditorWindow::RenderMenuBar()
{
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("New", "Ctrl+N"))
            {
                NewAsset();
            }
            if (ImGui::MenuItem("Open...", "Ctrl+O"))
            {
                LoadAsset();
            }
            if (ImGui::MenuItem("Save", "Ctrl+S"))
            {
                SaveAsset();
            }
            if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S"))
            {
                SaveAssetAs();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Close"))
            {
                Close();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit"))
        {
            if (ImGui::MenuItem("Delete", "Delete", false, SelectedWidgetIndex >= 0))
            {
                DeleteSelectedWidget();
            }
            if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, SelectedWidgetIndex >= 0))
            {
                DuplicateSelectedWidget();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View"))
        {
            ImGui::MenuItem("Show Grid", nullptr, &bShowGrid);
            ImGui::MenuItem("Snap to Grid", nullptr, &bSnapToGrid);
            ImGui::Separator();
            if (ImGui::MenuItem("Reset Zoom"))
            {
                CanvasZoom = 1.0f;
                CanvasPan = ImVec2(0, 0);
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }
}

void SUIEditorWindow::RenderToolbar()
{
    // 툴바 버튼들
    if (ImGui::Button("New"))
    {
        NewAsset();
    }
    ImGui::SameLine();

    if (ImGui::Button("Open"))
    {
        LoadAsset();
    }
    ImGui::SameLine();

    if (ImGui::Button("Save"))
    {
        SaveAsset();
    }
    ImGui::SameLine();

    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    ImGui::Checkbox("Grid", &bShowGrid);
    ImGui::SameLine();
    ImGui::Checkbox("Snap", &bSnapToGrid);
    ImGui::SameLine();

    ImGui::SetNextItemWidth(60);
    ImGui::DragFloat("##GridSize", &GridSize, 1.0f, 5.0f, 100.0f, "%.0f");
    ImGui::SameLine();

    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    ImGui::Text("Zoom: %.0f%%", CanvasZoom * 100.0f);
    ImGui::SameLine();
    if (ImGui::Button("-"))
    {
        ZoomAroundCenter(CanvasZoom - 0.1f);
    }
    ImGui::SameLine();
    if (ImGui::Button("+"))
    {
        ZoomAroundCenter(CanvasZoom + 0.1f);
    }
    ImGui::SameLine();
    if (ImGui::Button("Fit"))
    {
        bInitialZoomCalculated = false;  // 다음 렌더에서 재계산
    }

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    // 캔버스(뷰포트) 크기 표시
    ImGui::TextDisabled("Canvas: %.0fx%.0f", CanvasSize.x, CanvasSize.y);

    ImGui::Separator();
}

void SUIEditorWindow::RenderWidgetPalette(float Width, float Height)
{
    ImGui::Text("Widget Palette");
    ImGui::Separator();

    // 위젯 타입 버튼들
    const char* widgetTypes[] = { "ProgressBar", "Texture", "Rect" };

    for (const char* type : widgetTypes)
    {
        if (ImGui::Button(type, ImVec2(Width - 20, 30)))
        {
            CreateWidget(type);
        }
    }

    ImGui::Separator();
    ImGui::TextWrapped("Drag widgets to canvas or click to create at center.");
}

void SUIEditorWindow::RenderCanvas(float Width, float Height)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImVec2(Width - 10, Height - 10);

    // 패널 크기 저장 (툴바 줌 버튼에서 사용)
    CanvasPanelSize = canvasSize;

    // 첫 렌더링 시 캔버스가 패널에 맞도록 줌 계산
    if (!bInitialZoomCalculated && CanvasSize.x > 0 && CanvasSize.y > 0)
    {
        float zoomX = canvasSize.x / CanvasSize.x;
        float zoomY = canvasSize.y / CanvasSize.y;
        CanvasZoom = std::min(zoomX, zoomY) * 0.95f;  // 95%로 약간 여유
        CanvasZoom = std::clamp(CanvasZoom, 0.1f, 4.0f);

        // 캔버스를 패널 중앙에 배치
        float scaledW = CanvasSize.x * CanvasZoom;
        float scaledH = CanvasSize.y * CanvasZoom;
        CanvasPan.x = (canvasSize.x - scaledW) * 0.5f;
        CanvasPan.y = (canvasSize.y - scaledH) * 0.5f;

        bInitialZoomCalculated = true;
    }

    // 캔버스 배경
    drawList->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
                            IM_COL32(40, 40, 40, 255));

    // 클리핑 영역 설정
    drawList->PushClipRect(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), true);

    // 그리드 그리기
    if (bShowGrid)
    {
        DrawGrid(drawList, canvasPos, canvasSize, CanvasZoom);
    }

    // 캔버스 영역 표시 (실제 뷰포트 크기)
    ImVec2 canvasBoundsMin = ImVec2(canvasPos.x + CanvasPan.x, canvasPos.y + CanvasPan.y);
    ImVec2 canvasBoundsMax = ImVec2(canvasBoundsMin.x + CanvasSize.x * CanvasZoom,
                                     canvasBoundsMin.y + CanvasSize.y * CanvasZoom);
    drawList->AddRect(canvasBoundsMin, canvasBoundsMax, IM_COL32(100, 100, 100, 255), 0, 0, 2.0f);

    // 위젯 그리기
    for (size_t i = 0; i < CurrentAsset.Widgets.size(); i++)
    {
        DrawWidget(drawList, CurrentAsset.Widgets[i], canvasBoundsMin, CanvasZoom);
    }

    // 선택된 위젯 핸들
    if (SelectedWidgetIndex >= 0 && SelectedWidgetIndex < (int)CurrentAsset.Widgets.size())
    {
        DrawSelectionHandles(drawList, CurrentAsset.Widgets[SelectedWidgetIndex], canvasBoundsMin, CanvasZoom);
    }

    drawList->PopClipRect();

    // 캔버스 상호작용
    ImGui::InvisibleButton("canvas", canvasSize);

    if (ImGui::IsItemHovered())
    {
        // 마우스 휠 줌 (패널 중앙 기준)
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0)
        {
            ZoomAroundCenter(CanvasZoom + wheel * 0.1f);
        }

        // 마우스 드래그 (패닝 - 우클릭, 중간 버튼 또는 Alt+좌클릭)
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Right) ||
            ImGui::IsMouseDragging(ImGuiMouseButton_Middle) ||
            (ImGui::IsMouseDragging(ImGuiMouseButton_Left) && ImGui::GetIO().KeyAlt))
        {
            ImVec2 delta = ImGui::GetIO().MouseDelta;
            CanvasPan.x += delta.x;
            CanvasPan.y += delta.y;
        }

        // 위젯 선택/이동/리사이즈 (좌클릭)
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::GetIO().KeyAlt)
        {
            ImVec2 mousePos = ImGui::GetMousePos();
            ImVec2 localPos = ImVec2((mousePos.x - canvasBoundsMin.x) / CanvasZoom,
                                      (mousePos.y - canvasBoundsMin.y) / CanvasZoom);

            // 먼저 핸들 클릭 체크 (선택된 위젯이 있을 때만)
            int32_t handleIndex = HitTestHandle(localPos);
            if (handleIndex >= 0)
            {
                bDraggingHandle = true;
                DragHandleIndex = handleIndex;
                DragStartPos = localPos;
                auto& widget = CurrentAsset.Widgets[SelectedWidgetIndex];
                DragWidgetStartPos = ImVec2(widget.X, widget.Y);
                DragWidgetStartSize = ImVec2(widget.Width, widget.Height);
            }
            else
            {
                // 위젯 클릭 체크
                int32_t hitIndex = HitTest(localPos);
                SelectWidget(hitIndex);

                if (hitIndex >= 0)
                {
                    bDraggingWidget = true;
                    DragStartPos = localPos;
                    DragWidgetStartPos = ImVec2(CurrentAsset.Widgets[hitIndex].X,
                                                CurrentAsset.Widgets[hitIndex].Y);
                }
            }
        }

        // 핸들 드래그 리사이즈
        if (bDraggingHandle && SelectedWidgetIndex >= 0)
        {
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
            {
                ImVec2 mousePos = ImGui::GetMousePos();
                ImVec2 localPos = ImVec2((mousePos.x - canvasBoundsMin.x) / CanvasZoom,
                                          (mousePos.y - canvasBoundsMin.y) / CanvasZoom);

                float deltaX = localPos.x - DragStartPos.x;
                float deltaY = localPos.y - DragStartPos.y;

                auto& widget = CurrentAsset.Widgets[SelectedWidgetIndex];
                float newX = DragWidgetStartPos.x;
                float newY = DragWidgetStartPos.y;
                float newW = DragWidgetStartSize.x;
                float newH = DragWidgetStartSize.y;

                // 핸들별 리사이즈 로직
                // 0: 좌상, 1: 상, 2: 우상, 3: 우, 4: 우하, 5: 하, 6: 좌하, 7: 좌
                switch (DragHandleIndex)
                {
                case 0: // 좌상
                    newX += deltaX; newW -= deltaX;
                    newY += deltaY; newH -= deltaY;
                    break;
                case 1: // 상
                    newY += deltaY; newH -= deltaY;
                    break;
                case 2: // 우상
                    newW += deltaX;
                    newY += deltaY; newH -= deltaY;
                    break;
                case 3: // 우
                    newW += deltaX;
                    break;
                case 4: // 우하
                    newW += deltaX;
                    newH += deltaY;
                    break;
                case 5: // 하
                    newH += deltaY;
                    break;
                case 6: // 좌하
                    newX += deltaX; newW -= deltaX;
                    newH += deltaY;
                    break;
                case 7: // 좌
                    newX += deltaX; newW -= deltaX;
                    break;
                }

                // 최소 크기 보장
                const float minSize = 10.0f;
                if (newW < minSize)
                {
                    if (DragHandleIndex == 0 || DragHandleIndex == 6 || DragHandleIndex == 7)
                        newX = DragWidgetStartPos.x + DragWidgetStartSize.x - minSize;
                    newW = minSize;
                }
                if (newH < minSize)
                {
                    if (DragHandleIndex == 0 || DragHandleIndex == 1 || DragHandleIndex == 2)
                        newY = DragWidgetStartPos.y + DragWidgetStartSize.y - minSize;
                    newH = minSize;
                }

                if (bSnapToGrid)
                {
                    newX = std::round(newX / GridSize) * GridSize;
                    newY = std::round(newY / GridSize) * GridSize;
                    newW = std::round(newW / GridSize) * GridSize;
                    newH = std::round(newH / GridSize) * GridSize;
                    if (newW < minSize) newW = minSize;
                    if (newH < minSize) newH = minSize;
                }

                widget.X = newX;
                widget.Y = newY;
                widget.Width = newW;
                widget.Height = newH;
                bModified = true;
            }
        }

        // 위젯 드래그 이동
        if (bDraggingWidget && SelectedWidgetIndex >= 0 && !bDraggingHandle)
        {
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
            {
                ImVec2 mousePos = ImGui::GetMousePos();
                ImVec2 localPos = ImVec2((mousePos.x - canvasBoundsMin.x) / CanvasZoom,
                                          (mousePos.y - canvasBoundsMin.y) / CanvasZoom);

                float deltaX = localPos.x - DragStartPos.x;
                float deltaY = localPos.y - DragStartPos.y;

                float newX = DragWidgetStartPos.x + deltaX;
                float newY = DragWidgetStartPos.y + deltaY;

                if (bSnapToGrid)
                {
                    newX = std::round(newX / GridSize) * GridSize;
                    newY = std::round(newY / GridSize) * GridSize;
                }

                CurrentAsset.Widgets[SelectedWidgetIndex].X = newX;
                CurrentAsset.Widgets[SelectedWidgetIndex].Y = newY;
                bModified = true;
            }
        }

        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            bDraggingWidget = false;
            bDraggingHandle = false;
            DragHandleIndex = -1;
        }
    }
}

void SUIEditorWindow::RenderPropertyPanel(float Width, float Height)
{
    ImGui::Text("Properties");
    ImGui::Separator();

    FUIEditorWidget* widget = GetSelectedWidget();
    if (!widget)
    {
        ImGui::TextDisabled("No widget selected");
        return;
    }

    // 이름
    char nameBuf[256];
    strncpy_s(nameBuf, widget->Name.c_str(), sizeof(nameBuf) - 1);
    if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf)))
    {
        widget->Name = nameBuf;
        bModified = true;
    }

    ImGui::Text("Type: %s", widget->Type.c_str());

    ImGui::Separator();
    ImGui::Text("Transform");

    if (ImGui::DragFloat("X", &widget->X, 1.0f)) bModified = true;
    if (ImGui::DragFloat("Y", &widget->Y, 1.0f)) bModified = true;
    if (ImGui::DragFloat("Width", &widget->Width, 1.0f, 1.0f, 2000.0f)) bModified = true;
    if (ImGui::DragFloat("Height", &widget->Height, 1.0f, 1.0f, 2000.0f)) bModified = true;
    if (ImGui::DragInt("Z-Order", &widget->ZOrder)) bModified = true;

    ImGui::Separator();

    // 타입별 프로퍼티
    if (widget->Type == "ProgressBar")
    {
        ImGui::Text("ProgressBar");
        if (ImGui::SliderFloat("Progress", &widget->Progress, 0.0f, 1.0f)) bModified = true;

        const char* modeNames[] = { "Color", "Texture" };
        UResourceManager& ResMgr = UResourceManager::GetInstance();
        TArray<FString> TexturePaths = ResMgr.GetAllFilePaths<UTexture>();

        // === Foreground 설정 ===
        ImGui::Separator();
        ImGui::Text("Foreground");

        int fgMode = static_cast<int>(widget->ForegroundMode);
        if (ImGui::Combo("FG Mode", &fgMode, modeNames, IM_ARRAYSIZE(modeNames)))
        {
            widget->ForegroundMode = static_cast<EProgressBarMode>(fgMode);
            bModified = true;
        }

        if (widget->ForegroundMode == EProgressBarMode::Color)
        {
            if (ImGui::ColorEdit4("FG Color", widget->ForegroundColor)) bModified = true;
            if (ImGui::ColorEdit4("Low Color", widget->LowColor)) bModified = true;
            if (ImGui::SliderFloat("Low Threshold", &widget->LowThreshold, 0.0f, 1.0f)) bModified = true;
        }
        else
        {
            // Foreground 텍스처 콤보박스
            int fgIndex = 0;
            for (int idx = 0; idx < TexturePaths.Num(); ++idx)
            {
                if (TexturePaths[idx] == widget->ForegroundTexturePath.c_str())
                {
                    fgIndex = idx + 1;
                    break;
                }
            }
            FString fgPreview = "None";
            if (fgIndex > 0)
            {
                std::filesystem::path p(TexturePaths[fgIndex - 1]);
                fgPreview = p.filename().string();
            }
            if (ImGui::BeginCombo("FG Texture", fgPreview.c_str()))
            {
                bool selNone = (fgIndex == 0);
                if (ImGui::Selectable("None", selNone))
                {
                    widget->ForegroundTexturePath = "";
                    bModified = true;
                }
                if (selNone) ImGui::SetItemDefaultFocus();
                for (int i = 0; i < TexturePaths.Num(); ++i)
                {
                    bool selected = (fgIndex == i + 1);
                    std::filesystem::path p(TexturePaths[i]);
                    FString displayName = p.filename().string();
                    if (ImGui::Selectable(displayName.c_str(), selected))
                    {
                        widget->ForegroundTexturePath = TexturePaths[i].c_str();
                        bModified = true;
                    }
                    if (selected) ImGui::SetItemDefaultFocus();
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("%s", TexturePaths[i].c_str());
                }
                ImGui::EndCombo();
            }
        }

        // === Background 설정 ===
        ImGui::Separator();
        ImGui::Text("Background");

        int bgMode = static_cast<int>(widget->BackgroundMode);
        if (ImGui::Combo("BG Mode", &bgMode, modeNames, IM_ARRAYSIZE(modeNames)))
        {
            widget->BackgroundMode = static_cast<EProgressBarMode>(bgMode);
            bModified = true;
        }

        if (widget->BackgroundMode == EProgressBarMode::Color)
        {
            if (ImGui::ColorEdit4("BG Color", widget->BackgroundColor)) bModified = true;
        }
        else
        {
            // Background 텍스처 콤보박스
            int bgIndex = 0;
            for (int idx = 0; idx < TexturePaths.Num(); ++idx)
            {
                if (TexturePaths[idx] == widget->BackgroundTexturePath.c_str())
                {
                    bgIndex = idx + 1;
                    break;
                }
            }
            FString bgPreview = "None";
            if (bgIndex > 0)
            {
                std::filesystem::path p(TexturePaths[bgIndex - 1]);
                bgPreview = p.filename().string();
            }
            if (ImGui::BeginCombo("BG Texture", bgPreview.c_str()))
            {
                bool selNone = (bgIndex == 0);
                if (ImGui::Selectable("None", selNone))
                {
                    widget->BackgroundTexturePath = "";
                    bModified = true;
                }
                if (selNone) ImGui::SetItemDefaultFocus();
                for (int i = 0; i < TexturePaths.Num(); ++i)
                {
                    bool selected = (bgIndex == i + 1);
                    std::filesystem::path p(TexturePaths[i]);
                    FString displayName = p.filename().string();
                    if (ImGui::Selectable(displayName.c_str(), selected))
                    {
                        widget->BackgroundTexturePath = TexturePaths[i].c_str();
                        bModified = true;
                    }
                    if (selected) ImGui::SetItemDefaultFocus();
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("%s", TexturePaths[i].c_str());
                }
                ImGui::EndCombo();
            }
        }

        ImGui::Separator();

        if (ImGui::DragFloat("Border Width", &widget->BorderWidth, 0.1f, 0.0f, 10.0f)) bModified = true;
        if (ImGui::Checkbox("Right to Left", &widget->bRightToLeft)) bModified = true;
    }
    else if (widget->Type == "Texture")
    {
        ImGui::Text("Texture");

        // 텍스처 콤보박스
        UResourceManager& ResMgr = UResourceManager::GetInstance();
        TArray<FString> TexturePaths = ResMgr.GetAllFilePaths<UTexture>();

        // 현재 선택된 인덱스 찾기
        int CurrentIndex = 0; // 0 = None
        for (int idx = 0; idx < TexturePaths.Num(); ++idx)
        {
            if (TexturePaths[idx] == widget->TexturePath.c_str())
            {
                CurrentIndex = idx + 1;
                break;
            }
        }

        // 프리뷰 텍스트 (파일명만 표시)
        FString Preview = "None";
        if (CurrentIndex > 0)
        {
            std::filesystem::path p(TexturePaths[CurrentIndex - 1]);
            Preview = p.filename().string();
        }

        if (ImGui::BeginCombo("Texture", Preview.c_str()))
        {
            // None 옵션
            bool selNone = (CurrentIndex == 0);
            if (ImGui::Selectable("None", selNone))
            {
                widget->TexturePath = "";
                bModified = true;
            }
            if (selNone) ImGui::SetItemDefaultFocus();

            // 텍스처 목록
            for (int i = 0; i < TexturePaths.Num(); ++i)
            {
                bool selected = (CurrentIndex == i + 1);
                std::filesystem::path p(TexturePaths[i]);
                FString DisplayName = p.filename().string();

                if (ImGui::Selectable(DisplayName.c_str(), selected))
                {
                    widget->TexturePath = TexturePaths[i].c_str();
                    bModified = true;
                }
                if (selected) ImGui::SetItemDefaultFocus();

                // 툴팁으로 전체 경로 표시
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("%s", TexturePaths[i].c_str());
                }
            }
            ImGui::EndCombo();
        }

        ImGui::Text("SubUV");
        if (ImGui::DragInt("Columns (NX)", &widget->SubUV_NX, 1, 1, 16)) bModified = true;
        if (ImGui::DragInt("Rows (NY)", &widget->SubUV_NY, 1, 1, 16)) bModified = true;
        int maxFrame = widget->SubUV_NX * widget->SubUV_NY - 1;
        if (ImGui::DragInt("Frame", &widget->SubUV_Frame, 1, 0, maxFrame)) bModified = true;
        if (ImGui::Checkbox("Additive Blend", &widget->bAdditive)) bModified = true;
    }
    else if (widget->Type == "Rect")
    {
        ImGui::Text("Rect");
        if (ImGui::ColorEdit4("Color", widget->ForegroundColor)) bModified = true;
    }

    ImGui::Separator();
    ImGui::Text("Binding");

    char bindBuf[256];
    strncpy_s(bindBuf, widget->BindingKey.c_str(), sizeof(bindBuf) - 1);
    if (ImGui::InputText("Binding Key", bindBuf, sizeof(bindBuf)))
    {
        widget->BindingKey = bindBuf;
        bModified = true;
    }
    ImGui::TextDisabled("e.g., Player.Health");
}

void SUIEditorWindow::RenderHierarchy(float Width, float Height)
{
    ImGui::Text("Hierarchy - %s", CurrentAsset.Name.c_str());
    ImGui::SameLine(Width - 100);
    ImGui::Text("%d widgets", (int)CurrentAsset.Widgets.size());
    ImGui::Separator();

    for (size_t i = 0; i < CurrentAsset.Widgets.size(); i++)
    {
        const auto& widget = CurrentAsset.Widgets[i];
        bool selected = (SelectedWidgetIndex == (int)i);

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        if (selected) flags |= ImGuiTreeNodeFlags_Selected;

        ImGui::TreeNodeEx((void*)(intptr_t)i, flags, "[%s] %s", widget.Type.c_str(), widget.Name.c_str());

        if (ImGui::IsItemClicked())
        {
            SelectWidget((int32_t)i);
        }
    }
}

// ============================================
// 캔버스 렌더링 헬퍼
// ============================================

void SUIEditorWindow::DrawWidget(ImDrawList* DrawList, const FUIEditorWidget& Widget, ImVec2 CanvasOrigin, float Zoom)
{
    ImVec2 min = ImVec2(CanvasOrigin.x + Widget.X * Zoom, CanvasOrigin.y + Widget.Y * Zoom);
    ImVec2 max = ImVec2(min.x + Widget.Width * Zoom, min.y + Widget.Height * Zoom);

    UResourceManager& ResMgr = UResourceManager::GetInstance();

    if (Widget.Type == "ProgressBar")
    {
        ImVec2 originalMin = min;  // 테두리용 저장

        // === 배경 렌더링 ===
        if (Widget.BackgroundMode == EProgressBarMode::Texture)
        {
            // 배경 텍스처 모드
            if (!Widget.BackgroundTexturePath.empty())
            {
                UTexture* bgTex = ResMgr.Load<UTexture>(FString(Widget.BackgroundTexturePath.c_str()));
                if (bgTex && bgTex->GetShaderResourceView())
                {
                    DrawList->AddImage((ImTextureID)bgTex->GetShaderResourceView(), min, max);
                }
                else
                {
                    DrawList->AddRectFilled(min, max, IM_COL32(60, 60, 60, 200));
                }
            }
            else
            {
                DrawList->AddRectFilled(min, max, IM_COL32(60, 60, 60, 200));
            }
        }
        else
        {
            // 배경 색상 모드
            ImU32 bgColor = IM_COL32(
                (int)(Widget.BackgroundColor[0] * 255),
                (int)(Widget.BackgroundColor[1] * 255),
                (int)(Widget.BackgroundColor[2] * 255),
                (int)(Widget.BackgroundColor[3] * 255)
            );
            DrawList->AddRectFilled(min, max, bgColor);
        }

        // === 전경 렌더링 (Progress 반영) ===
        float fillWidth = Widget.Width * Widget.Progress * Zoom;
        ImVec2 fgMin, fgMax;

        if (Widget.bRightToLeft)
        {
            fgMin = ImVec2(max.x - fillWidth, min.y);
            fgMax = max;
        }
        else
        {
            fgMin = min;
            fgMax = ImVec2(min.x + fillWidth, max.y);
        }

        if (Widget.ForegroundMode == EProgressBarMode::Texture)
        {
            // 전경 텍스처 모드
            if (!Widget.ForegroundTexturePath.empty())
            {
                UTexture* fgTex = ResMgr.Load<UTexture>(FString(Widget.ForegroundTexturePath.c_str()));
                if (fgTex && fgTex->GetShaderResourceView())
                {
                    ImVec2 uvMin(0, 0), uvMax(1, 1);
                    if (Widget.bRightToLeft)
                    {
                        uvMin.x = 1.0f - Widget.Progress;
                    }
                    else
                    {
                        uvMax.x = Widget.Progress;
                    }
                    DrawList->AddImage((ImTextureID)fgTex->GetShaderResourceView(), fgMin, fgMax, uvMin, uvMax);
                }
            }
        }
        else
        {
            // 전경 색상 모드
            ImU32 fgColor = IM_COL32(
                (int)(Widget.ForegroundColor[0] * 255),
                (int)(Widget.ForegroundColor[1] * 255),
                (int)(Widget.ForegroundColor[2] * 255),
                (int)(Widget.ForegroundColor[3] * 255)
            );

            if (Widget.Progress <= Widget.LowThreshold)
            {
                fgColor = IM_COL32(
                    (int)(Widget.LowColor[0] * 255),
                    (int)(Widget.LowColor[1] * 255),
                    (int)(Widget.LowColor[2] * 255),
                    (int)(Widget.LowColor[3] * 255)
                );
            }

            DrawList->AddRectFilled(fgMin, fgMax, fgColor);
        }

        // 테두리
        if (Widget.BorderWidth > 0)
        {
            DrawList->AddRect(originalMin, max, IM_COL32(255, 255, 255, 255), 0, 0, Widget.BorderWidth);
        }
    }
    else if (Widget.Type == "Texture")
    {
        // 실제 텍스처 표시
        if (!Widget.TexturePath.empty())
        {
            UTexture* tex = ResMgr.Load<UTexture>(FString(Widget.TexturePath.c_str()));
            if (tex && tex->GetShaderResourceView())
            {
                // SubUV 계산
                float uMin = 0.0f, vMin = 0.0f, uMax = 1.0f, vMax = 1.0f;
                if (Widget.SubUV_NX > 1 || Widget.SubUV_NY > 1)
                {
                    int frameX = Widget.SubUV_Frame % Widget.SubUV_NX;
                    int frameY = Widget.SubUV_Frame / Widget.SubUV_NX;
                    float cellW = 1.0f / Widget.SubUV_NX;
                    float cellH = 1.0f / Widget.SubUV_NY;
                    uMin = frameX * cellW;
                    vMin = frameY * cellH;
                    uMax = uMin + cellW;
                    vMax = vMin + cellH;
                }

                if (Widget.bAdditive)
                {
                    // Additive 모드 - 더 밝게 표현 (ImGui에서 완전한 additive는 어려움)
                    DrawList->AddImage((ImTextureID)tex->GetShaderResourceView(), min, max,
                                       ImVec2(uMin, vMin), ImVec2(uMax, vMax), IM_COL32(255, 255, 255, 200));
                }
                else
                {
                    DrawList->AddImage((ImTextureID)tex->GetShaderResourceView(), min, max,
                                       ImVec2(uMin, vMin), ImVec2(uMax, vMax));
                }
            }
            else
            {
                // 텍스처 로드 실패 - 플레이스홀더
                DrawList->AddRectFilled(min, max, IM_COL32(80, 80, 120, 200));
                DrawList->AddRect(min, max, IM_COL32(150, 150, 200, 255));
                ImVec2 center = ImVec2((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
                DrawList->AddText(ImVec2(center.x - 20, center.y - 5), IM_COL32(255, 100, 100, 255), "[ERR]");
            }
        }
        else
        {
            // 텍스처 미설정 - 플레이스홀더
            DrawList->AddRectFilled(min, max, IM_COL32(80, 80, 120, 200));
            DrawList->AddRect(min, max, IM_COL32(150, 150, 200, 255));
            ImVec2 center = ImVec2((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
            DrawList->AddText(ImVec2(center.x - 20, center.y - 5), IM_COL32(200, 200, 200, 255), "[TEX]");
        }
    }
    else if (Widget.Type == "Rect")
    {
        ImU32 color = IM_COL32(
            (int)(Widget.ForegroundColor[0] * 255),
            (int)(Widget.ForegroundColor[1] * 255),
            (int)(Widget.ForegroundColor[2] * 255),
            (int)(Widget.ForegroundColor[3] * 255)
        );
        DrawList->AddRectFilled(min, max, color);
    }

    // 위젯 이름 표시
    DrawList->AddText(ImVec2(min.x + 2, min.y + 2), IM_COL32(255, 255, 255, 180), Widget.Name.c_str());
}

void SUIEditorWindow::DrawSelectionHandles(ImDrawList* DrawList, const FUIEditorWidget& Widget, ImVec2 CanvasOrigin, float Zoom)
{
    ImVec2 min = ImVec2(CanvasOrigin.x + Widget.X * Zoom, CanvasOrigin.y + Widget.Y * Zoom);
    ImVec2 max = ImVec2(min.x + Widget.Width * Zoom, min.y + Widget.Height * Zoom);

    // 선택 테두리
    DrawList->AddRect(min, max, IM_COL32(0, 150, 255, 255), 0, 0, 2.0f);

    // 리사이즈 핸들 (8개 모서리/변)
    const float handleSize = 6.0f;
    ImU32 handleColor = IM_COL32(0, 150, 255, 255);

    ImVec2 handles[8] = {
        ImVec2(min.x, min.y),                                   // 좌상
        ImVec2((min.x + max.x) * 0.5f, min.y),                  // 상
        ImVec2(max.x, min.y),                                   // 우상
        ImVec2(max.x, (min.y + max.y) * 0.5f),                  // 우
        ImVec2(max.x, max.y),                                   // 우하
        ImVec2((min.x + max.x) * 0.5f, max.y),                  // 하
        ImVec2(min.x, max.y),                                   // 좌하
        ImVec2(min.x, (min.y + max.y) * 0.5f),                  // 좌
    };

    for (int i = 0; i < 8; i++)
    {
        DrawList->AddRectFilled(
            ImVec2(handles[i].x - handleSize * 0.5f, handles[i].y - handleSize * 0.5f),
            ImVec2(handles[i].x + handleSize * 0.5f, handles[i].y + handleSize * 0.5f),
            handleColor
        );
    }
}

void SUIEditorWindow::DrawGrid(ImDrawList* DrawList, ImVec2 CanvasOrigin, ImVec2 CanvasSize, float Zoom)
{
    float gridStep = GridSize * Zoom;
    ImU32 gridColor = IM_COL32(60, 60, 60, 255);

    for (float x = fmodf(CanvasPan.x, gridStep); x < CanvasSize.x; x += gridStep)
    {
        DrawList->AddLine(
            ImVec2(CanvasOrigin.x + x, CanvasOrigin.y),
            ImVec2(CanvasOrigin.x + x, CanvasOrigin.y + CanvasSize.y),
            gridColor
        );
    }

    for (float y = fmodf(CanvasPan.y, gridStep); y < CanvasSize.y; y += gridStep)
    {
        DrawList->AddLine(
            ImVec2(CanvasOrigin.x, CanvasOrigin.y + y),
            ImVec2(CanvasOrigin.x + CanvasSize.x, CanvasOrigin.y + y),
            gridColor
        );
    }
}

// ============================================
// 위젯 조작
// ============================================

void SUIEditorWindow::SelectWidget(int32_t Index)
{
    DeselectAll();
    SelectedWidgetIndex = Index;
    if (Index >= 0 && Index < (int)CurrentAsset.Widgets.size())
    {
        CurrentAsset.Widgets[Index].bSelected = true;
    }
}

void SUIEditorWindow::DeselectAll()
{
    for (auto& widget : CurrentAsset.Widgets)
    {
        widget.bSelected = false;
    }
    SelectedWidgetIndex = -1;
}

void SUIEditorWindow::DeleteSelectedWidget()
{
    if (SelectedWidgetIndex >= 0 && SelectedWidgetIndex < (int)CurrentAsset.Widgets.size())
    {
        CurrentAsset.Widgets.erase(CurrentAsset.Widgets.begin() + SelectedWidgetIndex);
        SelectedWidgetIndex = -1;
        bModified = true;
    }
}

void SUIEditorWindow::DuplicateSelectedWidget()
{
    if (SelectedWidgetIndex >= 0 && SelectedWidgetIndex < (int)CurrentAsset.Widgets.size())
    {
        FUIEditorWidget copy = CurrentAsset.Widgets[SelectedWidgetIndex];
        copy.Name += "_copy";
        copy.X += 20;
        copy.Y += 20;
        CurrentAsset.Widgets.push_back(copy);
        SelectWidget((int32_t)CurrentAsset.Widgets.size() - 1);
        bModified = true;
    }
}

FUIEditorWidget* SUIEditorWindow::GetSelectedWidget()
{
    if (SelectedWidgetIndex >= 0 && SelectedWidgetIndex < (int)CurrentAsset.Widgets.size())
    {
        return &CurrentAsset.Widgets[SelectedWidgetIndex];
    }
    return nullptr;
}

int32_t SUIEditorWindow::HitTest(ImVec2 CanvasPos)
{
    // 역순으로 검사 (위에 있는 위젯 우선)
    for (int i = (int)CurrentAsset.Widgets.size() - 1; i >= 0; i--)
    {
        const auto& widget = CurrentAsset.Widgets[i];
        if (CanvasPos.x >= widget.X && CanvasPos.x <= widget.X + widget.Width &&
            CanvasPos.y >= widget.Y && CanvasPos.y <= widget.Y + widget.Height)
        {
            return i;
        }
    }
    return -1;
}

int32_t SUIEditorWindow::HitTestHandle(ImVec2 CanvasPos)
{
    if (SelectedWidgetIndex < 0 || SelectedWidgetIndex >= (int)CurrentAsset.Widgets.size())
        return -1;

    const auto& widget = CurrentAsset.Widgets[SelectedWidgetIndex];
    const float handleSize = 8.0f;  // 히트 영역 크기

    float minX = widget.X;
    float minY = widget.Y;
    float maxX = widget.X + widget.Width;
    float maxY = widget.Y + widget.Height;
    float midX = (minX + maxX) * 0.5f;
    float midY = (minY + maxY) * 0.5f;

    // 8개의 핸들 위치 (0: 좌상, 1: 상, 2: 우상, 3: 우, 4: 우하, 5: 하, 6: 좌하, 7: 좌)
    ImVec2 handles[8] = {
        ImVec2(minX, minY),  // 0: 좌상
        ImVec2(midX, minY),  // 1: 상
        ImVec2(maxX, minY),  // 2: 우상
        ImVec2(maxX, midY),  // 3: 우
        ImVec2(maxX, maxY),  // 4: 우하
        ImVec2(midX, maxY),  // 5: 하
        ImVec2(minX, maxY),  // 6: 좌하
        ImVec2(minX, midY),  // 7: 좌
    };

    for (int i = 0; i < 8; i++)
    {
        if (CanvasPos.x >= handles[i].x - handleSize * 0.5f &&
            CanvasPos.x <= handles[i].x + handleSize * 0.5f &&
            CanvasPos.y >= handles[i].y - handleSize * 0.5f &&
            CanvasPos.y <= handles[i].y + handleSize * 0.5f)
        {
            return i;
        }
    }

    return -1;
}

void SUIEditorWindow::ZoomAroundCenter(float NewZoom)
{
    NewZoom = std::clamp(NewZoom, 0.1f, 4.0f);

    if (CanvasPanelSize.x > 0 && CanvasPanelSize.y > 0)
    {
        float oldZoom = CanvasZoom;

        // 패널 중앙 좌표
        ImVec2 panelCenter = ImVec2(CanvasPanelSize.x * 0.5f, CanvasPanelSize.y * 0.5f);

        // 패널 중앙에 해당하는 캔버스 좌표 계산
        ImVec2 canvasCoord = ImVec2(
            (panelCenter.x - CanvasPan.x) / oldZoom,
            (panelCenter.y - CanvasPan.y) / oldZoom
        );

        // 새 줌 적용 후 같은 캔버스 좌표가 패널 중앙에 유지되도록 Pan 조정
        CanvasPan.x = panelCenter.x - canvasCoord.x * NewZoom;
        CanvasPan.y = panelCenter.y - canvasCoord.y * NewZoom;
    }

    CanvasZoom = NewZoom;
}

void SUIEditorWindow::CreateWidget(const std::string& Type)
{
    FUIEditorWidget widget;
    widget.Type = Type;
    widget.Name = Type + "_" + std::to_string(CurrentAsset.Widgets.size());

    // 캔버스 중앙에 배치
    widget.X = (CanvasSize.x - widget.Width) * 0.5f;
    widget.Y = (CanvasSize.y - widget.Height) * 0.5f;

    if (Type == "ProgressBar")
    {
        widget.Width = 200;
        widget.Height = 30;
    }
    else if (Type == "Texture")
    {
        widget.Width = 100;
        widget.Height = 100;
    }
    else if (Type == "Rect")
    {
        widget.Width = 100;
        widget.Height = 100;
    }

    CurrentAsset.Widgets.push_back(widget);
    SelectWidget((int32_t)CurrentAsset.Widgets.size() - 1);
    bModified = true;
}

// ============================================
// 에셋 관리
// ============================================

void SUIEditorWindow::NewAsset()
{
    CurrentAsset = FUIAsset();
    CurrentAsset.Name = "NewUI";
    CurrentAssetPath.clear();
    SelectedWidgetIndex = -1;
    bModified = false;
}

void SUIEditorWindow::LoadAsset()
{
    std::filesystem::path selectedPath = FPlatformProcess::OpenLoadFileDialog(
        UIAssetPath.wstring(),
        L".uiasset",
        L"UI Asset Files"
    );

    if (!selectedPath.empty())
    {
        LoadAsset(selectedPath.string());
    }
}

void SUIEditorWindow::LoadAsset(const std::string& Path)
{
    if (CurrentAsset.LoadFromFile(Path))
    {
        CurrentAssetPath = Path;
        SelectedWidgetIndex = -1;
        bModified = false;
    }
}

void SUIEditorWindow::SaveAsset()
{
    if (CurrentAssetPath.empty())
    {
        SaveAssetAs();
    }
    else
    {
        if (CurrentAsset.SaveToFile(CurrentAssetPath))
        {
            bModified = false;
        }
    }
}

void SUIEditorWindow::SaveAssetAs()
{
    std::filesystem::path selectedPath = FPlatformProcess::OpenSaveFileDialog(
        UIAssetPath.wstring(),
        L".uiasset",
        L"UI Asset Files",
        L"NewUI"
    );

    if (!selectedPath.empty())
    {
        std::string path = selectedPath.string();

        // 확장자 추가
        if (path.find(".uiasset") == std::string::npos)
        {
            path += ".uiasset";
        }

        CurrentAssetPath = path;

        // 파일명에서 에셋 이름 추출
        std::filesystem::path p(path);
        CurrentAsset.Name = p.stem().string();

        if (CurrentAsset.SaveToFile(path))
        {
            bModified = false;
        }
    }
}
