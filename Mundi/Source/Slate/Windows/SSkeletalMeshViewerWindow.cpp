#include "pch.h"
#include "SSkeletalMeshViewerWindow.h"
#include "FViewport.h"
#include "FViewportClient.h"
#include "Source/Runtime/Engine/SkeletalViewer/SkeletalViewerBootstrap.h"
#include "Source/Editor/PlatformProcess.h"
#include "Source/Runtime/Engine/GameFramework/SkeletalMeshActor.h"
#include "Source/Runtime/Engine/GameFramework/StaticMeshActor.h"
#include "Source/Runtime/Engine/Components/SkeletalMeshComponent.h"
#include "Source/Runtime/Engine/Components/StaticMeshComponent.h"
#include "Source/Runtime/Engine/Components/LineComponent.h"
#include "SelectionManager.h"
#include "USlateManager.h"
#include "BoneAnchorComponent.h"
#include "SkinningStats.h"
#include "Source/Runtime/Engine/Animation/AnimSequence.h"
#include "Source/Runtime/Engine/Animation/AnimMontage.h"
#include "Source/Runtime/Engine/Animation/AnimInstance.h"
#include "Source/Runtime/Engine/Animation/AnimTypes.h"
#include "Source/Runtime/Engine/Collision/Picking.h"
#include "Source/Runtime/Engine/Animation/AnimNotify_PlaySound.h"
#include "Source/Runtime/AssetManagement/ResourceManager.h"
#include "Source/Editor/PlatformProcess.h"
#include "Source/Runtime/Core/Misc/PathUtils.h"
#include <filesystem>
#include <unordered_set>
#include <limits>
#include <cstdlib>
#include "Source/Runtime/Engine/GameFramework/CameraActor.h"
#include "Source/Runtime/Engine/Physics/PhysicsAsset.h"
#include "Source/Runtime/Engine/Physics/BodySetup.h"
#include "Source/Runtime/Engine/Physics/PhysicalMaterial.h"
#include <cstring>
#include "RenderManager.h"
#include "WindowsBinWriter.h"
#include "Source/Runtime/Engine/Animation/AnimNotify_CallFunction.h"
#include "Source/Runtime/Engine/Animation/AnimNotify_ParticleStart.h"
#include "Source/Runtime/Engine/Animation/AnimNotify_ParticleEnd.h"
namespace
{
    using FBoneNameSet = std::unordered_set<FName>;

    FBoneNameSet GatherPhysicsAssetBones(const UPhysicsAsset* PhysicsAsset)
    {
        FBoneNameSet Result;
        if (!PhysicsAsset)
        {
            return Result;
        }

        for (UBodySetup* Body : PhysicsAsset->BodySetups)
        {
            if (!Body)
            {
                continue;
            }

            if (Body->BoneName.IsValid())
            {
                Result.insert(Body->BoneName);
            }
        }

        return Result;
    }

    bool SkeletonSupportsBones(const FSkeleton* Skeleton, const FBoneNameSet& RequiredBones)
    {
        if (!Skeleton || RequiredBones.empty())
        {
            return false;
        }

        for (const FName& BoneName : RequiredBones)
        {
            if (!BoneName.IsValid())
            {
                continue;
            }

            if (Skeleton->FindBoneIndex(BoneName) == INDEX_NONE)
            {
                return false;
            }
        }

        return true;
    }

    USkeletalMesh* FindCompatibleMesh(const FBoneNameSet& RequiredBones)
    {
        if (RequiredBones.empty())
        {
            return nullptr;
        }

        USkeletalMesh* BestMatch = nullptr;
        int32 BestScore = std::numeric_limits<int32>::max();

        TArray<USkeletalMesh*> AllMeshes = UResourceManager::GetInstance().GetAll<USkeletalMesh>();
        for (USkeletalMesh* Mesh : AllMeshes)
        {
            if (!Mesh)
            {
                continue;
            }

            const FSkeleton* Skeleton = Mesh->GetSkeleton();
            if (!SkeletonSupportsBones(Skeleton, RequiredBones))
            {
                continue;
            }

            const int32 BoneDifference = std::abs(Skeleton->GetNumBones() - static_cast<int32>(RequiredBones.size()));
            if (!BestMatch || BoneDifference < BestScore)
            {
                BestMatch = Mesh;
                BestScore = BoneDifference;
            }
        }

        return BestMatch;
    }
}
SSkeletalMeshViewerWindow::SSkeletalMeshViewerWindow()
{
    PhysicsAssetPath = std::filesystem::path(GDataDir) / "Physics";

    CenterRect = FRect(0, 0, 0, 0);
    
    IconFirstFrame = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/PlayControlsToFront.png");
    IconLastFrame = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/PlayControlsToEnd.png");
    IconPrevFrame = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/PlayControlsToPrevious.png");
    IconNextFrame = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/PlayControlsToNext.png");
    IconPlay = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/PlayControlsPlayForward.png");
    IconReversePlay = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/PlayControlsPlayReverse.png");
    IconPause = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/PlayControlsPause.png");
    IconRecord = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/PlayControlsRecord.png");
    IconRecordActive = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/PlayControlsRecord.png");
    IconLoop = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/PlayControlsLooping.png");
    IconNoLoop = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/PlayControlsNoLooping.png");

    // Initialize physics constraint graph editor
    ed::Config PhysicsGraphConfig;
    PhysicsGraphContext = ed::CreateEditor(&PhysicsGraphConfig);

    PhysicsNodeHeaderBg = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/BlueprintBackground.png");
    int32 TexWidth = PhysicsNodeHeaderBg ? PhysicsNodeHeaderBg->GetWidth() : 0;
    int32 TexHeight = PhysicsNodeHeaderBg ? PhysicsNodeHeaderBg->GetHeight() : 0;
    PhysicsGraphBuilder = new util::BlueprintNodeBuilder(
        reinterpret_cast<ImTextureID>(PhysicsNodeHeaderBg ? PhysicsNodeHeaderBg->GetShaderResourceView() : nullptr),
        TexWidth,
        TexHeight
    );
}

SSkeletalMeshViewerWindow::~SSkeletalMeshViewerWindow()
{
    // Clean up physics graph resources
    if (PhysicsGraphBuilder)
    {
        delete PhysicsGraphBuilder;
        PhysicsGraphBuilder = nullptr;
    }

    if (PhysicsGraphContext)
    {
        ed::DestroyEditor(PhysicsGraphContext);
        PhysicsGraphContext = nullptr;
    }

    // 닫을 때, Notifies를 저장 
    SaveAllNotifiesOnClose();

    // Clean up tabs if any
    for (int i = 0; i < Tabs.Num(); ++i)
    {
        ViewerState* State = Tabs[i];
        SkeletalViewerBootstrap::DestroyViewerState(State);
    }
    Tabs.Empty();
    ActiveState = nullptr;
}

// Compose default meta path under Data for an animation
static FString MakeDefaultMetaPath(const UAnimSequenceBase* Anim)
{
    FString BaseDir = GDataDir; // e.g., "Data"
    FString FileName = "AnimNotifies.anim.json";
    if (Anim)
    {
        const FString Src = Anim->GetFilePath();
        if (!Src.empty())
        {
            // Extract file name without extension
            size_t pos = Src.find_last_of("/\\");
            FString Just = (pos == FString::npos) ? Src : Src.substr(pos + 1);
            size_t dot = Just.find_last_of('.') ;
            if (dot != FString::npos) Just = Just.substr(0, dot);
            FileName = Just + ".anim.json";
        }
    }
    return NormalizePath(BaseDir + "/" + FileName);
}

void SSkeletalMeshViewerWindow::SaveAllNotifiesOnClose()
{
    for (int i = 0; i < Tabs.Num(); ++i)
    {
        ViewerState* State = Tabs[i];
        if (!State) continue;

        // 애니메이션 노티파이 저장
        if (State->CurrentAnimation)
        {
            const FString OutPath = MakeDefaultMetaPath(State->CurrentAnimation);
            State->CurrentAnimation->SaveMeta(OutPath);
        }

        // 몽타주 노티파이 저장
        if (State->CurrentMontage)
        {
            const FString OutPath = MakeDefaultMetaPath(State->CurrentMontage);
            State->CurrentMontage->SaveMeta(OutPath);
        }
        if (State->EditingMontage)
        {
            const FString OutPath = MakeDefaultMetaPath(State->EditingMontage);
            State->EditingMontage->SaveMeta(OutPath);
        }
    }
}

bool SSkeletalMeshViewerWindow::Initialize(float StartX, float StartY, float Width, float Height, UWorld* InWorld, ID3D11Device* InDevice)
{
    World = InWorld;
    Device = InDevice;
    
    SetRect(StartX, StartY, StartX + Width, StartY + Height);

    // Create first tab/state
    OpenNewTab("Viewer 1");
    if (ActiveState && ActiveState->Viewport)
    {
        ActiveState->Viewport->Resize((uint32)StartX, (uint32)StartY, (uint32)Width, (uint32)Height);
    }

    bRequestFocus = true;
    return true;
}

