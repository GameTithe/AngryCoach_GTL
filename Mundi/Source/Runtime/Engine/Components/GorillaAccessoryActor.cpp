#include "pch.h"
#include "GorillaAccessoryActor.h"
#include "AngryCoachCharacter.h"
#include "GorillaLightAttackSkill.h"
#include "GorillaHeavyAttackSkill.h"
#include "GorillaSpecialAttackSkill.h"
#include "SkeletalMeshComponent.h"
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
	
	// if (!GorillaPhysicsAsset)
	// {
	// 	const FString GorillaPhysicsAssetPath = "Data/Physics/Gorilla_Physics.phys.json";
	// 	GorillaPhysicsAsset = RESOURCE.Load<UPhysicsAsset>(GorillaPhysicsAssetPath);
	// 	if (GorillaPhysicsAsset)
	// 	{
	// 		UE_LOG("[AGorillaAccessoryActor] Loaded Gorilla Physics Asset from: %s", GorillaPhysicsAssetPath.c_str());
	// 	}
	// 	else
	// 	{
	// 		UE_LOG("[AGorillaAccessoryActor] FAILED to load Gorilla Physics Asset from: %s", GorillaPhysicsAssetPath.c_str());
	// 	}
	// }
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
		GorillaSkeletalMeshPath = "Data/FBX/Player/Animation/Gorilla/Gorilla.fbx";
	
		// 고릴라 텍스처 로드 (생성자에서 한 번만 로드)
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
	// --- --- --- --- --- --- --- --- --- --- ---

	// --- 토글 로직 ---
	bIsGorillaFormActive = !bIsGorillaFormActive; // 상태 먼저 전환

	if (bIsGorillaFormActive) // 고릴라 형태로 변경
	{
		UE_LOG("[AGorillaAccessoryActor] Switching to Gorilla Form.");
		CharacterMesh->SetSkeletalMesh(GorillaSkeletalMeshPath);
		CharacterMesh->SetAnimGraph(GorillaAnimGraph);
		CharacterMesh->SetPhysicsAsset(GorillaPhysicsAsset);
		if (GorillaSkinTexture) CharacterMesh->SetMaterialTextureByUser(1, EMaterialTextureSlot::Diffuse, GorillaSkinTexture);
		if (GorillaSkinNormal) CharacterMesh->SetMaterialTextureByUser(1, EMaterialTextureSlot::Normal, GorillaSkinNormal);
	}
	else // 원래 형태로 복원
	{
		UE_LOG("[AGorillaAccessoryActor] Switching to Original Form.");
		CharacterMesh->SetSkeletalMesh(DefaultSkeletalMeshPath);
		CharacterMesh->SetAnimGraph(DefaultAnimGraph);
		CharacterMesh->SetPhysicsAsset(DefaultPhysicsAsset);
		if (DefaultSkinTexture) CharacterMesh->SetMaterialTextureByUser(1, EMaterialTextureSlot::Diffuse, DefaultSkinTexture);
		if (DefaultSkinNormal) CharacterMesh->SetMaterialTextureByUser(1, EMaterialTextureSlot::Normal, DefaultSkinNormal);
	}
	// --- --- --- ---
}
