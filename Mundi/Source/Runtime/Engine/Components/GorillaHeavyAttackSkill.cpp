#include "pch.h"
#include "GorillaHeavyAttackSkill.h"

UGorillaHeavyAttackSkill::UGorillaHeavyAttackSkill()
{
	ObjectName = "GorillaHeavyAttack";
	Montage = RESOURCE.Load<UAnimMontage>("Data/Montages/GorillaHeavy.NewMontage.montage.json");
}

void UGorillaHeavyAttackSkill::Activate(AActor* Caster)
{
	Super::Activate(Caster);
	UE_LOG("[Skill] Gorilla Heavy Attack!");
}