void SSkeletalMeshViewerWindow::OnRender()
{
    // If window is closed, don't render
    if (!bIsOpen)
    {
        if (!bSavedOnClose)
        {
            SaveAllNotifiesOnClose();
            bSavedOnClose = true;
        }
        return;
    }

    // Parent detachable window (movable, top-level) with solid background
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoSavedSettings;

    if (!bInitialPlacementDone)
    {
        ImGui::SetNextWindowPos(ImVec2(Rect.Left, Rect.Top));
        ImGui::SetNextWindowSize(ImVec2(Rect.GetWidth(), Rect.GetHeight()));
        bInitialPlacementDone = true;
    }

    if (bRequestFocus)
    {
        ImGui::SetNextWindowFocus();
    }
    bool bViewerVisible = false;
    if (ImGui::Begin("Skeletal Mesh Viewer", &bIsOpen, flags))
    {
        bViewerVisible = true;
        // Render tab bar and switch active state
        if (ImGui::BeginTabBar("SkeletalViewerTabs", ImGuiTabBarFlags_AutoSelectNewTabs | ImGuiTabBarFlags_Reorderable))
        {
            for (int i = 0; i < Tabs.Num(); ++i)
            {
                ViewerState* State = Tabs[i];
                bool open = true;
                if (ImGui::BeginTabItem(State->Name.ToString().c_str(), &open))
                {
                    ActiveTabIndex = i;
                    ActiveState = State;
                    ImGui::EndTabItem();
                }
                if (!open)
                {
                    CloseTab(i);
                    break;
                }
            }
            if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing))
            {
                char label[32]; sprintf_s(label, "Viewer %d", Tabs.Num() + 1);
                OpenNewTab(label);
            }
            ImGui::EndTabBar();
        }

        // ===== 래그돌 시뮬레이션 토글 버튼 (상단 툴바) =====
        if (ActiveState)
        {
            ImGui::Spacing();

            // 토글 버튼 스타일 설정
            if (ActiveState->bSimulatePhysics)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));        // 활성: 녹색
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.5f, 0.1f, 1.0f));
            }
            else
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.2f, 0.2f, 1.0f));        // 비활성: 빨강
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.3f, 0.3f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.1f, 0.1f, 1.0f));
            }

            const char* buttonLabel = ActiveState->bSimulatePhysics ? "Simulate: ON" : "Simulate: OFF";
            if (ImGui::Button(buttonLabel, ImVec2(120, 0)))
            {
                // PhysicsAsset이 있는지 확인
                USkeletalMeshComponent* SkelComp = ActiveState->PreviewActor ?
                    ActiveState->PreviewActor->GetSkeletalMeshComponent() : nullptr;

                bool bHasPhysicsAsset = SkelComp && SkelComp->GetPhysicsAsset();
                bool bHasPhysicsBodies = SkelComp && SkelComp->GetNumBodies() > 0;

                if (!bHasPhysicsAsset)
                {
                    UE_LOG("[Viewer] Cannot enable simulation: No PhysicsAsset loaded!");
                }
                else if (!bHasPhysicsBodies)
                {
                    // PhysicsAsset은 있지만 Body가 생성되지 않은 경우 - 다시 생성 시도
                    UE_LOG("[Viewer] PhysicsAsset exists but no bodies. Re-applying...");
                    SkelComp->SetPhysicsAsset(SkelComp->GetPhysicsAsset());
                    bHasPhysicsBodies = SkelComp->GetNumBodies() > 0;
                }

                if (bHasPhysicsAsset && bHasPhysicsBodies)
                {
                    ActiveState->bSimulatePhysics = !ActiveState->bSimulatePhysics;

                    // PhysicsState 전환 (SkeletalMeshComponent)
                    if (SkelComp)
                    {
                        if (ActiveState->bSimulatePhysics)
                        {
                            SkelComp->SetPhysicsAnimationState(EPhysicsAnimationState::PhysicsDriven);
                            UE_LOG("[Viewer] Ragdoll simulation enabled (Bodies: %d)", SkelComp->GetNumBodies());
                        }
                        else
                        {
                            SkelComp->SetPhysicsAnimationState(EPhysicsAnimationState::AnimationDriven);
                            SkelComp->ResetToBindPose();  // 원래 포즈로 리셋
                            UE_LOG("[Viewer] Ragdoll simulation disabled");
                        }
                    }
                }
            }
            ImGui::PopStyleColor(3);

            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Toggle physics simulation for ragdoll.\nWhen ON, the skeleton will be driven by physics.\nWhen OFF, the skeleton follows animation.");
            }

            ImGui::Separator();
        }

        ImVec2 pos = ImGui::GetWindowPos();
        ImVec2 size = ImGui::GetWindowSize();
        Rect.Left = pos.x; Rect.Top = pos.y; Rect.Right = pos.x + size.x; Rect.Bottom = pos.y + size.y; Rect.UpdateMinMax();

        ImVec2 contentAvail = ImGui::GetContentRegionAvail();
        float totalWidth = contentAvail.x;
        float totalHeight = contentAvail.y;

        float leftWidth = totalWidth * LeftPanelRatio;
        float rightWidth = totalWidth * RightPanelRatio;
        float centerWidth = totalWidth - leftWidth - rightWidth;

        // 상단 패널(뷰포트, 속성)과 하단 패널(타임라인, 브라우저)의 높이를 미리 계산합니다.
        const float BottomPanelHeight = totalHeight * BottomPanelRatio;
        float TopPanelHeight = totalHeight - BottomPanelHeight;

        // 패널 사이의 간격 조정
        if (ImGui::GetStyle().ItemSpacing.y > 0)
        {
            TopPanelHeight -= ImGui::GetStyle().ItemSpacing.y;
        }
        // 최소 높이 보장
        TopPanelHeight = std::max(TopPanelHeight, 50.0f);

        // Remove spacing between panels
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

        // Left panel - Asset Browser & Bone Hierarchy
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
        ImGui::BeginChild("LeftPanel", ImVec2(leftWidth, totalHeight), true, ImGuiWindowFlags_NoScrollbar);
        ImGui::PopStyleVar();

        if (ActiveState)
        {
            // Asset Browser Section
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.25f, 0.35f, 0.50f, 0.8f));
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
            ImGui::Indent(8.0f);
            ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
            ImGui::Text("Asset Browser");
            ImGui::PopFont();
            ImGui::Unindent(8.0f);
            ImGui::PopStyleColor();

            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.35f, 0.45f, 0.60f, 0.7f));
            ImGui::Separator();
            ImGui::PopStyleColor();
            ImGui::Spacing();

            // Mesh path section
            ImGui::BeginGroup();
            ImGui::Text("Mesh Path:");
            ImGui::PushItemWidth(-1.0f);
            ImGui::InputTextWithHint("##MeshPath", "Browse for FBX file...", ActiveState->MeshPathBuffer, sizeof(ActiveState->MeshPathBuffer));
            ImGui::PopItemWidth();

            ImGui::Spacing();

            // Buttons
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.28f, 0.40f, 0.55f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.50f, 0.70f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.25f, 0.35f, 0.50f, 1.0f));

            float buttonWidth = (leftWidth - 24.0f) * 0.5f - 4.0f;
            if (ImGui::Button("Browse...", ImVec2(buttonWidth, 32)))
            {
                auto widePath = FPlatformProcess::OpenLoadFileDialog(UTF8ToWide(GDataDir), L"fbx", L"FBX Files");
                if (!widePath.empty())
                {
                    FString s = WideToUTF8(widePath.wstring());
                    strncpy_s(ActiveState->MeshPathBuffer, s.c_str(), sizeof(ActiveState->MeshPathBuffer) - 1);
                }
            }

            ImGui::SameLine();

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.60f, 0.40f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.70f, 0.50f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.50f, 0.30f, 1.0f));
            if (ImGui::Button("Load FBX", ImVec2(buttonWidth, 32)))
            {
                FString Path = ActiveState->MeshPathBuffer;
                if (!Path.empty())
                {
                    USkeletalMesh* Mesh = UResourceManager::GetInstance().Load<USkeletalMesh>(Path);
                    if (Mesh && ActiveState->PreviewActor)
                    {
                        ActiveState->PreviewActor->SetSkeletalMesh(Path);
                        ActiveState->CurrentMesh = Mesh;
                        ActiveState->LoadedMeshPath = Path;  // Track for resource unloading
                        if (auto* Skeletal = ActiveState->PreviewActor->GetSkeletalMeshComponent())
                        {
                            Skeletal->SetVisibility(ActiveState->bShowMesh);
                        }
                        ActiveState->bBoneLinesDirty = true;
                        if (auto* LineComp = ActiveState->PreviewActor->GetBoneLineComponent())
                        {
                            LineComp->ClearLines();
                            LineComp->SetLineVisible(ActiveState->bShowBones);
                        }
                    }
                }
            }
            ImGui::PopStyleColor(6);
            ImGui::EndGroup();

            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.35f, 0.45f, 0.60f, 0.7f));
            ImGui::Separator();
            ImGui::PopStyleColor();
            ImGui::Spacing();

            // Display Options
            ImGui::BeginGroup();
            ImGui::Text("Display Options:");
            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.25f, 0.30f, 0.35f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.40f, 0.70f, 1.00f, 1.0f));

            if (ImGui::Checkbox("Show Mesh", &ActiveState->bShowMesh))
            {
                if (ActiveState->PreviewActor && ActiveState->PreviewActor->GetSkeletalMeshComponent())
                {
                    ActiveState->PreviewActor->GetSkeletalMeshComponent()->SetVisibility(ActiveState->bShowMesh);
                }
            }

            ImGui::SameLine();
            if (ImGui::Checkbox("Show Bones", &ActiveState->bShowBones))
            {
                if (ActiveState->PreviewActor && ActiveState->PreviewActor->GetBoneLineComponent())
                {
                    ActiveState->PreviewActor->GetBoneLineComponent()->SetLineVisible(ActiveState->bShowBones);
                }
                if (ActiveState->bShowBones)
                {
                    ActiveState->bBoneLinesDirty = true;
                }
            }

			/*ImGui::SameLine();*/
            if (ImGui::Checkbox("Show Bodies", &ActiveState->bShowBodies))
            {
                if (ActiveState->PreviewActor && ActiveState->PreviewActor->GetBodyLineComponent())
                {
                    ActiveState->PreviewActor->GetBodyLineComponent()->SetLineVisible(ActiveState->bShowBodies);
                }
            }

            /*ImGui::SameLine();*/
            if (ImGui::Checkbox("Show Constraints line", &ActiveState->bShowConstraintLines))
            {
                if (ActiveState->PreviewActor && ActiveState->PreviewActor->GetConstraintLineComponent())
                {
                    ActiveState->PreviewActor->GetConstraintLineComponent()->SetLineVisible(ActiveState->bShowConstraintLines);
                }
            }

            ImGui::SameLine();
            if (ImGui::Checkbox("Show Constraints limits", &ActiveState->bShowConstraintLimits))
            {
                if (ActiveState->PreviewActor && ActiveState->PreviewActor->GetConstraintLimitLineComponent())
                {
                    ActiveState->PreviewActor->GetConstraintLimitLineComponent()->SetLineVisible(ActiveState->bShowConstraintLimits);
                }
            }

            ImGui::PopStyleColor(2);
            ImGui::EndGroup();           

            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.35f, 0.45f, 0.60f, 0.7f));
            ImGui::Separator();
            ImGui::PopStyleColor();
            ImGui::Spacing();

            // Bone Hierarchy Section
            ImGui::Text("Bone Hierarchy:");
            ImGui::Spacing();

            if (!ActiveState->CurrentMesh)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
                ImGui::TextWrapped("No skeletal mesh loaded.");
                ImGui::PopStyleColor();
            }
            else
            {
                const FSkeleton* Skeleton = ActiveState->CurrentMesh->GetSkeleton();
                if (!Skeleton || Skeleton->Bones.IsEmpty())
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
                    ImGui::TextWrapped("This mesh has no skeleton data.");
                    ImGui::PopStyleColor();
                }
                else
                {
                    // Calculate height for bone tree view - reserve space for physics graph below
                    float availableHeight = ImGui::GetContentRegionAvail().y;
                    float reservedForGraph = 250.0f; // Reserve at least 250px for physics graph
                    float boneTreeHeight = availableHeight - reservedForGraph;

                    // Ensure minimum height for bone tree
                    boneTreeHeight = std::max(boneTreeHeight, 200.0f);

                    // Scrollable tree view
                    ImGui::BeginChild("BoneTreeView", ImVec2(0, boneTreeHeight), true);
                    const TArray<FBone>& Bones = Skeleton->Bones;
                    TArray<TArray<int32>> Children;
                    Children.resize(Bones.size());
                    for (int32 i = 0; i < Bones.size(); ++i)
                    {
                        int32 Parent = Bones[i].ParentIndex;
                        if (Parent >= 0 && Parent < Bones.size())
                        {
                            Children[Parent].Add(i);
                        }
                    }

                    // Track which bone was right-clicked to open context menu
                    int RightClickedBoneIndex = -1;

                    // Tree line drawing setup
                    ImDrawList* DrawList = ImGui::GetWindowDrawList();
                    const ImU32 LineColor = IM_COL32(90, 90, 90, 255);
                    const float IndentPerLevel = ImGui::GetStyle().IndentSpacing;

                    // Structure to store node position info for line drawing
                    struct NodePosInfo
                    {
                        float CenterY;
                        float BaseX;
                        int Depth;
                        int32 ParentIndex;
                        bool bIsLastChild;
                    };
                    std::vector<NodePosInfo> NodePositions;
                    NodePositions.resize(Bones.size());

                    std::function<void(int32, int, bool)> DrawNode = [&](int32 Index, int Depth, bool bIsLastChild)
                    {
                        const bool bLeaf = Children[Index].IsEmpty();
                        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth;

                        if (bLeaf)
                        {
                            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                        }

                        if (ActiveState->ExpandedBoneIndices.count(Index) > 0)
                        {
                            ImGui::SetNextItemOpen(true);
                        }

                        if (ActiveState->SelectedBoneIndex == Index)
                        {
                            flags |= ImGuiTreeNodeFlags_Selected;
                        }

                        ImGui::PushID(Index);
                        const char* Label = Bones[Index].Name.c_str();

                        // Get position before drawing tree node for line connections
                        ImVec2 CursorPos = ImGui::GetCursorScreenPos();
                        float ItemCenterY = CursorPos.y + ImGui::GetFrameHeight() * 0.5f;
                        float BaseX = CursorPos.x;

                        // Store position info for later line drawing
                        NodePositions[Index].CenterY = ItemCenterY;
                        NodePositions[Index].BaseX = BaseX;
                        NodePositions[Index].Depth = Depth;
                        NodePositions[Index].ParentIndex = Bones[Index].ParentIndex;
                        NodePositions[Index].bIsLastChild = bIsLastChild;

                        if (ActiveState->SelectedBoneIndex == Index)
                        {
                            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.35f, 0.55f, 0.85f, 0.8f));
                            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.40f, 0.60f, 0.90f, 1.0f));
                            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.30f, 0.50f, 0.80f, 1.0f));
                        }

                        bool open = ImGui::TreeNodeEx((void*)(intptr_t)Index, flags, "%s", Label ? Label : "<noname>");

                        if (ActiveState->SelectedBoneIndex == Index)
                        {
                            ImGui::PopStyleColor(3);
                        }

                        if (ImGui::IsItemToggledOpen())
                        {
                            if (open)
                                ActiveState->ExpandedBoneIndices.insert(Index);
                            else
                                ActiveState->ExpandedBoneIndices.erase(Index);
                        }

                        // 본 우클릭 이벤트
                        if (ImGui::BeginPopupContextItem("BoneContextMenu"))
                        {
                            if (Index >= 0 && Index < Bones.size())
                            {
                                const char* ClickedBoneName = Bones[Index].Name.c_str();
                                UPhysicsAsset* Phys = ActiveState->CurrentPhysicsAsset;
                                if (Phys && ImGui::MenuItem("Add Body"))
                                {
                                    UBodySetup* NewBody = NewObject<UBodySetup>();
                                    if (NewBody)
                                    {
                                        NewBody->BoneName = FName(ClickedBoneName);
                                        Phys->BodySetups.Add(NewBody);
                                        Phys->BuildBodySetupIndexMap();

                                        // Set newly created body as selected so right panel shows its details
                                        ActiveState->SelectedBodySetup = NewBody;
                                        ActiveState->SelectedPreviewMesh = nullptr;
                                        UE_LOG("Added UBodySetup for bone %s to PhysicsAsset %s", ClickedBoneName, Phys->GetName().ToString().c_str());
                                    }
                                }

                                // 소켓 추가 메뉴
                                if (ImGui::MenuItem("Add Socket"))
                                {
                                    FSkeleton* MutableSkeleton = const_cast<FSkeleton*>(Skeleton);

                                    // 고유한 소켓 이름 생성
                                    FString NewSocketName = FString(ClickedBoneName) + "_Socket";
                                    int32 Counter = 1;
                                    while (MutableSkeleton->FindSocket(FName(NewSocketName)) != nullptr)
                                    {
                                        NewSocketName = FString(ClickedBoneName) + "_Socket" + std::to_string(Counter);
                                        Counter++;
                                    }

                                    FSkeletalMeshSocket NewSocket;
                                    NewSocket.SocketName = NewSocketName;
                                    NewSocket.BoneName = ClickedBoneName;
                                    NewSocket.RelativeLocation = FVector::Zero();
                                    NewSocket.RelativeRotation = FQuat::Identity();
                                    NewSocket.RelativeScale = FVector::One();

                                    MutableSkeleton->AddSocket(NewSocket);

                                    // 새로 생성된 소켓 선택
                                    ActiveState->SelectedSocketIndex = static_cast<int32>(MutableSkeleton->Sockets.size()) - 1;
                                    ActiveState->SelectedBodySetup = nullptr;
                                    ActiveState->SelectedBodyIndex = -1;
                                    ActiveState->SelectedConstraintIndex = -1;
                                    ActiveState->SelectedPreviewMesh = nullptr;

                                    // 편집용 값 초기화
                                    ActiveState->EditSocketLocation = NewSocket.RelativeLocation;
                                    ActiveState->EditSocketRotation = NewSocket.RelativeRotation.ToEulerZYXDeg();
                                    ActiveState->EditSocketScale = NewSocket.RelativeScale;

                                    // 기즈모를 새 소켓 위치로 이동 (이전 소켓에 대한 writeback 방지)
                                    if (ActiveState->PreviewActor)
                                    {
                                        ActiveState->PreviewActor->RepositionAnchorToSocket(ActiveState->SelectedSocketIndex);
                                        if (USceneComponent* Anchor = ActiveState->PreviewActor->GetBoneGizmoAnchor())
                                        {
                                            ActiveState->World->GetSelectionManager()->SelectActor(ActiveState->PreviewActor);
                                            ActiveState->World->GetSelectionManager()->SelectComponent(Anchor);
                                        }
                                    }

                                    // 변경 플래그 설정
                                    ActiveState->bSocketTransformChanged = true;

                                    UE_LOG("Added socket %s to bone %s", NewSocketName.c_str(), ClickedBoneName);
                                }
                            }
                            ImGui::EndPopup();
                        }

						// 본 좌클릭 이벤트
                        if (ImGui::IsItemClicked())
                        {
                            // Clear body selection when a bone itself is clicked
                            ActiveState->SelectedBodySetup = nullptr;
                            // Clear constraint selection when body is selected
                            ActiveState->SelectedConstraintIndex = -1;
                            // Clear socket selection when bone is clicked
                            ActiveState->SelectedSocketIndex = -1;
                            // Clear preview mesh selection when bone is clicked
                            ActiveState->SelectedPreviewMesh = nullptr;

							ActiveState->SelectedBodyIndexForGraph = -1;

                            if (ActiveState->SelectedBoneIndex != Index)
                            {
                                ActiveState->SelectedBoneIndex = Index;
                                ActiveState->bBoneLinesDirty = true;
                                
                                ExpandToSelectedBone(ActiveState, Index);

                                if (ActiveState->PreviewActor && ActiveState->World)
                                {
                                    ActiveState->PreviewActor->RepositionAnchorToBone(Index);
                                    if (USceneComponent* Anchor = ActiveState->PreviewActor->GetBoneGizmoAnchor())
                                    {
                                        ActiveState->World->GetSelectionManager()->SelectActor(ActiveState->PreviewActor);
                                        ActiveState->World->GetSelectionManager()->SelectComponent(Anchor);
                                    }
                                }
                            }
                        }
                        
						// ======== 바디 및 컨스트레인트 UI ===========
                        if (ActiveState->CurrentPhysicsAsset)
                        {
                            UBodySetup* MatchedBody = ActiveState->CurrentPhysicsAsset->FindBodySetup(FName(Label));
                            if (MatchedBody)
                            {
								// =========== 바디 UI ============
                                ImGui::Indent(14.0f);

                                // Make body entry selectable (unique ID per bone index)
                                char BodyLabel[256];
                                snprintf(BodyLabel, sizeof(BodyLabel), "Body: %s", MatchedBody->BoneName.ToString().c_str());

                                // Highlight when the bone is selected so selection is consistent
                                bool bBodySelected = (ActiveState->SelectedBoneIndex == Index);

                                // ==== 바디 UI 좌클릭 이벤트 ====
                                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.7f, 0.25f, 1.0f));
                                if (ImGui::Selectable(BodyLabel, bBodySelected))
                                {
                                    // When user selects a body, set SelectedBodySetup and also select corresponding bone
                                    ActiveState->SelectedBodySetup = MatchedBody;

                                    ActiveState->SelectedBodyIndex = ActiveState->CurrentPhysicsAsset->FindBodyIndex(FName(Label));

                                    ActiveState->SelectedBodyIndexForGraph = ActiveState->CurrentPhysicsAsset->FindBodyIndex(FName(Label));


                                    // Clear constraint selection when body is selected
                                    ActiveState->SelectedConstraintIndex = -1;
                                    // Clear preview mesh selection when body is selected
                                    ActiveState->SelectedPreviewMesh = nullptr;

                                    // When user selects a body, also select the corresponding bone and move gizmo
                                    if (ActiveState->SelectedBoneIndex != Index)
                                    {
                                        ActiveState->SelectedBoneIndex = Index;
                                        ActiveState->bBoneLinesDirty = true;

                                        ExpandToSelectedBone(ActiveState, Index);

                                        if (ActiveState->PreviewActor && ActiveState->World)
                                        {
                                            ActiveState->PreviewActor->RepositionAnchorToBone(Index);
                                            if (USceneComponent* Anchor = ActiveState->PreviewActor->GetBoneGizmoAnchor())
                                            {
                                                ActiveState->World->GetSelectionManager()->SelectActor(ActiveState->PreviewActor);
                                                ActiveState->World->GetSelectionManager()->SelectComponent(Anchor);
                                            }
                                        }
                                    }
                                }
                                ImGui::PopStyleColor();

                                UPhysicsAsset* Phys = ActiveState->CurrentPhysicsAsset;
                                FName CurrentBodyName = MatchedBody->BoneName;
								//TArray<const FPhysicsConstraintSetup*> ConnectedConstraints = ActiveState->CurrentPhysicsAsset->GetConstraintsForBody(CurrentBodyName);
								TArray<int32> ConnectedConstraintIndices = Phys->GetConstraintIndicesForBody(CurrentBodyName);
                                

                                // ===== 바디 UI 우클릭 이벤트 =======
                                if (ImGui::BeginPopupContextItem("BodyContextMenu"))
                                {
                                    UPhysicsAsset* Phys = ActiveState->CurrentPhysicsAsset;
                                    if (Phys && ImGui::BeginMenu("Add Constraint"))
                                    {
                                        // Display all bodies as potential child bodies for constraint
                                        for (UBodySetup* ChildBody : Phys->BodySetups)
                                        {
                                            if (!ChildBody) continue;

                                            // Skip self
                                            if (ChildBody == MatchedBody) continue;

                                            FString ChildBodyName = ChildBody->BoneName.ToString();
                                            if (ImGui::MenuItem(ChildBodyName.c_str()))
                                            {
                                                // Get skeleton to calculate bone transforms
                                                const FSkeleton* Skeleton = ActiveState->CurrentMesh ? ActiveState->CurrentMesh->GetSkeleton() : nullptr;
                                                assert(Skeleton && "Skeleton must be valid when adding constraints");
                                                if (Skeleton)
                                                {
                                                    int32 ParentBoneIndex = Skeleton->FindBoneIndex(MatchedBody->BoneName.ToString());
                                                    int32 ChildBoneIndex = Skeleton->FindBoneIndex(ChildBody->BoneName.ToString());

                                                    FPhysicsConstraintSetup NewConstraint;
                                                    NewConstraint.BodyNameA = MatchedBody->BoneName;  // Parent body
                                                    NewConstraint.BodyNameB = ChildBody->BoneName;    // Child body

                                                    // Calculate LocalFrameA: child bone's transform relative to parent bone's coordinate system
                                                    if (ParentBoneIndex != INDEX_NONE && ChildBoneIndex != INDEX_NONE &&
                                                        ActiveState->PreviewActor && ActiveState->PreviewActor->GetSkeletalMeshComponent())
                                                    {
                                                        FTransform ParentWorldTransform = ActiveState->PreviewActor->GetSkeletalMeshComponent()->GetBoneWorldTransform(ParentBoneIndex);
                                                        FTransform ChildWorldTransform = ActiveState->PreviewActor->GetSkeletalMeshComponent()->GetBoneWorldTransform(ChildBoneIndex);

                                                        // physicX에서는 로컬 좌표계의 scale을 아마? 무시하므로 1,1,1로 설정
                                                        ParentWorldTransform.Scale3D = FVector(1.0f, 1.0f, 1.0f);
                                                        ChildWorldTransform.Scale3D = FVector(1.0f, 1.0f, 1.0f);

                                                        // 조인트 프레임 회전 설정
                                                        // PxD6Joint 축: X=Twist, Y=Swing1(Twist 0도 기준), Z=Swing2
                                                        // 목표: 조인트 X축=본 Z축, 조인트 Y축=본 X축 (Twist 0도 기준을 +X로)
                                                        FQuat ZToX = FQuat::FromAxisAngle(FVector(0, 1, 0), -XM_PIDIV2);
                                                        FQuat TwistRef = FQuat::FromAxisAngle(FVector(1, 0, 0), -XM_PIDIV2);
                                                        FQuat JointFrameRotation = ZToX * TwistRef;

                                                        FTransform RelativeTransform = ParentWorldTransform.GetRelativeTransform(ChildWorldTransform);
                                                        NewConstraint.LocalFrameA = FTransform(RelativeTransform.Translation, RelativeTransform.Rotation * JointFrameRotation, FVector(1, 1, 1));
                                                        NewConstraint.LocalFrameB = FTransform(FVector::Zero(), JointFrameRotation, FVector(1, 1, 1));
                                                    }
                                                    else
                                                    {
                                                        // Fallback to identity if bones not found
                                                        assert(false && "Bone indices not found for constraint setup");
                                                        NewConstraint.LocalFrameA = FTransform();
                                                        NewConstraint.LocalFrameB = FTransform();
                                                    }

                                                    NewConstraint.TwistLimitMin = -45.0f;
                                                    NewConstraint.TwistLimitMax = 45.0f;
                                                    NewConstraint.SwingLimitY = 45.0f;
                                                    NewConstraint.SwingLimitZ = 45.0f;
                                                    NewConstraint.bEnableCollision = false;

                                                    // Add to physics asset
                                                    Phys->Constraints.Add(NewConstraint);

                                                    UE_LOG("Added constraint between %s (parent) and %s (child)",
                                                        MatchedBody->BoneName.ToString().c_str(),
                                                        ChildBody->BoneName.ToString().c_str());
                                                }
                                            }
                                        }
                                        ImGui::EndMenu();
                                    }
                                    if(Phys && ImGui::MenuItem("Delete Body"))
                                    {
                                        int32 BodyIndex = Phys->FindBodyIndex(MatchedBody->BoneName);
                                        if (BodyIndex != INDEX_NONE)
                                        {
											// remove connected constraints first
                                            // Remove from highest index to lowest to avoid shifting issues
                                            for (int i = ConnectedConstraintIndices.size() - 1; i >= 0; --i)
                                            {
                                                Phys->Constraints.RemoveAt(ConnectedConstraintIndices[i]);
                                            }
                                            ActiveState->SelectedConstraintIndex = -1;

                                            // Now remove the body
                                            Phys->BodySetups.RemoveAt(BodyIndex);
                                            Phys->BuildBodySetupIndexMap();

                                            // Clear selection if this body was selected
                                            if (ActiveState->SelectedBodySetup == MatchedBody)
                                            {
                                                ActiveState->SelectedBodySetup = nullptr;
                                                ActiveState->SelectedBodyIndex = -1;
                                            }
                                            UE_LOG("Deleted UBodySetup for bone %s from PhysicsAsset %s", MatchedBody->BoneName.ToString().c_str(), Phys->GetName().ToString().c_str());
                                        }
									}
                                    ImGui::EndPopup();
                                }

								// =========== Constraint UI ============
                                ImGui::Indent(14.0f);
                                
								// ConnectedConstraintIndices가 현재 프레임 내에서 변경될 수 있으므로(ex: 위에서 delete body 버튼으로 인해 어떤 constraints가 delete 됏을 경우) 재호출해서 다시 할당
                                ConnectedConstraintIndices = Phys->GetConstraintIndicesForBody(CurrentBodyName);
                                for (int32 ConstraintIdx : ConnectedConstraintIndices)
                                {
                                    const FPhysicsConstraintSetup& Constraint = Phys->Constraints[ConstraintIdx];

                                    // Determine the "other" body name
                                    FName OtherBodyName = (Constraint.BodyNameA == CurrentBodyName) ? Constraint.BodyNameB : Constraint.BodyNameA;

                                    char ConstraintLabel[256];
                                    snprintf(ConstraintLabel, sizeof(ConstraintLabel), "  %s <-> %s", CurrentBodyName.ToString().c_str(), OtherBodyName.ToString().c_str());

                                    bool bConstraintSelected = (ActiveState->SelectedConstraintIndex == ConstraintIdx);

                                    // ======= Constraint UI 좌클릭 이벤트 =======
                                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.85f, 0.85f, 1.0f));
                                    if (ImGui::Selectable(ConstraintLabel, bConstraintSelected))
                                    {
                                        // Select this constraint
                                        ActiveState->SelectedConstraintIndex = ConstraintIdx;

                                        ActiveState->SelectedBoneIndex = -1; // Clear bone selection when constraint is selected

										ActiveState->SelectedBodyIndexForGraph = -1;

                                        // Clear body selection when constraint is selected
                                        ActiveState->SelectedBodySetup = nullptr;
                                        ActiveState->SelectedBodyIndex = -1;
                                        // Clear preview mesh selection when constraint is selected
                                        ActiveState->SelectedPreviewMesh = nullptr;
                                    }
                                    ImGui::PopStyleColor();

                                    // ======= Constraint UI 우클릭 이벤트 =======
                                    if (ImGui::BeginPopupContextItem())
                                    {
                                        if (ImGui::MenuItem("Delete Constraint"))
                                        {
                                            Phys->Constraints.RemoveAt(ConstraintIdx);
                                            if (ActiveState->SelectedConstraintIndex == ConstraintIdx)
                                            {
                                                ActiveState->SelectedConstraintIndex = -1;
                                            }
                                            else if (ActiveState->SelectedConstraintIndex > ConstraintIdx)
                                            {
                                                ActiveState->SelectedConstraintIndex--;
                                            }
                                        }
                                        ImGui::EndPopup();
                                    }
                                }
                                ImGui::Unindent(14.0f);

                                ImGui::Unindent(14.0f);
                            }
                        }

                        // =========== Socket UI ============
                        // 현재 본에 연결된 소켓들을 표시
                        if (Skeleton)
                        {
                            const TArray<FSkeletalMeshSocket>& Sockets = Skeleton->Sockets;
                            const char* CurrentBoneName = Bones[Index].Name.c_str();

                            for (int32 si = 0; si < static_cast<int32>(Sockets.size()); ++si)
                            {
                                const FSkeletalMeshSocket& Socket = Sockets[si];
                                if (Socket.BoneName == CurrentBoneName)
                                {
                                    ImGui::Indent(14.0f);

                                    char SocketLabel[256];
                                    snprintf(SocketLabel, sizeof(SocketLabel), "[Socket] %s", Socket.SocketName.c_str());

                                    bool bSocketSelected = (ActiveState->SelectedSocketIndex == si);

                                    // 소켓 좌클릭 이벤트
                                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.9f, 0.4f, 1.0f)); // 초록색으로 구분
                                    if (ImGui::Selectable(SocketLabel, bSocketSelected))
                                    {
                                        ActiveState->SelectedSocketIndex = si;

                                        // 다른 선택 해제
                                        ActiveState->SelectedBodySetup = nullptr;
                                        ActiveState->SelectedBodyIndex = -1;
                                        ActiveState->SelectedConstraintIndex = -1;
                                        ActiveState->SelectedBodyIndexForGraph = -1;
                                        ActiveState->SelectedPreviewMesh = nullptr;
                                        ActiveState->SelectedBoneIndex = -1;  // 본 선택도 해제

                                        // 편집용 값 초기화
                                        ActiveState->EditSocketLocation = Socket.RelativeLocation;
                                        ActiveState->EditSocketRotation = Socket.RelativeRotation.ToEulerZYXDeg();
                                        ActiveState->EditSocketScale = Socket.RelativeScale;

                                        // 기즈모를 소켓 위치로 이동
                                        if (ActiveState->PreviewActor)
                                        {
                                            ActiveState->PreviewActor->RepositionAnchorToSocket(si);
                                            if (USceneComponent* Anchor = ActiveState->PreviewActor->GetBoneGizmoAnchor())
                                            {
                                                ActiveState->World->GetSelectionManager()->SelectActor(ActiveState->PreviewActor);
                                                ActiveState->World->GetSelectionManager()->SelectComponent(Anchor);
                                            }
                                        }
                                    }
                                    ImGui::PopStyleColor();

                                    // 소켓 우클릭 이벤트 - 메뉴
                                    if (ImGui::BeginPopupContextItem())
                                    {
                                        // 프리뷰 메시 스폰 - 로드된 StaticMesh 목록에서 선택
                                        if (ImGui::BeginMenu("Spawn Preview Mesh"))
                                        {
                                            TArray<UStaticMesh*> AllMeshes = UResourceManager::GetInstance().GetAll<UStaticMesh>();
                                            TArray<FString> AllPaths = UResourceManager::GetInstance().GetAllFilePaths<UStaticMesh>();

                                            if (AllMeshes.empty())
                                            {
                                                ImGui::TextDisabled("No static meshes loaded");
                                            }
                                            else
                                            {
                                                for (int32 i = 0; i < AllMeshes.size(); ++i)
                                                {
                                                    UStaticMesh* Mesh = AllMeshes[i];
                                                    if (!Mesh) continue;

                                                    // 파일명만 표시
                                                    FString DisplayName = AllPaths[i];
                                                    size_t LastSlash = DisplayName.find_last_of("/\\");
                                                    if (LastSlash != FString::npos)
                                                        DisplayName = DisplayName.substr(LastSlash + 1);

                                                    if (ImGui::MenuItem(DisplayName.c_str()))
                                                    {
                                                        if (ActiveState->PreviewActor)
                                                        {
                                                            USkeletalMeshComponent* SkelComp = ActiveState->PreviewActor->GetSkeletalMeshComponent();
                                                            if (SkelComp)
                                                            {
                                                                // PreviewActor에 StaticMeshComponent 직접 추가
                                                                UStaticMeshComponent* MeshComp = NewObject<UStaticMeshComponent>();
                                                                MeshComp->ObjectName = "SocketPreviewMesh_" + Socket.SocketName;

                                                                // 액터에 소유권 추가
                                                                ActiveState->PreviewActor->AddOwnedComponent(MeshComp);

                                                                // 소켓에 attach (등록 전에 해야 함)
                                                                MeshComp->SetupAttachment(SkelComp, FName(Socket.SocketName));

                                                                // 월드에 등록
                                                                MeshComp->RegisterComponent(ActiveState->World);

                                                                // 메시 설정
                                                                MeshComp->SetStaticMesh(AllPaths[i]);

                                                                // 상대 트랜스폼 설정
                                                                MeshComp->SetRelativeLocation(FVector::Zero());
                                                                MeshComp->SetRelativeRotation(FQuat::Identity());

                                                                // 월드 스케일 1이 되도록 상대 스케일 계산 (소켓 스케일 포함)
                                                                FTransform SocketWorld = SkelComp->GetSocketTransform(FName(Socket.SocketName));
                                                                FVector SocketWorldScale = SocketWorld.Scale3D;
                                                                FVector RelativeScaleForWorldOne = FVector(
                                                                    SocketWorldScale.X != 0.0f ? 1.0f / SocketWorldScale.X : 1.0f,
                                                                    SocketWorldScale.Y != 0.0f ? 1.0f / SocketWorldScale.Y : 1.0f,
                                                                    SocketWorldScale.Z != 0.0f ? 1.0f / SocketWorldScale.Z : 1.0f
                                                                );
                                                                MeshComp->SetRelativeScale(RelativeScaleForWorldOne);

                                                                // 프리뷰 메시 추적 및 선택
                                                                ActiveState->SpawnedPreviewMeshes.Add(MeshComp);
                                                                ActiveState->SelectedPreviewMesh = MeshComp;
                                                                ActiveState->EditPreviewMeshLocation = MeshComp->GetRelativeLocation();
                                                                ActiveState->EditPreviewMeshRotation = MeshComp->GetRelativeRotation().ToEulerZYXDeg();
                                                                ActiveState->EditPreviewMeshScale = MeshComp->GetRelativeScale();

                                                                // 다른 선택 해제
                                                                ActiveState->SelectedSocketIndex = -1;
                                                                ActiveState->SelectedBodySetup = nullptr;
                                                                ActiveState->SelectedConstraintIndex = -1;
                                                                ActiveState->SelectedBoneIndex = -1;
                                                                ActiveState->SelectedBodyIndex = -1;

                                                                // 기즈모를 프리뷰 메시 위치로 이동
                                                                ActiveState->PreviewActor->RepositionAnchorToPreviewMesh(MeshComp);
                                                                if (USceneComponent* Anchor = ActiveState->PreviewActor->GetBoneGizmoAnchor())
                                                                {
                                                                    ActiveState->World->GetSelectionManager()->SelectActor(ActiveState->PreviewActor);
                                                                    ActiveState->World->GetSelectionManager()->SelectComponent(Anchor);
                                                                }

                                                                UE_LOG("Added preview mesh '%s' at socket %s", DisplayName.c_str(), Socket.SocketName.c_str());
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                            ImGui::EndMenu();
                                        }

                                        ImGui::Separator();

                                        if (ImGui::MenuItem("Delete Socket"))
                                        {
                                            // const_cast로 수정 가능하게
                                            FSkeleton* MutableSkeleton = const_cast<FSkeleton*>(Skeleton);
                                            MutableSkeleton->RemoveSocket(FName(Socket.SocketName));

                                            // 선택 해제
                                            if (ActiveState->SelectedSocketIndex == si)
                                            {
                                                ActiveState->SelectedSocketIndex = -1;
                                            }
                                            else if (ActiveState->SelectedSocketIndex > si)
                                            {
                                                ActiveState->SelectedSocketIndex--;
                                            }

                                            // 변경 플래그 설정
                                            ActiveState->bSocketTransformChanged = true;

                                            UE_LOG("Deleted socket %s from bone %s", Socket.SocketName.c_str(), CurrentBoneName);
                                        }
                                        ImGui::EndPopup();
                                    }

                                    // 이 소켓에 붙은 프리뷰 메시들 표시
                                    for (int32 pi = 0; pi < ActiveState->SpawnedPreviewMeshes.Num(); ++pi)
                                    {
                                        UStaticMeshComponent* PreviewMesh = ActiveState->SpawnedPreviewMeshes[pi];
                                        if (!PreviewMesh) continue;

                                        // 이 소켓에 붙은 메시인지 확인
                                        if (PreviewMesh->GetAttachSocketName().ToString() == Socket.SocketName)
                                        {
                                            ImGui::Indent(14.0f);

                                            char PreviewLabel[256];
                                            snprintf(PreviewLabel, sizeof(PreviewLabel), "[Mesh] %s", PreviewMesh->GetName().c_str());

                                            bool bPreviewSelected = (ActiveState->SelectedPreviewMesh == PreviewMesh);

                                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.6f, 0.2f, 1.0f)); // 주황색
                                            if (ImGui::Selectable(PreviewLabel, bPreviewSelected))
                                            {
                                                ActiveState->SelectedPreviewMesh = PreviewMesh;
                                                ActiveState->EditPreviewMeshLocation = PreviewMesh->GetRelativeLocation();
                                                ActiveState->EditPreviewMeshRotation = PreviewMesh->GetRelativeRotation().ToEulerZYXDeg();
                                                ActiveState->EditPreviewMeshScale = PreviewMesh->GetRelativeScale();

                                                // 다른 선택 해제
                                                ActiveState->SelectedBoneIndex = -1;
                                                ActiveState->SelectedBodyIndex = -1;
                                                ActiveState->SelectedSocketIndex = -1;
                                                ActiveState->SelectedBodySetup = nullptr;
                                                ActiveState->SelectedConstraintIndex = -1;

                                                // 기즈모를 프리뷰 메시 위치로 이동
                                                if (ActiveState->PreviewActor && ActiveState->World)
                                                {
                                                    ActiveState->PreviewActor->RepositionAnchorToPreviewMesh(PreviewMesh);
                                                    if (USceneComponent* Anchor = ActiveState->PreviewActor->GetBoneGizmoAnchor())
                                                    {
                                                        ActiveState->World->GetSelectionManager()->SelectActor(ActiveState->PreviewActor);
                                                        ActiveState->World->GetSelectionManager()->SelectComponent(Anchor);
                                                    }
                                                }
                                            }
                                            ImGui::PopStyleColor();

                                            ImGui::Unindent(14.0f);
                                        }
                                    }

                                    ImGui::Unindent(14.0f);
                                }
                            }
                        }

                        if (!bLeaf && open)
                        {
                            int32 ChildCount = Children[Index].Num();
                            for (int32 ci = 0; ci < ChildCount; ++ci)
                            {
                                int32 Child = Children[Index][ci];
                                bool bChildIsLast = (ci == ChildCount - 1);
                                DrawNode(Child, Depth + 1, bChildIsLast);
                            }

                            ImGui::TreePop();
                        }
                        ImGui::PopID();
                    };

                    // First pass: draw all tree nodes and collect positions
                    for (int32 i = 0; i < Bones.size(); ++i)
                    {
                        if (Bones[i].ParentIndex < 0)
                        {
                            DrawNode(i, 0, true);
                        }
                    }

                    // Second pass: draw connecting lines based on collected positions
                    for (int32 i = 0; i < Bones.size(); ++i)
                    {
                        const NodePosInfo& Info = NodePositions[i];
                        if (Info.Depth == 0) continue; // Skip root nodes

                        int32 ParentIdx = Info.ParentIndex;
                        if (ParentIdx < 0 || ParentIdx >= Bones.size()) continue;

                        const NodePosInfo& ParentInfo = NodePositions[ParentIdx];

                        // Calculate the X position for vertical line (at parent's indent level)
                        float LineX = Info.BaseX - IndentPerLevel + 10.0f;

                        // Draw horizontal line from vertical line to this node
                        float LineEndX = Info.BaseX - 2.0f;
                        DrawList->AddLine(ImVec2(LineX, Info.CenterY), ImVec2(LineEndX, Info.CenterY), LineColor, 2.0f);

                        // Draw vertical line from parent to this node
                        // Find the first child of parent to get the starting Y
                        float VertStartY = ParentInfo.CenterY;
                        float VertEndY = Info.CenterY;

                        // Only draw vertical segment if this is the first child or we need to connect
                        if (!Children[ParentIdx].IsEmpty())
                        {
                            int32 FirstChild = Children[ParentIdx][0];
                            if (i == FirstChild)
                            {
                                // First child: draw from parent center to this node
                                DrawList->AddLine(ImVec2(LineX, VertStartY), ImVec2(LineX, VertEndY), LineColor, 2.0f);
                            }
                            else
                            {
                                // Not first child: draw from previous sibling to this node
                                // Find previous sibling
                                for (int32 ci = 1; ci < Children[ParentIdx].Num(); ++ci)
                                {
                                    if (Children[ParentIdx][ci] == i)
                                    {
                                        int32 PrevSibling = Children[ParentIdx][ci - 1];
                                        float PrevY = NodePositions[PrevSibling].CenterY;
                                        DrawList->AddLine(ImVec2(LineX, PrevY), ImVec2(LineX, VertEndY), LineColor, 2.0f);
                                        break;
                                    }
                                }
                            }
                        }
                    }

                    ImGui::EndChild();
                }
            }

            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.35f, 0.45f, 0.60f, 0.7f));
            ImGui::Separator();
            ImGui::PopStyleColor();
            ImGui::Spacing();

            // Physics Constraint Graph Section
            if (ActiveState->CurrentPhysicsAsset && ActiveState->CurrentPhysicsAsset->BodySetups.Num() > 0)
            {
                ImGui::Text("Physics Constraint Graph:");
                ImGui::Spacing();

                // Graph visualization area - use remaining space instead of fixed height
                float remainingHeight = ImGui::GetContentRegionAvail().y;
                ImGui::BeginChild("PhysicsGraphView", ImVec2(0, remainingHeight), true);
                DrawPhysicsConstraintGraph(ActiveState);
                ImGui::EndChild();
            }
        }
        else
        {
            ImGui::EndChild();
            ImGui::End();
            return;
        }
        ImGui::EndChild();

        ImGui::SameLine(0, 0); // No spacing between panels

        // Center panel (viewport area)
        ImGui::BeginChild("CenterColumn", ImVec2(centerWidth, totalHeight), false, ImGuiWindowFlags_NoScrollbar);
        {
            // 상단: 뷰포트 자식 창: 계산된 TopPanelHeight 사용
            ImGui::BeginChild("SkeletalMeshViewport", ImVec2(0, TopPanelHeight), true, ImGuiWindowFlags_NoScrollbar);
            {
                if (ImGui::IsWindowHovered())
                {
                    ImGui::GetIO().WantCaptureMouse = false;
                }

                ImVec2 childPos = ImGui::GetWindowPos();
                ImVec2 childSize = ImGui::GetWindowSize();
                ImVec2 rectMin = childPos;
                ImVec2 rectMax(childPos.x + childSize.x, childPos.y + childSize.y);
                CenterRect.Left = rectMin.x; CenterRect.Top = rectMin.y; CenterRect.Right = rectMax.x; CenterRect.Bottom = rectMax.y; CenterRect.UpdateMinMax();

                URenderer* CurrentRenderer = URenderManager::GetInstance().GetRenderer();
                D3D11RHI* RHIDevice = CurrentRenderer->GetRHIDevice();

                uint32 TotalWidth = RHIDevice->GetViewportWidth();
                uint32 TotalHeight = RHIDevice->GetViewportHeight();

                // UV 좌표를 정확하게 계산: 정확한 본 피킹을 위해 (자식 윈도우의 실제 콘텐츠 영역 사용)
                ImVec2 contentMin = ImGui::GetWindowContentRegionMin();
                ImVec2 contentMax = ImGui::GetWindowContentRegionMax();

                // 실제 렌더링 영역의 UV 계산
                float actualLeft = CenterRect.Left + contentMin.x;
                float actualTop = CenterRect.Top + contentMin.y;

                ImVec2 uv0(actualLeft / TotalWidth, actualTop / TotalHeight);
                ImVec2 uv1((actualLeft + (contentMax.x - contentMin.x)) / TotalWidth,
                    (actualTop + (contentMax.y - contentMin.y)) / TotalHeight);

                ID3D11ShaderResourceView* SRV = RHIDevice->GetCurrentSourceSRV();

                // ImGui::Image 사용 이유: 뷰포트가 Imgui 메뉴를 가려버리는 경우 방지 위함.
                ImGui::Image((void*)SRV, ImVec2(contentMax.x - contentMin.x, contentMax.y - contentMin.y), uv0, uv1);
            }
            ImGui::EndChild();

            ImGui::Separator();

            // 하단: 애니메이션 패널: 남은 공간(BottomPanelHeight)을 채움
            ImGui::BeginChild("AnimationPanel", ImVec2(0, 0), true);
            {
                if (ActiveState)
                {
                    DrawAnimationPanel(ActiveState);
                }
            }
            ImGui::EndChild();
        }
        ImGui::EndChild();

        ImGui::SameLine(0, 0); // No spacing between panels

        // Right panel - Bone Properties & Anim Browser
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
        // 1. RightPanel '컨테이너' 자식 창
        ImGui::BeginChild("RightPanel", ImVec2(rightWidth, totalHeight), true);
        ImGui::PopStyleVar();
        {
            // 2. 상단: "Bone Properties"를 위한 자식 창 (TopPanelHeight 사용)
            ImGui::BeginChild("BonePropertiesChild", ImVec2(0, TopPanelHeight), false); 
            {
                // Panel header
                ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.25f, 0.35f, 0.50f, 0.8f));
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
                ImGui::Indent(8.0f);
                ImGui::Text("Bone Properties");
                ImGui::Unindent(8.0f);
                ImGui::PopStyleColor();

                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.35f, 0.45f, 0.60f, 0.7f));
                ImGui::Separator();
                ImGui::PopStyleColor();

                if (ActiveState->SelectedBodySetup) // Body Properties
                {
                    UBodySetup* Body = ActiveState->SelectedBodySetup;
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.90f, 0.40f, 1.0f));
                    ImGui::Text("> Selected Body");
                    ImGui::PopStyleColor();

                    ImGui::Spacing();
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.95f, 1.00f, 1.0f));
                    ImGui::TextWrapped("%s", Body->BoneName.ToString().c_str());
                    ImGui::PopStyleColor();

                    ImGui::Spacing();
                    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.45f, 0.55f, 0.70f, 0.8f));
                    ImGui::Separator();
                    ImGui::PopStyleColor();

                    // Editable physics properties
                    ImGui::Text("Physics Properties:");
                    ImGui::Spacing();

                    //ImGui::PushItemWidth(-1);
                    ImGui::DragFloat("Mass", &Body->Mass, 0.1f, 0.01f, 10000.0f, "%.3f");
                    ImGui::DragFloat("Linear Damping", &Body->LinearDamping, 0.001f, 0.0f, 100.0f, "%.3f");
                    ImGui::DragFloat("Angular Damping", &Body->AngularDamping, 0.001f, 0.0f, 100.0f, "%.3f");
                    //ImGui::PopItemWidth();

                    ImGui::Spacing();
                    int CurrentIndex = static_cast<int>(Body->CollisionState);
                    static const char* Items[] = {
                        "NoCollision",
                        "QueryOnly (Trigger)",
                        "PhysicsOnly (Ragdoll)",
                        "Query + Physics",
                    };
                    bool bChanged = ImGui::Combo("Collision Settings", &CurrentIndex, Items, 4);
                    if (bChanged)
                    {
                        Body->CollisionState = static_cast<ECollisionState>(CurrentIndex);
                    }
                    ImGui::Checkbox("Simulate Physics", &Body->bSimulatePhysics);
                    ImGui::Checkbox("Enable Gravity", &Body->bEnableGravity);
                    
					// ======UPhysicalMaterial editing=======
                    ImGui::Spacing();
                    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.45f, 0.55f, 0.70f, 0.5f));
                    ImGui::Separator();
                    ImGui::PopStyleColor();
                    ImGui::Spacing();

					ImGui::Text("Material Properties:");
                    UPhysicalMaterial* PhysMat = Body->PhysMaterial;
                    if (!PhysMat)
                    {
                        PhysMat = NewObject<UPhysicalMaterial>();
                        Body->PhysMaterial = PhysMat;
                    }
					ImGui::DragFloat("Static Friction", &PhysMat->StaticFriction, 0.01f, 0.0f, 2.0f, "%.3f");
					ImGui::DragFloat("Dynamic Friction", &PhysMat->DynamicFriction, 0.01f, 0.0f, 2.0f, "%.3f");
					ImGui::DragFloat("Restitution", &PhysMat->Restitution, 0.01f, 0.0f, 1.0f, "%.3f");
					ImGui::DragFloat("Density", &PhysMat->Density, 0.1f, 0.1f, 20.0f, "%.3f");


					// =======Aggregate Geometry editing========
                    ImGui::Spacing();
                    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.45f, 0.55f, 0.70f, 0.5f));
                    ImGui::Separator();
                    ImGui::PopStyleColor();
                    ImGui::Spacing();

                    // AggGeom counts
                    int NumSpheres = Body->AggGeom.SphereElements.Num();
                    int NumBoxes = Body->AggGeom.BoxElements.Num();
                    int NumCapsules = Body->AggGeom.CapsuleElements.Num();
                    int NumConvex = Body->AggGeom.ConvexElements.Num();

                    ImGui::Text("Aggregate Geometry:");
                    ImGui::Spacing();

                    // Editable Sphere Elements
                    if (ImGui::CollapsingHeader("Sphere Elements", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        ImGui::Indent(10.0f);
                        ImGui::PushID("SphereElements");
                        
                        for (int si = 0; si < NumSpheres; ++si)
                        {
                            FKSphereElem& S = Body->AggGeom.SphereElements[si];
                            ImGui::PushID(si);
                            
                            if (ImGui::TreeNodeEx((void*)(intptr_t)si, ImGuiTreeNodeFlags_DefaultOpen, "Sphere [%d]", si))
                            {
                                //ImGui::PushItemWidth(-1);
                                ImGui::DragFloat3("Center", &S.Center.X, 0.01f, -10000.0f, 10000.0f, "%.2f");
                                ImGui::DragFloat("Radius", &S.Radius, 0.01f, 0.01f, 10000.0f, "%.2f");
                                //ImGui::PopItemWidth();
                                
                                if (ImGui::Button("Remove Sphere"))
                                {
                                    Body->AggGeom.SphereElements.RemoveAt(si);
                                    ActiveState->bChangedGeomNum = true;
                                    ImGui::TreePop();
                                    ImGui::PopID();
                                    break;
                                }
                                
                                ImGui::TreePop();
                            }
                            ImGui::PopID();
                        }
                        
                        if (ImGui::Button("Add Sphere"))
                        {
                            FKSphereElem NewSphere;
                            NewSphere.Center = FVector::Zero();
                            NewSphere.Radius = 0.5f;
                            Body->AggGeom.SphereElements.Add(NewSphere);
                            ActiveState->bChangedGeomNum = true;
                        }
                        
                        ImGui::PopID(); // Pop "SphereElements"
                        ImGui::Unindent(10.0f);
                    }

                    // Editable Box Elements
                    if (ImGui::CollapsingHeader("Box Elements"))
                    {
                        ImGui::Indent(10.0f);
                        ImGui::PushID("BoxElements");
                        
                        for (int bi = 0; bi < NumBoxes; ++bi)
                        {
                            FKBoxElem& B = Body->AggGeom.BoxElements[bi];
                            ImGui::PushID(bi);
                            
                            if (ImGui::TreeNodeEx((void*)(intptr_t)bi, ImGuiTreeNodeFlags_DefaultOpen, "Box [%d]", bi))
                            {
                                //ImGui::PushItemWidth(-1);
                                ImGui::DragFloat3("Center", &B.Center.X, 0.01f, -10000.0f, 10000.0f, "%.2f");
                                ImGui::DragFloat3("Extents", &B.Extents.X, 0.01f, 0.01f, 10000.0f, "%.2f");
                                

                                // Rotation as Euler angles
                                FVector EulerDeg = B.Rotation.ToEulerZYXDeg();
                                if (ImGui::DragFloat3("Rotation", &EulerDeg.X, 0.5f, -180.0f, 180.0f, "%.2f°"))
                                {
                                    B.Rotation = FQuat::MakeFromEulerZYX(EulerDeg);
                                }
                                //ImGui::PopItemWidth();
                                
                                if (ImGui::Button("Remove Box"))
                                {
                                    Body->AggGeom.BoxElements.RemoveAt(bi);
                                    ActiveState->bChangedGeomNum = true;
                                    ImGui::TreePop();
                                    ImGui::PopID();
                                    break;
                                }
                                
                                ImGui::TreePop();
                            }
                            ImGui::PopID();
                        }
                        
                        if (ImGui::Button("Add Box"))
                        {
                            FKBoxElem NewBox;
                            NewBox.Center = FVector::Zero();
                            NewBox.Extents = FVector(0.5f, 0.5f, 0.5f);
                            NewBox.Rotation = FQuat::Identity();
                            Body->AggGeom.BoxElements.Add(NewBox);
                            ActiveState->bChangedGeomNum = true;
                        }
                        
                        ImGui::PopID(); // Pop "BoxElements"
                        ImGui::Unindent(10.0f);
                    }

                    // Editable Capsule (Capsule) Elements
                    if (ImGui::CollapsingHeader("Capsule Elements"))
                    {
                        ImGui::Indent(10.0f);
                        ImGui::PushID("CapsuleElements");
                        
                        for (int si = 0; si < NumCapsules; ++si)
                        {
                            FKCapsuleElem& S = Body->AggGeom.CapsuleElements[si];
                            ImGui::PushID(si);
                            
                            if (ImGui::TreeNodeEx((void*)(intptr_t)si, ImGuiTreeNodeFlags_DefaultOpen, "Capsule [%d]", si))
                            {
                                //ImGui::PushItemWidth(-1);
                                ImGui::DragFloat3("Center", &S.Center.X, 0.01f, -10000.0f, 10000.0f, "%.2f");
                                ImGui::DragFloat("Radius", &S.Radius, 0.01f, 0.01f, 10000.0f, "%.2f");
                                ImGui::DragFloat("Half Length", &S.HalfLength, 0.01f, 0.01f, 10000.0f, "%.2f");
                                
                                // Rotation as Euler angles
                                FVector EulerDeg = S.Rotation.ToEulerZYXDeg();
                                if (ImGui::DragFloat3("Rotation", &EulerDeg.X, 0.5f, -180.0f, 180.0f, "%.2f°"))
                                {
                                    S.Rotation = FQuat::MakeFromEulerZYX(EulerDeg);
                                }
                                //ImGui::PopItemWidth();
                                
                                if (ImGui::Button("Remove Capsule"))
                                {
                                    Body->AggGeom.CapsuleElements.RemoveAt(si);
                                    ActiveState->bChangedGeomNum = true;
                                    ImGui::TreePop();
                                    ImGui::PopID();
                                    break;
                                }
                                
                                ImGui::TreePop();
                            }
                            ImGui::PopID();
                        }
                        
                        if (ImGui::Button("Add Capsule"))
                        {
                            FKCapsuleElem NewCapsule;
                            NewCapsule.Center = FVector::Zero();
                            NewCapsule.Radius = 0.3f;
                            NewCapsule.HalfLength = 0.5f;
                            NewCapsule.Rotation = FQuat::Identity();
                            Body->AggGeom.CapsuleElements.Add(NewCapsule);
                            ActiveState->bChangedGeomNum = true;
                        }
                        
                        ImGui::PopID(); // Pop "CapsuleElements"
                        ImGui::Unindent(10.0f);
                    }

                    // Convex Elements (read-only for now)
                    if (NumConvex > 0)
                    {
                        if (ImGui::CollapsingHeader("Convex Elements"))
                        {
                            ImGui::Indent(10.0f);
                            ImGui::TextDisabled("(Read-only: vertex editing not yet implemented)");
                            for (int ci = 0; ci < NumConvex; ++ci)
                            {
                                const auto& C = Body->AggGeom.ConvexElements[ci];
                                ImGui::Text("[%d] Vertices: %d", ci, C.Vertices.Num());
                            }
                            ImGui::Unindent(10.0f);
                        }
                    }
                }
                else if (ActiveState->SelectedConstraintIndex >= 0 && ActiveState->CurrentPhysicsAsset) // Constraint Properties
                {
                    UPhysicsAsset* Phys = ActiveState->CurrentPhysicsAsset;
                    if (ActiveState->SelectedConstraintIndex < Phys->Constraints.Num())
                    {
                        FPhysicsConstraintSetup& Constraint = Phys->Constraints[ActiveState->SelectedConstraintIndex];

                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.9f, 0.9f, 1.0f));
                        ImGui::Text("> Selected Constraint");
                        ImGui::PopStyleColor();

                        ImGui::Spacing();
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.95f, 1.00f, 1.0f));
                        ImGui::TextWrapped("%s (parent) -> %s (child)", 
                            Constraint.BodyNameA.ToString().c_str(), 
                            Constraint.BodyNameB.ToString().c_str());
                        ImGui::PopStyleColor();

                        ImGui::Spacing();
                        ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.45f, 0.55f, 0.70f, 0.8f));
                        ImGui::Separator();
                        ImGui::PopStyleColor();

                        // Editable constraint properties
                        ImGui::Text("Constraint Limits:");
                        ImGui::Spacing();

                        // Convert radians to degrees for editing
                        float TwistMinDeg = RadiansToDegrees(Constraint.TwistLimitMin);
                        float TwistMaxDeg = RadiansToDegrees(Constraint.TwistLimitMax);
                        float SwingYDeg = RadiansToDegrees(Constraint.SwingLimitY);
                        float SwingZDeg = RadiansToDegrees(Constraint.SwingLimitZ);

                        ImGui::Text("Twist Limits (X-axis):");
                        if (ImGui::DragFloat("Min Twist##Twist", &TwistMinDeg, 0.5f, 0.0f, TwistMaxDeg, "%.2f°"))
                        {
                            Constraint.TwistLimitMin = DegreesToRadians(TwistMinDeg);
                        }
                        if (ImGui::DragFloat("Max Twist##Twist", &TwistMaxDeg, 0.5f, TwistMinDeg, 360.0f, "%.2f°"))
                        {
                            Constraint.TwistLimitMax = DegreesToRadians(TwistMaxDeg);
                        }

                        ImGui::Spacing();
                        ImGui::Text("Swing Limits:");
                        if (ImGui::DragFloat("Swing Y##Swing", &SwingYDeg, 0.5f, 0.0f, 180.0f, "%.2f°"))
                        {
                            Constraint.SwingLimitY = DegreesToRadians(SwingYDeg);
                        }
                        if (ImGui::DragFloat("Swing Z##Swing", &SwingZDeg, 0.5f, 0.0f, 180.0f, "%.2f°"))
                        {
                            Constraint.SwingLimitZ = DegreesToRadians(SwingZDeg);
                        }

                        ImGui::Spacing();
                        ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.45f, 0.55f, 0.70f, 0.5f));
                        ImGui::Separator();
                        ImGui::PopStyleColor();
                        ImGui::Spacing();

                        ImGui::Text("Collision:");
                        ImGui::Checkbox("Enable Collision", &Constraint.bEnableCollision);

                        ImGui::Spacing();
                        ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.45f, 0.55f, 0.70f, 0.5f));
                        ImGui::Separator();
                        ImGui::PopStyleColor();
                        ImGui::Spacing();

                        // Local Frame editing (advanced)
                        if (ImGui::CollapsingHeader("Local Frames (Advanced)"))
                        {
                            // Joint 축 방향 (LocalFrameB.Rotation = JointFrameRotation)
                            // LocalFrameA.Rotation = RelativeTransform.Rotation * JointFrameRotation
                            // 따라서: JointFrameRotation = RelativeTransform.Rotation.Inverse() * LocalFrameA.Rotation

                            // RelativeTransform 계산을 위해 본 정보 가져오기
                            FQuat RelativeRotation = FQuat::Identity();
                            if (ActiveState->PreviewActor && ActiveState->PreviewActor->GetSkeletalMeshComponent())
                            {
                                const FSkeleton* Skel = ActiveState->CurrentMesh ? ActiveState->CurrentMesh->GetSkeleton() : nullptr;
                                if (Skel)
                                {
                                    int32 ParentBoneIdx = Skel->FindBoneIndex(Constraint.BodyNameA.ToString());
                                    int32 ChildBoneIdx = Skel->FindBoneIndex(Constraint.BodyNameB.ToString());

                                    if (ParentBoneIdx != INDEX_NONE && ChildBoneIdx != INDEX_NONE)
                                    {
                                        FTransform ParentWT = ActiveState->PreviewActor->GetSkeletalMeshComponent()->GetBoneWorldTransform(ParentBoneIdx);
                                        FTransform ChildWT = ActiveState->PreviewActor->GetSkeletalMeshComponent()->GetBoneWorldTransform(ChildBoneIdx);
                                        ParentWT.Scale3D = FVector(1, 1, 1);
                                        ChildWT.Scale3D = FVector(1, 1, 1);
                                        FTransform RelT = ParentWT.GetRelativeTransform(ChildWT);
                                        RelativeRotation = RelT.Rotation;
                                    }
                                }
                            }

                            ImGui::Text("Local Frame A (Parent):");
                            ImGui::DragFloat3("Position A", &Constraint.LocalFrameA.Translation.X, 0.01f, -1000.0f, 1000.0f, "%.3f");

                            FVector EulerA = Constraint.LocalFrameA.Rotation.ToEulerZYXDeg();
                            if (ImGui::DragFloat3("Rotation A", &EulerA.X, 0.5f, -180.0f, 180.0f, "%.2f°"))
                            {
                                FQuat NewRotationA = FQuat::MakeFromEulerZYX(EulerA);
                                Constraint.LocalFrameA.Rotation = NewRotationA;

                                // LocalFrameB도 동기화: JointFrameRotation = RelativeRotation.Inverse() * NewRotationA
                                FQuat NewJointFrameRotation = RelativeRotation.Inverse() * NewRotationA;
                                Constraint.LocalFrameB.Rotation = NewJointFrameRotation;
                            }

                            ImGui::Spacing();
                            ImGui::Text("Local Frame B (Child):");
                            ImGui::DragFloat3("Position B", &Constraint.LocalFrameB.Translation.X, 0.01f, -1000.0f, 1000.0f, "%.3f");

                            // LocalFrameB (JointFrameRotation) 표시 - 읽기 전용으로 변경
                            FVector EulerB = Constraint.LocalFrameB.Rotation.ToEulerZYXDeg();
                            ImGui::BeginDisabled(true);
                            ImGui::DragFloat3("Rotation B (Synced)", &EulerB.X, 0.5f, -180.0f, 180.0f, "%.2f°");
                            ImGui::EndDisabled();
                            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "(Auto-synced with Rotation A)");
                        }
                    }
                }
                else if (ActiveState->SelectedSocketIndex >= 0 && ActiveState->CurrentMesh) // Socket Properties
                {
                    FSkeleton* Skeleton = const_cast<FSkeleton*>(ActiveState->CurrentMesh->GetSkeleton());
                    if (Skeleton && ActiveState->SelectedSocketIndex < static_cast<int32>(Skeleton->Sockets.size()))
                    {
                        FSkeletalMeshSocket& SelectedSocket = Skeleton->Sockets[ActiveState->SelectedSocketIndex];

                        // 매 프레임 Socket 값을 EditSocket에 동기화 (기즈모 변경 반영)
                        // UI 슬라이더가 활성화되어 있지 않을 때만 동기화 (드래그 중엔 유지)
                        if (!ImGui::IsAnyItemActive())
                        {
                            ActiveState->EditSocketLocation = SelectedSocket.RelativeLocation;
                            ActiveState->EditSocketRotation = SelectedSocket.RelativeRotation.ToEulerZYXDeg();
                            ActiveState->EditSocketScale = SelectedSocket.RelativeScale;
                        }

                        // Selected socket header
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.9f, 0.4f, 1.0f));
                        ImGui::Text("> Selected Socket");
                        ImGui::PopStyleColor();

                        ImGui::Spacing();
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.95f, 1.00f, 1.0f));
                        ImGui::TextWrapped("%s", SelectedSocket.SocketName.c_str());
                        ImGui::PopStyleColor();

                        ImGui::Spacing();
                        ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.45f, 0.55f, 0.70f, 0.8f));
                        ImGui::Separator();
                        ImGui::PopStyleColor();

                        // 소켓 이름 편집
                        ImGui::Text("Socket Name:");
                        char SocketNameBuf[128];
                        strncpy_s(SocketNameBuf, SelectedSocket.SocketName.c_str(), sizeof(SocketNameBuf) - 1);
                        ImGui::PushItemWidth(-1);
                        ImGui::InputText("##SocketName", SocketNameBuf, sizeof(SocketNameBuf));
                        // Enter 또는 포커스 벗어날 때 적용
                        if (ImGui::IsItemDeactivatedAfterEdit())
                        {
                            // 중복 이름 체크
                            FString NewName = SocketNameBuf;
                            bool bDuplicate = false;
                            for (int32 i = 0; i < static_cast<int32>(Skeleton->Sockets.size()); ++i)
                            {
                                if (i != ActiveState->SelectedSocketIndex && Skeleton->Sockets[i].SocketName == NewName)
                                {
                                    bDuplicate = true;
                                    break;
                                }
                            }
                            if (!bDuplicate && !NewName.empty())
                            {
                                SelectedSocket.SocketName = NewName;
                                ActiveState->bSocketTransformChanged = true;
                            }
                        }
                        ImGui::PopItemWidth();

                        ImGui::Spacing();
                        ImGui::Text("Attached Bone: %s", SelectedSocket.BoneName.c_str());

                        ImGui::Spacing();
                        ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.45f, 0.55f, 0.70f, 0.8f));
                        ImGui::Separator();
                        ImGui::PopStyleColor();
                        ImGui::Spacing();

                        // Location 편집
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.5f, 1.0f));
                        ImGui::Text("Relative Location");
                        ImGui::PopStyleColor();

                        ImGui::PushItemWidth(-1);
                        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.28f, 0.20f, 0.20f, 0.6f));
                        bool bSocketChanged = false;
                        bSocketChanged |= ImGui::DragFloat("##SocketLocX", &ActiveState->EditSocketLocation.X, 0.1f, 0.0f, 0.0f, "X: %.3f");
                        bSocketChanged |= ImGui::DragFloat("##SocketLocY", &ActiveState->EditSocketLocation.Y, 0.1f, 0.0f, 0.0f, "Y: %.3f");
                        bSocketChanged |= ImGui::DragFloat("##SocketLocZ", &ActiveState->EditSocketLocation.Z, 0.1f, 0.0f, 0.0f, "Z: %.3f");
                        ImGui::PopStyleColor();
                        ImGui::PopItemWidth();

                        ImGui::Spacing();

                        // Rotation 편집
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 1.0f, 0.5f, 1.0f));
                        ImGui::Text("Relative Rotation");
                        ImGui::PopStyleColor();

                        ImGui::PushItemWidth(-1);
                        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.20f, 0.28f, 0.20f, 0.6f));
                        bSocketChanged |= ImGui::DragFloat("##SocketRotX", &ActiveState->EditSocketRotation.X, 0.5f, -180.0f, 180.0f, "X: %.2f°");
                        bSocketChanged |= ImGui::DragFloat("##SocketRotY", &ActiveState->EditSocketRotation.Y, 0.5f, -180.0f, 180.0f, "Y: %.2f°");
                        bSocketChanged |= ImGui::DragFloat("##SocketRotZ", &ActiveState->EditSocketRotation.Z, 0.5f, -180.0f, 180.0f, "Z: %.2f°");
                        ImGui::PopStyleColor();
                        ImGui::PopItemWidth();

                        ImGui::Spacing();

                        // Scale 편집
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 1.0f, 1.0f));
                        ImGui::Text("Relative Scale");
                        ImGui::PopStyleColor();

                        ImGui::PushItemWidth(-1);
                        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.20f, 0.20f, 0.28f, 0.6f));
                        bSocketChanged |= ImGui::DragFloat("##SocketScaleX", &ActiveState->EditSocketScale.X, 0.01f, 0.001f, 100.0f, "X: %.3f");
                        bSocketChanged |= ImGui::DragFloat("##SocketScaleY", &ActiveState->EditSocketScale.Y, 0.01f, 0.001f, 100.0f, "Y: %.3f");
                        bSocketChanged |= ImGui::DragFloat("##SocketScaleZ", &ActiveState->EditSocketScale.Z, 0.01f, 0.001f, 100.0f, "Z: %.3f");
                        ImGui::PopStyleColor();
                        ImGui::PopItemWidth();

                        // 변경 사항을 소켓에 적용
                        if (bSocketChanged)
                        {
                            SelectedSocket.RelativeLocation = ActiveState->EditSocketLocation;
                            SelectedSocket.RelativeRotation = FQuat::MakeFromEulerZYX(ActiveState->EditSocketRotation);
                            SelectedSocket.RelativeScale = ActiveState->EditSocketScale;
                            ActiveState->bSocketTransformChanged = true;

                            // 앵커 위치도 동기화 (writeback 방지를 위해 Editable을 잠시 끔)
                            if (ActiveState->PreviewActor)
                            {
                                if (UBoneAnchorComponent* Anchor = ActiveState->PreviewActor->GetBoneGizmoAnchor())
                                {
                                    Anchor->SetEditability(false);
                                    Anchor->UpdateAnchorFromBone();
                                    Anchor->SetEditability(true);
                                }
                            }

                            // 이 소켓에 붙은 프리뷰 메시들 트랜스폼 dirty 플래그 설정
                            for (UStaticMeshComponent* PreviewMesh : ActiveState->SpawnedPreviewMeshes)
                            {
                                if (PreviewMesh && PreviewMesh->GetAttachSocketName().ToString() == SelectedSocket.SocketName)
                                {
                                    // 베이스 클래스 포인터로 캐스팅해서 public OnTransformUpdated 호출
                                    static_cast<USceneComponent*>(PreviewMesh)->OnTransformUpdated();
                                }
                            }
                        }

                        ImGui::Spacing();
                        ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.45f, 0.55f, 0.70f, 0.8f));
                        ImGui::Separator();
                        ImGui::PopStyleColor();
                        ImGui::Spacing();

                        // 저장 버튼
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.60f, 0.40f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.70f, 0.50f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.50f, 0.30f, 1.0f));
                        if (ImGui::Button("Save Skeleton Data", ImVec2(-1, 30)))
                        {
                            if (SaveSkeletonData(ActiveState))
                            {
                                UE_LOG("Skeleton data with sockets saved successfully");
                            }
                        }
                        ImGui::PopStyleColor(3);

                        if (ActiveState->bSocketTransformChanged)
                        {
                            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "* Unsaved changes");
                        }
                    }
                }
                else if (ActiveState->SelectedBoneIndex >= 0 && ActiveState->CurrentMesh) // 선택된 본의 트랜스폼 편집 UI
                {
                    const FSkeleton* Skeleton = ActiveState->CurrentMesh->GetSkeleton();
                    if (Skeleton && ActiveState->SelectedBoneIndex < Skeleton->Bones.size())
                    {
                        const FBone& SelectedBone = Skeleton->Bones[ActiveState->SelectedBoneIndex];

                        // Selected bone header with icon-like prefix
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.90f, 0.40f, 1.0f));
                        ImGui::Text("> Selected Bone");
                        ImGui::PopStyleColor();

                        ImGui::Spacing();
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.95f, 1.00f, 1.0f));
                        ImGui::TextWrapped("%s", SelectedBone.Name.c_str());
                        ImGui::PopStyleColor();

                        ImGui::Spacing();
                        ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.45f, 0.55f, 0.70f, 0.8f));
                        ImGui::Separator();
                        ImGui::PopStyleColor();

                        // 본의 현재 트랜스폼 가져오기 (편집 중이 아닐 때만)
                        if (!ActiveState->bBoneRotationEditing)
                        {
                            UpdateBoneTransformFromSkeleton(ActiveState);
                        }

                        ImGui::Spacing();

                        // Location 편집
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.5f, 1.0f));
                        ImGui::Text("Location");
                        ImGui::PopStyleColor();

                        ImGui::PushItemWidth(-1);
                        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.28f, 0.20f, 0.20f, 0.6f));
                        bool bLocationChanged = false;
                        bLocationChanged |= ImGui::DragFloat("##BoneLocX", &ActiveState->EditBoneLocation.X, 0.1f, 0.0f, 0.0f, "X: %.3f");
                        bLocationChanged |= ImGui::DragFloat("##BoneLocY", &ActiveState->EditBoneLocation.Y, 0.1f, 0.0f, 0.0f, "Y: %.3f");
                        bLocationChanged |= ImGui::DragFloat("##BoneLocZ", &ActiveState->EditBoneLocation.Z, 0.1f, 0.0f, 0.0f, "Z: %.3f");
                        ImGui::PopStyleColor();
                        ImGui::PopItemWidth();

                        if (bLocationChanged)
                        {
                            ApplyBoneTransform(ActiveState);
                            ActiveState->bBoneLinesDirty = true;
                        }

                        ImGui::Spacing();

                        // Rotation 편집
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 1.0f, 0.5f, 1.0f));
                        ImGui::Text("Rotation");
                        ImGui::PopStyleColor();

                        ImGui::PushItemWidth(-1);
                        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.20f, 0.28f, 0.20f, 0.6f));
                        bool bRotationChanged = false;

                        if (ImGui::IsAnyItemActive())
                        {
                            ActiveState->bBoneRotationEditing = true;
                        }

                        bRotationChanged |= ImGui::DragFloat("##BoneRotX", &ActiveState->EditBoneRotation.X, 0.5f, -180.0f, 180.0f, "X: %.2f°");
                        bRotationChanged |= ImGui::DragFloat("##BoneRotY", &ActiveState->EditBoneRotation.Y, 0.5f, -180.0f, 180.0f, "Y: %.2f°");
                        bRotationChanged |= ImGui::DragFloat("##BoneRotZ", &ActiveState->EditBoneRotation.Z, 0.5f, -180.0f, 180.0f, "Z: %.2f°");
                        ImGui::PopStyleColor();
                        ImGui::PopItemWidth();

                        if (!ImGui::IsAnyItemActive())
                        {
                            ActiveState->bBoneRotationEditing = false;
                        }

                        if (bRotationChanged)
                        {
                            ApplyBoneTransform(ActiveState);
                            ActiveState->bBoneLinesDirty = true;
                        }

                        ImGui::Spacing();

                        // Scale 편집
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 1.0f, 1.0f));
                        ImGui::Text("Scale");
                        ImGui::PopStyleColor();

                        ImGui::PushItemWidth(-1);
                        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.20f, 0.20f, 0.28f, 0.6f));
                        bool bScaleChanged = false;
                        bScaleChanged |= ImGui::DragFloat("##BoneScaleX", &ActiveState->EditBoneScale.X, 0.01f, 0.001f, 100.0f, "X: %.3f");
                        bScaleChanged |= ImGui::DragFloat("##BoneScaleY", &ActiveState->EditBoneScale.Y, 0.01f, 0.001f, 100.0f, "Y: %.3f");
                        bScaleChanged |= ImGui::DragFloat("##BoneScaleZ", &ActiveState->EditBoneScale.Z, 0.01f, 0.001f, 100.0f, "Z: %.3f");
                        ImGui::PopStyleColor();
                        ImGui::PopItemWidth();

                        if (bScaleChanged)
                        {
                            ApplyBoneTransform(ActiveState);
                            ActiveState->bBoneLinesDirty = true;
                        }
                    }
                }
                else if (ActiveState->SelectedPreviewMesh) // Preview Mesh Properties
                {
                    UStaticMeshComponent* PreviewMesh = ActiveState->SelectedPreviewMesh;

                    // Header
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.6f, 0.2f, 1.0f));
                    ImGui::Text("> Preview Mesh");
                    ImGui::PopStyleColor();

                    ImGui::Spacing();
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.95f, 1.00f, 1.0f));
                    ImGui::TextWrapped("%s", PreviewMesh->GetName().c_str());
                    ImGui::PopStyleColor();

                    ImGui::Spacing();
                    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.45f, 0.55f, 0.70f, 0.8f));
                    ImGui::Separator();
                    ImGui::PopStyleColor();

                    // 현재 트랜스폼 값 가져오기 (UI 드래그 중이 아닐 때만)
                    if (!ImGui::IsAnyItemActive())
                    {
                        ActiveState->EditPreviewMeshLocation = PreviewMesh->GetRelativeLocation();
                        ActiveState->EditPreviewMeshRotation = PreviewMesh->GetRelativeRotation().ToEulerZYXDeg();
                        ActiveState->EditPreviewMeshScale = PreviewMesh->GetRelativeScale();
                    }

                    ImGui::Spacing();
                    ImGui::Text("Transform");
                    ImGui::Spacing();

                    // Location 편집
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.5f, 1.0f));
                    ImGui::Text("Location");
                    ImGui::PopStyleColor();

                    ImGui::PushItemWidth(-1);
                    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.20f, 0.20f, 0.28f, 0.6f));
                    bool bLocChanged = false;
                    bLocChanged |= ImGui::DragFloat("##PreviewLocX", &ActiveState->EditPreviewMeshLocation.X, 0.01f, -1000.0f, 1000.0f, "X: %.3f");
                    bLocChanged |= ImGui::DragFloat("##PreviewLocY", &ActiveState->EditPreviewMeshLocation.Y, 0.01f, -1000.0f, 1000.0f, "Y: %.3f");
                    bLocChanged |= ImGui::DragFloat("##PreviewLocZ", &ActiveState->EditPreviewMeshLocation.Z, 0.01f, -1000.0f, 1000.0f, "Z: %.3f");
                    ImGui::PopStyleColor();
                    ImGui::PopItemWidth();

                    if (bLocChanged)
                    {
                        PreviewMesh->SetRelativeLocation(ActiveState->EditPreviewMeshLocation);
                        // 기즈모 위치 업데이트
                        if (UBoneAnchorComponent* Anchor = ActiveState->PreviewActor->GetBoneGizmoAnchor())
                        {
                            Anchor->UpdateAnchorFromBone();
                        }
                    }

                    ImGui::Spacing();

                    // Rotation 편집
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 1.0f, 0.5f, 1.0f));
                    ImGui::Text("Rotation");
                    ImGui::PopStyleColor();

                    ImGui::PushItemWidth(-1);
                    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.20f, 0.20f, 0.28f, 0.6f));
                    bool bRotChanged = false;
                    bRotChanged |= ImGui::DragFloat("##PreviewRotX", &ActiveState->EditPreviewMeshRotation.X, 0.5f, -180.0f, 180.0f, "X: %.2f°");
                    bRotChanged |= ImGui::DragFloat("##PreviewRotY", &ActiveState->EditPreviewMeshRotation.Y, 0.5f, -180.0f, 180.0f, "Y: %.2f°");
                    bRotChanged |= ImGui::DragFloat("##PreviewRotZ", &ActiveState->EditPreviewMeshRotation.Z, 0.5f, -180.0f, 180.0f, "Z: %.2f°");
                    ImGui::PopStyleColor();
                    ImGui::PopItemWidth();

                    if (bRotChanged)
                    {
                        PreviewMesh->SetRelativeRotation(FQuat::MakeFromEulerZYX(ActiveState->EditPreviewMeshRotation));
                        // 기즈모 위치 업데이트
                        if (UBoneAnchorComponent* Anchor = ActiveState->PreviewActor->GetBoneGizmoAnchor())
                        {
                            Anchor->UpdateAnchorFromBone();
                        }
                    }

                    ImGui::Spacing();

                    // Relative Scale 편집
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 1.0f, 1.0f));
                    ImGui::Text("Relative Scale");
                    ImGui::PopStyleColor();

                    ImGui::PushItemWidth(-1);
                    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.20f, 0.20f, 0.28f, 0.6f));
                    bool bScaleChanged = false;
                    bScaleChanged |= ImGui::DragFloat("##PreviewScaleX", &ActiveState->EditPreviewMeshScale.X, 0.01f, 0.001f, 100.0f, "X: %.3f");
                    bScaleChanged |= ImGui::DragFloat("##PreviewScaleY", &ActiveState->EditPreviewMeshScale.Y, 0.01f, 0.001f, 100.0f, "Y: %.3f");
                    bScaleChanged |= ImGui::DragFloat("##PreviewScaleZ", &ActiveState->EditPreviewMeshScale.Z, 0.01f, 0.001f, 100.0f, "Z: %.3f");
                    ImGui::PopStyleColor();
                    ImGui::PopItemWidth();

                    if (bScaleChanged)
                    {
                        PreviewMesh->SetRelativeScale(ActiveState->EditPreviewMeshScale);
                    }

                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();

                    // Delete 버튼
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
                    if (ImGui::Button("Delete Preview Mesh", ImVec2(-1, 0)))
                    {
                        ActiveState->SpawnedPreviewMeshes.Remove(PreviewMesh);
                        ActiveState->PreviewActor->RemoveOwnedComponent(PreviewMesh);
                        ActiveState->SelectedPreviewMesh = nullptr;
                        ActiveState->bPreviewMeshTransformChanged = false;
                    }
                    ImGui::PopStyleColor(2);
                }
                else
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
                    ImGui::TextWrapped("Select a bone, body, or constraint from the hierarchy to edit its properties.");
                    ImGui::PopStyleColor();
                }
            }
            ImGui::EndChild(); // "BonePropertiesChild"

            ImGui::Separator();

            // 3. 하단: "Asset Browser"를 위한 자식 창 (남은 공간 모두 사용)
            ImGui::BeginChild("AssetBrowserChild", ImVec2(0, 0), true);
            {
                DrawAssetBrowserPanel(ActiveState);
            }
            ImGui::EndChild(); // "AssetBrowserChild"
        }
        ImGui::EndChild(); // "RightPanel"

        // Pop the ItemSpacing style
        ImGui::PopStyleVar();
    }
    ImGui::End();

    // If collapsed or not visible, clear the center rect so we don't render a floating viewport
    if (!bViewerVisible)
    {
        CenterRect = FRect(0, 0, 0, 0);
        CenterRect.UpdateMinMax();
    }

    // If window was closed via X button, notify the manager to clean up
    if (!bIsOpen)
    {
        USlateManager::GetInstance().CloseSkeletalMeshViewer();
    }

    bRequestFocus = false;
}

