#include "pch.h"
#include "CloakLightAttackSkill.h" 
#include "CloakAccessoryActor.h"
#include "AngryCoachCharacter.h"
#include "Source/Runtime/Engine/Animation/AnimMontage.h"
#include "CharacterMovementComponent.h"
UCloakLightAttackSkill::UCloakLightAttackSkill()
{
	ObjectName = "CloakLightAttack";

	Montage = RESOURCE.Load<UAnimMontage>("Data/Montages/PlayerWeak.montage.json");
	if (!Montage)
	{
		UE_LOG("[CloakLightAttackSkill] Failed to load knife2 montage!");
	}
}

void UCloakLightAttackSkill::Activate(AActor* Caster)
{
	//Super::Activate(Caster); float DamageMultiplier = 1.0f;
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
