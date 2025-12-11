#include "pch.h"
#include "GorillaSpecialAttackSkill.h"

#include "AngryCoachCharacter.h"
#include "GorillaAccessoryActor.h" // AGorillaAccessoryActor
#include "FAudioDevice.h"

UGorillaSpecialAttackSkill::UGorillaSpecialAttackSkill()
{
	ObjectName = "GorillaSpecialAttack";
	Montage = RESOURCE.Load<UAnimMontage>("Data/Montages/GorillaSpecial.montage.json");
}

void UGorillaSpecialAttackSkill::Activate(AActor* Caster)
{
	// - 애니메이션 재생 (더 느리고 강한 모션)
	if (!SourceAccessory || !SourceAccessory->GetOwningCharacter() || !Montage)
	{
		return;
	}

	AGorillaAccessoryActor* GA = Cast<AGorillaAccessoryActor>(SourceAccessory);
	if (GA)
	{
		AAngryCoachCharacter* Character = SourceAccessory->GetOwningCharacter();

		// 사운드 재생
		if (Character && Character->GetSkillSound())
		{
			FAudioDevice::PlaySoundAtLocationOneShot(Character->GetSkillSound(), Character->GetActorLocation());
		}

		// 스킬 이펙트 파티클 재생
		SourceAccessory->PlaySkillEffectParticle();

		if (!GA->GetIsGorillaForm())
		{
			// 변신: 몽타주 재생 (파티클 포함)
			if (Character)
			{
				Character->PlayMontage(Montage);
			}
		}
		else
		{
			// 변신 해제: 파티클 없이 즉시 해제
			GA->ToggleGorillaForm();
			UE_LOG("[GorillaSpecialAttack] Toggle to human form (no montage)");
		}
	}
}
