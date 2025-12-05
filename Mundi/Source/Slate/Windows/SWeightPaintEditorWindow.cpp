#include "pch.h"
#include "SWeightPaintEditorWindow.h"
#include "Source/Runtime/Engine/WeightPaint/WeightPaintViewportClient.h"
#include "Source/Runtime/Engine/WeightPaint/ClothWeightAsset.h"
#include "Source/Runtime/Renderer/FViewport.h"
#include "Source/Runtime/Renderer/Renderer.h"
#include "Source/Runtime/Core/Object/ObjectFactory.h"
#include "Source/Editor/Gizmo/GizmoActor.h"
#include "RenderManager.h"
#include "World.h"
#include "SkeletalMeshActor.h"
#include "SkeletalMeshComponent.h"
#include "SkeletalMesh.h"
#include "ClothComponent.h"
#include "ResourceManager.h"
#include "CameraActor.h"
#include "DirectionalLightActor.h"
#include "DirectionalLightComponent.h"
#include "Source/Runtime/Engine/Collision/AABB.h"

#include <imgui.h>
#include <d3d11.h>
#include <algorithm>
#include <filesystem>

SWeightPaintEditorWindow::SWeightPaintEditorWindow()
{
}

SWeightPaintEditorWindow::~SWeightPaintEditorWindow()
{
    UnloadMesh();

    if (PaintState.Viewport)
    {
        PaintState.Viewport->Cleanup();
        delete PaintState.Viewport;
        PaintState.Viewport = nullptr;
    }

    if (PaintState.Client)
    {
        delete PaintState.Client;
        PaintState.Client = nullptr;
    }

    // Preview World 정리
    if (PreviewWorld)
    {
        ObjectFactory::DeleteObject(PreviewWorld);
        PreviewWorld = nullptr;
    }
}

bool SWeightPaintEditorWindow::Initialize(float StartX, float StartY, float Width, float Height,
                                           UWorld* InWorld, ID3D11Device* InDevice)
{
    Device = InDevice;
    if (!Device) return false;

    SetRect(StartX, StartY, StartX + Width, StartY + Height);

    // 1. 별도 Preview World 생성
    PreviewWorld = NewObject<UWorld>();
    PreviewWorld->SetWorldType(EWorldType::PreviewMinimal);
    PreviewWorld->Initialize();

    // Weight Paint Editor에서는 에디터 프리미티브(기즈모, 그리드 등)를 렌더링하지 않음
    // FSceneRenderer::Render()에서 !World->bPie 조건으로 기즈모를 렌더링하므로 bPie=true로 설정
    PreviewWorld->bPie = true;

    // 에디터 아이콘 비활성화
    PreviewWorld->GetRenderSettings().DisableShowFlag(EEngineShowFlags::SF_EditorIcon);
    PreviewWorld->GetRenderSettings().DisableShowFlag(EEngineShowFlags::SF_Grid);  // 그리드도 비활성화

    // 기즈모 완전 비활성화 (추가 안전장치)
    if (AGizmoActor* Gizmo = PreviewWorld->GetGizmoActor())
    {
        Gizmo->SetbRender(false);
    }

    // InWorld의 조명/메시 관련 ShowFlags만 복사 (에디터 관련은 복사하지 않음)
    if (InWorld)
    {
        URenderSettings& RenderSettings = PreviewWorld->GetRenderSettings();
        const URenderSettings& InRenderSettings = InWorld->GetRenderSettings();

        // 렌더링에 필요한 플래그만 활성화
        if (InRenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_SkeletalMeshes))
            RenderSettings.EnableShowFlag(EEngineShowFlags::SF_SkeletalMeshes);
        if (InRenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_Lighting))
            RenderSettings.EnableShowFlag(EEngineShowFlags::SF_Lighting);
        if (InRenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_StaticMeshes))
            RenderSettings.EnableShowFlag(EEngineShowFlags::SF_StaticMeshes);
    }

    // PaintState에 Preview World 설정
    PaintState.World = PreviewWorld;

    // 2. 뷰포트 생성
    PaintState.Viewport = new FViewport();
    if (!PaintState.Viewport->Initialize(0, 0, 1, 1, Device))
    {
        delete PaintState.Viewport;
        PaintState.Viewport = nullptr;
        return false;
    }

    // 3. 뷰포트 클라이언트 생성
    FWeightPaintViewportClient* Client = new FWeightPaintViewportClient();
    Client->SetWorld(PreviewWorld);
    Client->SetViewportType(EViewportType::Perspective);
    Client->SetViewMode(EViewMode::VMI_Lit_Phong);
    Client->SetPaintState(&PaintState);
    PaintState.Client = Client;
    PaintState.Viewport->SetViewportClient(Client);

    // 카메라 설정 (Week13 PAE 패턴: 정면에서 바라보기)
    ACameraActor* Camera = Client->GetCamera();
    Camera->SetActorLocation(FVector(200.0f, 0.0f, 100.0f));  // 충분히 떨어진 위치
    Camera->SetActorRotation(FVector(0.0f, -15.0f, 180.0f));  // 원점을 바라보도록
    Camera->SetCameraPitch(-15.0f);
    Camera->SetCameraYaw(180.0f);
    PreviewWorld->SetEditorCameraActor(Camera);

    // Directional Light 생성 (Week13 PAE 패턴: 조명이 있어야 메시가 보임)
    ADirectionalLightActor* DirLight = PreviewWorld->SpawnActor<ADirectionalLightActor>();
    if (DirLight)
    {
        DirLight->SetActorRotation(FVector(-30.0f, 180.0f, 0.0f));  // 위에서 비스듬하게
        UDirectionalLightComponent* LightComp = DirLight->GetLightComponent();
        if (LightComp)
        {
            LightComp->SetLightColor(FLinearColor(1.0f, 0.98f, 0.95f));  // 따뜻한 백색
            LightComp->SetIntensity(3.0f);
            LightComp->SetCastShadows(true);
        }
    }

    // 뷰포트 영역은 RenderViewportPanel에서 동적으로 계산됨
    ViewportRect = FRect(0, 0, 0, 0);

    bRequestFocus = true;

    return true;
}

