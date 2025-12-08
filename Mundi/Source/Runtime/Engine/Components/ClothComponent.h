#pragma once
#include "SkinnedMeshComponent.h"
#include "StaticMesh.h"
#include <unordered_map>
#include "NvCloth/include/Factory.h"
#include "NvCloth/include/Fabric.h"
#include "NvCloth/include/Cloth.h"
#include "NvCloth/include/Solver.h"
#include "NvCloth/include/Callbacks.h"
#include "NvCloth/include/NvClothExt/ClothFabricCooker.h"
#include "foundation/PxAllocatorCallback.h"
#include "foundation/PxErrorCallback.h"
#include "UClothComponent.generated.h"

class UClothWeightAsset;

/**
 * @brief Cloth 시뮬레이션 설정
 */

struct FClothSimulationSettings
{
	// Physics properties
	float Mass = 1.0f;                          // 전체 질량
	float Damping = 0.1f;                       // 감쇠 (0-1) - 낮춰서 더 자연스럽게
	float LinearDrag = 0.05f;                   // 선형 저항 - 낮춰서 공기 저항 감소
	float AngularDrag = 0.1f;                   // 각속도 저항 - 낮춰서 회전이 더 자유롭게
	float Friction = 0.5f;                      // 마찰
	// Stretch constraints (Horizontal/Vertical)
	float StretchStiffness = 0.5f;              // 신축 강성 (0-1)
	float StretchStiffnessMultiplier = 0.5f;    // 강성 배율
	float CompressionLimit = 1.0f;              // 압축 제한
	float StretchLimit = 1.0f;                  // 신장 제한

	// Bend constraints
	float BendStiffness = 0.5f;                 // 굽힘 강성 (0-1)
	float BendStiffnessMultiplier = 0.5f;       // 굽힘 배율

	// Shear constraints
	float ShearStiffness = 0.5f;                // 전단 강성 (0-1)
	float ShearStiffnessMultiplier = 0.75f;     // 전단 배율

	// Collision
	float CollisionMassScale = 0.0f;            // 충돌 질량 스케일
	bool bEnableContinuousCollision = true;     // 연속 충돌 검사
	float CollisionFriction = 0.0f;             // 충돌 마찰

	// Self collision
	bool bEnableSelfCollision = false;          // 자체 충돌 활성화
	float SelfCollisionDistance = 0.0f;         // 자체 충돌 거리
	float SelfCollisionStiffness = 1.0f;        // 자체 충돌 강성

	// Solver
	int32 SolverFrequency = 120;                // Solver 주파수 (Hz)
	int32 StiffnessFrequency = 10;              // 강성 업데이트 주파수

	// Gravity
	bool bUseGravity = true;                    // 중력 사용
	FVector GravityOverride = FVector(0, 0, -9.80f); // 중력 오버라이드 (cm/s^2) - 현실적인 중력

	// Wind
	FVector WindVelocity = FVector(0, 0, 0);    // 바람 속도 (cm/s) - 기본값: 바람 없음
	float WindDrag = 0.1f;                      // 바람 저항 (0-1) - 낮춰서 저항 감소
	float WindLift = 0.1f;                      // 바람 양력 (0-1) - 낮춰서 떠오르는 효과 감소

	// Tether constraints (거리 제약)
	bool bUseTethers = true;                    // Tether 사용
	float TetherStiffness = 0.2f;               // Tether 강성
	float TetherScale = 1.2f;                   // Tether 스케일

	// Fixed vertices (고정 정점)
	float FixedVertexRatio = 0.1f;              // 상단에서 고정할 정점 비율 (0-1)

	// Inertia
	float LinearInertiaScale = 1.0f;            // 선형 관성 스케일
	float AngularInertiaScale = 1.0f;           // 각 관성 스케일
	float CentrifugalInertiaScale = 1.0f;       // 원심력 관성 스케일

};
class UClothComponent : public USkinnedMeshComponent
{
	GENERATED_REFLECTION_BODY()
public:
	UClothComponent();
	virtual ~UClothComponent();

