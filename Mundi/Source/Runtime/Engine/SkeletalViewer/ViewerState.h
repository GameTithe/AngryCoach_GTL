#pragma once

enum class EAssetBrowserMode : uint8
{
    Animation,
    Montage,
    PhysicsAsset
};

class ASkeletalMeshActor;
class FViewport;
class FViewportClient;
class UAnimSequence;
class UAnimMontage;
class USkeletalMesh;
class UWorld;
class UPhysicsAsset;

class ViewerState
{
public:
    FName Name;
    UWorld* World = nullptr;
    FViewport* Viewport = nullptr;
    FViewportClient* Client = nullptr;
    
    // Have a pointer to the currently selected mesh to render in the viewer
    ASkeletalMeshActor* PreviewActor = nullptr;
    USkeletalMesh* CurrentMesh = nullptr;
    FString LoadedMeshPath;  // Track loaded mesh path for unloading
    int32 SelectedBoneIndex = -1;
    bool bShowMesh = true;
    bool bShowBones = true;
    // Bone line rebuild control
    bool bBoneLinesDirty = true;      // true면 본 라인 재구성
    int32 LastSelectedBoneIndex = -1; // 색상 갱신을 위한 이전 선택 인덱스
    // UI path buffer per-tab
    char MeshPathBuffer[260] = {0};
    std::set<int32> ExpandedBoneIndices;

    // 본 트랜스폼 편집 관련
    FVector EditBoneLocation;
    FVector EditBoneRotation;  // Euler angles in degrees
    FVector EditBoneScale;
    
    bool bBoneTransformChanged = false;
    bool bBoneRotationEditing = false;

    // 애니메이션 관련
    UAnimSequence* CurrentAnimation = nullptr;
    
    float CurrentAnimTime = 0.0f;
    int32 CurrentAnimFrames = 0;
    
    float TimelineScale = 10.0f;
    float TimelineOffset = 0.0f;
    
    bool bIsPlaying = false;
    bool bIsPlayingReverse = false;
    bool bTimeChanged = false;
    bool bIsRecording = false;
    bool bIsLooping = true;

	// ======== 피직스 애셋 관련 ==========
    
    // Asset Browser Mode
    EAssetBrowserMode AssetBrowserMode = EAssetBrowserMode::Animation;
    
    // Physics Asset
    UPhysicsAsset* CurrentPhysicsAsset = nullptr;

    // Selected BodySetup (for showing body details / selection)
    UBodySetup* SelectedBodySetup = nullptr;
	int32 SelectedBodyIndex = -1;

    // Selected Constraint (for showing constraint details / selection)
    int32 SelectedConstraintIndex = -1;

	int32 SelectedBodyIndexForGraph = -1; // for physics graph visualization

    // Rename state for Physics Asset: use boolean (CurrentPhysicsAsset holds the asset being renamed)
    bool bIsRenaming = false;                          // true while inline-rename is active
    char PhysicsAssetNameBuffer[128] = { 0 };          // temporary editable b

	bool bChangedGeomNum = false;

	bool bShowBodies = true;
	bool bShowConstraintLines = true;
	bool bShowConstraintLimits = true;

	// ======== 래그돌 시뮬레이션 관련 ==========
	bool bSimulatePhysics = false;  // 물리 시뮬레이션 활성화 여부 (래그돌 토글)

	// ======== 몽타주 편집 관련 ==========
	UAnimMontage* CurrentMontage = nullptr;          // 현재 편집 중인 몽타주
	UAnimMontage* EditingMontage = nullptr;          // 새로 생성 중인 몽타주 (저장 전)
	int32 SelectedMontageSection = -1;               // 현재 선택된 섹션 인덱스
	char MontageNameBuffer[128] = {0};               // 몽타주 이름 입력 버퍼
	bool bCreatingNewMontage = false;                // 새 몽타주 생성 모드
	float MontagePreviewTime = 0.0f;                 // 몽타주 타임라인 시간

	// 섹션 블렌딩
	int32 CurrentMontageSection = -1;                // 현재 재생 중인 섹션
	int32 PrevMontageSection = -1;                   // 이전 섹션 (블렌딩용)
	float SectionBlendTime = 0.0f;                   // 블렌드 진행 시간
	float SectionBlendDuration = 0.0f;              // 블렌드 총 시간
	float PrevSectionEndTime = 0.0f;                 // 이전 섹션 끝 시간
};