FViewport* SWeightPaintEditorWindow::GetViewport() const
{
    return PaintState.Viewport;
}

FWeightPaintViewportClient* SWeightPaintEditorWindow::GetViewportClient() const
{
    return static_cast<FWeightPaintViewportClient*>(PaintState.Client);
}

void SWeightPaintEditorWindow::OnRender()
{
    if (!bIsOpen)
        return;

    ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar;

    // 초기 배치
    if (!bInitialPlacementDone)
    {
        ImGui::SetNextWindowPos(ImVec2(Rect.Left, Rect.Top), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(GetWidth(), GetHeight()), ImGuiCond_FirstUseEver);
        bInitialPlacementDone = true;
    }

    if (bRequestFocus)
    {
        ImGui::SetNextWindowFocus();
        bRequestFocus = false;
    }

    if (ImGui::Begin("Weight Paint Editor", &bIsOpen, WindowFlags))
    {
        // 메뉴바
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Load Mesh..."))
                {
                    // TODO: 파일 다이얼로그
                }
                if (ImGui::MenuItem("Save Weights"))
                {
                    SaveWeightsToAsset();
                }
                if (ImGui::MenuItem("Load Weights"))
                {
                    LoadWeightsFromAsset();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Close"))
                {
                    bIsOpen = false;
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Edit"))
            {
                if (ImGui::MenuItem("Reset to 1.0 (Free)"))
                {
                    ResetWeights(1.0f);
                }
                if (ImGui::MenuItem("Reset to 0.0 (Fixed)"))
                {
                    ResetWeights(0.0f);
                }
                if (ImGui::MenuItem("Apply to Cloth"))
                {
                    ApplyWeightsToCloth();
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View"))
            {
                ImGui::Checkbox("Show Weights", &PaintState.bShowWeightVisualization);
                ImGui::Checkbox("Show Mesh", &PaintState.bShowMesh);
                ImGui::Checkbox("Show Bones", &PaintState.bShowBones);
                ImGui::Checkbox("Show Wireframe", &PaintState.bShowWireframe);
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        // 창 크기 업데이트
        ImVec2 WindowPos = ImGui::GetWindowPos();
        ImVec2 WindowSize = ImGui::GetWindowSize();
        Rect.Left = WindowPos.x;
        Rect.Top = WindowPos.y;
        Rect.Right = WindowPos.x + WindowSize.x;
        Rect.Bottom = WindowPos.y + WindowSize.y;
        Rect.UpdateMinMax();

        // 컨텐츠 영역 시작
        ImVec2 ContentMin = ImGui::GetWindowContentRegionMin();
        ImVec2 ContentMax = ImGui::GetWindowContentRegionMax();
        float ContentWidth = ContentMax.x - ContentMin.x;
        float ContentHeight = ContentMax.y - ContentMin.y;

        // 왼쪽 패널 (브러시 설정 + 가중치 정보)
        ImGui::BeginChild("LeftPanel", ImVec2(LeftPanelWidth, ContentHeight), true);
        {
            RenderBrushPanel();
            ImGui::Separator();
            RenderWeightInfoPanel();
            ImGui::Separator();
            RenderMeshBrowserPanel();
        }
        ImGui::EndChild();

        ImGui::SameLine();

        // 오른쪽 영역 (뷰포트)
        float ViewportWidth = ContentWidth - LeftPanelWidth - 10.0f;
        float ViewportHeight = ContentHeight;

        // 오른쪽 뷰포트 영역 (RenderViewportPanel이 직접 BeginChild/EndChild 처리)
        RenderViewportPanel(ViewportWidth, ViewportHeight);
    }
    ImGui::End();
}

void SWeightPaintEditorWindow::RenderToolbar()
{
    // 간단한 툴바 (브러시 모드 선택)
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));

    const char* ModeNames[] = { "Paint", "Erase", "Smooth", "Fill" };
    int CurrentMode = static_cast<int>(PaintState.BrushMode);

    ImGui::Text("Mode:");
    ImGui::SameLine();
    for (int i = 0; i < 4; ++i)
    {
        if (i > 0) ImGui::SameLine();

        bool Selected = (CurrentMode == i);
        if (Selected)
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.6f, 1.0f, 1.0f));

        if (ImGui::Button(ModeNames[i], ImVec2(60, 25)))
        {
            PaintState.BrushMode = static_cast<EWeightPaintMode>(i);
        }

        if (Selected)
            ImGui::PopStyleColor();
    }

    ImGui::PopStyleVar();
}

void SWeightPaintEditorWindow::RenderBrushPanel()
{
    ImGui::Text("Brush Settings");
    ImGui::Separator();

    // 브러시 모드
    ImGui::Text("Mode:");
    const char* ModeNames[] = { "Paint", "Erase", "Smooth", "Fill" };
    int CurrentMode = static_cast<int>(PaintState.BrushMode);
    if (ImGui::Combo("##BrushMode", &CurrentMode, ModeNames, 4))
    {
        PaintState.BrushMode = static_cast<EWeightPaintMode>(CurrentMode);
    }

    ImGui::Spacing();

    // 브러시 반경
    ImGui::Text("Radius:");
    ImGui::SliderFloat("##Radius", &PaintState.BrushRadius, 0.5f, 50.0f, "%.1f cm");

    // 브러시 강도
    ImGui::Text("Strength:");
    ImGui::SliderFloat("##Strength", &PaintState.BrushStrength, 0.01f, 1.0f, "%.2f");

    // 브러시 폴오프
    ImGui::Text("Falloff:");
    ImGui::SliderFloat("##Falloff", &PaintState.BrushFalloff, 0.0f, 1.0f, "%.2f");
    ImGui::TextDisabled("0 = Hard edge, 1 = Soft");

    ImGui::Spacing();

    // 페인트 값 (Paint 모드에서만)
    if (PaintState.BrushMode == EWeightPaintMode::Paint || PaintState.BrushMode == EWeightPaintMode::Fill)
    {
        ImGui::Text("Paint Value:");
        ImGui::SliderFloat("##PaintValue", &PaintState.PaintValue, 0.0f, 1.0f, "%.2f");

        // 빠른 선택 버튼
        ImGui::Text("Quick Select:");
        if (ImGui::Button("0.0", ImVec2(50, 0)))
            PaintState.PaintValue = 0.0f;
        ImGui::SameLine();
        if (ImGui::Button("0.5", ImVec2(50, 0)))
            PaintState.PaintValue = 0.5f;
        ImGui::SameLine();
        if (ImGui::Button("1.0", ImVec2(50, 0)))
            PaintState.PaintValue = 1.0f;

        // 색상 미리보기
        FVector4 PreviewColor = FWeightPaintState::WeightToColor(PaintState.PaintValue);
        ImVec4 ImColor(PreviewColor.X, PreviewColor.Y, PreviewColor.Z, 1.0f);
        ImGui::ColorButton("##ValuePreview", ImColor, 0, ImVec2(ImGui::GetContentRegionAvail().x, 20));
    }
}

void SWeightPaintEditorWindow::RenderWeightInfoPanel()
{
    ImGui::Text("Weight Info");
    ImGui::Separator();

    // 가중치 통계
    size_t TotalVertices = PaintState.ClothVertexWeights.size();
    int FixedCount = 0;
    int FreeCount = 0;
    int PartialCount = 0;

    for (const auto& Pair : PaintState.ClothVertexWeights)
    {
        if (Pair.second < 0.01f)
            FixedCount++;
        else if (Pair.second > 0.99f)
            FreeCount++;
        else
            PartialCount++;
    }

    ImGui::Text("Painted Vertices: %zu", TotalVertices);
    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "  Fixed (0.0): %d", FixedCount);
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.3f, 1.0f), "  Partial: %d", PartialCount);
    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "  Free (1.0): %d", FreeCount);

    ImGui::Spacing();

    // 호버 중인 버텍스 정보
    if (PaintState.HoveredVertexIndex >= 0)
    {
        float Weight = PaintState.GetVertexWeight(static_cast<uint32>(PaintState.HoveredVertexIndex));
        ImGui::Text("Hovered Vertex: %d", PaintState.HoveredVertexIndex);
        ImGui::Text("  Weight: %.3f", Weight);

        FVector4 Color = FWeightPaintState::WeightToColor(Weight);
        ImVec4 ImColor(Color.X, Color.Y, Color.Z, 1.0f);
        ImGui::ColorButton("##HoverColor", ImColor, 0, ImVec2(50, 20));
    }
    else
    {
        ImGui::TextDisabled("Hover over a vertex...");
    }

    ImGui::Spacing();

    // 시각화 옵션
    ImGui::Text("Visualization:");
    ImGui::Checkbox("Show Weights", &PaintState.bShowWeightVisualization);
    ImGui::Checkbox("Show Wireframe", &PaintState.bShowWireframe);
}

