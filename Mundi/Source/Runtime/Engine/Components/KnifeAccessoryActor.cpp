#include "pch.h"
#include "KnifeAccessoryActor.h"
#include "KnifeLightAttackSkill.h"
#include "KnifeHeavyAttackSkill.h"

AKnifeAccessoryActor::AKnifeAccessoryActor()
{
	ObjectName = "KnifeAccessory";
	AccessoryName = "Knife";
	Description = "Fast knife attacks";
	AttachSocketName = FName("hand_r");

	GrantedSkills.clear();

	UKnifeLightAttackSkill* LightSkill = NewObject<UKnifeLightAttackSkill>();
	UKnifeHeavyAttackSkill* HeavySkill = NewObject<UKnifeHeavyAttackSkill>();

	GrantedSkills.Add(ESkillSlot::LightAttack, LightSkill);
	GrantedSkills.Add(ESkillSlot::HeavyAttack, HeavySkill);
}

void AKnifeAccessoryActor::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		GrantedSkills.clear();
		UKnifeLightAttackSkill* LightSkill = NewObject<UKnifeLightAttackSkill>();
		UKnifeHeavyAttackSkill* HeavySkill = NewObject<UKnifeHeavyAttackSkill>();
		GrantedSkills.Add(ESkillSlot::LightAttack, LightSkill);
		GrantedSkills.Add(ESkillSlot::HeavyAttack, HeavySkill);
	}
}

void AKnifeAccessoryActor::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();

	GrantedSkills.clear();
	UKnifeLightAttackSkill* LightSkill = NewObject<UKnifeLightAttackSkill>();
	UKnifeHeavyAttackSkill* HeavySkill = NewObject<UKnifeHeavyAttackSkill>();
	GrantedSkills.Add(ESkillSlot::LightAttack, LightSkill);
	GrantedSkills.Add(ESkillSlot::HeavyAttack, HeavySkill);
}
