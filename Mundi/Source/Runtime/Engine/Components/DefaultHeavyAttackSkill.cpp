#include "pch.h"
#include "DefaultHeavyAttackSkill.h"

UDefaultHeavyAttackSkill::UDefaultHeavyAttackSkill()
{
	ObjectName = "DefaultHeavyAttack";
}

void UDefaultHeavyAttackSkill::Activate(AActor* Caster)
{
	Super::Activate(Caster);

	UE_LOG("[Skill] 맨몸 Heavy Attack!");
}
