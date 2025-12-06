#include "pch.h"
#include "CloakLightAttackSkill.h"

UCloakLightAttackSkill::UCloakLightAttackSkill()
{
	ObjectName = "CloakLightAttack";
}

void UCloakLightAttackSkill::Activate(AActor* Caster)
{
	Super::Activate(Caster);
	UE_LOG("[Skill] Cloak Light Attack!");
}
