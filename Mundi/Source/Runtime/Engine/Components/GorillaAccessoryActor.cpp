#include "pch.h"
#include "GorillaAccessoryActor.h"
#include "AngryCoachCharacter.h"
#include "GorillaLightAttackSkill.h"
#include "GorillaHeavyAttackSkill.h"
#include "GorillaSpecialAttackSkill.h"
#include "SkeletalMeshComponent.h"
#include "SphereComponent.h"
#include "BlueprintGraph/AnimationGraph.h"
#include "Source/Runtime/AssetManagement/SkeletalMesh.h"
#include "Source/Runtime/AssetManagement/Texture.h" // UTexture
#include "Source/Runtime/Core/Misc/JsonSerializer.h"
#include "Source/Runtime/Engine/Components/MeshComponent.h" // EMaterialTextureSlot

AGorillaAccessoryActor::AGorillaAccessoryActor()
{
	ObjectName = "GorillaAccessory";
	AccessoryName = "Gorilla";
	Description = "Powerful gorilla attacks";
	AttachSocketName = FName("HeadWeapon");
	bIsGorillaFormActive = false;

	GrantedSkills.clear();

	UGorillaLightAttackSkill* LightSkill = NewObject<UGorillaLightAttackSkill>();
	UGorillaHeavyAttackSkill* HeavySkill = NewObject<UGorillaHeavyAttackSkill>();
	UGorillaSpecialAttackSkill* SpecialSkill = NewObject<UGorillaSpecialAttackSkill>();

	GrantedSkills.Add(ESkillSlot::LightAttack, LightSkill);
	GrantedSkills.Add(ESkillSlot::HeavyAttack, HeavySkill);
	GrantedSkills.Add(ESkillSlot::Specical, SpecialSkill);

	// 양손 AttackShape 생성 (왼손/오른손)
	if (USphereComponent* LeftShape = Cast<USphereComponent>(CreateAttackShape<USphereComponent>(FName("LeftAttackShape"))))
	{
		LeftShape->SphereRadius = 2.f;
		LeftShape->SetGenerateOverlapEvents(false);
		LeftShape->SetBlockComponent(false);
		LeftShape->bOverrideCollisionSetting = true;
		LeftShape->CollisionEnabled = ECollisionState::QueryOnly;
		UE_LOG("[GorillaAccessory] LeftAttackShape created, Radius=%.2f", LeftShape->SphereRadius);
	}
	if (USphereComponent* RightShape = Cast<USphereComponent>(CreateAttackShape<USphereComponent>(FName("RightAttackShape"))))
	{
		RightShape->SphereRadius = 2.f;
		RightShape->SetGenerateOverlapEvents(false);
		RightShape->SetBlockComponent(false);
		RightShape->bOverrideCollisionSetting = true;
		RightShape->CollisionEnabled = ECollisionState::QueryOnly;
		UE_LOG("[GorillaAccessory] RightAttackShape created, Radius=%.2f", RightShape->SphereRadius);
	}
	UE_LOG("[GorillaAccessory] Total AttackShapes: %d", AttackShapes.Num());
}

