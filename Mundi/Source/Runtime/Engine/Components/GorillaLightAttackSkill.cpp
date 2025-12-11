#include "pch.h"
#include "GorillaLightAttackSkill.h"

#include "AngryCoachCharacter.h"
#include "GorillaAccessoryActor.h"
#include "ParticleSystemComponent.h"

UGorillaLightAttackSkill::UGorillaLightAttackSkill()
{
	ObjectName = "GorillaLightAttack";
	Montage = RESOURCE.Load<UAnimMontage>("Data/Montages/GorillaLight.montage.json");
	HumanMontage = RESOURCE.Load<UAnimMontage>("Data/Montages/PlayerWeak.montage.json");
}

void UGorillaLightAttackSkill::Activate(AActor* Caster)
{
	// - 애니메이션 재생 (더 느리고 강한 모션)
	if (!SourceAccessory || !SourceAccessory->GetOwningCharacter() || !Montage)
	{
		return;
	}

	AGorillaAccessoryActor* GA = Cast<AGorillaAccessoryActor>(SourceAccessory);
	if (GA)
	{
		if (GA->GetIsGorillaForm())
		{
			AAngryCoachCharacter* Character = SourceAccessory->GetOwningCharacter();
			Character->PlayMontage(Montage);

			// 베이스 이펙트 파티클 재생
			if (SourceAccessory->BaseEffectParticle)
			{
				SourceAccessory->BaseEffectParticle->ResetAndActivate();
			}
		}
		else
		{
			AAngryCoachCharacter* Character = SourceAccessory->GetOwningCharacter();
			Character->PlayMontage(HumanMontage);
		}
	}
}
