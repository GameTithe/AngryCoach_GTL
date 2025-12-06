#include "pch.h"
#include "GorillaAccessoryActor.h"
#include "GorillaLightAttackSkill.h"
#include "GorillaHeavyAttackSkill.h"

AGorillaAccessoryActor::AGorillaAccessoryActor()
{
	ObjectName = "GorillaAccessory";
	AccessoryName = "Gorilla";
	Description = "Powerful gorilla attacks";
	AttachSocketName = FName("hand_r");

	GrantedSkills.clear();

	UGorillaLightAttackSkill* LightSkill = NewObject<UGorillaLightAttackSkill>();
	UGorillaHeavyAttackSkill* HeavySkill = NewObject<UGorillaHeavyAttackSkill>();

	GrantedSkills.Add(ESkillSlot::LightAttack, LightSkill);
	GrantedSkills.Add(ESkillSlot::HeavyAttack, HeavySkill);
}

void AGorillaAccessoryActor::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		GrantedSkills.clear();
		UGorillaLightAttackSkill* LightSkill = NewObject<UGorillaLightAttackSkill>();
		UGorillaHeavyAttackSkill* HeavySkill = NewObject<UGorillaHeavyAttackSkill>();
		GrantedSkills.Add(ESkillSlot::LightAttack, LightSkill);
		GrantedSkills.Add(ESkillSlot::HeavyAttack, HeavySkill);
	}
}

void AGorillaAccessoryActor::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();

	GrantedSkills.clear();
	UGorillaLightAttackSkill* LightSkill = NewObject<UGorillaLightAttackSkill>();
	UGorillaHeavyAttackSkill* HeavySkill = NewObject<UGorillaHeavyAttackSkill>();
	GrantedSkills.Add(ESkillSlot::LightAttack, LightSkill);
	GrantedSkills.Add(ESkillSlot::HeavyAttack, HeavySkill);
}