void SSkeletalMeshViewerWindow::OnUpdate(float DeltaSeconds)
{
    if (!ActiveState || !ActiveState->Viewport)
        return;

    if (ActiveState && ActiveState->Client)
    {
        ActiveState->Client->Tick(DeltaSeconds);
    }

    if (ActiveState->bTimeChanged)
    {
        ActiveState->bBoneLinesDirty = true;
    }

    // ============================================================
    // 몽타주 모드 재생 처리
    // ============================================================
    UAnimMontage* ActiveMontage = nullptr;
    if (ActiveState->AssetBrowserMode == EAssetBrowserMode::Montage)
    {
        ActiveMontage = ActiveState->bCreatingNewMontage ? ActiveState->EditingMontage : ActiveState->CurrentMontage;
    }

    if (ActiveMontage && ActiveMontage->HasSections())
    {
        // 섹션별 시작 시간/길이 계산
        float MontageTotalLength = 0.0f;
        TArray<float> SectionStartTimes;
        TArray<float> SectionLengths;

        for (int32 i = 0; i < ActiveMontage->GetNumSections(); ++i)
        {
            SectionStartTimes.Add(MontageTotalLength);
            UAnimSequence* Seq = ActiveMontage->GetSectionSequence(i);
            float Len = Seq ? Seq->GetPlayLength() : 0.0f;
            SectionLengths.Add(Len);
            MontageTotalLength += Len;
        }

        // 현재 섹션 찾기 (PlayRate 적용을 위해 먼저 계산)
        int32 CurrentPlaySection = 0;
        for (int32 i = 0; i < SectionStartTimes.Num(); ++i)
        {
            float SectionEnd = SectionStartTimes[i] + SectionLengths[i];
            if (ActiveState->MontagePreviewTime < SectionEnd || i == SectionStartTimes.Num() - 1)
            {
                CurrentPlaySection = i;
                break;
            }
        }

        // 섹션별 PlayRate 가져오기
        float SectionPlayRate = 1.0f;
        if (CurrentPlaySection >= 0 && CurrentPlaySection < ActiveMontage->GetNumSections())
        {
            SectionPlayRate = ActiveMontage->Sections[CurrentPlaySection].PlayRate;
        }

        // 재생 시간 진행 (섹션 PlayRate 적용)
        if (ActiveState->bIsPlaying && MontageTotalLength > 0.0f)
        {
            ActiveState->MontagePreviewTime += DeltaSeconds * SectionPlayRate;
            if (ActiveState->MontagePreviewTime >= MontageTotalLength)
            {
                if (ActiveState->bIsLooping || ActiveMontage->bLoop)
                    ActiveState->MontagePreviewTime = std::fmodf(ActiveState->MontagePreviewTime, MontageTotalLength);
                else
                {
                    ActiveState->MontagePreviewTime = MontageTotalLength;
                    ActiveState->bIsPlaying = false;
                }
            }
        }
        else if (ActiveState->bIsPlayingReverse && MontageTotalLength > 0.0f)
        {
            ActiveState->MontagePreviewTime -= DeltaSeconds * SectionPlayRate;
            if (ActiveState->MontagePreviewTime < 0.0f)
            {
                if (ActiveState->bIsLooping || ActiveMontage->bLoop)
                    ActiveState->MontagePreviewTime += MontageTotalLength;
                else
                {
                    ActiveState->MontagePreviewTime = 0.0f;
                    ActiveState->bIsPlayingReverse = false;
                }
            }
        }

        // 현재 섹션 찾기
        int32 NewSection = 0;
        float LocalTime = ActiveState->MontagePreviewTime;
        for (int32 i = 0; i < SectionStartTimes.Num(); ++i)
        {
            float SectionEnd = SectionStartTimes[i] + SectionLengths[i];
            if (ActiveState->MontagePreviewTime < SectionEnd || i == SectionStartTimes.Num() - 1)
            {
                NewSection = i;
                LocalTime = ActiveState->MontagePreviewTime - SectionStartTimes[i];
                LocalTime = FMath::Clamp(LocalTime, 0.0f, SectionLengths[i]);
                break;
            }
        }

        // 섹션 변경 감지 -> 블렌딩 시작
        if (NewSection != ActiveState->CurrentMontageSection)
        {
            if (ActiveState->CurrentMontageSection >= 0 && NewSection < ActiveMontage->GetNumSections())
            {
                float BlendTime = ActiveMontage->Sections[NewSection].BlendInTime;
                UE_LOG("[Montage] Section %d -> %d, BlendTime: %.3f", ActiveState->CurrentMontageSection, NewSection, BlendTime);
                if (BlendTime > 0.0f)
                {
                    ActiveState->PrevMontageSection = ActiveState->CurrentMontageSection;
                    ActiveState->PrevSectionEndTime = SectionLengths[ActiveState->CurrentMontageSection] - 0.001f;
                    ActiveState->SectionBlendTime = 0.0f;
                    ActiveState->SectionBlendDuration = BlendTime;
                }
                else
                {
                    ActiveState->PrevMontageSection = -1;
                }
            }
            ActiveState->CurrentMontageSection = NewSection;
        }

        // 블렌드 진행
        bool bBlending = (ActiveState->PrevMontageSection >= 0 &&
                          ActiveState->SectionBlendDuration > 0.0f &&
                          ActiveState->SectionBlendTime < ActiveState->SectionBlendDuration);

        if (ActiveState->PrevMontageSection >= 0)
        {
            ActiveState->SectionBlendTime += DeltaSeconds;
            if (ActiveState->SectionBlendTime >= ActiveState->SectionBlendDuration)
            {
                ActiveState->PrevMontageSection = -1;
                bBlending = false;
            }
        }

        // 애니메이션 설정
        UAnimSequence* CurrSeq = ActiveMontage->GetSectionSequence(NewSection);
        USkeletalMeshComponent* MeshComp = nullptr;
        if (CurrSeq && ActiveState->PreviewActor)
        {
            MeshComp = ActiveState->PreviewActor->GetSkeletalMeshComponent();
            if (MeshComp)
            {
                if (ActiveState->CurrentAnimation != CurrSeq)
                {
                    ActiveState->CurrentAnimation = CurrSeq;
                    MeshComp->SetAnimation(CurrSeq);
                }
                MeshComp->SetAnimationTime(LocalTime);
                ActiveState->CurrentAnimTime = LocalTime;
                MeshComp->SetPlaying(ActiveState->bIsPlaying || ActiveState->bIsPlayingReverse);
            }
        }

        ActiveState->bBoneLinesDirty = true;
        ActiveState->bTimeChanged = false;

        // World Tick (TickAnimation 실행됨)
        if (ActiveState->World)
        {
            FPhysScene* PhysScene = ActiveState->World->GetPhysScene();
            if (PhysScene && ActiveState->bSimulatePhysics)
                PhysScene->WaitForSimulation();
            ActiveState->World->Tick(DeltaSeconds);
            if (PhysScene && ActiveState->bSimulatePhysics)
                PhysScene->StepSimulation(DeltaSeconds);
            if (ActiveState->World->GetGizmoActor())
                ActiveState->World->GetGizmoActor()->ProcessGizmoModeSwitch();
        }

        // 블렌딩 포즈 적용 (World->Tick 이후, TickAnimation 결과 덮어쓰기)
        if (bBlending && MeshComp && ActiveState->PrevMontageSection >= 0)
        {
            UAnimSequence* PrevSeq = ActiveMontage->GetSectionSequence(ActiveState->PrevMontageSection);
            if (PrevSeq && CurrSeq)
            {
                USkeletalMesh* SkelMesh = MeshComp->GetSkeletalMesh();
                if (!SkelMesh || !SkelMesh->GetSkeletalMeshData()) return;
                const FSkeleton& Skeleton = SkelMesh->GetSkeletalMeshData()->Skeleton;
                int32 SkeletonBones = Skeleton.Bones.Num();

                // 현재 로컬 포즈 복사 (기본값)
                TArray<FTransform> BlendedPose = MeshComp->GetCurrentLocalSpacePose();
                if (BlendedPose.Num() != SkeletonBones) return;

                float Alpha = ActiveState->SectionBlendTime / ActiveState->SectionBlendDuration;
                Alpha = FMath::Clamp(Alpha, 0.0f, 1.0f);

                // 이전 애니메이션 포즈 추출 및 매핑
                UAnimDataModel* PrevModel = PrevSeq->GetDataModel();
                UAnimDataModel* CurrModel = CurrSeq->GetDataModel();
                if (!PrevModel || !CurrModel) return;

                FAnimExtractContext PrevCtx(ActiveState->PrevSectionEndTime, true);
                FAnimExtractContext CurrCtx(LocalTime, true);
                FPoseContext PrevPoseCtx(PrevModel->GetNumBoneTracks());
                FPoseContext CurrPoseCtx(CurrModel->GetNumBoneTracks());

                PrevSeq->GetAnimationPose(PrevPoseCtx, PrevCtx);
                CurrSeq->GetAnimationPose(CurrPoseCtx, CurrCtx);

                const TArray<FBoneAnimationTrack>& PrevTracks = PrevModel->GetBoneAnimationTracks();
                const TArray<FBoneAnimationTrack>& CurrTracks = CurrModel->GetBoneAnimationTracks();

                // 본 이름으로 매핑하여 블렌딩
                for (int32 i = 0; i < CurrTracks.Num(); ++i)
                {
                    int32 BoneIdx = Skeleton.FindBoneIndex(CurrTracks[i].Name);
                    if (BoneIdx == INDEX_NONE || BoneIdx >= SkeletonBones) continue;

                    FTransform CurrTrans = CurrPoseCtx.Pose[i];
                    FTransform PrevTrans = CurrTrans; // 기본값

                    // 이전 애니메이션에서 같은 본 찾기
                    for (int32 j = 0; j < PrevTracks.Num(); ++j)
                    {
                        if (PrevTracks[j].Name == CurrTracks[i].Name)
                        {
                            PrevTrans = PrevPoseCtx.Pose[j];
                            break;
                        }
                    }

                    // 블렌딩
                    BlendedPose[BoneIdx].Translation = FMath::Lerp(PrevTrans.Translation, CurrTrans.Translation, Alpha);
                    BlendedPose[BoneIdx].Rotation = FQuat::Slerp(PrevTrans.Rotation, CurrTrans.Rotation, Alpha);
                    BlendedPose[BoneIdx].Scale3D = FMath::Lerp(PrevTrans.Scale3D, CurrTrans.Scale3D, Alpha);
                }

                MeshComp->SetAnimationPose(BlendedPose);
            }
        }
        return;
    }

    if (!ActiveState->CurrentAnimation || !ActiveState->CurrentAnimation->GetDataModel())
    {
        if (ActiveState->World)
        {
            // 물리 시뮬레이션이 활성화된 경우, PhysScene을 별도로 Tick
            FPhysScene* PhysScene = ActiveState->World->GetPhysScene();
            if (PhysScene && ActiveState->bSimulatePhysics)
            {
                // 물리 결과 수확 (이전 프레임의 시뮬레이션 결과)
                PhysScene->WaitForSimulation();
            }

            ActiveState->World->Tick(DeltaSeconds);

            // 물리 시뮬레이션 시작 (non-blocking)
            if (PhysScene && ActiveState->bSimulatePhysics)
            {
                PhysScene->StepSimulation(DeltaSeconds);
            }

            if (ActiveState->World->GetGizmoActor())
                ActiveState->World->GetGizmoActor()->ProcessGizmoModeSwitch();
        }

        if(ActiveState->bTimeChanged)
        {
             ActiveState->bTimeChanged = false;
        }
        return;
    }

    UAnimDataModel* DataModel = nullptr;
    if (ActiveState->CurrentAnimation)
    {
        DataModel = ActiveState->CurrentAnimation->GetDataModel();
    }

    bool bIsPlaying = ActiveState->bIsPlaying || ActiveState->bIsPlayingReverse;
    bool bTimeAdvancedThisFrame = false;

    if (DataModel && DataModel->GetPlayLength() > 0.0f)
    {
        if (ActiveState->bIsPlaying)
        {
            ActiveState->CurrentAnimTime += DeltaSeconds;
            if (ActiveState->CurrentAnimTime > DataModel->GetPlayLength())
            {
                if (ActiveState->bIsLooping)
                {
                    ActiveState->CurrentAnimTime = std::fmodf(ActiveState->CurrentAnimTime, DataModel->GetPlayLength());
                }
                else
                {
                    ActiveState->CurrentAnimTime = DataModel->GetPlayLength();
                    ActiveState->bIsPlaying = false;
                }
            }
            bTimeAdvancedThisFrame = true;
        }
        else if (ActiveState->bIsPlayingReverse)
        {
            ActiveState->CurrentAnimTime -= DeltaSeconds;
            if (ActiveState->CurrentAnimTime < 0.0f) 
            {
                if (ActiveState->bIsLooping)
                {
                    ActiveState->CurrentAnimTime += DataModel->GetPlayLength();
                }
                else
                {
                    ActiveState->CurrentAnimTime = 0.0f;
                    ActiveState->bIsPlayingReverse = false;
                }
            }
            bTimeAdvancedThisFrame = true;
        }
    }

    bool bUpdateAnimation = bTimeAdvancedThisFrame || ActiveState->bTimeChanged;

    if (ActiveState->PreviewActor && ActiveState->PreviewActor->GetSkeletalMeshComponent())
    {
        auto SkeletalMeshComponent = ActiveState->PreviewActor->GetSkeletalMeshComponent();
        
        SkeletalMeshComponent->SetPlaying(bIsPlaying);
        SkeletalMeshComponent->SetLooping(ActiveState->bIsLooping);

        if (bUpdateAnimation)
        {
            bool OriginalPlaying = SkeletalMeshComponent->IsPlaying();
            if (ActiveState->bTimeChanged) 
            {
                SkeletalMeshComponent->SetPlaying(true);
            }
            
            SkeletalMeshComponent->SetAnimationTime(ActiveState->CurrentAnimTime);

            if (ActiveState->bTimeChanged) 
            {
                SkeletalMeshComponent->SetPlaying(OriginalPlaying);
                ActiveState->bTimeChanged = false; 
            }

            ActiveState->bBoneLinesDirty = true;
        }

        // 소켓에 붙은 프리뷰 메시들의 트랜스폼 업데이트
        for (UStaticMeshComponent* PreviewMesh : ActiveState->SpawnedPreviewMeshes)
        {
            if (!PreviewMesh) continue;

            FName SocketName = PreviewMesh->GetAttachSocketName();
            if (!SocketName.IsValid()) continue;

            FTransform SocketTransform = SkeletalMeshComponent->GetSocketTransform(SocketName);
            FTransform LocalTransform(PreviewMesh->GetRelativeLocation(), PreviewMesh->GetRelativeRotation(), PreviewMesh->GetRelativeScale());
            FTransform WorldTransform = SocketTransform.GetWorldTransform(LocalTransform);
            PreviewMesh->SetWorldTransform(WorldTransform);
        }
    }

    if (ActiveState->World)
    {
        // 물리 시뮬레이션이 활성화된 경우, PhysScene을 별도로 Tick
        // World->Tick()은 bPie 플래그로 물리를 제어하지만, 뷰어는 별도로 관리
        FPhysScene* PhysScene = ActiveState->World->GetPhysScene();
        if (PhysScene && ActiveState->bSimulatePhysics)
        {
            // 물리 결과 수확 (이전 프레임의 시뮬레이션 결과)
            PhysScene->WaitForSimulation();
        }

        ActiveState->World->Tick(DeltaSeconds);

        // 물리 시뮬레이션 시작 (non-blocking)
        if (PhysScene && ActiveState->bSimulatePhysics)
        {
            PhysScene->StepSimulation(DeltaSeconds);
        }

        if (ActiveState->World->GetGizmoActor())
            ActiveState->World->GetGizmoActor()->ProcessGizmoModeSwitch();
    }
}

