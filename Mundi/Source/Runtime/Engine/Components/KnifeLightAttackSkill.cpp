#include "pch.h"
#include "KnifeLightAttackSkill.h"
#include "KnifeAccessoryActor.h"
#include "AngryCoachCharacter.h"
#include "Source/Runtime/Engine/Animation/AnimMontage.h"
#include "CharacterMovementComponent.h"

UKnifeLightAttackSkill::UKnifeLightAttackSkill()
{
	ObjectName = "KnifeLightAttack";
	Montage = RESOURCE.Load<UAnimMontage>("Data/Montages/Knife1.montage.json");
	if (!Montage)
	{
		UE_LOG("[KnifeLightAttackSkill] Failed to load knife1 montage!");
	}
}

void UKnifeLightAttackSkill::Activate(AActor* Caster)
{
	Super::Activate(Caster);

	float DamageMultiplier = 1.0f;
	if (AKnifeAccessoryActor* KnifeAccessory = Cast<AKnifeAccessoryActor>(SourceAccessory))
	{
		DamageMultiplier = KnifeAccessory->KnifeDamageMultiplier;
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
