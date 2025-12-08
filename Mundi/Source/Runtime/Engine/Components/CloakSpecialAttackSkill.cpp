#include "pch.h"
#include "CloakSpecialAttackSkill.h"
#include "CloakAccessoryActor.h"
#include "AngryCoachCharacter.h"
#include "Source/Runtime/Engine/Animation/AnimMontage.h"
#include "CharacterMovementComponent.h"


UCloakSpecialAttackSkill::UCloakSpecialAttackSkill()
{
	ObjectName = "CloakSpecialAttack";

	Montage = RESOURCE.Load<UAnimMontage>("Data/Montages/PlayerSpecial.montage.json");
	if (!Montage)
	{
		UE_LOG("[CloakSpecialSkill] Failed to load Cloak montage!");
	}
}

void UCloakSpecialAttackSkill::Activate(AActor* Caster)
{
	// Super::Activate(Caster);

	float DamageMultiplier = 1.0f;
	float Range = 150.0f;
	if (ACloakAccessoryActor* CloakAccessory = Cast<ACloakAccessoryActor>(SourceAccessory))
	{
		//DamageMultiplier = CloakAccessory->KnifeDamageMultiplier;
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
