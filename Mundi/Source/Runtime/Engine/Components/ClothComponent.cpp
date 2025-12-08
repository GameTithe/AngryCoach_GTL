#include "pch.h"
#include "ClothComponent.h"
#include "Source/Runtime/Engine/Physics/Cloth/ClothManager.h"
#include "Source/Runtime/Engine/WeightPaint/ClothWeightAsset.h"
#include "ResourceManager.h"
#include "SceneView.h"
#include "MeshBatchElement.h"
#include <d3d11.h>
#include <cfloat>
#include <filesystem>

using namespace nv::cloth;

UClothComponent::UClothComponent()
{
	bCanEverTick = true;
	bClothEnabled = true;
	bClothInitialized = false;
	cloth = nullptr;
	fabric = nullptr;
	phases = nullptr; 

	bHasSavedOriginalState = false;
}

UClothComponent::~UClothComponent()
{
	ReleaseCloth();

	//if (ClothVertexBuffer)
	//{
	//	ClothVertexBuffer->Release();
	//	ClothVertexBuffer = nullptr;
	//}
	//
	//if (ClothIndexBuffer)
	//{
	//	ClothIndexBuffer->Release();
	//	ClothIndexBuffer = nullptr;
	//}
}

void UClothComponent::InitializeComponent()
{
	Super::InitializeComponent();

	if (bClothEnabled && !bClothInitialized && SkeletalMesh)
	{
		SetupClothFromMesh();

		// SetupClothFromMesh에서 성공적으로 초기화되었는지 확인
		if (cloth && fabric)
		{
			bClothInitialized = true;
		}
	}
}

void UClothComponent::BeginPlay()
{
	Super::BeginPlay();

	if (bClothInitialized && CPUSkinnedVertexBuffer)
	{
		UpdateVerticesFromCloth();
	}
}

void UClothComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);

	if (IsPendingDestroy() || !IsRegistered())
	{
		return;
	}

	// 에디터 모드에서는 시뮬레이션 X, PIE에서만 실행
	if (GetWorld() && !GetWorld()->bPie)
	{
		return;
	}

	if (!bClothEnabled)
	{
		return;
	}

	if (!bClothInitialized)
	{
		InitializeComponent();
	}

	UpdateClothSimulation(DeltaTime);
}

void UClothComponent::UpdateClothSimulation(float DeltaTime)
{
	if (!cloth)
	{
		UE_LOG("[ClothComponent] Cloth is NULL");
		return;
	}

	// 캐릭터와 부착된 정점 갱신
	AttachingClothToCharacter();

	// Cloth Manager가 ClothSimulation을 돌려줄거임

	// 결과 가져오기
	RetrievingSimulateResult();

	// 렌더링 정점 갱신
	UpdateVerticesFromCloth();
}

void UClothComponent::AttachingClothToCharacter()
{
	if (!cloth || AttachmentVertices.Num() == 0)
		return;

	// 정점 데이터를 직접 갱신
	MappedRange<physx::PxVec4> particles = cloth->getCurrentParticles();

	for (int i = 0; i < AttachmentVertices.Num(); ++i)
	{
		int32 vertexIndex = AttachmentVertices[i];

		if (vertexIndex >= 0 && vertexIndex < particles.size())
		{
			FVector attachPos = GetAttachmentPosition(i);

			// 캐릭터에 부착되는 부분이, w = 0.0으로 세팅
			particles[vertexIndex] = physx::PxVec4(attachPos.X, attachPos.Y, attachPos.Z, 0.0f);
		}
	}
}

void UClothComponent::RetrievingSimulateResult()
{
	if (!cloth)
	{
		return;
	}

	nv::cloth::MappedRange<physx::PxVec4> particles = cloth->getCurrentParticles();

	// Manager에서 돌린 simulation 결과물 복사
	PreviousParticles.SetNum(particles.size());
	for (int i = 0; i < particles.size(); ++i)
	{
		PreviousParticles[i] = particles[i];
	}
}

void UClothComponent::ApplyClothProperties()
{
	if (!cloth)
		return;

	// 중력 설정 반영
	if (ClothSettings.bUseGravity)
	{
		cloth->setGravity(physx::PxVec3(
			ClothSettings.GravityOverride.X,
			ClothSettings.GravityOverride.Y,
			ClothSettings.GravityOverride.Z
		));
	}

	// 감쇠 설정 반영
	cloth->setDamping(physx::PxVec3(
		ClothSettings.Damping,
		ClothSettings.Damping,
		ClothSettings.Damping
	));

	cloth->setFriction(0.5f);

	// 선형/각속도 저항
	cloth->setLinearDrag(physx::PxVec3(
		ClothSettings.LinearDrag,
		ClothSettings.LinearDrag,
		ClothSettings.LinearDrag
	));

	cloth->setAngularDrag(physx::PxVec3(
		ClothSettings.AngularDrag,
		ClothSettings.AngularDrag,
		ClothSettings.AngularDrag
	));

	// Solver 주파수 설정 반영
	cloth->setSolverFrequency(60.0f); //default is 300  게임 fps 보다 낮게 설정하면 시각적으로 어색함

	// 바람 설정 반영
	cloth->setWindVelocity(physx::PxVec3(
		ClothSettings.WindVelocity.X,
		ClothSettings.WindVelocity.Y,
		ClothSettings.WindVelocity.Z
	));

	cloth->setDragCoefficient(ClothSettings.WindDrag);
	cloth->setLiftCoefficient(ClothSettings.WindLift);
}