	// Lifecycle
	virtual void InitializeComponent() override;
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime) override;
	virtual void OnCreatePhysicsState() override;
	virtual void CollectMeshBatches(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View) override;
	virtual void DuplicateSubObjects() override;
	virtual FAABB GetWorldAABB() const override;

	// Cloth Set up
	void SetupClothFromMesh();
	void ReleaseCloth();

	// Helper functions (for attachment)
	int32 GetBoneIndex(const FName& BoneName) const;
	FTransform GetBoneTransform(int32 BoneIndex) const;
	FVector GetBoneLocation(const FName& BoneName);
	//FVector GetAttachmentPosition(int32 AttachmentIndex);

	// Cloth Paint API
	int32 GetClothVertexCount() const { return ClothParticles.Num(); }
	FVector GetClothVertexPosition(int32 ClothVertexIndex) const;
	float GetVertexWeight(int32 ClothVertexIndex) const;
	const TArray<uint32>& GetClothIndices() const { return ClothIndices; }
	void SetVertexWeight(int32 ClothVertexIndex, float Weight);
	void SetVertexWeightByMeshVertex(uint32 MeshVertexIndex, float Weight);
	void InitializeVertexWeights(float DefaultWeight = 1.0f);
	void ApplyPaintedWeights();
	void ApplyWeightsToClothParticles();  // ClothParticles.w에 weight 적용 (CreateClothInstance 전에 호출)
	bool LoadWeightsToArray();            // ClothVertexWeights 배열에만 로드 (NvCloth 적용 안함)
	void LoadWeightsFromMap(const std::unordered_map<uint32, float>& InClothVertexWeights);
	const TArray<float>& GetVertexWeights() const { return ClothVertexWeights; }
	const TArray<uint32>& GetClothVertexToMeshVertexMapping() const { return ClothVertexToMeshVertex; }
	int32 FindClothVertexByMeshVertex(uint32 MeshVertexIndex) const;

	// Weight Asset API
	/**
	 * @brief ClothWeightAsset 파일 경로 설정
	 * 경로가 변경되면 자동으로 weight를 다시 로드하고 NvCloth에 적용합니다.
	 */
	void SetClothWeightAssetPath(const FString& InPath);
	const FString& GetClothWeightAssetPath() const { return ClothWeightAssetPath; }

	// Wind API
	/**
	 * @brief Wind 속도를 실시간으로 설정
	 * @param InWindVelocity 바람 속도 벡터 (cm/s)
	 */
	void SetWindVelocity(const FVector& InWindVelocity);

	/**
	 * @brief ClothWeightAsset 파일에서 가중치 로드
	 * @return 로드 성공 여부
	 */
	bool LoadWeightsFromAsset();

	/**
	 * @brief ClothWeightAsset을 직접 적용
	 */
	bool LoadWeightsFromAsset(UClothWeightAsset* InAsset);

	// Weight Visualization
	void SetWeightVisualizationEnabled(bool bEnabled) { bShowWeightVisualization = bEnabled; }
	bool IsWeightVisualizationEnabled() const { return bShowWeightVisualization; }
	void UpdateVertexColorsFromWeights();
	static FVector WeightToColor(float Weight);

protected:
	// Internal helper
	void CreateClothFabric();
	void CreateClothInstance();
	void CreatePhaseConfig(); 
	void CreateSolver(); 
	void BuildClothMesh();
	 
	//Simulate
	void UpdateClothSimulation(float DeltaTime);
	void AttachingClothToCharacter();
	void RetrievingSimulateResult();
	void ApplyClothProperties();
	void ApplyTetherConstraint();

	void UpdateVerticesFromCloth();
	void RecalculateNormals();
	FVector GetAttachmentPosition(int AttachmentIndex);

	// PIE State Management
	TArray<physx::PxVec4> CacheOriginalParticles;    // PIE 시작 전 원본 위치 (복구용)
	bool bHasSavedOriginalState = false;        // 원본 상태 저장 여부
	void SaveOriginalState();
	 

	// CPU-side mesh data
	//TArray<FNormalVertex> ClothVertices;       // 원본 정점 데이터 
	//TArray<FNormalVertex> RenderedVertices;    // 렌더링용 정점 데이터 (시뮬레이션 결과 반영)
	//TArray<uint32> ClothMeshIndices;

	// GPU Buffers
	//ID3D11Buffer* ClothVertexBuffer = nullptr;
	//ID3D11Buffer* ClothIndexBuffer = nullptr;

	// Cloth Simulation Data
	TArray<physx::PxVec4> ClothParticles;
	TArray<physx::PxVec4> PreviousParticles;
	TArray<physx::PxVec3> ClothNormals;
	TArray<uint32> ClothIndices;
	TArray<float> ClothInvMasses;

	// 메시 정점 인덱스 -> Cloth 파티클 인덱스 매핑
	TArray<int32> MeshVertexToClothParticle;

	// Cloth 파티클 -> 원본 메시 정점들 매핑 (UV seam 대응)
	TArray<TArray<uint32>> ParticleToGlobalVertices;

	// Cloth 정점 인덱스 -> 대표 메시 정점 인덱스 매핑
	TArray<uint32> ClothVertexToMeshVertex;

	TArray<int32> AttachmentVertices;
	TArray<FName> AttachmentBoneNames;
	TArray<FVector> AttachmentOffsets;

	nv::cloth::Fabric* fabric;
	nv::cloth::Cloth* cloth;
	nv::cloth::Vector<int32_t>::Type phaseTypeInfo;
	nv::cloth::PhaseConfig* phases;

	// Setting
	FClothSimulationSettings ClothSettings;
	bool bClothEnabled = true;
	bool bClothInitialized = false;

	// Cloth Paint
	TArray<float> ClothVertexWeights;  // 각 Cloth 버텍스의 가중치 (0=고정, 1=자유)
	float DefaultInvMass = 0.5f;       // 기본 invMass 값
	bool bShowWeightVisualization = false;  // Weight 시각화 모드

	UPROPERTY(EditAnywhere, Category="Cloth", Tooltip="Cloth Weight 에셋 파일 경로")
	FString ClothWeightAssetPath;      // ClothWeightAsset 파일 경로

	// Attachment Data

	// Constraints
};