#include "pch.h"
#include "PunchAccessoryActor.h"
#include "PunchLightAttackSkill.h"
#include "PunchHeavyAttackSkill.h"

APunchAccessoryActor::APunchAccessoryActor()
{
	ObjectName = "PunchAccessory";
	AccessoryName = "Punch";
	Description = "Basic punch attacks";
	AttachSocketName = FName("hand_r");

	// 기본 악세서리 스킬 제거 후 펀치 스킬로 교체
	GrantedSkills.clear();

	UPunchLightAttackSkill* LightSkill = NewObject<UPunchLightAttackSkill>();
	UPunchHeavyAttackSkill* HeavySkill = NewObject<UPunchHeavyAttackSkill>();

	GrantedSkills.Add(ESkillSlot::LightAttack, LightSkill);
	GrantedSkills.Add(ESkillSlot::HeavyAttack, HeavySkill);
}

void APunchAccessoryActor::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		// 스킬 재생성 (펀치 스킬로)
		GrantedSkills.clear();
		UPunchLightAttackSkill* LightSkill = NewObject<UPunchLightAttackSkill>();
		UPunchHeavyAttackSkill* HeavySkill = NewObject<UPunchHeavyAttackSkill>();
		GrantedSkills.Add(ESkillSlot::LightAttack, LightSkill);
		GrantedSkills.Add(ESkillSlot::HeavyAttack, HeavySkill);
	}
}

void APunchAccessoryActor::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();

	// 스킬 재생성 (펀치 스킬로)
	GrantedSkills.clear();
	UPunchLightAttackSkill* LightSkill = NewObject<UPunchLightAttackSkill>();
	UPunchHeavyAttackSkill* HeavySkill = NewObject<UPunchHeavyAttackSkill>();
	GrantedSkills.Add(ESkillSlot::LightAttack, LightSkill);
	GrantedSkills.Add(ESkillSlot::HeavyAttack, HeavySkill);
}
