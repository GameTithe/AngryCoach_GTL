#include "pch.h"
#include "SUIEditorWindow.h"
#include "imgui.h"
#include "PlatformProcess.h"
#include "Source/Runtime/Core/Misc/JsonSerializer.h"
#include "Source/Game/UI/GameUIManager.h"
#include "Source/Runtime/AssetManagement/ResourceManager.h"
#include "Source/Runtime/AssetManagement/Texture.h"
#include <algorithm>  // std::transform

// ============================================
// FUIAsset 직렬화
// ============================================

bool FUIAsset::SaveToFile(const std::string& Path) const
{
    JSON doc = JSON::Make(JSON::Class::Object);

    // Name
    doc["name"] = Name;

    // 디자인 해상도 (캔버스 크기)
    doc["canvasWidth"] = CanvasWidth;
    doc["canvasHeight"] = CanvasHeight;

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

        // Rect specific
        if (widget.Type == "Rect")
        {
            JSON color = JSON::Make(JSON::Class::Array);
            color.append(widget.ForegroundColor[0], widget.ForegroundColor[1],
                        widget.ForegroundColor[2], widget.ForegroundColor[3]);
            widgetObj["color"] = color;
        }

        // Button specific
        if (widget.Type == "Button")
        {
            widgetObj["texturePath"] = widget.TexturePath;
            if (!widget.HoveredTexturePath.empty())
                widgetObj["hoveredTexturePath"] = widget.HoveredTexturePath;
            if (!widget.PressedTexturePath.empty())
                widgetObj["pressedTexturePath"] = widget.PressedTexturePath;
            if (!widget.DisabledTexturePath.empty())
                widgetObj["disabledTexturePath"] = widget.DisabledTexturePath;

            // State alpha values
            widgetObj["normalAlpha"] = widget.NormalAlpha;
            widgetObj["hoveredAlpha"] = widget.HoveredAlpha;
            widgetObj["pressedAlpha"] = widget.PressedAlpha;
            widgetObj["disabledAlpha"] = widget.DisabledAlpha;

            widgetObj["interactable"] = widget.bInteractable;

            // Hover Scale 효과
            if (widget.HoverScale > 1.0f)
            {
                widgetObj["hoverScale"] = widget.HoverScale;
                widgetObj["hoverScaleDuration"] = widget.HoverScaleDuration;
            }

            // SubUV 설정 (Button도 TextureWidget 상속)
            widgetObj["subUV_NX"] = widget.SubUV_NX;
            widgetObj["subUV_NY"] = widget.SubUV_NY;
            widgetObj["subUV_Frame"] = widget.SubUV_Frame;
        }

        // Binding
        if (!widget.BindingKey.empty())
        {
            widgetObj["bind"] = widget.BindingKey;
        }

        // Enter Animation
        if (widget.EnterAnim.Type != 0)
        {
            JSON enterAnim = JSON::Make(JSON::Class::Object);
            enterAnim["type"] = widget.EnterAnim.Type;
            enterAnim["duration"] = widget.EnterAnim.Duration;
            enterAnim["easing"] = widget.EnterAnim.Easing;
            enterAnim["delay"] = widget.EnterAnim.Delay;
            enterAnim["offset"] = widget.EnterAnim.Offset;
            widgetObj["enterAnim"] = enterAnim;
        }

        // Exit Animation
        if (widget.ExitAnim.Type != 0)
        {
            JSON exitAnim = JSON::Make(JSON::Class::Object);
            exitAnim["type"] = widget.ExitAnim.Type;
            exitAnim["duration"] = widget.ExitAnim.Duration;
            exitAnim["easing"] = widget.ExitAnim.Easing;
            exitAnim["delay"] = widget.ExitAnim.Delay;
            exitAnim["offset"] = widget.ExitAnim.Offset;
            widgetObj["exitAnim"] = exitAnim;
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

    // 디자인 해상도 (캔버스 크기)
    FJsonSerializer::ReadFloat(doc, "canvasWidth", CanvasWidth, 1920.0f, false);
    FJsonSerializer::ReadFloat(doc, "canvasHeight", CanvasHeight, 1080.0f, false);

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

            // Rect
            if (FJsonSerializer::ReadLinearColor(widgetObj, "color", tempColor, FLinearColor(0.2f, 0.8f, 0.2f, 1.0f), false))
            {
                widget.ForegroundColor[0] = tempColor.R;
                widget.ForegroundColor[1] = tempColor.G;
                widget.ForegroundColor[2] = tempColor.B;
                widget.ForegroundColor[3] = tempColor.A;
            }

            // Button
            if (FJsonSerializer::ReadString(widgetObj, "hoveredTexturePath", tempStr, "", false))
                widget.HoveredTexturePath = tempStr;
            if (FJsonSerializer::ReadString(widgetObj, "pressedTexturePath", tempStr, "", false))
                widget.PressedTexturePath = tempStr;
            if (FJsonSerializer::ReadString(widgetObj, "disabledTexturePath", tempStr, "", false))
                widget.DisabledTexturePath = tempStr;

            // New alpha format
            FJsonSerializer::ReadFloat(widgetObj, "normalAlpha", widget.NormalAlpha, 1.0f, false);
            FJsonSerializer::ReadFloat(widgetObj, "hoveredAlpha", widget.HoveredAlpha, 1.0f, false);
            FJsonSerializer::ReadFloat(widgetObj, "pressedAlpha", widget.PressedAlpha, 1.0f, false);
            FJsonSerializer::ReadFloat(widgetObj, "disabledAlpha", widget.DisabledAlpha, 0.5f, false);

            // Legacy tint format 호환 (alpha만 사용)
            if (FJsonSerializer::ReadLinearColor(widgetObj, "normalTint", tempColor, FLinearColor(1, 1, 1, 1), false))
                widget.NormalAlpha = tempColor.A;
            if (FJsonSerializer::ReadLinearColor(widgetObj, "hoveredTint", tempColor, FLinearColor(1, 1, 1, 1), false))
                widget.HoveredAlpha = tempColor.A;
            if (FJsonSerializer::ReadLinearColor(widgetObj, "pressedTint", tempColor, FLinearColor(1, 1, 1, 1), false))
                widget.PressedAlpha = tempColor.A;
            if (FJsonSerializer::ReadLinearColor(widgetObj, "disabledTint", tempColor, FLinearColor(0.5f, 0.5f, 0.5f, 0.5f), false))
                widget.DisabledAlpha = tempColor.A;
            FJsonSerializer::ReadBool(widgetObj, "interactable", widget.bInteractable, true, false);

            // Hover Scale 효과
            FJsonSerializer::ReadFloat(widgetObj, "hoverScale", widget.HoverScale, 1.0f, false);
            FJsonSerializer::ReadFloat(widgetObj, "hoverScaleDuration", widget.HoverScaleDuration, 0.1f, false);

            // Binding
            if (FJsonSerializer::ReadString(widgetObj, "bind", tempStr, "", false))
                widget.BindingKey = tempStr;

            // Enter Animation
            JSON enterAnimObj;
            if (FJsonSerializer::ReadObject(widgetObj, "enterAnim", enterAnimObj, JSON(), false))
            {
                int32 animType = 0;
                FJsonSerializer::ReadInt32(enterAnimObj, "type", animType, 0, false);
                widget.EnterAnim.Type = animType;
                FJsonSerializer::ReadFloat(enterAnimObj, "duration", widget.EnterAnim.Duration, 0.3f, false);
                int32 easing = 2;
                FJsonSerializer::ReadInt32(enterAnimObj, "easing", easing, 2, false);
                widget.EnterAnim.Easing = easing;
                FJsonSerializer::ReadFloat(enterAnimObj, "delay", widget.EnterAnim.Delay, 0.0f, false);
                FJsonSerializer::ReadFloat(enterAnimObj, "offset", widget.EnterAnim.Offset, 100.0f, false);
            }

            // Exit Animation
            JSON exitAnimObj;
            if (FJsonSerializer::ReadObject(widgetObj, "exitAnim", exitAnimObj, JSON(), false))
            {
                int32 animType = 0;
                FJsonSerializer::ReadInt32(exitAnimObj, "type", animType, 0, false);
                widget.ExitAnim.Type = animType;
                FJsonSerializer::ReadFloat(exitAnimObj, "duration", widget.ExitAnim.Duration, 0.3f, false);
                int32 easing = 2;
                FJsonSerializer::ReadInt32(exitAnimObj, "easing", easing, 2, false);
                widget.ExitAnim.Easing = easing;
                FJsonSerializer::ReadFloat(exitAnimObj, "delay", widget.ExitAnim.Delay, 0.0f, false);
                FJsonSerializer::ReadFloat(exitAnimObj, "offset", widget.ExitAnim.Offset, 100.0f, false);
            }

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

    if (ImGui::Button("Save As"))
    {
        SaveAssetAs();
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

    // 캔버스(디자인 해상도) 크기 입력
    ImGui::Text("Canvas:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(60);
    int canvasW = static_cast<int>(CanvasSize.x);
    if (ImGui::DragInt("##CanvasW", &canvasW, 1.0f, 100, 4096))
    {
        if (canvasW < 100) canvasW = 100;
        if (canvasW > 4096) canvasW = 4096;
        CanvasSize.x = static_cast<float>(canvasW);
        bModified = true;
    }
    ImGui::SameLine();
    ImGui::Text("x");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(60);
    int canvasH = static_cast<int>(CanvasSize.y);
    if (ImGui::DragInt("##CanvasH", &canvasH, 1.0f, 100, 4096))
    {
        if (canvasH < 100) canvasH = 100;
        if (canvasH > 4096) canvasH = 4096;
        CanvasSize.y = static_cast<float>(canvasH);
        bModified = true;
    }

    ImGui::Separator();
}

void SUIEditorWindow::RenderWidgetPalette(float Width, float Height)
{
    ImGui::Text("Widget Palette");
    ImGui::Separator();

    // 위젯 타입 버튼들
    const char* widgetTypes[] = { "ProgressBar", "Texture", "Rect", "Button" };

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
        LastSelectedWidgetIndex = -1;
        return;
    }

    // 선택된 위젯이 변경되면 입력 버퍼를 새 위젯 값으로 동기화
    if (SelectedWidgetIndex != LastSelectedWidgetIndex)
    {
        strncpy_s(WidgetNameBuffer, widget->Name.c_str(), sizeof(WidgetNameBuffer) - 1);
        strncpy_s(BindingKeyBuffer, widget->BindingKey.c_str(), sizeof(BindingKeyBuffer) - 1);
        LastSelectedWidgetIndex = SelectedWidgetIndex;
    }

    // 이름 (멤버 버퍼 사용)
    if (ImGui::InputText("Name", WidgetNameBuffer, sizeof(WidgetNameBuffer)))
    {
        widget->Name = WidgetNameBuffer;
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
            // 검색 가능한 Foreground 텍스처 콤보박스
            if (TextureComboWithSearch("FG Texture", widget->ForegroundTexturePath, TexturePaths,
                                        FGTextureSearchFilter, sizeof(FGTextureSearchFilter)))
            {
                bModified = true;
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
            // 검색 가능한 Background 텍스처 콤보박스
            if (TextureComboWithSearch("BG Texture", widget->BackgroundTexturePath, TexturePaths,
                                        BGTextureSearchFilter, sizeof(BGTextureSearchFilter)))
            {
                bModified = true;
            }
        }

        ImGui::Separator();

        if (ImGui::DragFloat("Border Width", &widget->BorderWidth, 0.1f, 0.0f, 10.0f)) bModified = true;
        if (ImGui::Checkbox("Right to Left", &widget->bRightToLeft)) bModified = true;
    }
    else if (widget->Type == "Texture")
    {
        ImGui::Text("Texture");

        // 검색 가능한 텍스처 콤보박스
        UResourceManager& ResMgr = UResourceManager::GetInstance();
        TArray<FString> TexturePaths = ResMgr.GetAllFilePaths<UTexture>();

        if (TextureComboWithSearch("Texture", widget->TexturePath, TexturePaths,
                                    TextureSearchFilter, sizeof(TextureSearchFilter)))
        {
            bModified = true;
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
    else if (widget->Type == "Button")
    {
        ImGui::Text("Button");

        // 기본 텍스처
        UResourceManager& ResMgr = UResourceManager::GetInstance();
        TArray<FString> TexturePaths = ResMgr.GetAllFilePaths<UTexture>();

        if (TextureComboWithSearch("Normal Texture", widget->TexturePath, TexturePaths,
                                    TextureSearchFilter, sizeof(TextureSearchFilter)))
        {
            bModified = true;
        }

        // 상태별 텍스처
        if (TextureComboWithSearch("Hovered Texture", widget->HoveredTexturePath, TexturePaths,
                                    TextureSearchFilter, sizeof(TextureSearchFilter)))
        {
            bModified = true;
        }
        if (TextureComboWithSearch("Pressed Texture", widget->PressedTexturePath, TexturePaths,
                                    TextureSearchFilter, sizeof(TextureSearchFilter)))
        {
            bModified = true;
        }
        if (TextureComboWithSearch("Disabled Texture", widget->DisabledTexturePath, TexturePaths,
                                    TextureSearchFilter, sizeof(TextureSearchFilter)))
        {
            bModified = true;
        }

        ImGui::Separator();

        // 상태별 투명도
        ImGui::Text("State Alpha");
        if (ImGui::SliderFloat("Normal Alpha", &widget->NormalAlpha, 0.0f, 1.0f, "%.2f")) bModified = true;
        if (ImGui::SliderFloat("Hovered Alpha", &widget->HoveredAlpha, 0.0f, 1.0f, "%.2f")) bModified = true;
        if (ImGui::SliderFloat("Pressed Alpha", &widget->PressedAlpha, 0.0f, 1.0f, "%.2f")) bModified = true;
        if (ImGui::SliderFloat("Disabled Alpha", &widget->DisabledAlpha, 0.0f, 1.0f, "%.2f")) bModified = true;

        ImGui::Separator();

        // 호버 스케일 효과
        ImGui::Text("Hover Effect");
        if (ImGui::DragFloat("Hover Scale", &widget->HoverScale, 0.01f, 1.0f, 2.0f, "%.2f"))
            bModified = true;
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("1.0 = no effect, 1.1 = 10%% scale up");
        if (widget->HoverScale > 1.0f)
        {
            if (ImGui::DragFloat("Scale Duration", &widget->HoverScaleDuration, 0.01f, 0.0f, 1.0f, "%.2f s"))
                bModified = true;
        }

        ImGui::Separator();

        if (ImGui::Checkbox("Interactable", &widget->bInteractable)) bModified = true;

        ImGui::Separator();

        // SubUV (Button도 TextureWidget 상속)
        ImGui::Text("SubUV");
        if (ImGui::DragInt("Columns (NX)", &widget->SubUV_NX, 1, 1, 16)) bModified = true;
        if (ImGui::DragInt("Rows (NY)", &widget->SubUV_NY, 1, 1, 16)) bModified = true;
        int maxFrame = widget->SubUV_NX * widget->SubUV_NY - 1;
        if (ImGui::DragInt("Frame", &widget->SubUV_Frame, 1, 0, maxFrame)) bModified = true;
    }

    // === Animation 설정 ===
    ImGui::Separator();
    if (ImGui::CollapsingHeader("Animation", ImGuiTreeNodeFlags_DefaultOpen))
    {
        const char* animTypeNames[] = {
            "None", "Fade", "Slide Left", "Slide Right", "Slide Top", "Slide Bottom",
            "Scale", "Fade+Slide Left", "Fade+Slide Right", "Fade+Slide Top", "Fade+Slide Bottom", "Fade+Scale"
        };
        const char* easingNames[] = { "Linear", "Ease In", "Ease Out", "Ease In/Out" };

        // Enter Animation
        ImGui::Text("Enter Animation");
        ImGui::PushID("EnterAnim");
        if (ImGui::Combo("Type##Enter", &widget->EnterAnim.Type, animTypeNames, IM_ARRAYSIZE(animTypeNames))) bModified = true;
        if (widget->EnterAnim.Type != 0)
        {
            if (ImGui::DragFloat("Duration##Enter", &widget->EnterAnim.Duration, 0.01f, 0.0f, 5.0f, "%.2f s")) bModified = true;
            if (ImGui::Combo("Easing##Enter", &widget->EnterAnim.Easing, easingNames, IM_ARRAYSIZE(easingNames))) bModified = true;
            if (ImGui::DragFloat("Delay##Enter", &widget->EnterAnim.Delay, 0.01f, 0.0f, 5.0f, "%.2f s")) bModified = true;
            // Slide 타입일 경우에만 Offset 표시
            if (widget->EnterAnim.Type >= 2 && widget->EnterAnim.Type <= 5 ||
                widget->EnterAnim.Type >= 7 && widget->EnterAnim.Type <= 10)
            {
                if (ImGui::DragFloat("Offset##Enter", &widget->EnterAnim.Offset, 10.0f, 0.0f, 3000.0f, "%.0f px")) bModified = true;
            }
        }
        ImGui::PopID();

        ImGui::Spacing();

        // Exit Animation
        ImGui::Text("Exit Animation");
        ImGui::PushID("ExitAnim");
        if (ImGui::Combo("Type##Exit", &widget->ExitAnim.Type, animTypeNames, IM_ARRAYSIZE(animTypeNames))) bModified = true;
        if (widget->ExitAnim.Type != 0)
        {
            if (ImGui::DragFloat("Duration##Exit", &widget->ExitAnim.Duration, 0.01f, 0.0f, 5.0f, "%.2f s")) bModified = true;
            if (ImGui::Combo("Easing##Exit", &widget->ExitAnim.Easing, easingNames, IM_ARRAYSIZE(easingNames))) bModified = true;
            if (ImGui::DragFloat("Delay##Exit", &widget->ExitAnim.Delay, 0.01f, 0.0f, 5.0f, "%.2f s")) bModified = true;
            // Slide 타입일 경우에만 Offset 표시
            if (widget->ExitAnim.Type >= 2 && widget->ExitAnim.Type <= 5 ||
                widget->ExitAnim.Type >= 7 && widget->ExitAnim.Type <= 10)
            {
                if (ImGui::DragFloat("Offset##Exit", &widget->ExitAnim.Offset, 10.0f, 0.0f, 3000.0f, "%.0f px")) bModified = true;
            }
        }
        ImGui::PopID();
    }

    ImGui::Separator();
    ImGui::Text("Binding");

    // Binding Key (멤버 버퍼 사용 - 선택 변경 시 상단에서 동기화됨)
    if (ImGui::InputText("Binding Key", BindingKeyBuffer, sizeof(BindingKeyBuffer)))
    {
        widget->BindingKey = BindingKeyBuffer;
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

        // 우클릭 컨텍스트 메뉴
        if (ImGui::BeginPopupContextItem())
        {
            SelectWidget((int32_t)i);  // 우클릭 시 선택

            if (ImGui::MenuItem("Delete"))
            {
                DeleteSelectedWidget();
                ImGui::EndPopup();
                break;  // 삭제 후 루프 탈출
            }
            if (ImGui::MenuItem("Duplicate"))
            {
                DuplicateSelectedWidget();
            }
            ImGui::EndPopup();
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
    else if (Widget.Type == "Button")
    {
        // 버튼 미리보기: 4개 텍스처를 2x2로 배치
        float halfW = (max.x - min.x) * 0.5f;
        float halfH = (max.y - min.y) * 0.5f;

        // 4개 영역 정의: Normal(좌상), Hovered(우상), Pressed(좌하), Disabled(우하)
        ImVec2 cells[4][2] = {
            { min, ImVec2(min.x + halfW, min.y + halfH) },                                    // Normal
            { ImVec2(min.x + halfW, min.y), ImVec2(max.x, min.y + halfH) },                   // Hovered
            { ImVec2(min.x, min.y + halfH), ImVec2(min.x + halfW, max.y) },                   // Pressed
            { ImVec2(min.x + halfW, min.y + halfH), max }                                      // Disabled
        };

        const char* labels[4] = { "N", "H", "P", "D" };
        std::string texPaths[4] = {
            Widget.TexturePath,
            Widget.HoveredTexturePath.empty() ? Widget.TexturePath : Widget.HoveredTexturePath,
            Widget.PressedTexturePath.empty() ? Widget.TexturePath : Widget.PressedTexturePath,
            Widget.DisabledTexturePath.empty() ? Widget.TexturePath : Widget.DisabledTexturePath
        };
        float alphas[4] = {
            Widget.NormalAlpha,
            Widget.HoveredAlpha,
            Widget.PressedAlpha,
            Widget.DisabledAlpha
        };

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

        for (int i = 0; i < 4; i++)
        {
            ImVec2 cellMin = cells[i][0];
            ImVec2 cellMax = cells[i][1];

            // Alpha만 적용 (RGB는 255 고정)
            ImU32 tintColor = IM_COL32(255, 255, 255, (int)(alphas[i] * 255.0f));

            if (!texPaths[i].empty())
            {
                UTexture* tex = ResMgr.Load<UTexture>(FString(texPaths[i].c_str()));
                if (tex && tex->GetShaderResourceView())
                {
                    DrawList->AddImage((ImTextureID)tex->GetShaderResourceView(), cellMin, cellMax,
                                       ImVec2(uMin, vMin), ImVec2(uMax, vMax), tintColor);
                }
                else
                {
                    DrawList->AddRectFilled(cellMin, cellMax, IM_COL32(80, 80, 120, 200));
                }
            }
            else
            {
                DrawList->AddRectFilled(cellMin, cellMax, IM_COL32(80, 80, 120, 200));
            }

            // 상태 라벨 표시
            DrawList->AddText(ImVec2(cellMin.x + 2, cellMin.y + 2), IM_COL32(255, 255, 0, 200), labels[i]);
        }

        // 구분선
        DrawList->AddLine(ImVec2(min.x + halfW, min.y), ImVec2(min.x + halfW, max.y), IM_COL32(255, 255, 255, 100));
        DrawList->AddLine(ImVec2(min.x, min.y + halfH), ImVec2(max.x, min.y + halfH), IM_COL32(255, 255, 255, 100));

        // 외곽선
        DrawList->AddRect(min, max, IM_COL32(100, 200, 100, 255));
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

        // 로드한 캔버스 크기 적용
        CanvasSize.x = CurrentAsset.CanvasWidth;
        CanvasSize.y = CurrentAsset.CanvasHeight;
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
        // 현재 캔버스 크기를 에셋에 저장
        CurrentAsset.CanvasWidth = CanvasSize.x;
        CurrentAsset.CanvasHeight = CanvasSize.y;

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

        // 현재 캔버스 크기를 에셋에 저장
        CurrentAsset.CanvasWidth = CanvasSize.x;
        CurrentAsset.CanvasHeight = CanvasSize.y;

        if (CurrentAsset.SaveToFile(path))
        {
            bModified = false;
        }
    }
}

// ============================================
// 검색 가능한 텍스처 콤보박스
// ============================================

bool SUIEditorWindow::TextureComboWithSearch(const char* label, std::string& outPath,
                                              const TArray<FString>& texturePaths,
                                              char* searchBuffer, size_t bufferSize)
{
    bool modified = false;

    // 현재 선택된 인덱스 찾기
    int currentIndex = 0; // 0 = None
    for (int idx = 0; idx < texturePaths.Num(); ++idx)
    {
        if (texturePaths[idx] == outPath.c_str())
        {
            currentIndex = idx + 1;
            break;
        }
    }

    // 프리뷰 텍스트 (파일명만 표시)
    FString preview = "None";
    if (currentIndex > 0)
    {
        std::filesystem::path p(texturePaths[currentIndex - 1]);
        preview = p.filename().string();
    }

    if (ImGui::BeginCombo(label, preview.c_str()))
    {
        // 검색 입력 필드
        ImGui::SetNextItemWidth(-1);
        if (ImGui::IsWindowAppearing())
        {
            ImGui::SetKeyboardFocusHere();
            searchBuffer[0] = '\0';  // 콤보박스 열릴 때 검색어 초기화
        }
        ImGui::InputTextWithHint("##search", "Search...", searchBuffer, bufferSize);

        ImGui::Separator();

        // 검색어를 소문자로 변환
        std::string searchLower = searchBuffer;
        std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);

        // None 옵션 (검색어가 없거나 "none"과 매칭될 때만)
        if (searchLower.empty() || std::string("none").find(searchLower) != std::string::npos)
        {
            bool selNone = (currentIndex == 0);
            if (ImGui::Selectable("None", selNone))
            {
                outPath = "";
                modified = true;
            }
            if (selNone) ImGui::SetItemDefaultFocus();
        }

        // 텍스처 목록 (필터링 적용)
        for (int i = 0; i < texturePaths.Num(); ++i)
        {
            std::filesystem::path p(texturePaths[i]);
            FString displayName = p.filename().string();

            // 검색 필터링 (대소문자 무시)
            if (!searchLower.empty())
            {
                std::string nameLower = displayName.c_str();
                std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                if (nameLower.find(searchLower) == std::string::npos)
                {
                    continue;  // 매칭되지 않으면 스킵
                }
            }

            bool selected = (currentIndex == i + 1);
            if (ImGui::Selectable(displayName.c_str(), selected))
            {
                outPath = texturePaths[i].c_str();
                modified = true;
            }
            if (selected) ImGui::SetItemDefaultFocus();

            // 툴팁으로 전체 경로 표시
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("%s", texturePaths[i].c_str());
            }
        }

        ImGui::EndCombo();
    }

    return modified;
}