void AGorillaAccessoryActor::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		GrantedSkills.clear();
		UGorillaLightAttackSkill* LightSkill = NewObject<UGorillaLightAttackSkill>();
		UGorillaHeavyAttackSkill* HeavySkill = NewObject<UGorillaHeavyAttackSkill>();
		UGorillaSpecialAttackSkill* SpecialSkill = NewObject<UGorillaSpecialAttackSkill>();
		GrantedSkills.Add(ESkillSlot::LightAttack, LightSkill);
		GrantedSkills.Add(ESkillSlot::HeavyAttack, HeavySkill);
		GrantedSkills.Add(ESkillSlot::Specical, SpecialSkill);

		// AttackShape 설정 (있으면 업데이트, 없으면 재생성)
		if (AttackShapes.Num() == 0)
		{
			if (USphereComponent* LeftShape = Cast<USphereComponent>(CreateAttackShape<USphereComponent>(FName("LeftAttackShape"))))
			{
				LeftShape->SphereRadius = 2.f;
				LeftShape->SetGenerateOverlapEvents(false);
				LeftShape->SetBlockComponent(false);
				LeftShape->bOverrideCollisionSetting = true;
				LeftShape->CollisionEnabled = ECollisionState::QueryOnly;
			}
			if (USphereComponent* RightShape = Cast<USphereComponent>(CreateAttackShape<USphereComponent>(FName("RightAttackShape"))))
			{
				RightShape->SphereRadius = 2.f;
				RightShape->SetGenerateOverlapEvents(false);
				RightShape->SetBlockComponent(false);
				RightShape->bOverrideCollisionSetting = true;
				RightShape->CollisionEnabled = ECollisionState::QueryOnly;
			}
			UE_LOG("[GorillaAccessory] Serialize: AttackShapes recreated, count=%d", AttackShapes.Num());
		}
		else
		{
			// 기존 Shape 설정 업데이트
			for (UShapeComponent* Shape : AttackShapes)
			{
				if (USphereComponent* Sphere = Cast<USphereComponent>(Shape))
				{
					Sphere->SphereRadius = 2.f;
					Sphere->SetGenerateOverlapEvents(false);
					Sphere->SetBlockComponent(false);
					Sphere->bOverrideCollisionSetting = true;
					Sphere->CollisionEnabled = ECollisionState::QueryOnly;
				}
			}
			UE_LOG("[GorillaAccessory] Serialize: AttackShapes updated, count=%d", AttackShapes.Num());
		}
	}
}

void AGorillaAccessoryActor::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();

	GrantedSkills.clear();
	UGorillaLightAttackSkill* LightSkill = NewObject<UGorillaLightAttackSkill>();
	UGorillaHeavyAttackSkill* HeavySkill = NewObject<UGorillaHeavyAttackSkill>();
	UGorillaSpecialAttackSkill* SpecialSkill = NewObject<UGorillaSpecialAttackSkill>();
	GrantedSkills.Add(ESkillSlot::LightAttack, LightSkill);
	GrantedSkills.Add(ESkillSlot::HeavyAttack, HeavySkill);
	GrantedSkills.Add(ESkillSlot::Specical, SpecialSkill);

	// AttackShape 설정 (있으면 업데이트, 없으면 재생성)
	if (AttackShapes.Num() == 0)
	{
		if (USphereComponent* LeftShape = Cast<USphereComponent>(CreateAttackShape<USphereComponent>(FName("LeftAttackShape"))))
		{
			LeftShape->SphereRadius = 2.f;
			LeftShape->SetGenerateOverlapEvents(false);
			LeftShape->SetBlockComponent(false);
		}
		if (USphereComponent* RightShape = Cast<USphereComponent>(CreateAttackShape<USphereComponent>(FName("RightAttackShape"))))
		{
			RightShape->SphereRadius = 2.f;
			RightShape->SetGenerateOverlapEvents(false);
			RightShape->SetBlockComponent(false);
		}
	}
	else
	{
		for (UShapeComponent* Shape : AttackShapes)
		{
			if (USphereComponent* Sphere = Cast<USphereComponent>(Shape))
			{
				Sphere->SphereRadius = 2.f;
				Sphere->SetGenerateOverlapEvents(false);
				Sphere->SetBlockComponent(false);
			}
		}
	}
}

void AGorillaAccessoryActor::Equip(AAngryCoachCharacter* OwnerCharacter)
{
	// 부모 클래스의 기본 장착 로직 호출
	Super::Equip(OwnerCharacter);

	if (!OwnerCharacter)
		return;

	// AttackShapes를 캐릭터의 손 소켓에 직접 부착
	USkeletalMeshComponent* CharacterMesh = OwnerCharacter->GetMesh();
	if (CharacterMesh)
	{
		ReattachAttackShapesToHands(CharacterMesh);
	}
}

