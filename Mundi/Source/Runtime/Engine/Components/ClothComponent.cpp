#include "pch.h"
#include "ClothComponent.h"
#include "Source/Runtime/Engine/Physics/Cloth/ClothManager.h"

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

}

void UClothComponent::InitializeComponent()
{
	Super::InitializeComponent();

	// VetexBuffer 생성
	if (SkeletalMesh && !CPUSkinnedVertexBuffer)
	{
		SkeletalMesh->CreateCPUSkinnedVertexBuffer(&CPUSkinnedVertexBuffer);
	}

	if (bClothEnabled && !bClothInitialized)
	{
		SetupClothFromMesh();
	}

	// Cloth는 CPU에서 스키닝된 버퍼를 직접 업데이트하므로 기본 스키닝 업데이트를 막아둔다
	bSkinningMatricesDirty = false;
}

void UClothComponent::BeginPlay()
{
	Super::BeginPlay();

	// TODO: 이게 왜 필요함?   
	// 초기화가 되었고, Vertex가 존재한다면 Cloth의 위치에 맞게 vertex 업데이트
	if (bClothInitialized && CPUSkinnedVertexBuffer && SkeletalMesh)
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
			particles[vertexIndex] = physx::PxVec4(attachPos.X , attachPos.Y, attachPos.Z, 0.0f);
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

	//GPU에서 처리하도록 코드를 추가하는것도 생각
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
	//cloth->setSolverFrequency(120.0f);
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
	//cloth->setTetherConstraintStiffness(0.0f); // 비활성화
	//cloth->setTetherConstraintStiffness(1.0f); // 완전 고정
}

void UClothComponent::UpdateVerticesFromCloth()
{
	if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshData() || PreviousParticles.Num() == 0)
	{
		return;
	}

	const auto& meshData = SkeletalMesh->GetSkeletalMeshData();
	const auto& groupInfos = SkeletalMesh->GetMeshGroupInfo();

	int32 ParticleIndex = 0;

	for (const auto& group : groupInfos)
	{
		UpdateSectionVertices(group, ParticleIndex);
	}

	// 노멀 재계산
	RecalculateNormals();

	// VertexBuffer 갱신
	if (CPUSkinnedVertexBuffer)
	{
		SkeletalMesh->UpdateVertexBuffer(SkinnedVertices, CPUSkinnedVertexBuffer);
	}
}

void UClothComponent::RecalculateNormals()
{
	if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshData())
		return;

	const auto& indices = SkeletalMesh->GetSkeletalMeshData()->Indices;
	if (indices.Num() == 0 || indices.Num() % 3 != 0)
		return;

	// 모든 노멀을 0으로 초기화
	for (auto& vertex : SkinnedVertices)
	{
		vertex.normal = FVector::Zero();
	}

	// 각각의 면위치 노멀을 계산해서 정점에 누적
	for (int32 i = 0; i < indices.Num(); i += 3)
	{
		uint32 idx0 = indices[i];
		uint32 idx1 = indices[i + 1];
		uint32 idx2 = indices[i + 2];

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
}

void UClothComponent::RestoreOrigina1lState()
{
}

void UClothComponent::ExtractClothSection(const FGroupInfo& Group, const TArray<FSkinnedVertex>& AllVertices, const TArray<uint32>& AllIndices)
{
}

void UClothComponent::ExtractClothSectionOrdered(const FGroupInfo& Group, const TArray<FSkinnedVertex>& AllVertices, const TArray<uint32>& AllIndices)
{
}

bool UClothComponent::ShouldFixVertex(const FSkinnedVertex& Vertex)
{
	return false;
}

void UClothComponent::UpdateSectionVertices(const FGroupInfo& Group, int32& ParticleIdx)
{
	// Section의 인덱스로부터 정점 추출
	const auto& AllIndices = SkeletalMesh->GetSkeletalMeshData()->Indices;
	const auto& AllVertices = SkeletalMesh->GetSkeletalMeshData()->Vertices;
	const auto& OriginalVertices = SkeletalMesh->GetSkeletalMeshData()->Vertices;

	TSet<uint32> UsedVertices;
	SkinnedVertices.resize(AllVertices.size());
	for (uint32 i = 0; i < Group.IndexCount; ++i)
	{
		uint32 GlobalVertexIdx = AllIndices[Group.StartIndex + i];

		if (!UsedVertices.Contains(GlobalVertexIdx))
		{
			UsedVertices.Add(GlobalVertexIdx);

			// Cloth 시뮬레이션 결과를 SkinnedVertices에 반영
			// 글로벌 정점 인덱스로 직접 매핑
			if (GlobalVertexIdx >= (uint32)PreviousParticles.Num()) { continue; }
			const physx::PxVec4& Particle = PreviousParticles[GlobalVertexIdx];
			const auto& OriginalVertex = OriginalVertices[GlobalVertexIdx];

			SkinnedVertices[GlobalVertexIdx].pos = FVector(Particle.x, Particle.y, Particle.z);
			SkinnedVertices[GlobalVertexIdx].tex = OriginalVertex.UV;
			SkinnedVertices[GlobalVertexIdx].Tangent = FVector4(OriginalVertex.Tangent.X, OriginalVertex.Tangent.Y, OriginalVertex.Tangent.Z, OriginalVertex.Tangent.W);
			SkinnedVertices[GlobalVertexIdx].color = FVector4(1, 1, 1, 1);
			SkinnedVertices[GlobalVertexIdx].normal = FVector(0, 0, 1);  // RecalculateNormals에서 재계산됨

			// 순차 ParticleIdx에 의존하지 않음
		}
	}
}

