#include "pch.h"
#include "ClothComponent.h"
#include "Source/Runtime/Engine/Physics/Cloth/ClothManager.h"

using namespace nv::cloth;
 
UClothComponent::UClothComponent()
{
	bCanEverTick = true;
	bClothEnabled = true;
	bClothInitialized = false;
	
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
	ID3D11Buffer* VertexBuffer = GetCPUSkinnedVertexBuffer();
	if (SkeletalMesh && !VertexBuffer)
	{
		SkeletalMesh->CreateCPUSkinnedVertexBuffer(&VertexBuffer);
	}

	// 
	if (bClothEnabled && !bClothInitialized)
	{
		SetupClothFromMesh();
	}
}

void UClothComponent::BeginPlay()
{
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
}

void UClothComponent::ApplyTetherConstraint()
{
}

void UClothComponent::UpdateVerticesFromCloth()
{
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

void UClothComponent::CollectMeshBatches(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View)
{
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
	FClothManager::GetInstance().AddClothToSolver(cloth);

	ApplyClothProperties();
	ApplyTetherConstraint();
	
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

	// // 1) Collect section indices (keep original order)
	//TArray<uint32> SectionIndices;
	//SectionIndices.Reserve(Group.IndexCount);
	//for (uint32 i = 0; i < Group.IndexCount; ++i)
	//{
	//	const uint32 GlobalIndex = AllIndices[Group.StartIndex + i];
	//	SectionIndices.Add(GlobalIndex);
	//}

	//// 2) Build ordered-unique vertex list by first appearance and a Global->Local map
	//TArray<uint32> OrderedUniqueGlobals;
	//OrderedUniqueGlobals.Reserve(SectionIndices.Num());
	//TMap<uint32, uint32> GlobalToLocal;

	//for (uint32 GlobalIdx : SectionIndices)
	//{
	//	if (!GlobalToLocal.Contains(GlobalIdx))
	//	{
	//		const uint32 NewLocal = (uint32)OrderedUniqueGlobals.Num();
	//		GlobalToLocal.Add(GlobalIdx, NewLocal);
	//		OrderedUniqueGlobals.Add(GlobalIdx);
	//	}
	//}

	//// 3) Append particles in the same ordered-unique order for stable indexing
	//for (uint32 GlobalIdx : OrderedUniqueGlobals)
	//{
	//	const auto& Vertex = AllVertices[GlobalIdx];
	//	const float invMass = 1.0f; //ShouldFixVertex(Vertex) ? 0.0f : 1.0f;
	//	ClothParticles.Add(physx::PxVec4(
	//		Vertex.Position.X,
	//		Vertex.Position.Y,
	//		Vertex.Position.Z,
	//		invMass
	//	));
	//}

	//// 4) Remap section indices to local (preserve triangle winding)
	//for (uint32 GlobalIdx : SectionIndices)
	//{
	//	const uint32 LocalIdx = GlobalToLocal[GlobalIdx];
	//	ClothIndices.Add(LocalIdx);
	//} 

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