void UClothComponent::ApplyTetherConstraint()
{
	if (!cloth || !ClothSettings.bUseTethers)
		return;

	cloth->setTetherConstraintScale(ClothSettings.TetherScale);
	cloth->setTetherConstraintStiffness(ClothSettings.TetherStiffness);
}

void UClothComponent::UpdateVerticesFromCloth()
{
	if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshData() || PreviousParticles.Num() == 0 || !CPUSkinnedVertexBuffer)
	{
		return;
	}

	const FSkeletalMeshData* MeshAsset = SkeletalMesh->GetSkeletalMeshData();
	const TArray<FSkinnedVertex>& OriginalVertices = MeshAsset->Vertices;

	// SkinnedVertices는 원본 메시의 정점 개수와 동일
	SkinnedVertices.SetNum(OriginalVertices.Num());

	// 각 particle의 결과를 해당하는 모든 원본 정점에 적용
	for (int32 ParticleIdx = 0; ParticleIdx < PreviousParticles.Num(); ++ParticleIdx)
	{
		const physx::PxVec4& particle = PreviousParticles[ParticleIdx];
		const FVector NewPos(particle.x, particle.y, particle.z);

		// 이 particle에 매핑된 모든 원본 정점들 업데이트
		if (ParticleIdx < ParticleToGlobalVertices.Num())
		{
			for (uint32 GlobalIdx : ParticleToGlobalVertices[ParticleIdx])
			{
				if (GlobalIdx < (uint32)OriginalVertices.Num())
				{
					// 위치는 시뮬레이션 결과 사용
					SkinnedVertices[GlobalIdx].pos = NewPos;

					// UV, Tangent, Color는 원본 메시 데이터 유지
					SkinnedVertices[GlobalIdx].tex = OriginalVertices[GlobalIdx].UV;
					SkinnedVertices[GlobalIdx].Tangent = OriginalVertices[GlobalIdx].Tangent;
					SkinnedVertices[GlobalIdx].color = OriginalVertices[GlobalIdx].Color;
					SkinnedVertices[GlobalIdx].normal = FVector(0, 0, 1); // RecalculateNormals에서 재계산됨
				}
			}
		}
	}

	// 노멀 재계산
	RecalculateNormals();

	// Vertex Buffer 갱신
	if (CPUSkinnedVertexBuffer)
	{
		SkeletalMesh->UpdateVertexBuffer(SkinnedVertices, CPUSkinnedVertexBuffer);
	}
}

void UClothComponent::RecalculateNormals()
{
	if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshData())
		return;

	const FSkeletalMeshData* MeshData = SkeletalMesh->GetSkeletalMeshData();
	const TArray<uint32>& Indices = MeshData->Indices;

	if (Indices.Num() == 0 || Indices.Num() % 3 != 0)
		return;

	// 모든 노멀을 0으로 초기화
	for (auto& vertex : SkinnedVertices)
	{
		vertex.normal = FVector::Zero();
	}

	// 각각의 면위치 노멀을 계산해서 정점에 누적
	for (int32 i = 0; i < Indices.Num(); i += 3)
	{
		uint32 idx0 = Indices[i];
		uint32 idx1 = Indices[i + 1];
		uint32 idx2 = Indices[i + 2];

		if (idx0 >= SkinnedVertices.Num() || idx1 >= SkinnedVertices.Num() || idx2 >= SkinnedVertices.Num())
			continue;

		const FVector& v0 = SkinnedVertices[idx0].pos;
		const FVector& v1 = SkinnedVertices[idx1].pos;
		const FVector& v2 = SkinnedVertices[idx2].pos;

		// 면위치의 벡터
		FVector edge1 = v1 - v0;
		FVector edge2 = v2 - v0;

		// 외적으로 노멀 계산 (시계방향 전제)
		FVector faceNormal = FVector::Cross(edge1, edge2);

		// 각 정점에 노멀 누적
		SkinnedVertices[idx0].normal += faceNormal;
		SkinnedVertices[idx1].normal += faceNormal;
		SkinnedVertices[idx2].normal += faceNormal;
	}

	// 누적된 노멀을 정규화
	for (auto& vertex : SkinnedVertices)
	{
		vertex.normal.Normalize();
	}
}

FVector UClothComponent::GetAttachmentPosition(int AttachmentIndex)
{
	if (AttachmentIndex < 0 || AttachmentIndex >= AttachmentBoneNames.Num())
	{
		return FVector::Zero();
	}

	FName boneName = AttachmentBoneNames[AttachmentIndex];
	int32 boneIndex = GetBoneIndex(boneName);

	if (boneIndex != INDEX_NONE)
	{
		FTransform boneTransform = GetBoneTransform(boneIndex);

		// 본으로부터 떨어지는 것도 감안
		FVector offset = AttachmentOffsets.IsValidIndex(AttachmentIndex) ? AttachmentOffsets[AttachmentIndex] : FVector::Zero();
		return boneTransform.TransformPosition(offset);
	}

	return FVector::Zero();
}

void UClothComponent::SaveOriginalState()
{
	if (!cloth || ClothParticles.Num() == 0) 
	{
		return;
	}
	 

	// 현재 ClothParticles의 초기 상태를 저장
	CacheOriginalParticles = ClothParticles;
	bHasSavedOriginalState = true;
}