void SSkeletalMeshViewerWindow::OnMouseMove(FVector2D MousePos)
{
    if (!ActiveState || !ActiveState->Viewport) return;

    if (CenterRect.Contains(MousePos))
    {
        FVector2D LocalPos = MousePos - FVector2D(CenterRect.Left, CenterRect.Top);
        ActiveState->Viewport->ProcessMouseMove((int32)LocalPos.X, (int32)LocalPos.Y);
    }
}

void SSkeletalMeshViewerWindow::OnMouseDown(FVector2D MousePos, uint32 Button)
{
    if (!ActiveState || !ActiveState->Viewport) return;

    if (CenterRect.Contains(MousePos))
    {
        FVector2D LocalPos = MousePos - FVector2D(CenterRect.Left, CenterRect.Top);

        // First, always try gizmo picking (pass to viewport)
        ActiveState->Viewport->ProcessMouseButtonDown((int32)LocalPos.X, (int32)LocalPos.Y, (int32)Button);

        // Left click: if no gizmo was picked, try bone picking
        if (Button == 0 && ActiveState->PreviewActor && ActiveState->CurrentMesh && ActiveState->Client && ActiveState->World)
        {
            // Check if gizmo was picked by checking selection
            UActorComponent* SelectedComp = ActiveState->World->GetSelectionManager()->GetSelectedComponent();

            // Only do bone picking if gizmo wasn't selected
            if (!SelectedComp || !Cast<UBoneAnchorComponent>(SelectedComp))
            {
                // Get camera from viewport client
                ACameraActor* Camera = ActiveState->Client->GetCamera();
                if (Camera)
                {
                    // Get camera vectors
                    FVector CameraPos = Camera->GetActorLocation();
                    FVector CameraRight = Camera->GetRight();
                    FVector CameraUp = Camera->GetUp();
                    FVector CameraForward = Camera->GetForward();

                    // Calculate viewport-relative mouse position
                    FVector2D ViewportMousePos(MousePos.X - CenterRect.Left, MousePos.Y - CenterRect.Top);
                    FVector2D ViewportSize(CenterRect.GetWidth(), CenterRect.GetHeight());

                    // Generate ray from mouse position
                    FRay Ray = MakeRayFromViewport(
                        Camera->GetViewMatrix(),
                        Camera->GetProjectionMatrix(CenterRect.GetWidth() / CenterRect.GetHeight(), ActiveState->Viewport),
                        CameraPos,
                        CameraRight,
                        CameraUp,
                        CameraForward,
                        ViewportMousePos,
                        ViewportSize
                    );

                    // Try to pick a bone
                    float HitDistance;
                    int32 PickedBoneIndex = ActiveState->PreviewActor->PickBone(Ray, HitDistance);

                    if (PickedBoneIndex >= 0)
                    {
                        // Bone was picked
                        ActiveState->SelectedBoneIndex = PickedBoneIndex;
                        ActiveState->bBoneLinesDirty = true;

                        ExpandToSelectedBone(ActiveState, PickedBoneIndex);

                        // Move gizmo to the selected bone
                        ActiveState->PreviewActor->RepositionAnchorToBone(PickedBoneIndex);
                        if (USceneComponent* Anchor = ActiveState->PreviewActor->GetBoneGizmoAnchor())
                        {
                            ActiveState->World->GetSelectionManager()->SelectActor(ActiveState->PreviewActor);
                            ActiveState->World->GetSelectionManager()->SelectComponent(Anchor);
                        }
                    }
                    else
                    {
                        // No bone was picked - clear selection
                        ActiveState->SelectedBoneIndex = -1;
                        ActiveState->bBoneLinesDirty = true;

                        // Hide gizmo and clear selection
                        if (UBoneAnchorComponent* Anchor = ActiveState->PreviewActor->GetBoneGizmoAnchor())
                        {
                            Anchor->SetVisibility(false);
                            Anchor->SetEditability(false);
                        }
                        ActiveState->World->GetSelectionManager()->ClearSelection();
                    }
                }
            }
        }
    }
}

