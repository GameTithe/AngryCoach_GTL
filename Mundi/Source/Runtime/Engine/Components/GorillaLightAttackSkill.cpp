#include "pch.h"
#include "GorillaLightAttackSkill.h"

UGorillaLightAttackSkill::UGorillaLightAttackSkill()
{
	ObjectName = "GorillaLightAttack";
}

void UGorillaLightAttackSkill::Activate(AActor* Caster)
{
	Super::Activate(Caster);
	UE_LOG("[Skill] Gorilla Light Attack!");
}