void UClothComponent::CollectMeshBatches(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View)
{
	// ClothComponent는 자체 렌더링 로직을 사용 (Super 호출하지 않음)

	// 1. SkeletalMesh 유효성 검사
	if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshData())
	{
		return;
	}

	// PIE 모드에서만 Cloth 초기화 시도
	if (GetWorld() && GetWorld()->bPie && !bClothInitialized && bClothEnabled)
	{
		InitializeComponent();
	}

	// 2. CPU 버퍼가 없으면 생성
	if (!CPUSkinnedVertexBuffer)
	{
		SkeletalMesh->CreateCPUSkinnedVertexBuffer(&CPUSkinnedVertexBuffer);
		UE_LOG("[ClothComponent] Created CPUSkinnedVertexBuffer in CollectMeshBatches");
	}

	// 3. CPU 버퍼 업데이트
	const FSkeletalMeshData* MeshAsset = SkeletalMesh->GetSkeletalMeshData();
	const TArray<FSkinnedVertex>& OriginalVertices = MeshAsset->Vertices;

	// PIE 모드이고 Cloth가 초기화되었으면 시뮬레이션 결과 사용
	if (bClothInitialized && bClothEnabled && SkinnedVertices.Num() > 0)
	{
		SkeletalMesh->UpdateVertexBuffer(SkinnedVertices, CPUSkinnedVertexBuffer);
	}
	else
	{
		// 에디터 모드 또는 PIE 종료 후: 원본 메시 데이터로 렌더링
		if (SkinnedVertices.Num() != OriginalVertices.Num())
		{
			SkinnedVertices.SetNum(OriginalVertices.Num());
		}

		for (int32 i = 0; i < OriginalVertices.Num(); ++i)
		{
			SkinnedVertices[i].pos = OriginalVertices[i].Position;
			SkinnedVertices[i].tex = OriginalVertices[i].UV;
			SkinnedVertices[i].Tangent = OriginalVertices[i].Tangent;
			SkinnedVertices[i].normal = OriginalVertices[i].Normal;
			SkinnedVertices[i].color = OriginalVertices[i].Color;
		}

		SkeletalMesh->UpdateVertexBuffer(SkinnedVertices, CPUSkinnedVertexBuffer);
	}

	// 4. Material & Shader 결정
	UMaterialInterface* Material = GetMaterial(0);
	UShader* Shader = nullptr;

	if (Material && Material->GetShader())
	{
		Shader = Material->GetShader();
	}
	else
	{
		Material = UResourceManager::GetInstance().GetDefaultMaterial();
		if (Material)
		{
			Shader = Material->GetShader();
		}
		if (!Material || !Shader)
		{
			UE_LOG("UClothComponent: 기본 머티리얼이 없습니다.");
			return;
		}
	}

	// 5. Shader Variant 컴파일
	FMeshBatchElement BatchElement;
	TArray<FShaderMacro> ShaderMacros = View->ViewShaderMacros;

	if (0 < Material->GetShaderMacros().Num())
	{
		ShaderMacros.Append(Material->GetShaderMacros());
	}

	FShaderVariant* ShaderVariant = Shader->GetOrCompileShaderVariant(ShaderMacros);

	if (ShaderVariant)
	{
		BatchElement.VertexShader = ShaderVariant->VertexShader;
		BatchElement.PixelShader = ShaderVariant->PixelShader;
		BatchElement.InputLayout = ShaderVariant->InputLayout;
	}

	// 6. Batch Element 설정 (부모 클래스의 버퍼 사용)
	BatchElement.Material = Material;
	BatchElement.VertexBuffer = CPUSkinnedVertexBuffer;  // 부모의 버퍼 사용
	BatchElement.VertexStride = SkeletalMesh->GetCPUSkinnedVertexStride();
	BatchElement.IndexBuffer = SkeletalMesh->GetIndexBuffer();  // 부모의 IndexBuffer 사용
	BatchElement.IndexCount = SkeletalMesh->GetIndexCount();
	BatchElement.StartIndex = 0;
	BatchElement.BaseVertexIndex = 0;
	BatchElement.WorldMatrix = GetWorldMatrix();
	BatchElement.ObjectID = InternalIndex;
	BatchElement.PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	OutMeshBatchElements.Add(BatchElement);
}

void UClothComponent::SetupClothFromMesh()
{
	if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshData())
	{
		UE_LOG("[ClothComponent] SetupClothFromMesh: ClothMesh is null");
		return;
	}

	// 이미 초기화가 되었다면, 기존것을 날리고 새로 할당
	if (bClothInitialized)
	{
		ReleaseCloth();
	}

	// CPUSkinnedVertexBuffer가 없으면 생성 (부모 클래스의 버퍼 사용)
	if (!CPUSkinnedVertexBuffer)
	{
		SkeletalMesh->CreateCPUSkinnedVertexBuffer(&CPUSkinnedVertexBuffer);
		UE_LOG("[ClothComponent] Created CPUSkinnedVertexBuffer");
	}

	BuildClothMesh();
	CreateClothFabric();

	// ★★★ 중요: CreateClothInstance 호출 전에 Weight를 ClothParticles에 적용! ★★★
	// NvCloth는 createCloth()시 ClothParticles를 복사하므로, 그 전에 invMass가 설정되어야 함
	if (!ClothWeightAssetPath.empty() || SkeletalMesh)
	{
		// Weight 에셋 로드 시도 (ClothVertexWeights 배열에 저장)
		if (!LoadWeightsToArray())
		{
			// 에셋이 없으면 기본 가중치로 초기화
			InitializeVertexWeights(1.0f);
		}

		// ClothParticles의 w(invMass)에 가중치 적용
		ApplyWeightsToClothParticles();
	}

	CreateClothInstance();
	CreatePhaseConfig();

	// 생성한 Cloth를 solver에 전달
	// 성공적으로 생성된 경우에만 솔버에 추가 및 초기화
	const bool bFabricValid = (fabric != nullptr);
	const bool bClothValid = (cloth != nullptr);
	const bool bHasTriangles = (ClothIndices.Num() >= 3) && (ClothIndices.Num() % 3 == 0);

	if (bFabricValid && bClothValid && bHasTriangles)
	{
		// Cloth 생성 후에도 current/previous particles 동기화
		ApplyPaintedWeights();

		// Solver에 추가
		FClothManager::GetInstance().AddClothToSolver(cloth);
		ApplyClothProperties();
		ApplyTetherConstraint();

		UE_LOG("[ClothComponent] Cloth initialized successfully - Particles: %d, Indices: %d", ClothParticles.Num(), ClothIndices.Num());
	}
	else
	{
		UE_LOG("[ClothComponent] Setup failed: fabric=%d cloth=%d indices=%d", bFabricValid, bClothValid, ClothIndices.Num());
	}

	// PIE모드에서 시뮬레이션 시작 전 원본 상태 저장
	if (GetWorld() && GetWorld()->bPie && !bHasSavedOriginalState)
	{
		SaveOriginalState();
	}
}

