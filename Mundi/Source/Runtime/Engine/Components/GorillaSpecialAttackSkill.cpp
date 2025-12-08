#include "pch.h"
#include "GorillaSpecialAttackSkill.h"
#include "GorillaAccessoryActor.h" // AGorillaAccessoryActor

UGorillaSpecialAttackSkill::UGorillaSpecialAttackSkill()
{
	ObjectName = "GorillaSpecialAttack";
}

void UGorillaSpecialAttackSkill::Activate(AActor* Caster)
{
	// 스킬은 자신을 소유한 악세서리에게 형태 전환을 요청
	AGorillaAccessoryActor* GorillaAccessory = Cast<AGorillaAccessoryActor>(SourceAccessory);
	if (GorillaAccessory)
	{
		GorillaAccessory->ToggleGorillaForm();
	}
	else
	{
		UE_LOG("[UGorillaSpecialAttackSkill] SourceAccessory is not a AGorillaAccessoryActor!");
	}
}