void SWeightPaintEditorWindow::RenderMeshBrowserPanel()
{
    ImGui::Text("Mesh");
    ImGui::Separator();

    // ResourceManager에서 SkeletalMesh 목록 가져오기
    TArray<FString> MeshPaths = UResourceManager::GetInstance().GetAllFilePaths<USkeletalMesh>();

    // 현재 선택된 인덱스 찾기
    static int SelectedMeshIndex = -1;
    if (PaintState.CurrentMesh && SelectedMeshIndex < 0)
    {
        for (int i = 0; i < MeshPaths.Num(); ++i)
        {
            if (MeshPaths[i] == PaintState.LoadedMeshPath)
            {
                SelectedMeshIndex = i;
                break;
            }
        }
    }

    // 콤보박스로 메쉬 선택
    ImGui::Text("Select Mesh:");
    if (MeshPaths.Num() > 0)
    {
        // 현재 선택된 항목 이름
        const char* PreviewName = (SelectedMeshIndex >= 0 && SelectedMeshIndex < MeshPaths.Num())
            ? MeshPaths[SelectedMeshIndex].c_str()
            : "-- Select --";

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (ImGui::BeginCombo("##MeshCombo", PreviewName))
        {
            for (int i = 0; i < MeshPaths.Num(); ++i)
            {
                // 파일명만 표시 (경로에서 추출)
                FString DisplayName = MeshPaths[i];
                size_t LastSlash = DisplayName.find_last_of("/\\");
                if (LastSlash != FString::npos)
                {
                    DisplayName = DisplayName.substr(LastSlash + 1);
                }

                bool bIsSelected = (SelectedMeshIndex == i);
                if (ImGui::Selectable(DisplayName.c_str(), bIsSelected))
                {
                    SelectedMeshIndex = i;
                    LoadSkeletalMesh(MeshPaths[i]);
                }
                if (bIsSelected)
                {
                    ImGui::SetItemDefaultFocus();
                }

                // 툴팁으로 전체 경로 표시
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("%s", MeshPaths[i].c_str());
                }
            }
            ImGui::EndCombo();
        }
    }
    else
    {
        ImGui::TextDisabled("No SkeletalMesh loaded in ResourceManager");
        ImGui::TextDisabled("Load meshes first via Content Browser");
    }

    ImGui::Spacing();

    // 현재 로드된 메쉬 정보
    if (PaintState.CurrentMesh)
    {
        ImGui::Text("Current:");

        // 파일명만 표시
        FString DisplayPath = PaintState.LoadedMeshPath;
        size_t LastSlash = DisplayPath.find_last_of("/\\");
        if (LastSlash != FString::npos)
        {
            DisplayPath = DisplayPath.substr(LastSlash + 1);
        }
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "%s", DisplayPath.c_str());

        if (ImGui::Button("Unload", ImVec2(-1, 0)))
        {
            UnloadMesh();
            SelectedMeshIndex = -1;
        }
    }
    else
    {
        ImGui::TextDisabled("No mesh loaded");
    }
}

