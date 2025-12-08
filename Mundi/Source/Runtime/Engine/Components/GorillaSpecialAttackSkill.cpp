#include "pch.h"
#include "GorillaSpecialAttackSkill.h"

#include "AngryCoachCharacter.h"
#include "GorillaAccessoryActor.h" // AGorillaAccessoryActor

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
		if (!GA->GetIsGorillaForm())
		{
			AAngryCoachCharacter* Character = SourceAccessory->GetOwningCharacter();
			Character->PlayMontage(Montage);
		}
		GA->ToggleGorillaForm();
	}
}
