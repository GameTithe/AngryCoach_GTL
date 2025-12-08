#include "pch.h"
#include "CloakHeavyAttackSkill.h" 
#include "CloakAccessoryActor.h"
#include "AngryCoachCharacter.h"
#include "Source/Runtime/Engine/Animation/AnimMontage.h"
#include "CharacterMovementComponent.h"

UCloakHeavyAttackSkill::UCloakHeavyAttackSkill()
{
	ObjectName = "CloakHeavyAttack";

	Montage = RESOURCE.Load<UAnimMontage>("Data/Montages/PlayerHeavy.montage.json");
	if (!Montage)
	{
		UE_LOG("[CloakHeavyAttackSkill] Failed to load knife2 montage!");
	}
}

void UCloakHeavyAttackSkill::Activate(AActor* Caster)
{
	// Super::Activate(Caster);

	float DamageMultiplier = 1.0f;
	float Range = 150.0f;
	if (ACloakAccessoryActor* CloakAccessory = Cast<ACloakAccessoryActor>(SourceAccessory))
	{
		//DamageMultiplier = KnifeAccessory->KnifeDamageMultiplier;
		//Range = KnifeAccessory->AttackRange;
	}

	// 애니메이션 재생
	if (AAngryCoachCharacter* Character = Cast<AAngryCoachCharacter>(Caster))
	{
		if (!Character->GetCharacterMovement()->IsFalling() && Montage)
		{
			Character->PlayMontage(Montage);
		}
	}
}