void UClothComponent::ReleaseCloth()
{
	// Cloth를 Solver에서 제거 (삭제 전 필수)
	if (cloth)
	{
		FClothManager::GetInstance().GetSolver()->removeCloth(cloth);
	}

	// Cloth 삭제
	if (cloth)
	{
		UE_LOG("[ClothComponent] Deleting cloth\n");
		NV_CLOTH_DELETE(cloth);
		cloth = nullptr;
	}
	// Phases 삭제
	if (phases)
	{
		UE_LOG("[ClothComponent] Deleting phases\n");
		delete[] phases;
		phases = nullptr;
	}
	// Fabric 해제
	if (fabric)
	{
		UE_LOG("[ClothComponent] Releasing fabric\n");
		fabric->decRefCount();
		fabric = nullptr;
	}

	// 초기화 상태 리셋
	bClothInitialized = false;
}

int32 UClothComponent::GetBoneIndex(const FName& BoneName) const
{
	// Cloth는 bone이 없으므로 항상 INDEX_NONE 반환
	return INDEX_NONE;
}

FTransform UClothComponent::GetBoneTransform(int32 BoneIndex) const
{
	// Cloth는 bone이 없으므로 기본 Transform 반환
	return FTransform();
}

FVector UClothComponent::GetBoneLocation(const FName& BoneName)
{
	return FVector::Zero();
}

void UClothComponent::OnCreatePhysicsState()
{
}

FAABB UClothComponent::GetWorldAABB() const
{
	// ClothComponent는 bone이 없으므로 현재 정점 위치로 AABB 계산
	if (!SkeletalMesh || SkinnedVertices.Num() == 0)
	{
		// 정점이 없으면 컴포넌트 위치 기준으로 작은 AABB 반환
		FVector Origin = GetWorldLocation();
		return FAABB(Origin - FVector(10.0f), Origin + FVector(10.0f));
	}

	// 현재 월드 변환
	const FMatrix& WorldMatrix = GetWorldMatrix();

	// 정점들로부터 AABB 계산
	FAABB WorldAABB = FAABB(FVector(FLT_MAX), FVector(-FLT_MAX));

	for (const FNormalVertex& Vertex : SkinnedVertices)
	{
		FVector WorldPos = WorldMatrix.TransformPosition(Vertex.pos);
		FAABB PointAABB(WorldPos, WorldPos);
		WorldAABB = FAABB::Union(WorldAABB, PointAABB);
	}

	// AABB가 유효하지 않으면 기본값 반환
	if (!WorldAABB.IsValid())
	{
		FVector Origin = GetWorldLocation();
		return FAABB(Origin - FVector(10.0f), Origin + FVector(10.0f));
	}

	return WorldAABB;
}

