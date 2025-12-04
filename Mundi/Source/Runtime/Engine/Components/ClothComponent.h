#pragma once 
#include "SkinnedMeshComponent.h"
#include "NvCloth/include/Factory.h"
#include "NvCloth/include/Fabric.h"
#include "NvCloth/include/Cloth.h"
#include "NvCloth/include/Solver.h"
#include "NvCloth/include/Callbacks.h"	
#include "NvCloth/include/NvClothExt/ClothFabricCooker.h"
#include "foundation/PxAllocatorCallback.h"
#include "foundation/PxErrorCallback.h"
#include "UClothComponent.generated.h"


/**
 * @brief Cloth 시뮬레이션 설정
 */

struct FClothSimulationSettings
{
	// Physics properties
	float Mass = 1.0f;                          // 전체 질량
	float Damping = 0.2f;                       // 감쇠 (0-1)
	float LinearDrag = 0.2f;                    // 선형 저항
	float AngularDrag = 0.2f;                   // 각속도 저항
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
	FVector GravityOverride = FVector(0, 0, -2.8f); // 중력 오버라이드 (cm/s^2) - 9.8 m/s^2

	// Wind
	FVector WindVelocity = FVector(-10, -10, 0);  // 바람 속도 (cm/s) - 기본값: 바람 없음
	float WindDrag = 0.5f;                      // 바람 저항 (0-1)
	float WindLift = 0.3f;                      // 바람 양력 (0-1)

	// Tether constraints (거리 제약)
	bool bUseTethers = true;                    // Tether 사용
	float TetherStiffness = 0.5f;               // Tether 강성
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
	
	// Cloth Set up
	void SetupClothFromMesh();
	void ReleaseCloth();

	// Helper functions
	int32 GetBoneIndex(const FName& BoneName) const;
	FTransform GetBoneTransform(int32 BoneIndex) const;
	FVector GetBoneLocation(const FName& BoneName);
	//FVector GetAttachmentPosition(int32 AttachmentIndex);

	// Cloth Pain API 
	/*
		int32 GetClothVertexCount() const { return ClothParticles.Num(); }
		FVector GetClothVertexPosition(int32 ClothVertexIndex) const;
		float GetVertexWeight(int32 ClothVertexIndex) const;
		void SetVertexWeight(int32 ClothVertexIndex, float Weight);
		void SetVertexWeightByMeshVertex(uint32 MeshVertexIndex, float Weight);
		void InitializeVertexWeights();
		void ApplyPaintedWeights();
		void LoadWeightsFromPhysicsAsset(const TMap<uint32, float>& InClothVertexWeights);  // PhysicsAsset에서 weight 로드
		const TArray<float>& GetVertexWeights() const { return ClothVertexWeights; }
		const TArray<uint32>& GetClothVertexToMeshVertexMapping() const { return ClothVertexToMeshVertex; }
		int32 FindClothVertexByMeshVertex(uint32 MeshVertexIndex) const; 
	*/

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
	FVector GetAttachmentPosition(int AttachmentIndex);

	// PIE State Management 
	bool bHasSavedOriginalState;

	void SaveOriginalState();
	void RestoreOrigina1lState();
	void ExtractClothSection(const FGroupInfo& Group, const TArray<FSkinnedVertex>& AllVertices, const TArray<uint32>& AllIndices);
	
	void ExtractClothSectionOrdered(const FGroupInfo& Group, const TArray<FSkinnedVertex>& AllVertices, const TArray<uint32>& AllIndices);
	bool ShouldFixVertex(const FSkinnedVertex& Vertex);
	void UpdateSectionVertices(const FGroupInfo& Group, int32& ParticleIdx);
	 
	// Cloth Data
	TArray<physx::PxVec4> ClothParticles;
	TArray<physx::PxVec4> PreviousParticles;
	TArray<physx::PxVec3> ClothNormals;
	TArray<uint32> ClothIndices;                 
	TArray<float> ClothInvMasses;               

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
	 
	// Attachment Data 

	// Constraints
};