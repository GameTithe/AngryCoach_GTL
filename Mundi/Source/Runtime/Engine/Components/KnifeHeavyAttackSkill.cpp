#include "pch.h"
#include "KnifeHeavyAttackSkill.h"
#include "KnifeAccessoryActor.h"

UKnifeHeavyAttackSkill::UKnifeHeavyAttackSkill()
{
	ObjectName = "KnifeHeavyAttack";
}

void UKnifeHeavyAttackSkill::Activate(AActor* Caster)
{
	Super::Activate(Caster);

	float DamageMultiplier = 1.0f;
	float Range = 150.0f;
	if (AKnifeAccessoryActor* KnifeAccessory = Cast<AKnifeAccessoryActor>(SourceAccessory))
	{
		DamageMultiplier = KnifeAccessory->KnifeDamageMultiplier;
		Range = KnifeAccessory->AttackRange;
	}

	UE_LOG("[Skill] Knife Heavy Attack! (Damage x%.2f, Range %.1f)", DamageMultiplier, Range);
}
