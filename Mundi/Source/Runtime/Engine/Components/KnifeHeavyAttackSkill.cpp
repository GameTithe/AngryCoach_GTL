#include "pch.h"
#include "KnifeHeavyAttackSkill.h"
#include "KnifeAccessoryActor.h"
#include "AngryCoachCharacter.h"
#include "Source/Runtime/Engine/Animation/AnimMontage.h"

UKnifeHeavyAttackSkill::UKnifeHeavyAttackSkill()
{
	ObjectName = "KnifeHeavyAttack";
	Montage = RESOURCE.Load<UAnimMontage>("Data/Montages/Knife2.montage.json");
	if (!Montage)
	{
		UE_LOG("[KnifeHeavyAttackSkill] Failed to load knife2 montage!");
	} 
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

	// 애니메이션 재생
	if (AAngryCoachCharacter* Character = Cast<AAngryCoachCharacter>(Caster))
	{
		if (Montage)
		{
			Character->PlayMontage(Montage);
		} 
	}
}