void UClothComponent::BuildClothMesh()
{
	if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshData())
		return;

	ClothParticles.Empty();
	ClothIndices.Empty();
	MeshVertexToClothParticle.Empty();
	ParticleToGlobalVertices.Empty();
	ClothVertexToMeshVertex.Empty();

	const FSkeletalMeshData* MeshAsset = SkeletalMesh->GetSkeletalMeshData();
	const TArray<FSkinnedVertex>& AllVertices = MeshAsset->Vertices;
	const TArray<uint32>& AllIndices = MeshAsset->Indices;

	// UV seam 처리: 같은 위치의 정점들을 하나의 particle로 병합
	const float POSITION_EPSILON = 0.001f;  // 위치 비교용 epsilon

	// 위치를 정수 그리드로 양자화하여 해시맵 키로 사용
	auto QuantizePosition = [POSITION_EPSILON](const FVector& Pos) -> uint64
	{
		// 각 축을 epsilon 단위로 양자화 (floor 사용하여 음수 좌표 올바르게 처리)
		int32 qx = static_cast<int32>(std::floor(Pos.X / POSITION_EPSILON));
		int32 qy = static_cast<int32>(std::floor(Pos.Y / POSITION_EPSILON));
		int32 qz = static_cast<int32>(std::floor(Pos.Z / POSITION_EPSILON));
		// 64비트 해시 생성 (21비트씩 사용, 부호 비트 포함)
		uint64 ux = static_cast<uint64>(static_cast<uint32>(qx)) & 0x1FFFFF;
		uint64 uy = static_cast<uint64>(static_cast<uint32>(qy)) & 0x1FFFFF;
		uint64 uz = static_cast<uint64>(static_cast<uint32>(qz)) & 0x1FFFFF;
		return (ux << 42) | (uy << 21) | uz;
	};

	// 양자화된 위치 -> particle 인덱스 매핑
	TMap<uint64, uint32> PositionToParticle;
	// Global 정점 -> particle 인덱스 매핑
	TMap<uint32, uint32> GlobalToParticle;
	// 처리 순서 유지용 - 첫 등장 순서대로 particle 생성
	TArray<uint32> ParticleOrder;  // 각 particle의 대표 global 인덱스

	// 1) 모든 인덱스를 순회하며 unique particle 생성
	for (uint32 i = 0; i < AllIndices.Num(); ++i)
	{
		uint32 GlobalIdx = AllIndices[i];

		if (GlobalToParticle.Contains(GlobalIdx))
			continue;

		const FVector& Pos = AllVertices[GlobalIdx].Position;
		uint64 PosKey = QuantizePosition(Pos);

		if (const uint32* ExistingParticle = PositionToParticle.Find(PosKey))
		{
			// 같은 위치에 이미 particle이 있음 - 해당 particle에 매핑
			GlobalToParticle.Add(GlobalIdx, *ExistingParticle);
			// ParticleToGlobalVertices에 추가
			uint32 ParticleLocalIdx = *ExistingParticle;
			if (ParticleLocalIdx < (uint32)ParticleToGlobalVertices.Num())
			{
				ParticleToGlobalVertices[ParticleLocalIdx].Add(GlobalIdx);
			}
		}
		else
		{
			// 새 particle 생성
			const uint32 NewParticleIdx = (uint32)ParticleOrder.Num();
			PositionToParticle.Add(PosKey, NewParticleIdx);
			GlobalToParticle.Add(GlobalIdx, NewParticleIdx);
			ParticleOrder.Add(GlobalIdx);

			// ParticleToGlobalVertices 배열에 새 항목 추가
			TArray<uint32> GlobalList;
			GlobalList.Add(GlobalIdx);
			ParticleToGlobalVertices.Add(GlobalList);
		}
	}

	// 2) Particle 생성 (순서대로)
	for (uint32 RepresentativeGlobalIdx : ParticleOrder)
	{
		const auto& Vertex = AllVertices[RepresentativeGlobalIdx];
		const float invMass = 0.5f;  // 기본 invMass
		ClothParticles.Add(physx::PxVec4(
			Vertex.Position.X,
			Vertex.Position.Y,
			Vertex.Position.Z,
			invMass
		));
	}

	// 3) 인덱스 리매핑 (triangle winding 보존)
	for (uint32 i = 0; i < AllIndices.Num(); ++i)
	{
		uint32 GlobalIdx = AllIndices[i];
		const uint32 ParticleIdx = GlobalToParticle[GlobalIdx];
		ClothIndices.Add(ParticleIdx);
	}

	// 4) ClothVertexToMeshVertex 매핑 (대표 정점만 저장)
	for (uint32 RepresentativeGlobalIdx : ParticleOrder)
	{
		ClothVertexToMeshVertex.Add(RepresentativeGlobalIdx);
	}

	// 5) MeshVertexToClothParticle 매핑 (전체 정점 -> particle)
	// -1로 초기화하여 매핑되지 않은 버텍스 구분
	MeshVertexToClothParticle.SetNum(AllVertices.Num());
	for (int32 i = 0; i < MeshVertexToClothParticle.Num(); ++i)
	{
		MeshVertexToClothParticle[i] = -1;
	}
	for (const auto& Pair : GlobalToParticle)
	{
		MeshVertexToClothParticle[Pair.first] = Pair.second;
	}

	PreviousParticles = ClothParticles;

	// 디버그 로그
	int32 MergedCount = 0;
	for (const auto& GlobalList : ParticleToGlobalVertices)
	{
		if (GlobalList.Num() > 1)
			MergedCount++;
	}

	UE_LOG("[ClothComponent] BuildClothMesh: Original vertices=%d, Cloth particles=%d, Merged positions=%d",
		AllVertices.Num(), ClothParticles.Num(), MergedCount);
}

void UClothComponent::CreateClothFabric()
{
	nv::cloth::Factory* factory = FClothManager::GetInstance().GetFactory();
	if (!factory)
	{
		UE_LOG("Failed to create cloth fabric: Factory is null!");
		return;
	}

	ClothMeshDesc meshDesc;
	meshDesc.setToDefault();

	meshDesc.points.data = ClothParticles.GetData();
	meshDesc.points.stride = sizeof(physx::PxVec4);
	meshDesc.points.count = ClothParticles.Num();

	meshDesc.triangles.data = ClothIndices.GetData();
	meshDesc.triangles.stride = sizeof(uint32) * 3;
	meshDesc.triangles.count = ClothIndices.Num() / 3;

	physx::PxVec3 gravity(0, 0, -981.0f); // cm/s^2

	fabric = NvClothCookFabricFromMesh(factory, meshDesc, gravity, &phaseTypeInfo, false);

	if (!fabric)
	{
		UE_LOG("Failed to cook cloth fabric!");
	}
}

void UClothComponent::CreateClothInstance()
{
	if (!FClothManager::GetInstance().GetFactory() || !fabric)
		return;

	cloth = FClothManager::GetInstance().GetFactory()->createCloth(
		nv::cloth::Range<physx::PxVec4>(ClothParticles.GetData(), ClothParticles.GetData() + ClothParticles.Num()),
		*fabric
	);

	if (!cloth)
	{
		UE_LOG("Failed to create cloth instance!");
	}
}