void UClothComponent::CollectMeshBatches(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View)
{
	if (!bClothEnabled || !SkeletalMesh || !SkeletalMesh->GetSkeletalMeshData())
	{
		return;
	}

	if (!bClothInitialized)
	{
		InitializeComponent();
	}

	// 부모(USkinnedMeshComponent)의 배치 생성 로직을 그대로 활용
	Super::CollectMeshBatches(OutMeshBatchElements, View);
}

void UClothComponent::SetupClothFromMesh()
{
	if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshData())
	{
		return;
	}

	// 이미 초기화가 되었다면, 기존것을 날리고 새로 할당 
	if (bClothInitialized)
	{
		ReleaseCloth();
	}
	
	BuildClothMesh();
	CreateClothFabric();
	CreateClothInstance();
	CreatePhaseConfig();
	
	// 생성한 Cloth를 solver에 전달
	// 성공적으로 생성된 경우에만 솔버에 추가 및 초기화
	{
		const bool bFabricValid = (fabric != nullptr);
		const bool bClothValid = (cloth != nullptr);
		const bool bHasTriangles = (ClothIndices.Num() >= 3) && (ClothIndices.Num() % 3 == 0);
		if (bFabricValid && bClothValid && bHasTriangles)
		{
			FClothManager::GetInstance().AddClothToSolver(cloth);
			ApplyClothProperties();
			ApplyTetherConstraint();
			bClothInitialized = true;
		}
		else
		{
			bClothInitialized = false;
			UE_LOG("[ClothComponent] Setup failed: fabric=%d cloth=%d indices=%d", bFabricValid, bClothValid, ClothIndices.Num());
		}
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
	if(!SkeletalMesh || !SkeletalMesh->GetSkeleton())
		return INDEX_NONE;

	const FSkeleton* Skeleton = SkeletalMesh->GetSkeleton();
	auto It = Skeleton->BoneNameToIndex.find(BoneName.ToString());
	if (It != Skeleton->BoneNameToIndex.end())
		return It->second;

	return INDEX_NONE;
}

FTransform UClothComponent::GetBoneTransform(int32 BoneIndex) const
{
	if (!SkeletalMesh || !SkeletalMesh->GetSkeleton())
		return FTransform();

	// BoneIndex가 유효하지 않으면, return 기본값
	const FSkeleton* Skeleton = SkeletalMesh->GetSkeleton();
	if (BoneIndex < 0 || BoneIndex >= Skeleton->Bones.Num())
	{
		return FTransform(); 
	}

	// BindPose로 부터 Transform 생성
	const FBone& Bone = Skeleton->Bones[BoneIndex];
	FTransform BoneTransform(Bone.BindPose);

	// 컴포넌트의 월드 공간 변환과 결합
	FTransform ComponentTransform = GetWorldTransform(); 
	return ComponentTransform.GetWorldTransform(BoneTransform);
}

FVector UClothComponent::GetBoneLocation(const FName& BoneName)
{
	int32 boneIndex = GetBoneIndex(BoneName);
	if (boneIndex != INDEX_NONE)
	{
		return GetBoneTransform(boneIndex).GetLocation();
	}
	return FVector::Zero();
} 

void UClothComponent::OnCreatePhysicsState()
{
	
}

void UClothComponent::BuildClothMesh()
{
	if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshData())
		return;
	 
	ClothParticles.Empty();
	ClothIndices.Empty();
	  
	const auto& meshData = SkeletalMesh->GetSkeletalMeshData();
	const auto& vertices = meshData->Vertices;
	const auto& indices = meshData->Indices; 

	for (const auto& vertex : vertices)
	{
		const float invMass = 1.0f; //ShouldFixVertex(Vertex) ? 0.0f : 1.0f;

		ClothParticles.Add(physx::PxVec4(
			vertex.Position.X,
			vertex.Position.Y,
			vertex.Position.Z,
			invMass
		));
	}

	// 삼각형 인덱스 복사 (Fabric 요건 충족)
	if (!indices.IsEmpty())
	{
		ClothIndices = indices;
	}

	PreviousParticles = ClothParticles;
}

void UClothComponent::CreateClothFabric()
{
	ClothMeshDesc meshDesc;
	meshDesc.setToDefault();

	meshDesc.points.data = ClothParticles.GetData();
	meshDesc.points.stride = sizeof(physx::PxVec4);
	meshDesc.points.count = ClothParticles.Num();

	meshDesc.triangles.data = ClothIndices.GetData();
	meshDesc.triangles.stride = sizeof(uint32) * 3;
	meshDesc.triangles.count = ClothIndices.Num() / 3;


	physx::PxVec3 gravity(0, 0, -981.0f); // cm/s^2

	fabric = NvClothCookFabricFromMesh(FClothManager::GetInstance().GetFactory(), meshDesc, gravity, &phaseTypeInfo, false);

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
			//UE_LOG(LogTemp, Error, TEXT("Invalid phase type!"));
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
