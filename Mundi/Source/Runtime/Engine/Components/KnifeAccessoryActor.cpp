#include "pch.h"
#include "KnifeAccessoryActor.h"
#include "KnifeLightAttackSkill.h"
#include "KnifeHeavyAttackSkill.h"
#include "KnifeSpecialAttackSkill.h"
#include "StaticMeshComponent.h"
#include "Source/Runtime/AssetManagement/ResourceManager.h"

AKnifeAccessoryActor::AKnifeAccessoryActor()
{
	ObjectName = "KnifeAccessory";
	AccessoryName = "Knife";
	Description = "Fast knife attacks";
	AttachSocketName = FName("LEFT_KNIFE");

	// 나이프 메시 로드 및 설정
	if (AccessoryMesh)
	{
		AccessoryMesh->SetStaticMesh("Data/Model/FlowerKnife.obj");
	}

	GrantedSkills.clear();

	UKnifeLightAttackSkill* LightSkill = NewObject<UKnifeLightAttackSkill>();
	UKnifeHeavyAttackSkill* HeavySkill = NewObject<UKnifeHeavyAttackSkill>();
	UKnifeSpecialAttackSkill* SpecialSkill = NewObject<UKnifeSpecialAttackSkill>();

	GrantedSkills.Add(ESkillSlot::LightAttack, LightSkill);
	GrantedSkills.Add(ESkillSlot::HeavyAttack, HeavySkill);
	GrantedSkills.Add(ESkillSlot::Specical, SpecialSkill);
}

void AKnifeAccessoryActor::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		GrantedSkills.clear();
		UKnifeLightAttackSkill* LightSkill = NewObject<UKnifeLightAttackSkill>();
		UKnifeHeavyAttackSkill* HeavySkill = NewObject<UKnifeHeavyAttackSkill>();
		UKnifeSpecialAttackSkill* SpecialSkill = NewObject<UKnifeSpecialAttackSkill>();

		GrantedSkills.Add(ESkillSlot::LightAttack, LightSkill);
		GrantedSkills.Add(ESkillSlot::HeavyAttack, HeavySkill);
		GrantedSkills.Add(ESkillSlot::Specical, SpecialSkill);
	}
}

void AKnifeAccessoryActor::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();

	GrantedSkills.clear();
	UKnifeLightAttackSkill* LightSkill = NewObject<UKnifeLightAttackSkill>();
	UKnifeHeavyAttackSkill* HeavySkill = NewObject<UKnifeHeavyAttackSkill>();
	UKnifeSpecialAttackSkill* SpecialSkill = NewObject<UKnifeSpecialAttackSkill>();

	GrantedSkills.Add(ESkillSlot::LightAttack, LightSkill);
	GrantedSkills.Add(ESkillSlot::HeavyAttack, HeavySkill);
	GrantedSkills.Add(ESkillSlot::Specical, SpecialSkill);
}