void UClothComponent::CreatePhaseConfig()
{
	if (!fabric || !cloth)
		return;

	int32 numPhases = fabric->getNumPhases();
	phases = new PhaseConfig[numPhases];

	for (int i = 0; i < numPhases; ++i)
	{
		phases[i].mPhaseIndex = i;

		// Phase 타입에 따라 제약 조건 반영
		switch (phaseTypeInfo[i])
		{
		case nv::cloth::ClothFabricPhaseType::eINVALID:
			break;
		case nv::cloth::ClothFabricPhaseType::eVERTICAL:
		case nv::cloth::ClothFabricPhaseType::eHORIZONTAL:
			phases[i].mStiffness = ClothSettings.StretchStiffness;
			phases[i].mStiffnessMultiplier = ClothSettings.StretchStiffnessMultiplier;
			phases[i].mCompressionLimit = ClothSettings.CompressionLimit;
			phases[i].mStretchLimit = ClothSettings.StretchLimit;
			break;
		case nv::cloth::ClothFabricPhaseType::eBENDING:
			phases[i].mStiffness = ClothSettings.BendStiffness;
			phases[i].mStiffnessMultiplier = ClothSettings.BendStiffnessMultiplier;
			phases[i].mCompressionLimit = 1.0f;
			phases[i].mStretchLimit = 1.0f;
			break;
		case nv::cloth::ClothFabricPhaseType::eSHEARING:
			phases[i].mStiffness = ClothSettings.ShearStiffness;
			phases[i].mStiffnessMultiplier = ClothSettings.ShearStiffnessMultiplier;
			phases[i].mCompressionLimit = 1.0f;
			phases[i].mStretchLimit = 1.0f;
			break;
		}
	}

	cloth->setPhaseConfig(nv::cloth::Range<nv::cloth::PhaseConfig>(phases, phases + numPhases));
}

void UClothComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();

	// NvCloth 리소스들은 복사하지 않음 (InitializeComponent에서 재생성)
	// fabric, cloth, phases는 포인터이므로 nullptr로 초기화
	fabric = nullptr;
	cloth = nullptr;
	phases = nullptr;

	// 초기화 플래그를 false로 설정하여 InitializeComponent에서 재생성되도록
	bClothInitialized = false;

}

// ============================================================
// Cloth Paint API 구현
// ============================================================

FVector UClothComponent::GetClothVertexPosition(int32 ClothVertexIndex) const
{
	if (ClothVertexIndex < 0 || ClothVertexIndex >= ClothParticles.Num())
	{
		return FVector::Zero();
	}

	const physx::PxVec4& Particle = ClothParticles[ClothVertexIndex];
	return FVector(Particle.x, Particle.y, Particle.z);
}

float UClothComponent::GetVertexWeight(int32 ClothVertexIndex) const
{
	if (ClothVertexIndex < 0 || ClothVertexIndex >= ClothVertexWeights.Num())
	{
		return 1.0f;  // 기본값: 자유
	}
	return ClothVertexWeights[ClothVertexIndex];
}

void UClothComponent::SetVertexWeight(int32 ClothVertexIndex, float Weight)
{
	if (ClothVertexIndex < 0 || ClothVertexIndex >= ClothParticles.Num())
	{
		return;
	}

	// 가중치 배열이 충분하지 않으면 확장
	if (ClothVertexWeights.Num() < ClothParticles.Num())
	{
		InitializeVertexWeights(1.0f);
	}

	// 0~1 범위로 클램프
	Weight = FMath::Clamp(Weight, 0.0f, 1.0f);
	ClothVertexWeights[ClothVertexIndex] = Weight;
}

void UClothComponent::SetVertexWeightByMeshVertex(uint32 MeshVertexIndex, float Weight)
{
	if (MeshVertexIndex >= (uint32)MeshVertexToClothParticle.Num())
	{
		return;
	}

	int32 ClothVertexIndex = MeshVertexToClothParticle[MeshVertexIndex];
	if (ClothVertexIndex >= 0)
	{
		SetVertexWeight(ClothVertexIndex, Weight);
	}
}

void UClothComponent::InitializeVertexWeights(float DefaultWeight)
{
	int32 VertexCount = ClothParticles.Num();
	ClothVertexWeights.SetNum(VertexCount);

	for (int32 i = 0; i < VertexCount; ++i)
	{
		ClothVertexWeights[i] = DefaultWeight;
	}
}

void UClothComponent::ApplyPaintedWeights()
{
	if (!cloth || ClothVertexWeights.Num() == 0)
	{
		return;
	}

	// NvCloth의 invMass 업데이트
	// Weight 0.0 = 고정 (invMass = 0)
	// Weight 1.0 = 자유 (invMass = DefaultInvMass)

	// ★ 중요: current와 previous 파티클 모두 업데이트해야 함!
	MappedRange<physx::PxVec4> currentParticles = cloth->getCurrentParticles();
	MappedRange<physx::PxVec4> previousParticles = cloth->getPreviousParticles();

	int32 AppliedCount = 0;
	int32 FixedCount = 0;

	for (int32 i = 0; i < ClothVertexWeights.Num() && i < (int32)currentParticles.size(); ++i)
	{
		float Weight = ClothVertexWeights[i];
		float InvMass = Weight * DefaultInvMass;  // Weight가 0이면 invMass도 0 (고정)

		// 위치는 유지하고 invMass(W)만 업데이트 - current particles
		currentParticles[i].w = InvMass;

		// previous particles도 동일하게 업데이트
		if (i < (int32)previousParticles.size())
		{
			previousParticles[i].w = InvMass;
		}

		AppliedCount++;
		if (InvMass == 0.0f)
		{
			FixedCount++;
		}
	}

	// ClothParticles(CPU-side)도 동기화
	for (int32 i = 0; i < ClothVertexWeights.Num() && i < ClothParticles.Num(); ++i)
	{
		float Weight = ClothVertexWeights[i];
		ClothParticles[i].w = Weight * DefaultInvMass;
	}

	UE_LOG("[ClothComponent] Applied weights to %d vertices (Fixed: %d)", AppliedCount, FixedCount);
}