void SWeightPaintEditorWindow::RenderViewportPanel(float Width, float Height)
{
    // ParticleViewer 패턴: 뷰포트 컨테이너
    ImGui::BeginChild("ViewportContainer", ImVec2(Width, Height), false);
    {
        // 뷰포트 렌더링 영역
        ImGui::BeginChild("Viewport", ImVec2(0, 0), true, ImGuiWindowFlags_NoScrollbar);
        {
            // 뷰포트에서 마우스 입력을 허용하도록 설정
            if (ImGui::IsWindowHovered())
            {
                ImGui::GetIO().WantCaptureMouse = false;
            }

            // 뷰포트 영역 계산 (전체 Child 윈도우 영역)
            ImVec2 childPos = ImGui::GetWindowPos();
            ImVec2 childSize = ImGui::GetWindowSize();
            ViewportRect.Left = childPos.x;
            ViewportRect.Top = childPos.y;
            ViewportRect.Right = childPos.x + childSize.x;
            ViewportRect.Bottom = childPos.y + childSize.y;
            ViewportRect.UpdateMinMax();

            // 메시가 없을 때 가이드 UI 표시
            if (!PaintState.CurrentMesh)
            {
                ImDrawList* DrawList = ImGui::GetWindowDrawList();
                ImVec2 ViewportPos = childPos;
                ImVec2 ViewportSize = childSize;

                // 배경 채우기
                DrawList->AddRectFilled(
                    ViewportPos,
                    ImVec2(ViewportPos.x + ViewportSize.x, ViewportPos.y + ViewportSize.y),
                    IM_COL32(40, 40, 40, 255)
                );

                // 중앙 가이드 박스
                float BoxWidth = 350.0f;
                float BoxHeight = 120.0f;
                float BoxX = ViewportPos.x + (ViewportSize.x - BoxWidth) * 0.5f;
                float BoxY = ViewportPos.y + (ViewportSize.y - BoxHeight) * 0.5f;

                // 박스 배경
                DrawList->AddRectFilled(
                    ImVec2(BoxX, BoxY),
                    ImVec2(BoxX + BoxWidth, BoxY + BoxHeight),
                    IM_COL32(60, 60, 60, 200),
                    8.0f
                );

                // 박스 테두리
                DrawList->AddRect(
                    ImVec2(BoxX, BoxY),
                    ImVec2(BoxX + BoxWidth, BoxY + BoxHeight),
                    IM_COL32(100, 100, 100, 255),
                    8.0f,
                    0,
                    2.0f
                );

                // 텍스트
                const char* MainText = "Load a SkeletalMesh to begin painting";
                const char* SubText = "Use the Mesh panel on the left";
                ImVec2 MainTextSize = ImGui::CalcTextSize(MainText);
                ImVec2 SubTextSize = ImGui::CalcTextSize(SubText);

                // 메인 텍스트 (흰색)
                DrawList->AddText(
                    ImVec2(BoxX + (BoxWidth - MainTextSize.x) * 0.5f, BoxY + 35.0f),
                    IM_COL32(220, 220, 220, 255),
                    MainText
                );

                // 서브 텍스트 (회색)
                DrawList->AddText(
                    ImVec2(BoxX + (BoxWidth - SubTextSize.x) * 0.5f, BoxY + 70.0f),
                    IM_COL32(150, 150, 150, 255),
                    SubText
                );
            }
        }
        ImGui::EndChild();
    }
    ImGui::EndChild();
}

