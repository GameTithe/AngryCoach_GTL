#include "pch.h"
#include "DefaultLightAttackSkill.h"

UDefaultLightAttackSkill::UDefaultLightAttackSkill()
{
	ObjectName = "DefaultLightAttack";
}

void UDefaultLightAttackSkill::Activate(AActor* Caster)
{
	Super::Activate(Caster);

	UE_LOG("[Skill] 맨몸 Light Attack!");
}