void UClothComponent::ApplyWeightsToClothParticles()
{
	// ClothVertexWeights 배열의 가중치를 ClothParticles의 w(invMass)에 적용
	// CreateClothInstance() 호출 전에 사용
	if (ClothVertexWeights.Num() == 0 || ClothParticles.Num() == 0)
	{
		return;
	}

	int32 FixedCount = 0;
	for (int32 i = 0; i < ClothVertexWeights.Num() && i < ClothParticles.Num(); ++i)
	{
		float Weight = ClothVertexWeights[i];
		float InvMass = Weight * DefaultInvMass;
		ClothParticles[i].w = InvMass;

		if (InvMass == 0.0f)
		{
			FixedCount++;
		}
	}

	// PreviousParticles도 동기화
	PreviousParticles = ClothParticles;

	UE_LOG("[ClothComponent] ApplyWeightsToClothParticles: %d particles (Fixed: %d)", ClothParticles.Num(), FixedCount);
}

bool UClothComponent::LoadWeightsToArray()
{
	// Weight 에셋에서 가중치를 ClothVertexWeights 배열에만 로드
	// (NvCloth에 적용하지 않음 - CreateClothInstance 전에 사용)

	if (ClothWeightAssetPath.empty())
	{
		// 경로가 설정되지 않은 경우, 메시 경로에서 자동 추론
		if (SkeletalMesh)
		{
			FString MeshPath = SkeletalMesh->GetFilePath();
			if (!MeshPath.empty())
			{
				size_t DotPos = MeshPath.find_last_of('.');
				FString BasePath = (DotPos != FString::npos) ? MeshPath.substr(0, DotPos) : MeshPath;
				ClothWeightAssetPath = BasePath + ".clothweight";
			}
		}
	}

	if (ClothWeightAssetPath.empty())
	{
		return false;
	}

	// 파일 존재 확인
	if (!std::filesystem::exists(ClothWeightAssetPath))
	{
		UE_LOG("[ClothComponent] Weight asset file not found: %s", ClothWeightAssetPath.c_str());
		return false;
	}

	// ClothWeightAsset 로드
	UClothWeightAsset* WeightAsset = NewObject<UClothWeightAsset>();
	if (!WeightAsset)
	{
		return false;
	}

	bool bSuccess = false;
	if (WeightAsset->Load(ClothWeightAssetPath))
	{
		const auto& WeightMap = WeightAsset->GetVertexWeights();

		// 먼저 기본값으로 초기화
		InitializeVertexWeights(1.0f);

		// 맵에서 가중치 로드
		int32 LoadedCount = 0;
		for (const auto& Pair : WeightMap)
		{
			uint32 VertexIndex = Pair.first;
			float Weight = Pair.second;

			if (VertexIndex < (uint32)ClothVertexWeights.Num())
			{
				ClothVertexWeights[VertexIndex] = FMath::Clamp(Weight, 0.0f, 1.0f);
				LoadedCount++;
			}
		}

		UE_LOG("[ClothComponent] LoadWeightsToArray: Loaded %d weights from asset", LoadedCount);
		bSuccess = (LoadedCount > 0);
	}

	ObjectFactory::DeleteObject(WeightAsset);
	return bSuccess;
}

void UClothComponent::LoadWeightsFromMap(const std::unordered_map<uint32, float>& InClothVertexWeights)
{
	if (ClothParticles.Num() == 0)
	{
		return;
	}

	// 먼저 기본값으로 초기화
	InitializeVertexWeights(1.0f);

	// 제공된 맵에서 가중치 로드
	for (const auto& Pair : InClothVertexWeights)
	{
		uint32 VertexIndex = Pair.first;
		float Weight = Pair.second;

		if (VertexIndex < (uint32)ClothVertexWeights.Num())
		{
			ClothVertexWeights[VertexIndex] = FMath::Clamp(Weight, 0.0f, 1.0f);
		}
	}

	// 적용
	ApplyPaintedWeights();
}

int32 UClothComponent::FindClothVertexByMeshVertex(uint32 MeshVertexIndex) const
{
	if (MeshVertexIndex >= (uint32)MeshVertexToClothParticle.Num())
	{
		return -1;
	}
	return MeshVertexToClothParticle[MeshVertexIndex];
}

FVector UClothComponent::WeightToColor(float Weight)
{
	// Weight 0.0 = Red (고정)
	// Weight 1.0 = Green (자유)
	// 중간값 = 노랑에서 주황 계열로 보간
	Weight = FMath::Clamp(Weight, 0.0f, 1.0f);

	float R = 1.0f - Weight;  // 0.0에서 빨강, 1.0에서 검정
	float G = Weight;          // 0.0에서 검정, 1.0에서 녹색
	float B = 0.0f;

	return FVector(R, G, B);
}