void SSkeletalMeshViewerWindow::OnMouseUp(FVector2D MousePos, uint32 Button)
{
    if (!ActiveState || !ActiveState->Viewport) return;

    if (CenterRect.Contains(MousePos))
    {
        FVector2D LocalPos = MousePos - FVector2D(CenterRect.Left, CenterRect.Top);
        ActiveState->Viewport->ProcessMouseButtonUp((int32)LocalPos.X, (int32)LocalPos.Y, (int32)Button);
    }
}

void SSkeletalMeshViewerWindow::OnRenderViewport()
{
    if (ActiveState && ActiveState->Viewport && CenterRect.GetWidth() > 0 && CenterRect.GetHeight() > 0)
    {
        const uint32 NewStartX = static_cast<uint32>(CenterRect.Left);
        const uint32 NewStartY = static_cast<uint32>(CenterRect.Top);
        const uint32 NewWidth  = static_cast<uint32>(CenterRect.Right - CenterRect.Left);
        const uint32 NewHeight = static_cast<uint32>(CenterRect.Bottom - CenterRect.Top);
        ActiveState->Viewport->Resize(NewStartX, NewStartY, NewWidth, NewHeight);

        // 본 오버레이 재구축
        if (ActiveState->bShowBones)
        {
            ActiveState->bBoneLinesDirty = true;
        }
        if (ActiveState->PreviewActor && ActiveState->CurrentMesh)
        {
            if (ActiveState->bShowBones && ActiveState->bBoneLinesDirty)
            {
                if (ULineComponent* LineComp = ActiveState->PreviewActor->GetBoneLineComponent())
                {
                    LineComp->SetLineVisible(true);
                }
                ActiveState->PreviewActor->RebuildBoneLines(ActiveState->SelectedBoneIndex);
                ActiveState->bBoneLinesDirty = false;
            }
            
			// Rebuild physics body lines and constraint lines if needed
            if (ActiveState->PreviewActor->GetSkeletalMeshComponent()->GetPhysicsAsset())
            {
                ActiveState->PreviewActor->RebuildBodyLines(ActiveState->bChangedGeomNum, ActiveState->SelectedBodyIndex);

                ActiveState->PreviewActor->RebuildConstraintLines(ActiveState->SelectedConstraintIndex);

                // Rebuild constraint angular limit visualization
                ActiveState->PreviewActor->RebuildConstraintLimitLines(ActiveState->SelectedConstraintIndex);
            }
        }

        

        // 뷰포트 렌더링 (ImGui보다 먼저)
        ActiveState->Viewport->Render();
    }
}

void SSkeletalMeshViewerWindow::LoadPhysicsAsset(UPhysicsAsset* PhysicsAsset)
{
    if (!ActiveState)
    {
        return;
    }

    ActiveState->CurrentPhysicsAsset = PhysicsAsset;

    if (!PhysicsAsset)
    {
        return;
    }

    PhysicsAsset->BuildRuntimeCache();

    FBoneNameSet RequiredBones = GatherPhysicsAssetBones(PhysicsAsset);

    const bool bHasCompatibleMesh = ActiveState->CurrentMesh && SkeletonSupportsBones(ActiveState->CurrentMesh->GetSkeleton(), RequiredBones);

    if (!bHasCompatibleMesh)
    {
        if (USkeletalMesh* AutoMesh = FindCompatibleMesh(RequiredBones))
        {
            const FString& MeshPath = AutoMesh->GetFilePath();
            if (!MeshPath.empty())
            {
                LoadSkeletalMesh(MeshPath);
                UE_LOG("SSkeletalMeshViewerWindow: Auto-loaded %s for PhysicsAsset %s", MeshPath.c_str(), PhysicsAsset->GetName().ToString().c_str());
            }
        }
        else if (!RequiredBones.empty())
        {
            UE_LOG("SSkeletalMeshViewerWindow: No compatible skeletal mesh found for PhysicsAsset %s", PhysicsAsset->GetName().ToString().c_str());
        }
    }

	USkeletalMeshComponent* SkeletalComp = ActiveState->PreviewActor->GetSkeletalMeshComponent();
    assert(SkeletalComp);
    if(SkeletalComp)
    {
        SkeletalComp->SetPhysicsAsset(PhysicsAsset);
	}
}

void SSkeletalMeshViewerWindow::SetPhysicsAssetSavePath(const FString& SavePath)
{
    if (!ActiveState || !ActiveState->CurrentPhysicsAsset)
    {
        return;
    }

    ActiveState->CurrentPhysicsAsset->SetFilePath(SavePath);
}

bool SSkeletalMeshViewerWindow::SavePhysicsAsset(ViewerState* State)
{
    if (!State || !State->CurrentPhysicsAsset)
    {
        return false;
    }

    FString TargetPath = State->CurrentPhysicsAsset->GetFilePath();

    if (TargetPath.empty())
    {
        FWideString Initial = UTF8ToWide(GDataDir) + L"/NewPhysicsAsset.phys";
        std::filesystem::path Selected = FPlatformProcess::OpenSaveFileDialog(Initial, L"phys", L"Physics Asset (*.phys)");
        if (Selected.empty())
        {
            return false;
        }

        TargetPath = ResolveAssetRelativePath(WideToUTF8(Selected.wstring()), GDataDir);
        State->CurrentPhysicsAsset->SetFilePath(TargetPath);
    }

    if (State->CurrentPhysicsAsset->SaveToFile(TargetPath))
    {
        UResourceManager::GetInstance().Add<UPhysicsAsset>(TargetPath, State->CurrentPhysicsAsset);
        UE_LOG("Physics asset saved: %s", TargetPath.c_str());
        return true;
    }

    UE_LOG("Failed to save physics asset: %s", TargetPath.c_str());
    return false;
}

bool SSkeletalMeshViewerWindow::SaveSkeletonData(ViewerState* State)
{
    if (!State || !State->CurrentMesh)
    {
        return false;
    }

    const FSkeletalMeshData* MeshData = State->CurrentMesh->GetSkeletalMeshData();
    if (!MeshData)
    {
        UE_LOG("Failed to save skeleton: MeshData is null");
        return false;
    }

    FString CacheFilePath = MeshData->CacheFilePath;
    if (CacheFilePath.empty())
    {
        UE_LOG("Failed to save skeleton: CacheFilePath is empty");
        return false;
    }

    try
    {
        // 1. 바이너리 저장 (메쉬 데이터)
        FWindowsBinWriter Writer(CacheFilePath);
        Writer << *const_cast<FSkeletalMeshData*>(MeshData);
        Writer.Close();

        // 2. 소켓 데이터를 JSON으로 별도 저장 (원본 FBX 경로 기준)
        FString SocketJsonPath = MeshData->PathFileName + ".socket.json";
        const TArray<FSkeletalMeshSocket>& Sockets = MeshData->Skeleton.Sockets;

        JSON SocketArray = JSON::Make(JSON::Class::Array);

        for (const auto& Socket : Sockets)
        {
            JSON SocketObj;
            to_json(SocketObj, Socket);
            SocketArray.append(SocketObj);
        }

        if (!FJsonSerializer::SaveJsonToFile(SocketArray, UTF8ToWide(SocketJsonPath.c_str())))
        {
            UE_LOG("Failed to save socket data to: %s", SocketJsonPath.c_str());
        }
        else
        {
            UE_LOG("Socket data saved to: %s", SocketJsonPath.c_str());
        }

        UE_LOG("Skeleton data saved to: %s", CacheFilePath.c_str());
        State->bSocketTransformChanged = false;
        return true;
    }
    catch (const std::exception& e)
    {
        UE_LOG("Failed to save skeleton data: %s", e.what());
        return false;
    }
}

void SSkeletalMeshViewerWindow::OpenNewTab(const char* Name)
{
    ViewerState* State = SkeletalViewerBootstrap::CreateViewerState(Name, World, Device);
    if (!State) return;

    Tabs.Add(State);
    ActiveTabIndex = Tabs.Num() - 1;
    ActiveState = State;
}

void SSkeletalMeshViewerWindow::CloseTab(int Index)
{
    if (Index < 0 || Index >= Tabs.Num()) return;
    ViewerState* State = Tabs[Index];
    SkeletalViewerBootstrap::DestroyViewerState(State);
    Tabs.RemoveAt(Index);
    if (Tabs.Num() == 0) { ActiveTabIndex = -1; ActiveState = nullptr; }
    else { ActiveTabIndex = std::min(Index, Tabs.Num() - 1); ActiveState = Tabs[ActiveTabIndex]; }
}

void SSkeletalMeshViewerWindow::LoadSkeletalMesh(const FString& Path)
{
    if (!ActiveState || Path.empty())
        return;

    // Load the skeletal mesh using the resource manager
    USkeletalMesh* Mesh = UResourceManager::GetInstance().Load<USkeletalMesh>(Path);
    if (Mesh && ActiveState->PreviewActor)
    {
        // Set the mesh on the preview actor
        ActiveState->PreviewActor->SetSkeletalMesh(Path);
        ActiveState->CurrentMesh = Mesh;
        ActiveState->LoadedMeshPath = Path;  // Track for resource unloading

        ActiveState->CurrentPhysicsAsset = ActiveState->PreviewActor->GetSkeletalMeshComponent()->GetPhysicsAsset();

        // Update mesh path buffer for display in UI
        strncpy_s(ActiveState->MeshPathBuffer, Path.c_str(), sizeof(ActiveState->MeshPathBuffer) - 1);

        // Sync mesh visibility with checkbox state
        if (auto* Skeletal = ActiveState->PreviewActor->GetSkeletalMeshComponent())
        {
            Skeletal->SetVisibility(ActiveState->bShowMesh);
        }

        // Mark bone lines as dirty to rebuild on next frame
        ActiveState->bBoneLinesDirty = true;

        // Expand all bones by default so bone tree is fully visible on load
        ActiveState->ExpandedBoneIndices.clear();
        if (const FSkeleton* Skeleton = Mesh->GetSkeleton())
        {
            for (int32 i = 0; i < Skeleton->Bones.size(); ++i)
            {
                ActiveState->ExpandedBoneIndices.insert(i);
            }
        }

        // Clear and sync bone line visibility
        if (auto* LineComp = ActiveState->PreviewActor->GetBoneLineComponent())
        {
            LineComp->ClearLines();
            LineComp->SetLineVisible(ActiveState->bShowBones);
        }

        UE_LOG("SSkeletalMeshViewerWindow: Loaded skeletal mesh from %s", Path.c_str());
    }
    else
    {
        UE_LOG("SSkeletalMeshViewerWindow: Failed to load skeletal mesh from %s", Path.c_str());
    }
}

void SSkeletalMeshViewerWindow::UpdateBoneTransformFromSkeleton(ViewerState* State)
{
    if (!State || !State->CurrentMesh || State->SelectedBoneIndex < 0)
        return;
        
    // 본의 로컬 트랜스폼에서 값 추출
    const FTransform& BoneTransform = State->PreviewActor->GetSkeletalMeshComponent()->GetBoneLocalTransform(State->SelectedBoneIndex);
    State->EditBoneLocation = BoneTransform.Translation;
    State->EditBoneRotation = BoneTransform.Rotation.ToEulerZYXDeg();
    State->EditBoneScale = BoneTransform.Scale3D;
}

void SSkeletalMeshViewerWindow::ApplyBoneTransform(ViewerState* State)
{
    if (!State || !State->CurrentMesh || State->SelectedBoneIndex < 0)
        return;

    FTransform NewTransform(State->EditBoneLocation, FQuat::MakeFromEulerZYX(State->EditBoneRotation), State->EditBoneScale);
    State->PreviewActor->GetSkeletalMeshComponent()->SetBoneLocalTransform(State->SelectedBoneIndex, NewTransform);
}

void SSkeletalMeshViewerWindow::ExpandToSelectedBone(ViewerState* State, int32 BoneIndex)
{
    if (!State || !State->CurrentMesh)
        return;
        
    const FSkeleton* Skeleton = State->CurrentMesh->GetSkeleton();
    if (!Skeleton || BoneIndex < 0 || BoneIndex >= Skeleton->Bones.size())
        return;
    
    // 선택된 본부터 루트까지 모든 부모를 펼침
    int32 CurrentIndex = BoneIndex;
    while (CurrentIndex >= 0)
    {
        State->ExpandedBoneIndices.insert(CurrentIndex);
        CurrentIndex = Skeleton->Bones[CurrentIndex].ParentIndex;
    }
}

