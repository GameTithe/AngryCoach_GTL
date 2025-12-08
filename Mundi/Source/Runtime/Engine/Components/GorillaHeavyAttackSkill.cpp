#include "pch.h"
#include "GorillaHeavyAttackSkill.h"

#include "AccessoryActor.h"
#include "AngryCoachCharacter.h"
#include "GorillaAccessoryActor.h"

UGorillaHeavyAttackSkill::UGorillaHeavyAttackSkill()
{
	ObjectName = "GorillaHeavyAttack";
	Montage = RESOURCE.Load<UAnimMontage>("Data/Montages/GorillaHeavy.montage.json");
	HumanMontage = RESOURCE.Load<UAnimMontage>("Data/Montages/PlayerHeavy.montage.json");
}

void UGorillaHeavyAttackSkill::Activate(AActor* Caster)
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
		}
		else
		{
			AAngryCoachCharacter* Character = SourceAccessory->GetOwningCharacter();
			Character->PlayMontage(HumanMontage);
		}
	}
}