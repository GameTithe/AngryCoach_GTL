#include "pch.h"
#include "KnifeSpecialAttackSkill.h"
#include "KnifeAccessoryActor.h"
#include "AngryCoachCharacter.h"
#include "Source/Runtime/Engine/Animation/AnimMontage.h"
#include "CharacterMovementComponent.h"
#include "FAudioDevice.h"


UKnifeSpecialAttackSkill::UKnifeSpecialAttackSkill()
{
	ObjectName = "KnifeSpecialAttack";

	Montage = RESOURCE.Load<UAnimMontage>("Data/Montages/KnifeSpecial.montage.json");
	if (!Montage)
	{
		UE_LOG("[KnifeSpecialAttackSkill] Failed to load knife2 montage!");
	}
}

void UKnifeSpecialAttackSkill::Activate(AActor* Caster)
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

			// 스킬 이펙트 파티클 재생
			if (SourceAccessory)
			{
				SourceAccessory->PlaySkillEffectParticle();
			}

			Character->PlayMontage(Montage);
		}
	}
}
