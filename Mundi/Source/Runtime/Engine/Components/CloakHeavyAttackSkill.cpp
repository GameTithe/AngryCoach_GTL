#include "pch.h"
#include "CloakHeavyAttackSkill.h"

UCloakHeavyAttackSkill::UCloakHeavyAttackSkill()
{
	ObjectName = "CloakHeavyAttack";
}

void UCloakHeavyAttackSkill::Activate(AActor* Caster)
{
	Super::Activate(Caster);
	UE_LOG("[Skill] Cloak Heavy Attack!");
}