void SWeightPaintEditorWindow::OnUpdate(float DeltaSeconds)
{
    if (!bIsOpen)
        return;

    if (PaintState.Client)
    {
        PaintState.Client->Tick(DeltaSeconds);
    }
}

void SWeightPaintEditorWindow::OnMouseMove(FVector2D MousePos)
{
    if (!bIsOpen)
        return;

    // 뷰포트 영역 내 마우스인지 확인
    if (ViewportRect.Contains(MousePos) && PaintState.Viewport)
    {
        int32 LocalX = static_cast<int32>(MousePos.X - ViewportRect.Left);
        int32 LocalY = static_cast<int32>(MousePos.Y - ViewportRect.Top);
        PaintState.Viewport->ProcessMouseMove(LocalX, LocalY);
    }
}

void SWeightPaintEditorWindow::OnMouseDown(FVector2D MousePos, uint32 Button)
{
    if (!bIsOpen)
        return;

    if (ViewportRect.Contains(MousePos) && PaintState.Viewport)
    {
        int32 LocalX = static_cast<int32>(MousePos.X - ViewportRect.Left);
        int32 LocalY = static_cast<int32>(MousePos.Y - ViewportRect.Top);
        PaintState.Viewport->ProcessMouseButtonDown(LocalX, LocalY, Button);
    }
}

