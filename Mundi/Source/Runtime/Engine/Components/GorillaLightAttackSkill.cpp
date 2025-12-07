#include "pch.h"
#include "GorillaLightAttackSkill.h"

UGorillaLightAttackSkill::UGorillaLightAttackSkill()
{
	ObjectName = "GorillaLightAttack";
	Montage = RESOURCE.Load<UAnimMontage>("Data/Montages/GorillaLight.NewMontage.montage.json");
}

void UGorillaLightAttackSkill::Activate(AActor* Caster)
{
	Super::Activate(Caster);
	UE_LOG("[Skill] Gorilla Light Attack!");
}