void SSkeletalMeshViewerWindow::DrawAnimationPanel(ViewerState* State)
{
    // ============================================================
    // 몽타주 모드 체크 (Montage 탭에서 몽타주 선택 시 자동 활성화)
    // ============================================================
    UAnimMontage* PreviewMontage = nullptr;
    bool bMontagePreviewing = false;

    // Montage 탭이고 몽타주가 선택되어 있으면 자동으로 몽타주 타임라인 표시
    if (State->AssetBrowserMode == EAssetBrowserMode::Montage)
    {
        PreviewMontage = State->bCreatingNewMontage ? State->EditingMontage : State->CurrentMontage;
        if (PreviewMontage && PreviewMontage->HasSections())
        {
            bMontagePreviewing = true;
        }
    }

    // 몽타주 전체 길이 및 섹션별 시작 시간 계산
    float MontageTotalLength = 0.0f;
    TArray<float> SectionStartTimes;  // 각 섹션의 시작 시간
    TArray<float> SectionLengths;     // 각 섹션의 길이
    if (bMontagePreviewing && PreviewMontage)
    {
        for (int32 i = 0; i < PreviewMontage->GetNumSections(); ++i)
        {
            SectionStartTimes.Add(MontageTotalLength);
            UAnimSequence* Seq = PreviewMontage->GetSectionSequence(i);
            float SecLen = Seq ? Seq->GetPlayLength() : 0.0f;
            SectionLengths.Add(SecLen);
            MontageTotalLength += SecLen;
        }
    }

    bool bHasAnimation = !!(State->CurrentAnimation) || bMontagePreviewing;

    UAnimDataModel* DataModel = nullptr;
    if (State->CurrentAnimation && !bMontagePreviewing)
    {
         DataModel = State->CurrentAnimation->GetDataModel();
    }

    float PlayLength = 0.0f;
    int32 FrameRate = 30;  // 기본 30fps
    int32 NumberOfFrames = 0;
    int32 NumberOfKeys = 0;

    if (bMontagePreviewing)
    {
        PlayLength = MontageTotalLength;
        NumberOfFrames = static_cast<int32>(MontageTotalLength * 30.0f);  // 30fps 기준
    }
    else if (DataModel)
    {
        PlayLength = DataModel->GetPlayLength();
        FrameRate = DataModel->GetFrameRate();
        NumberOfFrames = DataModel->GetNumberOfFrames();
        NumberOfKeys = DataModel->GetNumberOfKeys();
    }

    float FrameDuration = 0.0f;
    if (bHasAnimation && NumberOfFrames > 0)
    {
        FrameDuration = PlayLength / static_cast<float>(NumberOfFrames);
    }
    else
    {
        FrameDuration = (1.0f / 30.0f); // 애니메이션 없을 시 30fps로 가정
    }

    // 몽타주 미리보기 시 현재 시간 참조 변경
    float& CurrentTime = bMontagePreviewing ? State->MontagePreviewTime : State->CurrentAnimTime;

    float ControlHeight = ImGui::GetFrameHeightWithSpacing();
     
    // --- 1. 메인 타임라인 에디터 (테이블 기반) ---
    ImGui::BeginChild("TimelineEditor", ImVec2(0, -ControlHeight));

    ImGuiTableFlags TableFlags = ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_ScrollX |
                                 ImGuiTableFlags_BordersOuter | ImGuiTableFlags_NoSavedSettings;

    if (ImGui::BeginTable("TimelineTable", 2, TableFlags))
    {
        // --- 1.1. 테이블 컬럼 설정 ---
        ImGui::TableSetupColumn("Tracks", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, 200.0f);
        ImGui::TableSetupColumn("Timeline", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoHeaderLabel);

        bool bIsTimelineHovered = false;
        float FrameAtMouse = 0.0f;

        // --- 1.2. 헤더 행 (필터 + 눈금을자) ---
        ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
        
        // 헤더 - 컬럼 0: 필터
        ImGui::TableSetColumnIndex(0);
        ImGui::PushItemWidth(-1);
        static char filterText[128] = "";
        ImGui::InputTextWithHint("##Filter", "Filter...", filterText, sizeof(filterText));
        ImGui::PopItemWidth();

        // 헤더 - 컬럼 1: 눈금자 (Ruler)
        ImGui::TableSetColumnIndex(1);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.2f, 0.22f, 0.24f, 1.0f));
        if (ImGui::BeginChild("Ruler", ImVec2(0, ImGui::GetFrameHeightWithSpacing()), false, ImGuiWindowFlags_NoScrollbar))
        {
            ImDrawList* DrawList = ImGui::GetWindowDrawList();
            ImVec2 P = ImGui::GetCursorScreenPos();
            ImVec2 Size = ImGui::GetWindowSize();

            auto FrameToPixel = [&](float Frame) { return P.x + (Frame - State->TimelineOffset) * State->TimelineScale; };
            auto PixelToFrame = [&](float Pixel) { return (Pixel - P.x) / State->TimelineScale + State->TimelineOffset; };

            ImGui::InvisibleButton("##RulerInput", Size);
            if (ImGui::IsItemHovered())
            {
                bIsTimelineHovered = true;
                FrameAtMouse = PixelToFrame(ImGui::GetIO().MousePos.x);
            }

            if (bHasAnimation)
            {
                int FrameStep = 10;
                if (State->TimelineScale < 0.5f)
                {
                    FrameStep = 50;
                    
                }
                else if (State->TimelineScale < 2.0f)
                {
                    FrameStep = 20;
                }

                float StartFrame = (float)(int)PixelToFrame(P.x) - 1.0f;
                float EndFrame = (float)(int)PixelToFrame(P.x + Size.x) + 1.0f;
                StartFrame = ImMax(StartFrame, 0.0f);

                for (float F = StartFrame; F <= EndFrame && F <= NumberOfFrames; F += 1.0f)
                {
                    int Frame = (int)F;
                    float X = FrameToPixel((float)Frame);
                    if (X < P.x || X > P.x + Size.x) continue;

                    float TickHeight = (Frame % 5 == 0) ? (Size.y * 0.5f) : (Size.y * 0.3f);
                    if (Frame % FrameStep == 0) TickHeight = Size.y * 0.7f;
                    
                    DrawList->AddLine(ImVec2(X, P.y + Size.y - TickHeight), ImVec2(X, P.y + Size.y), IM_COL32(150, 150, 150, 255));
                    
                    if (Frame % FrameStep == 0)
                    {
                        char Text[16]; snprintf(Text, 16, "%d", Frame);
                        DrawList->AddText(ImVec2(X + 2, P.y), IM_COL32_WHITE, Text);
                    }
                }
            }

            if (bHasAnimation)
            {
                // 몽타주 미리보기 시 섹션 경계선 그리기
                if (bMontagePreviewing && PreviewMontage)
                {
                    for (int32 i = 0; i < SectionStartTimes.Num(); ++i)
                    {
                        float SectionFrame = SectionStartTimes[i] / FrameDuration;
                        float SectionX = FrameToPixel(SectionFrame);
                        if (SectionX >= P.x && SectionX <= P.x + Size.x)
                        {
                            // 섹션 경계선 (녹색 점선)
                            DrawList->AddLine(ImVec2(SectionX, P.y), ImVec2(SectionX, P.y + Size.y), IM_COL32(100, 255, 100, 200), 2.0f);
                        }
                    }
                }

                float PlayheadFrame = CurrentTime / FrameDuration;
                float PlayheadX = FrameToPixel(PlayheadFrame);
                if (PlayheadX >= P.x && PlayheadX <= P.x + Size.x)
                {
                    DrawList->AddLine(ImVec2(PlayheadX, P.y), ImVec2(PlayheadX, P.y + Size.y), IM_COL32(255, 0, 0, 255), 2.0f);
                }
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();

        // --- 1.3. 몽타주 섹션 트랙 (몽타주 미리보기 시에만) ---
        if (bMontagePreviewing && PreviewMontage)
        {
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            bool bSectionTrackVisible = ImGui::TreeNodeEx("섹션", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Leaf);

            ImGui::TableSetColumnIndex(1);
            float SectionTrackHeight = ImGui::GetTextLineHeight() * 2.0f;
            ImVec2 SectionTrackSize = ImVec2(ImGui::GetContentRegionAvail().x, SectionTrackHeight);

            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.12f, 0.18f, 1.0f));
            if (ImGui::BeginChild("SectionTrack", SectionTrackSize, false, ImGuiWindowFlags_NoScrollbar))
            {
                ImDrawList* DrawList = ImGui::GetWindowDrawList();
                ImVec2 P = ImGui::GetCursorScreenPos();
                ImVec2 Size = ImGui::GetWindowSize();

                auto FrameToPixel = [&](float Frame) { return P.x + (Frame - State->TimelineOffset) * State->TimelineScale; };
                auto PixelToFrame = [&](float Pixel) { return (Pixel - P.x) / State->TimelineScale + State->TimelineOffset; };

                // 입력 처리용 InvisibleButton
                ImGui::InvisibleButton("##SectionTrackInput", Size);
                if (ImGui::IsItemHovered())
                {
                    bIsTimelineHovered = true;
                    FrameAtMouse = PixelToFrame(ImGui::GetIO().MousePos.x);
                }

                // 각 섹션을 색상 블록으로 표시
                ImU32 SectionColors[] = {
                    IM_COL32(80, 120, 200, 180),   // 파랑
                    IM_COL32(200, 120, 80, 180),   // 주황
                    IM_COL32(80, 200, 120, 180),   // 초록
                    IM_COL32(200, 80, 180, 180),   // 보라
                    IM_COL32(200, 200, 80, 180),   // 노랑
                };
                int NumColors = sizeof(SectionColors) / sizeof(SectionColors[0]);

                for (int32 i = 0; i < PreviewMontage->GetNumSections(); ++i)
                {
                    float StartFrame = SectionStartTimes[i] / FrameDuration;
                    float EndFrame = (SectionStartTimes[i] + SectionLengths[i]) / FrameDuration;

                    float XStart = FrameToPixel(StartFrame);
                    float XEnd = FrameToPixel(EndFrame);

                    if (XEnd < P.x || XStart > P.x + Size.x) continue;

                    float ViewXStart = ImMax(XStart, P.x);
                    float ViewXEnd = ImMin(XEnd, P.x + Size.x);

                    if (ViewXEnd > ViewXStart)
                    {
                        ImU32 Color = SectionColors[i % NumColors];
                        bool bIsSelected = (State->SelectedMontageSection == i);
                        if (bIsSelected)
                        {
                            Color = IM_COL32(255, 255, 255, 200);
                        }

                        // 섹션 배경
                        DrawList->AddRectFilled(
                            ImVec2(ViewXStart, P.y + 2),
                            ImVec2(ViewXEnd, P.y + Size.y - 2),
                            Color,
                            4.0f
                        );

                        // 섹션 테두리
                        DrawList->AddRect(
                            ImVec2(ViewXStart, P.y + 2),
                            ImVec2(ViewXEnd, P.y + Size.y - 2),
                            IM_COL32(255, 255, 255, 100),
                            4.0f
                        );

                        // 섹션 이름 표시
                        FMontageSection& Section = PreviewMontage->Sections[i];
                        ImGui::PushClipRect(ImVec2(ViewXStart, P.y), ImVec2(ViewXEnd, P.y + Size.y), true);
                        DrawList->AddText(ImVec2(XStart + 4, P.y + 4), IM_COL32_WHITE, Section.Name.c_str());
                        ImGui::PopClipRect();

                        // 클릭으로 섹션 선택 및 시간 점프
                        if (ImGui::IsMouseHoveringRect(ImVec2(ViewXStart, P.y), ImVec2(ViewXEnd, P.y + Size.y)) &&
                            ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                        {
                            State->SelectedMontageSection = i;
                            State->MontagePreviewTime = SectionStartTimes[i];
                            State->bIsPlaying = false;
                        }
                    }
                }

                // 플레이헤드
                float PlayheadFrame = CurrentTime / FrameDuration;
                float PlayheadX = FrameToPixel(PlayheadFrame);
                if (PlayheadX >= P.x && PlayheadX <= P.x + Size.x)
                {
                    DrawList->AddLine(ImVec2(PlayheadX, P.y), ImVec2(PlayheadX, P.y + Size.y), IM_COL32(255, 0, 0, 255), 2.0f);
                }
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();

            if (bSectionTrackVisible)
            {
                ImGui::TreePop();
            }
        }

        // --- 1.4. 노티파이 트랙 행 ---
        ImGui::TableNextRow();
        
        ImGui::TableSetColumnIndex(0);
        bool bNodeVisible = ImGui::TreeNodeEx("노티파이", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Leaf);

        ImGui::TableSetColumnIndex(1);
        float TrackHeight = ImGui::GetTextLineHeight() * 1.5f;
        ImVec2 TrackSize = ImVec2(ImGui::GetContentRegionAvail().x, TrackHeight);
        
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.18f, 0.19f, 0.2f, 1.0f));
        if (ImGui::BeginChild("NotifyTrack", TrackSize, false, ImGuiWindowFlags_NoScrollbar))
        {
            ImDrawList* DrawList = ImGui::GetWindowDrawList();
            ImVec2 P = ImGui::GetCursorScreenPos();
            ImVec2 Size = ImGui::GetWindowSize();
            
            auto FrameToPixel = [&](float Frame) { return P.x + (Frame - State->TimelineOffset) * State->TimelineScale; };
            auto PixelToFrame = [&](float Pixel) { return (Pixel - P.x) / State->TimelineScale + State->TimelineOffset; };

            ImGui::InvisibleButton("##NotifyTrackInput", Size);
            if (ImGui::IsItemHovered())
            {
                bIsTimelineHovered = true;
                FrameAtMouse = PixelToFrame(ImGui::GetIO().MousePos.x);
            }
            static float RightClickFrame = 0.0f;
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
            {
                RightClickFrame = PixelToFrame(ImGui::GetIO().MousePos.x);
            }

            // 노티파이 소스 결정 (몽타주 모드면 몽타주, 아니면 CurrentAnimation)
            UAnimSequenceBase* NotifySource = bMontagePreviewing ? static_cast<UAnimSequenceBase*>(PreviewMontage) : static_cast<UAnimSequenceBase*>(State->CurrentAnimation);

            // Context menu to add notifies
            if (ImGui::BeginPopupContextItem("NotifyTrackContext"))
            {
                if (ImGui::BeginMenu("Add Notify"))
                {
                    if (ImGui::MenuItem("Sound Notify"))
                    {
                        if (bHasAnimation && NotifySource)
                        {
                            float ClickFrame = RightClickFrame;
                            float TimeSec = ImClamp(ClickFrame * FrameDuration, 0.0f, PlayLength);
                            // Sound Notify 추가
                            UAnimNotify_PlaySound* NewNotify = NewObject<UAnimNotify_PlaySound>();
                            if (NewNotify)
                            {
                                // 기본 SoundNotify는 sound 없음
                                NewNotify->Sound = nullptr;
                                NotifySource->AddPlaySoundNotify(TimeSec, NewNotify, 0.0f);
                            }
                        }
                    }
                    if (ImGui::MenuItem("Particle Start Notify"))
                    {
                        if (bHasAnimation && NotifySource)
                        {
                            float ClickFrame = RightClickFrame;
                            float TimeSec = ImClamp(ClickFrame * FrameDuration, 0.0f, PlayLength);

                            // Particle Start Notify 추가
                            UAnimNotify_ParticleStart* NewNotify = NewObject<UAnimNotify_ParticleStart>();
                            if (NewNotify)
                            {
                                NotifySource->AddParticleStartNotify(TimeSec, NewNotify);
                            }
                        }
                    }
                    if (ImGui::MenuItem("Particle End Notify"))
                    {
                        if (bHasAnimation && NotifySource)
                        {
                            float ClickFrame = RightClickFrame;
                            float TimeSec = ImClamp(ClickFrame * FrameDuration, 0.0f, PlayLength);

                            // Particle End Notify 추가
                            UAnimNotify_ParticleEnd* NewNotify = NewObject<UAnimNotify_ParticleEnd>();
                            if (NewNotify)
                            {
                                NotifySource->AddParticleEndNotify(TimeSec, NewNotify);
                            }
                        }
                    }
                    // 함수 호출 노티파이
                    if (ImGui::MenuItem("Call Func Notify"))
                    {
                        bOpenNamePopup = true;
                        memset(FunctionNameBuffer, 0, sizeof(FunctionNameBuffer));
                    }
                    
                    ImGui::EndMenu();
                }

                ImGui::Separator();
                if (ImGui::MenuItem("Delete Notify"))
                {
                    if (bHasAnimation && NotifySource)
                    {
                        const float ClickFrame = RightClickFrame;
                        const float ClickTimeSec = ImClamp(ClickFrame * FrameDuration, 0.0f, PlayLength);

                        TArray<FAnimNotifyEvent>& Events = NotifySource->GetAnimNotifyEvents();

                        int DeleteIndex = -1;
                        float BestDist = 1e9f;
                        const float Tolerance = FMath::Max(FrameDuration * 0.5f, 0.05f);

                        for (int i = 0; i < Events.Num(); ++i)
                        {
                            const FAnimNotifyEvent& E = Events[i];
                            float Dist = 1e9f;
                            if (E.IsState())
                            {
                                const float Start = E.GetTriggerTime();
                                const float End = E.GetEndTriggerTime();
                                if (ClickTimeSec >= Start - Tolerance && ClickTimeSec <= End + Tolerance)
                                {
                                    Dist = 0.0f;
                                }
                                else
                                {
                                    Dist = (ClickTimeSec < Start) ? (Start - ClickTimeSec) : (ClickTimeSec - End);
                                }
                            }
                            else
                            {
                                Dist = FMath::Abs(E.GetTriggerTime() - ClickTimeSec);
                            }

                            if (Dist <= Tolerance && Dist < BestDist)
                            {
                                BestDist = Dist;
                                DeleteIndex = i;
                            }
                        }

                        if (DeleteIndex >= 0)
                        {
                            Events.RemoveAt(DeleteIndex);
                        }
                    }
                }
                ImGui::EndPopup();
            }

            // 함수 호출 노티파이에 함수명 저장
            if (bOpenNamePopup)
            {
                ImGui::OpenPopup("Enter Function Name");
                bOpenNamePopup = false;
            }

            // 모달 팝업 (배경 클릭 막음)
            if (ImGui::BeginPopupModal("Enter Function Name", NULL, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::Text("Enter the name of the function to call:");
    
                // 텍스트 입력창 (Enter 치면 true 반환)
                bool bEnterPressed = ImGui::InputText("Function Name", FunctionNameBuffer, IM_ARRAYSIZE(FunctionNameBuffer), ImGuiInputTextFlags_EnterReturnsTrue);
    
                // 'Add' 버튼을 누르거나 텍스트창에서 엔터를 쳤을 때
                if (ImGui::Button("Add") || bEnterPressed)
                {
                    if (bHasAnimation && NotifySource)
                    {
                        float ClickFrame = RightClickFrame; // 우클릭했던 위치 기억 필요
                        float TimeSec = ImClamp(ClickFrame * FrameDuration, 0.0f, PlayLength);

                        // 1. 객체 생성
                        UAnimNotify_CallFunction* NewNotify = NewObject<UAnimNotify_CallFunction>();
                        if (NewNotify)
                        {
                            // 2. [중요] 입력받은 이름 저장!
                            NewNotify->FunctionName = FName(FString(FunctionNameBuffer));
                
                            // 3. 트랙에 추가
                            NotifySource->AddCallFuncNotify(TimeSec, NewNotify);
                        }
                    }
                    ImGui::CloseCurrentPopup(); // 팝업 닫기
                }

                ImGui::SameLine();

                if (ImGui::Button("Cancel"))
                {
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }

            if (bHasAnimation && NotifySource)
            {
                // Draw and hit-test notifies (now draggable)
                TArray<FAnimNotifyEvent>& Events = NotifySource->GetAnimNotifyEvents();
                for (int i = 0; i < Events.Num(); ++i)
                {
                    FAnimNotifyEvent& Notify = Events[i];
                    float TriggerFrame = Notify.TriggerTime / FrameDuration;
                    float DurationFrames = (Notify.Duration > 0.0f) ? (Notify.Duration / FrameDuration) : 0.5f;
                    
                    float XStart = FrameToPixel(TriggerFrame);
                    float XEnd = FrameToPixel(TriggerFrame + DurationFrames);

                    if (XEnd < P.x || XStart > P.x + Size.x) continue;

                    float ViewXStart = ImMax(XStart, P.x);
                    float ViewXEnd = ImMin(XEnd, P.x + Size.x);

                    if (ViewXEnd > ViewXStart)
                    {
                        // Hover/click detection rect
                        ImVec2 RMin(ViewXStart, P.y);
                        ImVec2 RMax(ViewXEnd, P.y + Size.y);
                        bool bHover = ImGui::IsMouseHoveringRect(RMin, RMax);
                        bool bPressed = bHover && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
                        bool bDoubleClicked = bHover && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
                        

                        // Styling
                        ImU32 FillCol = IM_COL32(100, 100, 255, bHover ? 140 : 100);
                        ImU32 LineCol = IM_COL32(200, 200, 255, 150);
                        DrawList->AddRectFilled(
                            ImVec2(ViewXStart, P.y),
                            ImVec2(ViewXEnd, P.y + Size.y),
                            FillCol
                        );
                        DrawList->AddRect(
                            ImVec2(ViewXStart, P.y), 
                            ImVec2(ViewXEnd, P.y + Size.y), 
                            LineCol
                        );
                        
                        ImGui::PushClipRect(ImVec2(ViewXStart, P.y), ImVec2(ViewXEnd, P.y + Size.y), true);
                        // Label: use NotifyName if set, otherwise fallback based on type
                        FString Label = Notify.NotifyName.ToString();
                        if (Label.empty())
                        {
                            Label = Notify.Notify && Notify.Notify->IsA<UAnimNotify_PlaySound>() ? "PlaySound" : "Notify";
                        }
                        DrawList->AddText(ImVec2(XStart + 2, P.y + 2), IM_COL32_WHITE, Label.c_str());
                        ImGui::PopClipRect();

                        // Double-click opens edit popup; single-click starts dragging
                        if (bDoubleClicked)
                        {
                            SelectedNotifyIndex = i;
                            ImGui::OpenPopup("NotifyEditPopup");
                        }
                        else if (bPressed)
                        {
                            bDraggingNotify = true;
                            DraggingNotifyIndex = i;
                            DraggingStartMouseX = ImGui::GetIO().MousePos.x;
                            DraggingOrigTriggerTime = Notify.TriggerTime;
                            SelectedNotifyIndex = i;
                        }
                    }
                }

                // Update dragging movement (if any)
                if (bDraggingNotify && DraggingNotifyIndex >= 0 && DraggingNotifyIndex < Events.Num())
                {
                    if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
                    {
                        float deltaX = ImGui::GetIO().MousePos.x - DraggingStartMouseX;
                        float deltaFrames = deltaX / State->TimelineScale;
                        float newTime = DraggingOrigTriggerTime + (deltaFrames * FrameDuration);
                        newTime = ImClamp(newTime, 0.0f, PlayLength);
                        Events[DraggingNotifyIndex].TriggerTime = newTime;
                    }
                    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
                    {
                        bDraggingNotify = false;
                        DraggingNotifyIndex = -1;
                    }
                }
            }

            // Edit popup for a clicked notify (change sound)
            if (ImGui::BeginPopup("NotifyEditPopup"))
            {
                if (!bHasAnimation || !NotifySource)
                {
                    ImGui::TextDisabled("No animation.");
                }
                else if (SelectedNotifyIndex < 0 || SelectedNotifyIndex >= NotifySource->GetAnimNotifyEvents().Num())
                {
                    ImGui::TextDisabled("No notify selected.");
                }
                else
                {
                    TArray<FAnimNotifyEvent>& Events = NotifySource->GetAnimNotifyEvents();
                    FAnimNotifyEvent& Evt = Events[SelectedNotifyIndex];

                    if (Evt.Notify && Evt.Notify->IsA<UAnimNotify_PlaySound>())
                    {
                        UAnimNotify_PlaySound* PS = static_cast<UAnimNotify_PlaySound*>(Evt.Notify);

                        // Simple selection combo from ResourceManager
                        UResourceManager& ResMgr = UResourceManager::GetInstance();
                        TArray<FString> Paths = ResMgr.GetAllFilePaths<USound>();

                        FString CurrentPath = (PS->Sound) ? PS->Sound->GetFilePath() : "None";
                        int CurrentIndex = 0; // 0 = None
                        for (int idx = 0; idx < Paths.Num(); ++idx)
                        {
                            if (Paths[idx] == CurrentPath) { CurrentIndex = idx + 1; break; }
                        }

                        // Build items on the fly: "None" + all paths
                        FString Preview = (CurrentIndex == 0) ? FString("None") : Paths[CurrentIndex - 1];
                        if (ImGui::BeginCombo("Sound", Preview.c_str()))
                        {
                            // None option
                            bool selNone = (CurrentIndex == 0);
                            if (ImGui::Selectable("None", selNone))
                            {
                                PS->Sound = nullptr;
                                Evt.NotifyName = FName("PlaySound");
                            }
                            if (selNone) ImGui::SetItemDefaultFocus();

                            for (int i = 0; i < Paths.Num(); ++i)
                            {
                                bool selected = (CurrentIndex == i + 1);
                                const FString& Item = Paths[i];
                                if (ImGui::Selectable(Item.c_str(), selected))
                                {
                                    USound* NewSound = ResMgr.Load<USound>(Item);
                                    PS->Sound = NewSound;
                                    // Set label as filename
                                    std::filesystem::path p(Item);
                                    FString Base = p.filename().string();
                                    Evt.NotifyName = FName((FString("PlaySound: ") + Base).c_str());
                                }
                                if (selected) ImGui::SetItemDefaultFocus();
                            }
                            ImGui::EndCombo();
                        }

                        // Also allow loading from file system directly
                        if (ImGui::Button("Load .wav..."))
                        {
                            std::filesystem::path Sel = FPlatformProcess::OpenLoadFileDialog(UTF8ToWide(GDataDir) + L"/Audio", L"wav", L"WAV Files");
                            if (!Sel.empty())
                            {
                                FString PathUtf8 = WideToUTF8(Sel.generic_wstring());
                                USound* NewSound = UResourceManager::GetInstance().Load<USound>(PathUtf8);
                                if (NewSound)
                                {
                                    PS->Sound = NewSound;
                                    std::filesystem::path p2(PathUtf8);
                                    FString Base = p2.filename().string();
                                    Evt.NotifyName = FName((FString("PlaySound: ") + Base).c_str());
                                }
                            }
                        }
                    }
                    else
                    {
                        ImGui::TextDisabled("This notify type is not editable.");
                    }
                }

                ImGui::EndPopup();
            }

            if (bHasAnimation)
            {
                float PlayheadFrame = CurrentTime / FrameDuration;
                float PlayheadX = FrameToPixel(PlayheadFrame);
                if (PlayheadX >= P.x && PlayheadX <= P.x + Size.x)
                {
                    DrawList->AddLine(ImVec2(PlayheadX, P.y), ImVec2(PlayheadX, P.y + Size.y), IM_COL32(255, 0, 0, 255), 2.0f);
                }
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();

        if (bNodeVisible)
        {
            ImGui::TreePop();
        }

        // --- 1.5. 타임라인 패닝, 줌, 스크러빙 (테이블 내에서 처리) ---
        if (bHasAnimation && bIsTimelineHovered)
        {
            if (ImGui::GetIO().KeyCtrl && ImGui::GetIO().MouseWheel != 0)
            {
                // Ctrl + 스크롤 = 줌
                float NewScale = State->TimelineScale * powf(1.1f, ImGui::GetIO().MouseWheel);
                State->TimelineScale = ImClamp(NewScale, 0.1f, 100.0f);
                State->TimelineOffset = FrameAtMouse - (ImGui::GetIO().MousePos.x - ImGui::GetCursorScreenPos().x) / State->TimelineScale;
            }
            else if (ImGui::GetIO().MouseWheel != 0)
            {
                // 일반 스크롤 = 타임라인 좌우 이동
                float ScrollSpeed = 5.0f / State->TimelineScale;  // 줌 레벨에 따라 스크롤 속도 조절
                State->TimelineOffset += ImGui::GetIO().MouseWheel * ScrollSpeed;
            }
            else if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
            {
                float DeltaFrames = ImGui::GetIO().MouseDelta.x / State->TimelineScale;
                State->TimelineOffset -= DeltaFrames;
            }
            else if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                CurrentTime = ImClamp(FrameAtMouse * FrameDuration, 0.0f, PlayLength);
                State->bIsPlaying = false;
                State->bTimeChanged = true;

                // 몽타주 미리보기 시 해당 섹션의 애니메이션 업데이트
                if (bMontagePreviewing && PreviewMontage && State->PreviewActor)
                {
                    USkeletalMeshComponent* MeshComp = State->PreviewActor->GetSkeletalMeshComponent();
                    if (MeshComp)
                    {
                        // 현재 시간이 어느 섹션에 속하는지 찾기
                        float AccumTime = 0.0f;
                        for (int32 i = 0; i < SectionStartTimes.Num(); ++i)
                        {
                            float SectionEnd = SectionStartTimes[i] + SectionLengths[i];
                            if (CurrentTime < SectionEnd || i == SectionStartTimes.Num() - 1)
                            {
                                UAnimSequence* Seq = PreviewMontage->GetSectionSequence(i);
                                if (Seq)
                                {
                                    float LocalTime = CurrentTime - SectionStartTimes[i];
                                    LocalTime = ImClamp(LocalTime, 0.0f, SectionLengths[i]);
                                    MeshComp->SetAnimation(Seq);
                                    State->CurrentAnimation = Seq;
                                    State->CurrentAnimTime = LocalTime;
                                }
                                break;
                            }
                        }
                    }
                }
            }
        }

        State->TimelineOffset = std::max(State->TimelineOffset, 0.0f);

        ImGui::EndTable();
    }
    ImGui::EndChild(); // "TimelineEditor"

    ImGui::Separator();
    
    // --- 2. 하단 컨트롤 바 ---
    ImGui::BeginChild("BottomControls", ImVec2(0, ControlHeight), false, ImGuiWindowFlags_NoScrollbar);
    {
        const ImVec2 IconSizeVec(IconSize, IconSize);
        
        // 1. [첫 프레임] 버튼
        if (IconFirstFrame && IconFirstFrame->GetShaderResourceView())
        {
            if (ImGui::ImageButton("##FirstFrameBtn", (void*)IconFirstFrame->GetShaderResourceView(), IconSizeVec))
            {
                if (bHasAnimation)
                {
                    CurrentTime = 0.0f;
                    State->bIsPlaying = false;
                }
            }
        }

        ImGui::SameLine();

        // 2. [이전 프레임] 버튼
        if (IconPrevFrame && IconPrevFrame->GetShaderResourceView())
        {
            if (ImGui::ImageButton("##PrevFrameBtn", (void*)IconPrevFrame->GetShaderResourceView(), IconSizeVec))
            {
                if (bHasAnimation)
                {
                    CurrentTime = ImMax(0.0f, CurrentTime - FrameDuration);
                    State->bIsPlaying = false;
                    State->bTimeChanged = true;
                }
            }
        }
        ImGui::SameLine();
        
        // 3. [역재생/일시정지] 버튼
        UTexture* CurrentReverseIcon = State->bIsPlayingReverse ? IconPause : IconReversePlay;
        if (CurrentReverseIcon && CurrentReverseIcon->GetShaderResourceView())
        {
            bool bIsPlayingReverse = State->bIsPlayingReverse;
            if (bIsPlayingReverse)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
            }
        
            if (ImGui::ImageButton("##ReversePlayBtn", (void*)CurrentReverseIcon->GetShaderResourceView(), IconSizeVec))
            {
                if (bIsPlayingReverse) 
                {
                    State->bIsPlaying = false;
                    State->bIsPlayingReverse = false;
                }
                else 
                {
                    State->bIsPlaying = false;
                    State->bIsPlayingReverse = true;
                }
            }
            if (bIsPlayingReverse)
            {
                ImGui::PopStyleColor();
            }
        }
        ImGui::SameLine();

        // 4. [녹화] 버튼
        UTexture* CurrentRecordIcon = State->bIsRecording ? IconRecordActive : IconRecord;
        if (CurrentRecordIcon && CurrentRecordIcon->GetShaderResourceView())
        {
            bool bWasRecording = State->bIsRecording;

            if (bWasRecording)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.1f, 0.1f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.1f, 0.1f, 1.0f));
            }

            if (ImGui::ImageButton("##RecordBtn", (void*)CurrentRecordIcon->GetShaderResourceView(), IconSizeVec))
            {
                State->bIsRecording = !State->bIsRecording; // 상태 변경
            }

            if (bWasRecording) 
            {
                ImGui::PopStyleColor(3);
            }
        }
        ImGui::SameLine();

        // 5. [재생/일시정지] 버튼
        UTexture* CurrentPlayIcon = State->bIsPlaying ? IconPause : IconPlay;
        if (CurrentPlayIcon && CurrentPlayIcon->GetShaderResourceView())
        {
            bool bIsPlaying = State->bIsPlaying;
            if (bIsPlaying)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
            }
                
            if (ImGui::ImageButton("##PlayPauseBtn", (void*)CurrentPlayIcon->GetShaderResourceView(), IconSizeVec))
            {
                if (bIsPlaying)
                {
                    State->bIsPlaying = false;
                    State->bIsPlayingReverse = false;
                }
                else
                {
                    State->bIsPlaying = true;
                    State->bIsPlayingReverse = false;
                }
            }

            if (bIsPlaying)
            {
                ImGui::PopStyleColor();
            }
        }
        ImGui::SameLine();

        // 6. [다음 프레임] 버튼
        if (IconNextFrame && IconNextFrame->GetShaderResourceView())
        {
            if (ImGui::ImageButton("##NextFrameBtn", (void*)IconNextFrame->GetShaderResourceView(), IconSizeVec))
            {
                if (bHasAnimation)
                {
                    CurrentTime = ImMin(PlayLength, CurrentTime + FrameDuration);
                    State->bIsPlaying = false;
                    State->bTimeChanged = true;
                }
            }
        }
        ImGui::SameLine();

        // 7. [마지막 프레임] 버튼
        if (IconLastFrame && IconLastFrame->GetShaderResourceView())
        {
            if (ImGui::ImageButton("##LastFrameBtn", (void*)IconLastFrame->GetShaderResourceView(), IconSizeVec))
            {
                if (bHasAnimation)
                {
                    CurrentTime = PlayLength;
                    State->bIsPlaying = false;
                    State->bTimeChanged = true;
                }
            }
        }
        ImGui::SameLine();

        // 8. [루프] 버튼
        UTexture* CurrentLoopIcon = State->bIsLooping ? IconLoop : IconNoLoop;
        if (CurrentLoopIcon && CurrentLoopIcon->GetShaderResourceView())
        {
            bool bIsLooping = State->bIsLooping; 

            if (bIsLooping) 
            {
                ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
            }

            if (ImGui::ImageButton("##LoopBtn", (void*)CurrentLoopIcon->GetShaderResourceView(), IconSizeVec)) 
            { 
                State->bIsLooping = !State->bIsLooping;
            }

            if (bIsLooping) 
            {
                ImGui::PopStyleColor(); 
            }
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(%.2f / %.2f)", CurrentTime, PlayLength);

        // 몽타주 모드 표시
        if (bMontagePreviewing && PreviewMontage)
        {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.8f, 0.4f, 0.8f, 1.0f), "[Montage: %d sections]", PreviewMontage->GetNumSections());
        }
    }
    ImGui::EndChild();
}