void SWeightPaintEditorWindow::OnMouseUp(FVector2D MousePos, uint32 Button)
{
    if (!bIsOpen)
        return;

    if (PaintState.Viewport)
    {
        int32 LocalX = static_cast<int32>(MousePos.X - ViewportRect.Left);
        int32 LocalY = static_cast<int32>(MousePos.Y - ViewportRect.Top);
        PaintState.Viewport->ProcessMouseButtonUp(LocalX, LocalY, Button);
    }
}

void SWeightPaintEditorWindow::LoadSkeletalMesh(const FString& Path)
{
    if (Path.empty())
        return;

    // 기존 메쉬 언로드
    UnloadMesh();

    // 새 메쉬 로드 (ResourceManager에서)
    USkeletalMesh* Mesh = UResourceManager::GetInstance().Load<USkeletalMesh>(Path);
    if (!Mesh)
    {
        UE_LOG("[WeightPaintEditor] Failed to load mesh: %s", Path.c_str());
        return;
    }

    PaintState.CurrentMesh = Mesh;
    PaintState.LoadedMeshPath = Path;

    UE_LOG("[WeightPaintEditor] Loading mesh: %s", Path.c_str());

    // 프리뷰 액터 생성
    if (PreviewWorld)
    {
        PaintState.PreviewActor = PreviewWorld->SpawnActor<ASkeletalMeshActor>(FTransform());
        if (PaintState.PreviewActor)
        {
            // Mundi 구조: SkeletalMeshComponent가 메시 렌더링 담당
            USkeletalMeshComponent* SkelComp = PaintState.PreviewActor->GetSkeletalMeshComponent();
            if (SkelComp)
            {
                SkelComp->SetSkeletalMesh(Path);
                SkelComp->SetVisibility(true);  // 항상 표시

                UE_LOG("[WeightPaintEditor] SkeletalMeshComponent set with mesh: %s", Path.c_str());

                // 메시 바운드 기준으로 카메라 위치 조정
                if (PaintState.Client)
                {
                    ACameraActor* Camera = PaintState.Client->GetCamera();
                    if (Camera && Mesh->GetSkeletalMeshData())
                    {
                        // 메시 크기에 맞게 카메라 거리 조정
                        FAABB Bounds = SkelComp->GetWorldAABB();
                        FVector Center = Bounds.GetCenter();
                        FVector HalfExtent = Bounds.GetHalfExtent();
                        float MaxExtent = std::max({ HalfExtent.X, HalfExtent.Y, HalfExtent.Z });

                        if (MaxExtent < 0.001f) MaxExtent = 50.0f;

                        float Distance = MaxExtent * 3.0f;
                        Camera->SetActorLocation(FVector(Distance, 0.0f, Center.Z + MaxExtent * 0.5f));
                        Camera->SetActorRotation(FVector(0.0f, -15.0f, 180.0f));
                        Camera->SetCameraPitch(-15.0f);
                        Camera->SetCameraYaw(180.0f);

                        UE_LOG("[WeightPaintEditor] Camera adjusted: distance=%.1f, center=(%.1f, %.1f, %.1f)",
                               Distance, Center.X, Center.Y, Center.Z);
                    }
                }
            }

            // Mundi 구조: ClothComponent는 Weight 데이터 관리용 (렌더링 X, 시뮬레이션 비활성화)
            // Weight Paint에서는 ClothComponent의 Weight 데이터만 사용
            UClothComponent* ClothComp = Cast<UClothComponent>(
                PaintState.PreviewActor->AddNewComponent(UClothComponent::StaticClass()));
            if (ClothComp)
            {
                ClothComp->SetSkeletalMesh(Path);
                ClothComp->SetVisibility(false);  // 렌더링은 SkeletalMeshComponent가 담당

                // Weight 데이터 초기화 (시뮬레이션 없이)
                // ClothComponent에서 버텍스 정보만 가져옴
                ClothComp->InitializeComponent();

                int32 VertexCount = ClothComp->GetClothVertexCount();
                UE_LOG("[WeightPaintEditor] ClothComponent initialized: %d vertices", VertexCount);

                if (VertexCount > 0)
                {
                    ClothComp->InitializeVertexWeights(1.0f);
                }

                PaintState.ClothComponent = ClothComp;
            }
        }
    }

    // WeightPaintState 가중치 초기화
    PaintState.ResetAllWeights(1.0f);

    UE_LOG("[WeightPaintEditor] Mesh loaded successfully");
}

