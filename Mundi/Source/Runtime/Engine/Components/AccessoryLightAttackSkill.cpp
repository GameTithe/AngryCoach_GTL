#include "pch.h"
#include "AccessoryLightAttackSkill.h"

UAccessoryLightAttackSkill::UAccessoryLightAttackSkill()
{
	ObjectName = "AccessoryLightAttack";
}

void UAccessoryLightAttackSkill::Activate(AActor* Caster)
{
	Super::Activate(Caster);

	UE_LOG("[Skill] Accessory Light Attack!");
}