void UClothComponent::UpdateVertexColorsFromWeights()
{
	if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshData() || !CPUSkinnedVertexBuffer)
	{
		return;
	}

	// 가중치 배열이 비어있으면 초기화
	if (ClothVertexWeights.Num() == 0 && ClothParticles.Num() > 0)
	{
		InitializeVertexWeights(1.0f);
	}

	const FSkeletalMeshData* MeshAsset = SkeletalMesh->GetSkeletalMeshData();
	const TArray<FSkinnedVertex>& OriginalVertices = MeshAsset->Vertices;

	// SkinnedVertices가 초기화되지 않았으면 초기화
	if (SkinnedVertices.Num() != OriginalVertices.Num())
	{
		SkinnedVertices.SetNum(OriginalVertices.Num());
		for (int32 i = 0; i < OriginalVertices.Num(); ++i)
		{
			SkinnedVertices[i].pos = OriginalVertices[i].Position;
			SkinnedVertices[i].tex = OriginalVertices[i].UV;
			SkinnedVertices[i].Tangent = OriginalVertices[i].Tangent;
			SkinnedVertices[i].normal = OriginalVertices[i].Normal;
			SkinnedVertices[i].color = OriginalVertices[i].Color;
		}
	}

	// 각 정점의 색상을 weight에 따라 업데이트
	for (int32 GlobalIdx = 0; GlobalIdx < SkinnedVertices.Num(); ++GlobalIdx)
	{
		// Global 정점 인덱스 -> Cloth 파티클 인덱스
		int32 ParticleIdx = FindClothVertexByMeshVertex(GlobalIdx);

		float Weight = 1.0f;  // 기본값: 자유
		if (ParticleIdx >= 0 && ParticleIdx < ClothVertexWeights.Num())
		{
			Weight = ClothVertexWeights[ParticleIdx];
		}

		// Weight를 색상으로 변환
		FVector Color = WeightToColor(Weight);
		SkinnedVertices[GlobalIdx].color = FVector4(Color.X, Color.Y, Color.Z, 1.0f);
	}

	// Vertex Buffer 업데이트
	if (CPUSkinnedVertexBuffer)
	{
		SkeletalMesh->UpdateVertexBuffer(SkinnedVertices, CPUSkinnedVertexBuffer);
	}

	UE_LOG("[ClothComponent] Updated vertex colors from weights (%d vertices)", SkinnedVertices.Num());
}

bool UClothComponent::LoadWeightsFromAsset()
{
	if (ClothWeightAssetPath.empty())
	{
		// 경로가 설정되지 않은 경우, 메시 경로에서 자동 추론
		if (SkeletalMesh)
		{
			FString MeshPath = SkeletalMesh->GetFilePath();
			if (!MeshPath.empty())
			{
				// 확장자 제거하고 .clothweight 추가
				size_t DotPos = MeshPath.find_last_of('.');
				FString BasePath = (DotPos != FString::npos) ? MeshPath.substr(0, DotPos) : MeshPath;
				ClothWeightAssetPath = BasePath + ".clothweight";
			}
		}
	}

	if (ClothWeightAssetPath.empty())
	{
		UE_LOG("[ClothComponent] No weight asset path specified");
		return false;
	}

	// 파일 존재 확인
	if (!std::filesystem::exists(ClothWeightAssetPath))
	{
		UE_LOG("[ClothComponent] Weight asset file not found: %s", ClothWeightAssetPath.c_str());
		return false;
	}

	// ClothWeightAsset 로드
	UClothWeightAsset* WeightAsset = NewObject<UClothWeightAsset>();
	if (!WeightAsset)
	{
		UE_LOG("[ClothComponent] Failed to create ClothWeightAsset");
		return false;
	}

	bool bSuccess = false;
	if (WeightAsset->Load(ClothWeightAssetPath))
	{
		bSuccess = LoadWeightsFromAsset(WeightAsset);
	}
	else
	{
		UE_LOG("[ClothComponent] Failed to load weight asset: %s", ClothWeightAssetPath.c_str());
	}

	// 임시 에셋 삭제
	ObjectFactory::DeleteObject(WeightAsset);
	return bSuccess;
}

bool UClothComponent::LoadWeightsFromAsset(UClothWeightAsset* InAsset)
{
	if (!InAsset)
	{
		return false;
	}

	if (ClothParticles.Num() == 0)
	{
		UE_LOG("[ClothComponent] Cannot apply weights: no cloth particles");
		return false;
	}

	// 에셋에서 가중치 맵 가져오기
	const auto& WeightMap = InAsset->GetVertexWeights();

	// 맵에서 가중치 로드
	LoadWeightsFromMap(WeightMap);

	UE_LOG("[ClothComponent] Loaded weights from asset: %d vertices",
		   InAsset->GetVertexCount());

	return true;
}

void UClothComponent::SetClothWeightAssetPath(const FString& InPath)
{
	// 경로가 같으면 무시
	if (ClothWeightAssetPath == InPath)
	{
		return;
	}

	ClothWeightAssetPath = InPath;

	// Cloth가 이미 초기화된 상태라면 weight를 다시 로드하고 적용
	if (bClothInitialized && cloth)
	{
		if (!InPath.empty())
		{
			if (LoadWeightsFromAsset())
			{
				UE_LOG("[ClothComponent] Reloaded weights from new asset: %s", InPath.c_str());
			}
			else
			{
				UE_LOG("[ClothComponent] Failed to load weights from: %s", InPath.c_str());
			}
		}
		else
		{
			// 경로가 비어있으면 기본 weight로 초기화
			InitializeVertexWeights(1.0f);
			ApplyPaintedWeights();
			UE_LOG("[ClothComponent] Reset weights to default (path cleared)");
		}
	}
}