void SSkeletalMeshViewerWindow::DrawAssetBrowserPanel(ViewerState* State)
{
    if (!State) return;

    // --- Tab Bar for switching between Animation and Physics Asset ---
    ImGui::PushStyleColor(ImGuiCol_Tab, ImVec4(0.25f, 0.35f, 0.45f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_TabActive, ImVec4(0.35f, 0.50f, 0.65f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TabHovered, ImVec4(0.30f, 0.45f, 0.60f, 1.0f));

    if (ImGui::BeginTabBar("AssetBrowserTabs", ImGuiTabBarFlags_None))
    {
        if (ImGui::BeginTabItem("Animation"))
        {
            State->AssetBrowserMode = EAssetBrowserMode::Animation;
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Montage"))
        {
            // 몽타주 탭 진입 시 CurrentAnimation 초기화
            if (State->AssetBrowserMode != EAssetBrowserMode::Montage)
            {
                State->CurrentAnimation = nullptr;
                State->MontagePreviewTime = 0.0f;
            }
            State->AssetBrowserMode = EAssetBrowserMode::Montage;
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Physics Asset"))
        {
            State->AssetBrowserMode = EAssetBrowserMode::PhysicsAsset;
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::PopStyleColor(3);
    ImGui::Spacing();

    // Gather skeleton bone names for assets compatibility check
    TArray<FName> SkeletonBoneNames;
    const FSkeleton* SkeletonPtr = nullptr;
    if (State->CurrentMesh)
    {
        SkeletonPtr = State->CurrentMesh->GetSkeleton();
        if (SkeletonPtr)
        {
            for (const FBone& Bone : SkeletonPtr->Bones)
            {
                SkeletonBoneNames.Add(FName(Bone.Name));
            }
        }
    }

    // --- Render appropriate browser based on mode ---
    if (State->AssetBrowserMode == EAssetBrowserMode::Animation)
    {
        // --- Animation Browser Content ---
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.25f, 0.50f, 0.35f, 0.8f));
        ImGui::Text("Animation Assets");
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.35f, 0.60f, 0.45f, 0.7f));
        ImGui::Separator();
        ImGui::PopStyleColor();
        ImGui::Spacing();

        TArray<UAnimSequence*> AnimSequences = UResourceManager::GetInstance().GetAll<UAnimSequence>();

        if (SkeletonBoneNames.Num() == 0)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
            ImGui::TextWrapped("Load a skeletal mesh first to see compatible animations.");
            ImGui::PopStyleColor();
        }
        else
        {
            TArray<UAnimSequence*> CompatibleAnims;
            for (UAnimSequence* Anim : AnimSequences)
            {
                if (!Anim) continue;
                if (Anim->IsCompatibleWith(SkeletonBoneNames))
                {
                    CompatibleAnims.Add(Anim);
                }
            }

            if (CompatibleAnims.IsEmpty())
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
                ImGui::TextWrapped("No compatible animations found for this skeleton.");
                ImGui::PopStyleColor();
            }
            else
            {
                for (UAnimSequence* Anim : CompatibleAnims)
                {
                    if (!Anim) continue;

                    FString AssetName = Anim->GetFilePath();
                    size_t lastSlash = AssetName.find_last_of("/\\");
                    if (lastSlash != FString::npos)
                    {
                        AssetName = AssetName.substr(lastSlash + 1);
                    }

                    bool bIsSelected = (State->CurrentAnimation == Anim);

                    if (ImGui::Selectable(AssetName.c_str(), bIsSelected))
                    {
                        if (State->PreviewActor && State->PreviewActor->GetSkeletalMeshComponent())
                        {
                            State->PreviewActor->GetSkeletalMeshComponent()->SetAnimation(Anim);
                        }
                        State->CurrentAnimation = Anim;
                        State->CurrentAnimTime = 0.0f;
                        State->bIsPlaying = false;
                        State->bIsPlayingReverse = false;
                    }
                }
            }
        }
    }
    else if (State->AssetBrowserMode == EAssetBrowserMode::Montage)
    {
        // --- Montage Editor Content ---
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.50f, 0.25f, 0.50f, 0.8f));
        ImGui::Text("Montage Editor");
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.60f, 0.35f, 0.60f, 0.7f));
        ImGui::Separator();
        ImGui::PopStyleColor();
        ImGui::Spacing();

        // 새 몽타주 생성 버튼
        if (ImGui::Button("New Montage"))
        {
            State->bCreatingNewMontage = true;
            State->EditingMontage = NewObject<UAnimMontage>();
            State->MontageNameBuffer[0] = '\0';
            State->SelectedMontageSection = -1;
        }

        ImGui::SameLine();

        // 몽타주 파일 로드 버튼
        if (ImGui::Button("Load .montage.json..."))
        {
            std::filesystem::path Sel = FPlatformProcess::OpenLoadFileDialog(
                UTF8ToWide(GDataDir) + L"/Montages", L"json", L"Montage Files");
            if (!Sel.empty())
            {
                FString PathUtf8 = WideToUTF8(Sel.generic_wstring());
                UAnimMontage* LoadedMontage = UResourceManager::GetInstance().Load<UAnimMontage>(PathUtf8);
                if (LoadedMontage)
                {
                    State->CurrentMontage = LoadedMontage;
                    State->EditingMontage = nullptr;
                    State->bCreatingNewMontage = false;
                    State->MontagePreviewTime = 0.0f;

                    // 첫 번째 섹션의 시퀀스로 초기화
                    if (LoadedMontage->HasSections())
                    {
                        UAnimSequence* FirstSeq = LoadedMontage->GetSectionSequence(0);
                        State->CurrentAnimation = FirstSeq;
                        State->CurrentAnimTime = 0.0f;
                        if (FirstSeq && State->PreviewActor && State->PreviewActor->GetSkeletalMeshComponent())
                        {
                            State->PreviewActor->GetSkeletalMeshComponent()->SetAnimation(FirstSeq);
                        }
                    }
                    else
                    {
                        State->CurrentAnimation = nullptr;
                    }
                }
            }
        }

        ImGui::Separator();

        // 기존 몽타주 목록
        ImGui::Text("Loaded Montages:");
        TArray<UAnimMontage*> Montages = UResourceManager::GetInstance().GetAll<UAnimMontage>();
        for (UAnimMontage* Montage : Montages)
        {
            if (!Montage) continue;

            FString MontPath = Montage->GetFilePath();
            FString DisplayName = MontPath;
            size_t lastSlash = DisplayName.find_last_of("/\\");
            if (lastSlash != FString::npos)
            {
                DisplayName = DisplayName.substr(lastSlash + 1);
            }
            if (DisplayName.empty()) DisplayName = "Unnamed Montage";

            bool bIsSelected = (State->CurrentMontage == Montage);
            if (ImGui::Selectable(DisplayName.c_str(), bIsSelected))
            {
                State->CurrentMontage = Montage;
                State->EditingMontage = nullptr;
                State->bCreatingNewMontage = false;
                State->MontagePreviewTime = 0.0f;

                // 첫 번째 섹션의 시퀀스로 초기화
                if (Montage->HasSections())
                {
                    UAnimSequence* FirstSeq = Montage->GetSectionSequence(0);
                    State->CurrentAnimation = FirstSeq;
                    State->CurrentAnimTime = 0.0f;
                    if (FirstSeq && State->PreviewActor && State->PreviewActor->GetSkeletalMeshComponent())
                    {
                        State->PreviewActor->GetSkeletalMeshComponent()->SetAnimation(FirstSeq);
                    }
                }
                else
                {
                    State->CurrentAnimation = nullptr;
                }
            }
        }

        ImGui::Separator();

        // 현재 편집 중인 몽타주 (새로 생성 중이거나 선택된 몽타주)
        UAnimMontage* ActiveMontage = State->bCreatingNewMontage ? State->EditingMontage : State->CurrentMontage;
        if (ActiveMontage)
        {
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.40f, 0.20f, 0.40f, 0.8f));
            if (ImGui::CollapsingHeader("Montage Properties", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::PopStyleColor();

                // 몽타주 이름 (새로 생성 시에만 편집 가능)
                if (State->bCreatingNewMontage)
                {
                    ImGui::InputText("Name", State->MontageNameBuffer, sizeof(State->MontageNameBuffer));
                }
                else
                {
                    FString Path = ActiveMontage->GetFilePath();
                    ImGui::Text("Path: %s", Path.c_str());
                }

                // 블렌드 시간
                ImGui::DragFloat("Blend In Time", &ActiveMontage->BlendInTime, 0.01f, 0.0f, 2.0f, "%.2f sec");
                ImGui::DragFloat("Blend Out Time", &ActiveMontage->BlendOutTime, 0.01f, 0.0f, 2.0f, "%.2f sec");
                ImGui::Checkbox("Loop", &ActiveMontage->bLoop);

                ImGui::Separator();

                // 섹션 편집
                ImGui::Text("Sections (%d):", ActiveMontage->GetNumSections());

                // 섹션 추가 버튼
                if (ImGui::Button("+ Add Section"))
                {
                    FString SectionName = "Section_" + std::to_string(ActiveMontage->GetNumSections());
                    ActiveMontage->AddSection(SectionName, nullptr, 0.1f);
                }

                // 섹션 목록
                for (int32 i = 0; i < ActiveMontage->GetNumSections(); ++i)
                {
                    ImGui::PushID(i);

                    FMontageSection& Section = ActiveMontage->Sections[i];
                    bool bSectionSelected = (State->SelectedMontageSection == i);

                    // 섹션 헤더
                    FString SectionLabel = Section.Name + " [" + std::to_string(i) + "]";
                    if (Section.Sequence)
                    {
                        SectionLabel += " - " + Section.Sequence->GetFilePath();
                        size_t pos = SectionLabel.find_last_of("/\\");
                        if (pos != FString::npos)
                        {
                            FString SeqName = Section.Sequence->GetFilePath();
                            size_t seqPos = SeqName.find_last_of("/\\");
                            if (seqPos != FString::npos) SeqName = SeqName.substr(seqPos + 1);
                            SectionLabel = Section.Name + " [" + std::to_string(i) + "] - " + SeqName;
                        }
                    }

                    ImGuiTreeNodeFlags NodeFlags = ImGuiTreeNodeFlags_OpenOnArrow;
                    if (bSectionSelected) NodeFlags |= ImGuiTreeNodeFlags_Selected;

                    bool bOpen = ImGui::TreeNodeEx(SectionLabel.c_str(), NodeFlags);

                    if (ImGui::IsItemClicked())
                    {
                        State->SelectedMontageSection = i;
                        // 섹션 시작 시간 계산
                        float SectionStart = 0.0f;
                        for (int32 j = 0; j < i; ++j)
                        {
                            UAnimSequence* PrevSeq = ActiveMontage->GetSectionSequence(j);
                            SectionStart += PrevSeq ? PrevSeq->GetPlayLength() : 0.0f;
                        }
                        State->MontagePreviewTime = SectionStart;
                        State->bIsPlaying = false;
                        State->bIsPlayingReverse = false;
                    }

                    if (bOpen)
                    {
                        // 섹션 이름 편집
                        char NameBuf[64];
                        strncpy_s(NameBuf, Section.Name.c_str(), sizeof(NameBuf) - 1);
                        if (ImGui::InputText("Name##Section", NameBuf, sizeof(NameBuf)))
                        {
                            Section.Name = NameBuf;
                        }

                        // 블렌드 인 시간
                        ImGui::DragFloat("Blend In##Section", &Section.BlendInTime, 0.01f, 0.0f, 2.0f, "%.2f sec");

                        // 재생 속도
                        ImGui::DragFloat("Play Rate##Section", &Section.PlayRate, 0.01f, 0.1f, 5.0f, "%.2fx");

                        // 시퀀스 선택 콤보
                        TArray<UAnimSequence*> AllSequences = UResourceManager::GetInstance().GetAll<UAnimSequence>();

                        // 현재 스켈레톤과 호환되는 시퀀스만 필터링
                        TArray<UAnimSequence*> CompatibleSequences;
                        for (UAnimSequence* Seq : AllSequences)
                        {
                            if (Seq && Seq->IsCompatibleWith(SkeletonBoneNames))
                            {
                                CompatibleSequences.Add(Seq);
                            }
                        }

                        FString CurrentSeqName = Section.Sequence ? Section.Sequence->GetFilePath() : "None";
                        size_t seqSlash = CurrentSeqName.find_last_of("/\\");
                        if (seqSlash != FString::npos) CurrentSeqName = CurrentSeqName.substr(seqSlash + 1);
                        if (CurrentSeqName.empty()) CurrentSeqName = "None";

                        if (ImGui::BeginCombo("Sequence##Section", CurrentSeqName.c_str()))
                        {
                            // None 옵션
                            if (ImGui::Selectable("None", Section.Sequence == nullptr))
                            {
                                Section.Sequence = nullptr;
                            }

                            for (UAnimSequence* Seq : CompatibleSequences)
                            {
                                FString SeqName = Seq->GetFilePath();
                                size_t sp = SeqName.find_last_of("/\\");
                                if (sp != FString::npos) SeqName = SeqName.substr(sp + 1);

                                bool bSelected = (Section.Sequence == Seq);
                                if (ImGui::Selectable(SeqName.c_str(), bSelected))
                                {
                                    Section.Sequence = Seq;
                                    // 선택한 시퀀스를 바로 미리보기
                                    if (State->PreviewActor && State->PreviewActor->GetSkeletalMeshComponent())
                                    {
                                        State->PreviewActor->GetSkeletalMeshComponent()->SetAnimation(Seq);
                                    }
                                    State->CurrentAnimation = Seq;
                                    State->CurrentAnimTime = 0.0f;
                                    State->bIsPlaying = false;
                                }
                            }

                            ImGui::EndCombo();
                        }

                        // 섹션 삭제 버튼
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
                        if (ImGui::Button("Delete Section"))
                        {
                            ActiveMontage->Sections.RemoveAt(i);
                            if (State->SelectedMontageSection >= ActiveMontage->GetNumSections())
                            {
                                State->SelectedMontageSection = ActiveMontage->GetNumSections() - 1;
                            }
                            ImGui::PopStyleColor();
                            ImGui::TreePop();
                            ImGui::PopID();
                            break; // 배열 수정됨, 루프 종료
                        }
                        ImGui::PopStyleColor();

                        ImGui::TreePop();
                    }

                    ImGui::PopID();
                }

                ImGui::Separator();

                // 저장 버튼
                if (ImGui::Button("Save Montage"))
                {
                    FString SavePath;
                    if (State->bCreatingNewMontage)
                    {
                        // 새 몽타주 저장 경로 선택
                        FString DefaultName = State->MontageNameBuffer[0] ? State->MontageNameBuffer : "NewMontage";
                        std::filesystem::path Sel = FPlatformProcess::OpenSaveFileDialog(
                            UTF8ToWide(GDataDir) + L"/Montages",
                            UTF8ToWide(DefaultName + ".montage.json"),
                            L"json", L"Montage Files");
                        if (!Sel.empty())
                        {
                            SavePath = WideToUTF8(Sel.generic_wstring());
                        }
                    }
                    else
                    {
                        // 기존 경로에 저장
                        SavePath = ActiveMontage->GetFilePath();
                        if (SavePath.empty())
                        {
                            std::filesystem::path Sel = FPlatformProcess::OpenSaveFileDialog(
                                UTF8ToWide(GDataDir) + L"/Montages",
                                L"Montage.montage.json",
                                L"json", L"Montage Files");
                            if (!Sel.empty())
                            {
                                SavePath = WideToUTF8(Sel.generic_wstring());
                            }
                        }
                    }

                    if (!SavePath.empty())
                    {
                        if (ActiveMontage->Save(SavePath))
                        {
                            ActiveMontage->SetFilePath(SavePath);
                            // 새로 생성한 경우 ResourceManager에 등록
                            if (State->bCreatingNewMontage)
                            {
                                UResourceManager::GetInstance().Add<UAnimMontage>(SavePath, ActiveMontage);
                                State->CurrentMontage = ActiveMontage;
                                State->EditingMontage = nullptr;
                                State->bCreatingNewMontage = false;
                            }
                            UE_LOG("Montage saved to: %s", SavePath.c_str());
                        }
                    }
                }

                ImGui::SameLine();

                // Save As 버튼
                if (ImGui::Button("Save As..."))
                {
                    std::filesystem::path Sel = FPlatformProcess::OpenSaveFileDialog(
                        UTF8ToWide(GDataDir) + L"/Montages",
                        L"Montage.montage.json",
                        L"json", L"Montage Files");
                    if (!Sel.empty())
                    {
                        FString SavePath = WideToUTF8(Sel.generic_wstring());
                        if (ActiveMontage->Save(SavePath))
                        {
                            UE_LOG("Montage saved to: %s", SavePath.c_str());
                        }
                    }
                }

                // 새로 생성 중인 경우 취소 버튼
                if (State->bCreatingNewMontage)
                {
                    ImGui::SameLine();
                    if (ImGui::Button("Cancel"))
                    {
                        DeleteObject(State->EditingMontage);
                        State->EditingMontage = nullptr;
                        State->bCreatingNewMontage = false;
                    }
                }
            }
            else
            {
                ImGui::PopStyleColor();
            }
        }
        else
        {
            ImGui::TextDisabled("No montage selected. Create a new one or load existing.");
        }
    }
    else if (State->AssetBrowserMode == EAssetBrowserMode::PhysicsAsset)
    {
        // --- Physics Asset Browser Content ---
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.50f, 0.35f, 0.25f, 0.8f));
        ImGui::Text("Physics Assets");
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.60f, 0.45f, 0.35f, 0.7f));
        ImGui::Separator();
        ImGui::PopStyleColor();
        ImGui::Spacing();

        // Get all Physics Assets from ResourceManager
        TArray<UPhysicsAsset*> PhysicsAssets = UResourceManager::GetInstance().GetAll<UPhysicsAsset>();

        if (SkeletonBoneNames.Num() == 0)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
            ImGui::TextWrapped("Load a skeletal mesh first to see compatible physics assets.");
            ImGui::PopStyleColor();
        }
        else
        {
            TArray<UPhysicsAsset*> CompatiblePhysicsAssets;
            for (UPhysicsAsset* PhysAsset : PhysicsAssets)
            {
                if (!PhysAsset) continue;

				int32 TotalBodies = 0; // 전체 바디 수
				int32 MatchedBodies = 0; // 스켈레탈 메시의 본과 매칭되는 바디 수

                const TArray<UBodySetup*> LocalBodySetups = PhysAsset->BodySetups;
                for (UBodySetup* Body : LocalBodySetups)
                {
                    if (!Body) continue;
                    TotalBodies++;

                    // UBodySetupCore::BoneName stores the bone mapping
                    if (SkeletonPtr && SkeletonPtr->FindBoneIndex(Body->BoneName) != INDEX_NONE)
                    {
                        MatchedBodies++;
                    }
                }

				if (TotalBodies == MatchedBodies) // 전부 일치하는 경우에만 허용
                {
                    CompatiblePhysicsAssets.Add(PhysAsset);
                }
            }

            if (CompatiblePhysicsAssets.IsEmpty())
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
                ImGui::TextWrapped("No compatible physics assets found for this skeleton.");
                ImGui::PopStyleColor();
            }
            else
            {
                for (UPhysicsAsset* PhysAsset : CompatiblePhysicsAssets)
                {
                    if (!PhysAsset) continue;

                    FString AssetName = PhysAsset->GetName().ToString();
                    if (AssetName.empty())
                    {
                        AssetName = "Unnamed Physics Asset";
                    }

                    const FString& AssetPath = PhysAsset->GetFilePath();
                    if (!AssetPath.empty())
                    {
                        std::filesystem::path DisplayPath(AssetPath);
                        FString FileName = DisplayPath.filename().string();
                        if (!FileName.empty())
                        {
                            PhysAsset->SetName(FName(FileName.c_str()));
                            AssetName = FileName;
                        }
                    }

                    bool bIsSelected = (State->CurrentPhysicsAsset == PhysAsset);
                    bool bIsCurrentMeshPhysicsAsset = (State->CurrentMesh && State->CurrentMesh->PhysicsAsset == PhysAsset);
                    // Rename mode is per-state boolean; check it's active and refers to this asset
                    bool bIsRenamingThis = (State->bIsRenaming && bIsSelected);

                    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.45f, 0.30f, 0.20f, 0.8f));
                    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.55f, 0.40f, 0.30f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.40f, 0.25f, 0.15f, 1.0f));

                    // Highlight current mesh's physics asset in blue
                    if (bIsCurrentMeshPhysicsAsset)
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.7f, 1.0f, 1.0f));
                    }

                    ImGui::PushID((void*)PhysAsset);
                    if (bIsRenamingThis)
                    {
                        if (State->PhysicsAssetNameBuffer[0] == '\0')
                        {
                            strncpy_s(State->PhysicsAssetNameBuffer, AssetName.c_str(), sizeof(State->PhysicsAssetNameBuffer) - 1);
                        }

                        char RenameLabel[64];
                        snprintf(RenameLabel, sizeof(RenameLabel), "##RenamePhysics_%p", (void*)PhysAsset);

                        // Request focus for the next widget (the InputText that follows)
                        ImGui::SetKeyboardFocusHere(0);
                        ImGui::PushItemWidth(-1);

                        int flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll;
                        bool committed = ImGui::InputText(RenameLabel, State->PhysicsAssetNameBuffer, sizeof(State->PhysicsAssetNameBuffer), flags);
                        if (committed || ImGui::IsItemDeactivated() || (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsItemActive()))
                        {
                            PhysAsset->SetName(FName(State->PhysicsAssetNameBuffer));
                            State->bIsRenaming = false;
                            State->PhysicsAssetNameBuffer[0] = '\0';
                            ImGui::ClearActiveID(); // 중요: 이전 Active ID 제거
                        }

                        ImGui::PopItemWidth();
                    }
                    else
                    {
                        if (ImGui::Selectable(AssetName.c_str(), bIsSelected))
                        {
                            State->CurrentPhysicsAsset = PhysAsset;

                            USkeletalMeshComponent* SkeletalComp = ActiveState->PreviewActor->GetSkeletalMeshComponent();
							assert(SkeletalComp);
                            if (SkeletalComp)
                            {
                                SkeletalComp->SetPhysicsAsset(PhysAsset);
                            }
                        }

                        // Enter rename mode on double-click: set per-state boolean and ensure CurrentPhysicsAsset points to the asset
                        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                        {
                            State->CurrentPhysicsAsset = PhysAsset; // ensure target is current
                            State->bIsRenaming = true;
                            strncpy_s(State->PhysicsAssetNameBuffer, AssetName.c_str(), sizeof(State->PhysicsAssetNameBuffer) - 1);
                        }
                    }
                    ImGui::PopID();

                    // Pop the blue text color if it was applied
                    if (bIsCurrentMeshPhysicsAsset)
                    {
                        ImGui::PopStyleColor();
                    }

                    ImGui::PopStyleColor(3);

                    // Tooltip
                    if (ImGui::IsItemHovered())
                    {
                        ImGui::BeginTooltip();
                        ImGui::Text("Physics Asset: %s", AssetName.c_str());
                        if (bIsCurrentMeshPhysicsAsset)
                        {
                            ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "  (Current Mesh's Physics Asset)");
                        }
                        ImGui::EndTooltip();
                    }
                }
            }

            // New Asset button: create a new UPhysicsAsset and assign to State->CurrentPhysicsAsset
            if (ImGui::Button("New Physics Asset"))
            {
                UPhysicsAsset* NewAsset = NewObject<UPhysicsAsset>();
                if (NewAsset)
                {
                    // Give the asset a sensible default name (ObjectFactory::NewObject already set ObjectName)
                    NewAsset->SetName(NewAsset->ObjectName);

					FString AssetNameStr = NewAsset->GetName().ToString();
                    UResourceManager::GetInstance().Add<UPhysicsAsset>(AssetNameStr, NewAsset);

                    // Assign to viewer state
                    State->CurrentPhysicsAsset = NewAsset;
                    State->CurrentPhysicsAsset->SetFilePath(FString());

                    USkeletalMeshComponent* SkeletalComp = ActiveState->PreviewActor->GetSkeletalMeshComponent();
                    assert(SkeletalComp);
                    if (SkeletalComp)
                    {
                        SkeletalComp->SetPhysicsAsset(NewAsset);
                    }

                    UE_LOG("Created new PhysicsAsset: %s", NewAsset->GetName().ToString().c_str());
                }
                else
                {
                    UE_LOG("Failed to create new UPhysicsAsset");
                }
            }

            if (State->CurrentPhysicsAsset)
            {
                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.60f, 0.45f, 0.35f, 0.7f));
                ImGui::Separator();
                ImGui::PopStyleColor();
                ImGui::Spacing();

                const FString& SavePath = State->CurrentPhysicsAsset->GetFilePath();
                if (!SavePath.empty())
                {
                    std::filesystem::path DisplayPath(SavePath);
                    FString FileName = DisplayPath.filename().string();
                    if (!FileName.empty())
                    {
                        State->CurrentPhysicsAsset->SetName(FName(FileName.c_str()));
                    }
                }

                ImGui::Text("Active Physics Asset: %s", State->CurrentPhysicsAsset->GetName().ToString().c_str());
                ImGui::Text("Save Path: %s", SavePath.empty() ? "<None>" : SavePath.c_str());

                // Save button: save currently selected PhysicsAsset to file
                if (ImGui::Button("Save to File"))
                {
                    if (State->CurrentPhysicsAsset)
                    {
                        FWideString WideInitialPath;
                        if (!PhysicsAssetPath.empty())
                        {
                            WideInitialPath = PhysicsAssetPath.wstring();
                        }
                        else
                        {
                            WideInitialPath = UTF8ToWide(GDataDir) + L"/NewPhysicsAsset.phys";
                        }
                        
                        std::filesystem::path WidePath = FPlatformProcess::OpenSaveFileDialog(WideInitialPath, L"phys", L"Physics Asset Files");
                        if (WidePath.empty()) { return;}
                        FString AbsolutePathUtf8 = NormalizePath(WideToUTF8(WidePath.wstring()));
                        FString BaseDirUtf8;
                        if (!PhysicsAssetPath.empty())
                        {
                            std::filesystem::path BaseDirPath = PhysicsAssetPath.parent_path();
                            BaseDirUtf8 = NormalizePath(WideToUTF8(BaseDirPath.wstring()));
                        }
                        else
                        {
                            BaseDirUtf8 = GDataDir;
                        }

                        FString PathStr = ResolveAssetRelativePath(AbsolutePathUtf8, BaseDirUtf8);

                        if (State->CurrentPhysicsAsset->SaveToFile(PathStr))
                        {
                            State->CurrentPhysicsAsset->SetFilePath(PathStr);
                            UE_LOG("PhysicsAsset saved to: %s", PathStr.c_str());
                        }
                        else
                        {
                            UE_LOG("Failed to save PhysicsAsset to: %s", PathStr.c_str());
                        }
                    }
                    else
                    {
                        UE_LOG("No PhysicsAsset selected to save");
                    }
                }

				ImGui::SameLine(0.0f, 20.0f);
                if(ImGui::Button("Apply to Current Mesh"))
                {
                    if (State->CurrentMesh && State->CurrentPhysicsAsset)
                    {
                        // 1. 현재 Mesh 리소스에 적용
                        State->CurrentMesh->PhysicsAsset = State->CurrentPhysicsAsset;
                        UE_LOG("Applied PhysicsAsset to current SkeletalMesh");

                        // 2. SkeletalMeshComponent에도 적용 (물리 바디 생성)
                        USkeletalMeshComponent* SkelComp = State->PreviewActor ?
                            State->PreviewActor->GetSkeletalMeshComponent() : nullptr;
                        if (SkelComp)
                        {
                            SkelComp->SetPhysicsAsset(State->CurrentPhysicsAsset);
                            UE_LOG("Applied PhysicsAsset to SkeletalMeshComponent (Bodies: %d)", SkelComp->GetNumBodies());
                        }

                        // 3. 메타파일에 저장
                        FString meshPath = State->CurrentMesh->GetPathFileName();
                        if (!meshPath.empty())
                        {
                            FString metaPath = meshPath + ".meta.json";
                            JSON metaJson = JSON::Make(JSON::Class::Object);
                            metaJson["DefaultPhysicsAsset"] = State->CurrentPhysicsAsset->GetFilePath().c_str();

                            std::ofstream outFile(metaPath, std::ios::out);
                            if (outFile.is_open())
                            {
                                outFile << metaJson.dump(4); // Pretty print
                                outFile.close();
                                UE_LOG("Saved SkeletalMesh metadata to: %s", metaPath.c_str());
                            }
                            else
                            {
                                UE_LOG("Failed to open metadata file for writing: %s", metaPath.c_str());
                            }
                        }
                        else
                        {
                            UE_LOG("Cannot save metadata: SkeletalMesh path is empty.");
                        }
                    }
                    else
                    {
                        if (!State->CurrentMesh)
                        {
                            UE_LOG("No SkeletalMesh loaded to apply PhysicsAsset to");
                        }
                        if (!State->CurrentPhysicsAsset)
                        {
                            UE_LOG("No PhysicsAsset selected to apply");
                        }
                    }
                }

                if (ImGui::Button("Generate All Body"))
                {
                    ImGui::OpenPopup("ShapeTypePopup");
                }
                auto RebuildPhysicsAssetWithShape = [&](EAggCollisionShapeType ShapeType)
                    {
                        if (!State->CurrentMesh)
                        {
                            UE_LOG("Cannot generate PhysicsAsset: No mesh available");
                            return;
                        }

                        if (!State->CurrentPhysicsAsset)
                        {
                            UE_LOG("Cannot generate PhysicsAsset: No physics asset instance available");
                            return;
                        }

                        USkeletalMeshComponent* SkeletalComponent = State->PreviewActor ? State->PreviewActor->GetSkeletalMeshComponent() : nullptr;
                        if (!SkeletalComponent)
                        {
                            UE_LOG("Cannot generate PhysicsAsset: Preview skeletal mesh component missing");
                            return;
                        }

                        State->CurrentPhysicsAsset->CreateGenerateAllBodySetup(
                            ShapeType,
                            State->CurrentMesh->GetSkeleton(),
                            SkeletalComponent
                        );

                        State->CurrentPhysicsAsset->SetFilePath(FString()); // 임시 저장 경로
                    };

                if (ImGui::BeginPopup("ShapeTypePopup"))
                {
                    if (ImGui::Selectable("Sphere"))
                    {
                        RebuildPhysicsAssetWithShape(EAggCollisionShapeType::Sphere);
                        ImGui::CloseCurrentPopup();
                    }
                    if (ImGui::Selectable("Box"))
                    {
                        RebuildPhysicsAssetWithShape(EAggCollisionShapeType::Box);
                        ImGui::CloseCurrentPopup();
                    }
                    if (ImGui::Selectable("Capsule"))
                    {
                        RebuildPhysicsAssetWithShape(EAggCollisionShapeType::Capsule);
                        ImGui::CloseCurrentPopup();
                    }

                    ImGui::EndPopup();
                }
            }
        }
    }
}