void AGorillaAccessoryActor::ReattachAttackShapesToHands(USkeletalMeshComponent* CharacterMesh)
{
	if (!CharacterMesh || AttackShapes.Num() < 2)
		return;

	// 왼손/오른손 소켓 이름 (스켈레탈 메시에 맞게 수정 필요)
	const FName LeftHandSocket = FName("LeftHandSocket");
	const FName RightHandSocket = FName("RightHandSocket");

	for (UShapeComponent* Shape : AttackShapes)
	{
		if (!Shape) continue;

		FString ShapeName = Shape->ObjectName.ToString();
		if (ShapeName.find("Left") != std::string::npos)
		{
			Shape->SetupAttachment(CharacterMesh, LeftHandSocket);
			if (OwningCharacter)
				Shape->RegisterComponent(OwningCharacter->GetWorld());
			UE_LOG("[GorillaAccessory] LeftAttackShape attached to %s", LeftHandSocket.ToString().c_str());
		}
		else if (ShapeName.find("Right") != std::string::npos)
		{
			Shape->SetupAttachment(CharacterMesh, RightHandSocket);
			if (OwningCharacter)
				Shape->RegisterComponent(OwningCharacter->GetWorld());
			UE_LOG("[GorillaAccessory] RightAttackShape attached to %s", RightHandSocket.ToString().c_str());
		}
	}
}

