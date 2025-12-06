#include "pch.h"
#include "GorillaHeavyAttackSkill.h"

UGorillaHeavyAttackSkill::UGorillaHeavyAttackSkill()
{
	ObjectName = "GorillaHeavyAttack";
}

void UGorillaHeavyAttackSkill::Activate(AActor* Caster)
{
	Super::Activate(Caster);
	UE_LOG("[Skill] Gorilla Heavy Attack!");
}
