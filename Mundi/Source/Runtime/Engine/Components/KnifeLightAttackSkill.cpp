#include "pch.h"
#include "KnifeLightAttackSkill.h"
#include "KnifeAccessoryActor.h"

UKnifeLightAttackSkill::UKnifeLightAttackSkill()
{
	ObjectName = "KnifeLightAttack";
}

void UKnifeLightAttackSkill::Activate(AActor* Caster)
{
	Super::Activate(Caster);

	float DamageMultiplier = 1.0f;
	if (AKnifeAccessoryActor* KnifeAccessory = Cast<AKnifeAccessoryActor>(SourceAccessory))
	{
		DamageMultiplier = KnifeAccessory->KnifeDamageMultiplier;
	}

	UE_LOG("[Skill] Knife Light Attack! (Damage x%.2f)", DamageMultiplier);
}