void SWeightPaintEditorWindow::UnloadMesh()
{
    // ClothComponent 참조 정리 (Actor 삭제 전에)
    PaintState.ClothComponent = nullptr;

    if (PaintState.PreviewActor && PreviewWorld)
    {
        PreviewWorld->AddPendingKillActor(PaintState.PreviewActor);
        PaintState.PreviewActor = nullptr;
    }

    if (PaintState.CurrentMesh && !PaintState.LoadedMeshPath.empty())
    {
        // ResourceManager가 리소스 캐시를 관리하므로 참조만 해제
        PaintState.CurrentMesh = nullptr;
        PaintState.LoadedMeshPath.clear();
    }

    PaintState.ClothVertexWeights.clear();
}

void SWeightPaintEditorWindow::ApplyWeightsToCloth()
{
    UClothComponent* ClothComp = PaintState.ClothComponent;
    if (!ClothComp)
    {
        UE_LOG("[WeightPaintEditor] No ClothComponent to apply weights");
        return;
    }

    // WeightPaintState의 가중치를 ClothComponent에 로드
    ClothComp->LoadWeightsFromMap(PaintState.ClothVertexWeights);

    // Weight 시각화 업데이트
    ClothComp->UpdateVertexColorsFromWeights();

    UE_LOG("[WeightPaintEditor] Applied weights to ClothComponent (%zu vertices)",
           PaintState.ClothVertexWeights.size());
}

void SWeightPaintEditorWindow::ResetWeights(float DefaultValue)
{
    PaintState.ResetAllWeights(DefaultValue);

    // ClothComponent에도 적용하고 시각화 업데이트
    if (PaintState.ClothComponent)
    {
        PaintState.ClothComponent->InitializeVertexWeights(DefaultValue);
        PaintState.ClothComponent->UpdateVertexColorsFromWeights();
    }
}

void SWeightPaintEditorWindow::SaveWeightsToAsset()
{
    if (PaintState.LoadedMeshPath.empty())
    {
        UE_LOG("[WeightPaintEditor] No mesh loaded, cannot save weights");
        return;
    }

    if (PaintState.ClothVertexWeights.empty())
    {
        UE_LOG("[WeightPaintEditor] No weights to save");
        return;
    }

    // 에셋 경로 생성 (메시 경로에서 파일명 추출하여 .clothweight 확장자 사용)
    FString MeshPath = PaintState.LoadedMeshPath;

    // 확장자 제거
    size_t DotPos = MeshPath.find_last_of('.');
    FString BasePath = (DotPos != FString::npos) ? MeshPath.substr(0, DotPos) : MeshPath;

    // .clothweight 확장자 추가
    FString WeightAssetPath = BasePath + ".clothweight";

    // ClothWeightAsset 생성 및 저장
    UClothWeightAsset* WeightAsset = NewObject<UClothWeightAsset>();
    if (!WeightAsset)
    {
        UE_LOG("[WeightPaintEditor] Failed to create ClothWeightAsset");
        return;
    }

    // 에셋 이름 설정 (파일명에서 추출)
    FString FileName = BasePath;
    size_t LastSlash = FileName.find_last_of("/\\");
    if (LastSlash != FString::npos)
    {
        FileName = FileName.substr(LastSlash + 1);
    }
    WeightAsset->SetAssetName(FName(FileName));
    WeightAsset->SetSkeletalMeshPath(PaintState.LoadedMeshPath);

    // 가중치 데이터 복사
    WeightAsset->LoadFromMap(PaintState.ClothVertexWeights);

    // 파일로 저장
    if (WeightAsset->SaveToFile(WeightAssetPath))
    {
        UE_LOG("[WeightPaintEditor] Saved weights to: %s (%zu vertices)",
               WeightAssetPath.c_str(), PaintState.ClothVertexWeights.size());
    }
    else
    {
        UE_LOG("[WeightPaintEditor] Failed to save weights to: %s", WeightAssetPath.c_str());
    }

    // 임시 에셋 삭제
    ObjectFactory::DeleteObject(WeightAsset);
}

