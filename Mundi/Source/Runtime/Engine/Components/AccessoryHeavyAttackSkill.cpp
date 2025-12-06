#include "pch.h"
#include "AccessoryHeavyAttackSkill.h"

UAccessoryHeavyAttackSkill::UAccessoryHeavyAttackSkill()
{
	ObjectName = "AccessoryHeavyAttack";
}

void UAccessoryHeavyAttackSkill::Activate(AActor* Caster)
{
	Super::Activate(Caster);

	UE_LOG("[Skill] Accessory Heavy Attack!");
}