void AGorillaAccessoryActor::ToggleGorillaForm()
{
	AAngryCoachCharacter* Character = GetOwningCharacter();
	if (!Character)
	{
		UE_LOG("[AGorillaAccessoryActor] OwningCharacter is null!");
		return;
	}

	USkeletalMeshComponent* CharacterMesh = Character->GetMesh();
	if (!CharacterMesh)
	{
		UE_LOG("[AGorillaAccessoryActor] CharacterMesh is null!");
		return;
	}

	if (GorillaSkeletalMeshPath.empty() && OwningCharacter)
	{
		// 고릴라 애셋 경로 설정 및 텍스처 로드
		// 참고: 이 애셋들은 현재 프로젝트에 존재하지 않으므로, 실제 파일 경로로 수정해야 합니다.
		// 고릴라 텍스처 로드 (생성자에서 한 번만 로드)
		GorillaSkeletalMeshPath = "Data/FBX/Player/Animation/Gorilla/Gorilla.fbx";
		GorillaSkinTexture = RESOURCE.Load<UTexture>(
			(OwningCharacter->bIsCGC ?
		("Data/FBX/Player/Animation/Gorilla/Texture/ICGRilla.png") :
		("Data/FBX/Player/Animation/Gorilla/Texture/BSHRilla.png"))
			);
		if (!GorillaSkinTexture) UE_LOG("[AGorillaAccessoryActor] Failed to load GorillaSkinTexture.");
		
		GorillaSkinNormal = RESOURCE.Load<UTexture>(
			(OwningCharacter->bIsCGC ?
		("Data/FBX/Player/Animation/Gorilla/Texture/ICGRillaNOrmal.png") :
		("Data/FBX/Player/Animation/Gorilla/Texture/BSHRillaNOrmal.png"))
			);
		if (!GorillaSkinNormal) UE_LOG("[AGorillaAccessoryActor] Failed to load GorillaSkinNormal.");
		if (!GorillaAnimGraph)
		{
			const FString GorillaAnimGraphPath = "Data/Graphs/Gorilla.graph";
			JSON GraphJson;
			if (FJsonSerializer::LoadJsonFromFile(GraphJson, UTF8ToWide(GorillaAnimGraphPath)))
			{
				GorillaAnimGraph = NewObject<UAnimationGraph>();
				GorillaAnimGraph->Serialize(true, GraphJson);
				UE_LOG("[AGorillaAccessoryActor] Loaded Gorilla Anim Graph from: %s", GorillaAnimGraphPath.c_str());
			}
			else
			{
				UE_LOG("[AGorillaAccessoryActor] FAILED to load Gorilla Anim Graph from: %s", GorillaAnimGraphPath.c_str());
			}
		}
	}
	// 2. 캐릭터의 기본 애셋이 저장되지 않았다면 저장 (텍스처 포함)
	if (DefaultSkeletalMeshPath.empty() && CharacterMesh->GetSkeletalMesh())
	{
		DefaultSkeletalMeshPath = CharacterMesh->GetSkeletalMesh()->GetPathFileName();
		DefaultAnimGraph = CharacterMesh->GetAnimGraph();
		DefaultPhysicsAsset = CharacterMesh->GetPhysicsAsset();
		
		DefaultSkinTexture = RESOURCE.Load<UTexture>(
			(OwningCharacter->bIsCGC ?
		("Data/FBX/Player/Textures/ICG.png") :
		("Data/FBX/Player/Textures/BSH.png"))
			);
		if (!DefaultSkinTexture) UE_LOG("[AGorillaAccessoryActor] Failed to load DefaultSkinTexture.");
		
		DefaultSkinNormal = RESOURCE.Load<UTexture>(
			(OwningCharacter->bIsCGC ?
		("Data/FBX/Player/Textures/ICG_NORMAL.png") :
		("Data/FBX/Player/Textures/BSH_NORMAL.png"))
			);
		if (!DefaultSkinNormal) UE_LOG("[AGorillaAccessoryActor] Failed to load DefaultSkinNormal.");
		
		UE_LOG("[AGorillaAccessoryActor] Stored default character assets.");
	}
	// --- --- --- --- --- --- --- --- --- --- --- `	
	// --- 토글 로직 ---
	bIsGorillaFormActive = !bIsGorillaFormActive; // 상태 먼저 전환

	AAngryCoachCharacter* AngryCoachCharacter = Cast<AAngryCoachCharacter>(Character); // 캐스팅
	if (!AngryCoachCharacter)
	{
		UE_LOG("[AGorillaAccessoryActor] Character is not AAngryCoachCharacter!");
		return;
	}
	if (bIsGorillaFormActive) // 고릴라 형태로 변경
	{
		UE_LOG("[AGorillaAccessoryActor] Switching to Gorilla Form.");
		CharacterMesh->SetSkeletalMesh(GorillaSkeletalMeshPath);
		CharacterMesh->SetAnimGraph(GorillaAnimGraph);
		CharacterMesh->SetPhysicsAsset(GorillaPhysicsAsset);

		// Particle Component 부착
		//UParticleSystemComponent* Aura = NewObject<UParticleSystemComponent>(GorillaSkeletalMeshPath)


		if (GorillaSkinTexture) CharacterMesh->SetMaterialTextureByUser(1, EMaterialTextureSlot::Diffuse, GorillaSkinTexture);
		if (GorillaSkinNormal) CharacterMesh->SetMaterialTextureByUser(1, EMaterialTextureSlot::Normal, GorillaSkinNormal);
		// HitReactionMontage 재생 여부 플래그를 false로 설정
		AngryCoachCharacter->bCanPlayHitReactionMontage = false;
		UE_LOG("[AGorillaAccessoryActor] Character HitReactionMontage disabled for Gorilla Form.");
	}
	else // 원래 형태로 복원
	{
		UE_LOG("[AGorillaAccessoryActor] Switching to Original Form.");
		CharacterMesh->SetSkeletalMesh(DefaultSkeletalMeshPath);
		CharacterMesh->SetAnimGraph(DefaultAnimGraph);
		CharacterMesh->SetPhysicsAsset(DefaultPhysicsAsset);
		if (DefaultSkinTexture) CharacterMesh->SetMaterialTextureByUser(1, EMaterialTextureSlot::Diffuse, DefaultSkinTexture);
		if (DefaultSkinNormal) CharacterMesh->SetMaterialTextureByUser(1, EMaterialTextureSlot::Normal, DefaultSkinNormal);
		// HitReactionMontage 재생 여부 플래그를 true로 설정
		AngryCoachCharacter->bCanPlayHitReactionMontage = true;
		UE_LOG("[AGorillaAccessoryActor] Character HitReactionMontage enabled for Original Form.");
	}

	// 스켈레탈 메시 변경 후 AttackShape를 새 메시의 소켓에 다시 부착
	ReattachAttackShapesToHands(CharacterMesh);
	// --- --- --- ---
}