void SWeightPaintEditorWindow::LoadWeightsFromAsset()
{
    if (PaintState.LoadedMeshPath.empty())
    {
        UE_LOG("[WeightPaintEditor] No mesh loaded, cannot load weights");
        return;
    }

    // 에셋 경로 생성 (메시 경로에서 확장자를 .clothweight로 변경)
    FString MeshPath = PaintState.LoadedMeshPath;

    // 확장자 제거
    size_t DotPos = MeshPath.find_last_of('.');
    FString BasePath = (DotPos != FString::npos) ? MeshPath.substr(0, DotPos) : MeshPath;

    // .clothweight 확장자 추가
    FString WeightAssetPath = BasePath + ".clothweight";

    // 파일 존재 확인
    if (!std::filesystem::exists(WeightAssetPath))
    {
        UE_LOG("[WeightPaintEditor] Weight asset not found: %s", WeightAssetPath.c_str());
        return;
    }

    // ClothWeightAsset 로드
    UClothWeightAsset* WeightAsset = NewObject<UClothWeightAsset>();
    if (!WeightAsset)
    {
        UE_LOG("[WeightPaintEditor] Failed to create ClothWeightAsset for loading");
        return;
    }

    if (WeightAsset->Load(WeightAssetPath))
    {
        // 가중치 데이터 복사
        const auto& LoadedWeights = WeightAsset->GetVertexWeights();
        PaintState.ClothVertexWeights.clear();
        for (const auto& Pair : LoadedWeights)
        {
            PaintState.ClothVertexWeights[Pair.first] = Pair.second;
        }

        // ClothComponent에도 적용
        if (PaintState.ClothComponent)
        {
            PaintState.ClothComponent->LoadWeightsFromMap(PaintState.ClothVertexWeights);
            PaintState.ClothComponent->UpdateVertexColorsFromWeights();
        }

        UE_LOG("[WeightPaintEditor] Loaded weights from: %s (%d vertices)",
               WeightAssetPath.c_str(), WeightAsset->GetVertexCount());
    }
    else
    {
        UE_LOG("[WeightPaintEditor] Failed to load weights from: %s", WeightAssetPath.c_str());
    }

    // 임시 에셋 삭제
    ObjectFactory::DeleteObject(WeightAsset);
}

void SWeightPaintEditorWindow::OnRenderViewport()
{
    // ParticleViewer 패턴: ImGui 렌더 전에 뷰포트 렌더링
    if (!bIsOpen || !PaintState.CurrentMesh)
        return;

    if (PaintState.Viewport && ViewportRect.GetWidth() > 0 && ViewportRect.GetHeight() > 0)
    {
        const uint32 NewStartX = static_cast<uint32>(ViewportRect.Left);
        const uint32 NewStartY = static_cast<uint32>(ViewportRect.Top);
        const uint32 NewWidth = static_cast<uint32>(ViewportRect.Right - ViewportRect.Left);
        const uint32 NewHeight = static_cast<uint32>(ViewportRect.Bottom - ViewportRect.Top);

        PaintState.Viewport->Resize(NewStartX, NewStartY, NewWidth, NewHeight);

        // 뷰포트 렌더링 (ImGui보다 먼저)
        PaintState.Viewport->Render();
    }
}

