#include "pch.h"
#include "PunchSpecialAttackSkill.h"
#include "KnifeAccessoryActor.h"
#include "AngryCoachCharacter.h"
#include "Source/Runtime/Engine/Animation/AnimMontage.h"
#include "CharacterMovementComponent.h"
#include "FAudioDevice.h"


UPunchSpecialAttackSkill::UPunchSpecialAttackSkill()
{
	ObjectName = "KnifeSpecialAttack";

	Montage = RESOURCE.Load<UAnimMontage>("Data/Montages/CloakSpecial.montage.json");
	if (!Montage)
	{
		UE_LOG("[KnifeSpecialAttackSkill] Failed to load knife2 montage!");
	}
}

void UPunchSpecialAttackSkill::Activate(AActor* Caster)
{
	//Super::Activate(Caster);

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
		if (!Character->GetCharacterMovement()->IsFalling() && Montage)
		{
			
			// 사운드 재생
			if (Character->GetSkillSound())
			{
				FAudioDevice::PlaySoundAtLocationOneShot(Character->GetSkillSound(), Character->GetActorLocation());
			}
			Character->PlayMontage(Montage);
		}
	}
}