void SSkeletalMeshViewerWindow::DrawPhysicsConstraintGraph(ViewerState* State)
{
    if (!State || !State->CurrentPhysicsAsset || !PhysicsGraphContext || !PhysicsGraphBuilder || State->SelectedBodyIndexForGraph == -1)
    {
        return;
    }

    // SelectedBodyIndexForGraph가 변경되었는지 추적
    static int32 PrevSelectedBodyIndexForGraph = -1;
    if (State->SelectedBodyIndexForGraph != PrevSelectedBodyIndexForGraph)
    {
        // 그래프 컨텍스트 재생성
        if (PhysicsGraphContext)
        {
            ed::DestroyEditor(PhysicsGraphContext);
        }

        ed::Config PhysicsGraphConfig;
        PhysicsGraphContext = ed::CreateEditor(&PhysicsGraphConfig);

        PrevSelectedBodyIndexForGraph = State->SelectedBodyIndexForGraph;
    }

    UPhysicsAsset* PhysAsset = State->CurrentPhysicsAsset;

    ed::SetCurrentEditor(PhysicsGraphContext);

    ImVec2 availableRegion = ImGui::GetContentRegionAvail();
    ed::Begin("PhysicsConstraintGraph", availableRegion);

    TArray<int32> ConnectedBodyIndices;
    TArray<int32> RelevantConstraintIndices;

    if (State->SelectedBodyIndexForGraph >= 0 && State->SelectedBodyIndexForGraph < PhysAsset->BodySetups.Num())
    {
        UBodySetup* SelectedBody = PhysAsset->BodySetups[State->SelectedBodyIndexForGraph];
        FName SelectedBodyName = SelectedBody->BoneName;

        // Find all constraints connected to selected body
        for (int32 ConstraintIdx = 0; ConstraintIdx < PhysAsset->Constraints.Num(); ++ConstraintIdx)
        {
            const FPhysicsConstraintSetup& Constraint = PhysAsset->Constraints[ConstraintIdx];

            if (Constraint.BodyNameA == SelectedBodyName || Constraint.BodyNameB == SelectedBodyName)
            {
                RelevantConstraintIndices.Add(ConstraintIdx);

                // Find the other body in this constraint
                FName OtherBodyName = (Constraint.BodyNameA == SelectedBodyName) ? Constraint.BodyNameB : Constraint.BodyNameA;
                int32 OtherBodyIndex = PhysAsset->FindBodyIndex(OtherBodyName);
                if (OtherBodyIndex != INDEX_NONE)
                {
                    ConnectedBodyIndices.Add(OtherBodyIndex);
                }
            }
        }
    }

    // === Render Body Nodes ===

    // Render selected body node (center)
    if (State->SelectedBodyIndexForGraph >= 0 && State->SelectedBodyIndexForGraph < PhysAsset->BodySetups.Num())
    {
        UBodySetup* SelectedBody = PhysAsset->BodySetups[State->SelectedBodyIndexForGraph];
        int32 NodeID = GetBodyNodeID(State->SelectedBodyIndexForGraph);

        ed::SetNodePosition(ed::NodeId(NodeID), ImVec2(100, 150));

        PhysicsGraphBuilder->Begin(ed::NodeId(NodeID));

        // Header with body color
        bool bIsSelected = (State->SelectedBodyIndex == State->SelectedBodyIndexForGraph);
        PhysicsGraphBuilder->Header(bIsSelected ? ImColor(255, 200, 100) : ImColor(150, 180, 255));
        ImGui::TextUnformatted(SelectedBody->BoneName.ToString().c_str());
        ImGui::Dummy(ImVec2(0, 28));
        PhysicsGraphBuilder->EndHeader();

        PhysicsGraphBuilder->Output(ed::PinId(GetBodyOutputPinID(State->SelectedBodyIndexForGraph)));
        PhysicsGraphBuilder->EndOutput();

        PhysicsGraphBuilder->Input(ed::PinId(GetBodyInputPinID(State->SelectedBodyIndexForGraph)));
        PhysicsGraphBuilder->EndInput();

        // Middle section with constraint info
        PhysicsGraphBuilder->Middle();
        FString Str = std::to_string(SelectedBody->GetTotalShapeCount()) + " shape(s)";
        ImGui::TextUnformatted(Str.c_str());

        PhysicsGraphBuilder->End();
    }

    // Render connected body nodes (arranged around selected body)
    int32 ConnectedIdx = 0;
    float FixedX = 100 + 500.0f; // Fixed x position (center x + radius)
	float StartY = 150.0f - ((ConnectedBodyIndices.size() - 1) * 50.0f); // Start y position
    float YSpacing = 100.0f; // Vertical spacing between nodes

    for (int32 BodyIndex : ConnectedBodyIndices)
    {
        if (BodyIndex < 0 || BodyIndex >= PhysAsset->BodySetups.Num())
            continue;

        UBodySetup* Body = PhysAsset->BodySetups[BodyIndex];
        int32 NodeID = GetBodyNodeID(BodyIndex);

        // Fixed x position, increasing y position for each connected body
        ImVec2 Pos(FixedX, StartY + (ConnectedIdx * YSpacing));

        ed::SetNodePosition(ed::NodeId(NodeID), Pos);

        PhysicsGraphBuilder->Begin(ed::NodeId(NodeID));

		bool bIsSelected = (State->SelectedBodyIndex == BodyIndex);

        // Header with different color for connected bodies
        PhysicsGraphBuilder->Header(bIsSelected ? ImColor(255, 200, 100) : ImColor(150, 180, 255));
        ImGui::TextUnformatted(Body->BoneName.ToString().c_str());
        ImGui::Dummy(ImVec2(0, 28));
        PhysicsGraphBuilder->EndHeader();

        // Middle section with constraint info
        PhysicsGraphBuilder->Middle();
        FString Str = std::to_string(Body->GetTotalShapeCount()) + " shape(s)";
        ImGui::TextUnformatted(Str.c_str());

        PhysicsGraphBuilder->Input(ed::PinId(GetBodyInputPinID(BodyIndex)));
        PhysicsGraphBuilder->EndInput();

        PhysicsGraphBuilder->End();

        ConnectedIdx++;
    }
    // === Render Constraint Nodes (between bodies) ===
    for (int32 ConstraintIdx : RelevantConstraintIndices)
    {
        if (ConstraintIdx < 0 || ConstraintIdx >= PhysAsset->Constraints.Num())
            continue;

        const FPhysicsConstraintSetup& Constraint = PhysAsset->Constraints[ConstraintIdx];
        int32 NodeID = GetConstraintNodeID(ConstraintIdx);

        int32 BodyAIndex = PhysAsset->FindBodyIndex(Constraint.BodyNameA);
        int32 BodyBIndex = PhysAsset->FindBodyIndex(Constraint.BodyNameB);

        if (BodyAIndex == INDEX_NONE || BodyBIndex == INDEX_NONE)
            continue;

        int32 InputBodyIndex = State->SelectedBodyIndexForGraph == BodyAIndex ? BodyAIndex : BodyBIndex;
        int32 OutputBodyIndex = (InputBodyIndex == BodyAIndex) ? BodyBIndex : BodyAIndex;

        // Position constraint node between the two bodies
        ImVec2 PosA = ed::GetNodePosition(ed::NodeId(GetBodyNodeID(InputBodyIndex)));
        ImVec2 PosB = ed::GetNodePosition(ed::NodeId(GetBodyNodeID(OutputBodyIndex)));
        ImVec2 MidPos((PosA.x + PosB.x) * 0.5f, PosB.y);

        ed::SetNodePosition(ed::NodeId(NodeID), MidPos);

        PhysicsGraphBuilder->Begin(ed::NodeId(NodeID));

        // Header with constraint color
        bool bIsSelected = (State->SelectedConstraintIndex == ConstraintIdx);
        PhysicsGraphBuilder->Header(bIsSelected ? ImColor(255, 150, 150) : ImColor(100, 255, 200));
        ImGui::TextUnformatted("Constraint");
        ImGui::Dummy(ImVec2(0, 28));
        PhysicsGraphBuilder->EndHeader();

        // Output pin to child body
        PhysicsGraphBuilder->Output(ed::PinId(GetConstraintOutputPinID(ConstraintIdx)));
        PhysicsGraphBuilder->EndOutput();

        // Input pin from parent body
        PhysicsGraphBuilder->Input(ed::PinId(GetConstraintInputPinID(ConstraintIdx)));
        PhysicsGraphBuilder->EndInput();

        // Middle section with constraint info
        PhysicsGraphBuilder->Middle();
		FString Str = Constraint.BodyNameA.ToString() + " -> " + Constraint.BodyNameB.ToString();
		ImGui::TextUnformatted(Str.c_str());

        PhysicsGraphBuilder->End();
    }

    // === Render Links between Bodies and Constraints ===
    for (int32 ConstraintIdx : RelevantConstraintIndices)
    {
        if (ConstraintIdx < 0 || ConstraintIdx >= PhysAsset->Constraints.Num())
            continue;

        const FPhysicsConstraintSetup& Constraint = PhysAsset->Constraints[ConstraintIdx];

        int32 BodyAIndex = PhysAsset->FindBodyIndex(Constraint.BodyNameA);
        int32 BodyBIndex = PhysAsset->FindBodyIndex(Constraint.BodyNameB);

        if (BodyAIndex == INDEX_NONE || BodyBIndex == INDEX_NONE)
            continue;

        int32 FromBodyIndex = State->SelectedBodyIndexForGraph == BodyAIndex ? BodyAIndex : BodyBIndex;
        int32 ToBodyIndex = (FromBodyIndex == BodyAIndex) ? BodyBIndex : BodyAIndex;

        // Link from parent body to constraint input
        uint64 LinkID_AtoConstraint = (1ULL << 56) | (uint64(ConstraintIdx) << 32) | uint64(FromBodyIndex);
        ed::Link(ed::LinkId(LinkID_AtoConstraint),
            ed::PinId(GetBodyOutputPinID(FromBodyIndex)),
            ed::PinId(GetConstraintInputPinID(ConstraintIdx)),
            ImColor(200, 200, 200), 2.0f);

        // Link from constraint output to child body
        uint64 LinkID_ConstraintToB = (2ULL << 56) | (uint64(ConstraintIdx) << 32) | uint64(ToBodyIndex);
        ed::Link(ed::LinkId(LinkID_ConstraintToB),
            ed::PinId(GetConstraintOutputPinID(ConstraintIdx)),
            ed::PinId(GetBodyInputPinID(ToBodyIndex)),
            ImColor(200, 200, 200), 2.0f);
    }

    // Handle node selection
    if (ed::BeginCreate())
    {

    }
    // Disable link creation in this graph
    ed::EndCreate();

    // Handle node selection - 개선된 버전
    ed::NodeId ClickedNodeId = 0;
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        ClickedNodeId = ed::GetHoveredNode();
    }

    if (ClickedNodeId)
    {
        int32 NodeIDValue = (int32)(uintptr_t)ClickedNodeId.Get();

        // Check if it's a body node (1000000 ~ 1999999)
        if (NodeIDValue >= 1000000 && NodeIDValue < 2000000)
        {
            int32 BodyIndex = NodeIDValue - 1000000;
            UE_LOG("Double-clicked Body node index: %d", BodyIndex);

            if (BodyIndex >= 0 && BodyIndex < PhysAsset->BodySetups.Num())
            {
                State->SelectedBodySetup = PhysAsset->BodySetups[BodyIndex];
                State->SelectedBodyIndex = BodyIndex;
                State->SelectedConstraintIndex = -1;
                State->SelectedPreviewMesh = nullptr;

                // Also select the corresponding bone
                if (State->CurrentMesh)
                {
                    const FSkeleton* Skeleton = State->CurrentMesh->GetSkeleton();
                    if (Skeleton)
                    {
                        int32 BoneIndex = Skeleton->FindBoneIndex(PhysAsset->BodySetups[BodyIndex]->BoneName);
                        if (BoneIndex != INDEX_NONE)
                        {
                            State->SelectedBoneIndex = BoneIndex;
                            State->bBoneLinesDirty = true;
                            ExpandToSelectedBone(State, BoneIndex);

                            if (State->PreviewActor && State->World)
                            {
                                State->PreviewActor->RepositionAnchorToBone(BoneIndex);
                                if (USceneComponent* Anchor = State->PreviewActor->GetBoneGizmoAnchor())
                                {
                                    State->World->GetSelectionManager()->SelectActor(State->PreviewActor);
                                    State->World->GetSelectionManager()->SelectComponent(Anchor);
                                }
                            }
                        }
                    }
                }

                // 이 경우에는 아래 처리를 안하면, 호버링에 다음과 같은 버그가 생김: 아래 조건에 해당하는 노드 클릭 후, 마지막 constraint 노드에 호버링하면, 이 노드가 하이라이트 되버림
                if (State->SelectedBodyIndex == State->SelectedBodyIndexForGraph)
                {
                    // Node Editor 종료
                    ed::End();
                    ed::SetCurrentEditor(nullptr);

                    // 컨텍스트 재생성
                    if (PhysicsGraphContext)
                    {
                        ed::DestroyEditor(PhysicsGraphContext);
                    }

                    ed::Config PhysicsGraphConfig;
                    PhysicsGraphContext = ed::CreateEditor(&PhysicsGraphConfig);

                    // 즉시 반환 (다음 프레임에서 깨끗한 상태로 다시 그림)
                    return;
                }
            }
        }
        // Check if it's a constraint node (3000000 ~ 3999999)
        else if (NodeIDValue >= 3000000 && NodeIDValue < 4000000)
        {
            int32 ConstraintIndex = NodeIDValue - 3000000;
            UE_LOG("Double-clicked ConstraintIndex: %d", ConstraintIndex);

            if (ConstraintIndex >= 0 && ConstraintIndex < PhysAsset->Constraints.Num())
            {
                State->SelectedConstraintIndex = ConstraintIndex;
                State->SelectedBodySetup = nullptr;
                State->SelectedBodyIndex = -1;
                State->SelectedBoneIndex = -1;
                State->SelectedPreviewMesh = nullptr;
            }
        }
    }

    ed::End();
    ed::SetCurrentEditor(nullptr);
}
