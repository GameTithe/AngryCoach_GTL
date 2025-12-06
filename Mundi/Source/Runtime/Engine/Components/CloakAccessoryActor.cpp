#include "pch.h"
#include "CloakAccessoryActor.h"
#include "CloakLightAttackSkill.h"
#include "CloakHeavyAttackSkill.h"

ACloakAccessoryActor::ACloakAccessoryActor()
{
	ObjectName = "CloakAccessory";
	AccessoryName = "Cloak";
	Description = "Cloak skills";
	AttachSocketName = FName("spine_03");

	GrantedSkills.clear();

	UCloakLightAttackSkill* LightSkill = NewObject<UCloakLightAttackSkill>();
	UCloakHeavyAttackSkill* HeavySkill = NewObject<UCloakHeavyAttackSkill>();

	GrantedSkills.Add(ESkillSlot::LightAttack, LightSkill);
	GrantedSkills.Add(ESkillSlot::HeavyAttack, HeavySkill);
}

void ACloakAccessoryActor::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		GrantedSkills.clear();
		UCloakLightAttackSkill* LightSkill = NewObject<UCloakLightAttackSkill>();
		UCloakHeavyAttackSkill* HeavySkill = NewObject<UCloakHeavyAttackSkill>();
		GrantedSkills.Add(ESkillSlot::LightAttack, LightSkill);
		GrantedSkills.Add(ESkillSlot::HeavyAttack, HeavySkill);
	}
}

void ACloakAccessoryActor::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();

	GrantedSkills.clear();
	UCloakLightAttackSkill* LightSkill = NewObject<UCloakLightAttackSkill>();
	UCloakHeavyAttackSkill* HeavySkill = NewObject<UCloakHeavyAttackSkill>();
	GrantedSkills.Add(ESkillSlot::LightAttack, LightSkill);
	GrantedSkills.Add(ESkillSlot::HeavyAttack, HeavySkill);
}
