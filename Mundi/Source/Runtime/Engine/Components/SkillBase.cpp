#include "pch.h"
#include "SkillBase.h"
#include "AccessoryActor.h"
#include "AngryCoachCharacter.h"


USkillBase::USkillBase()
{

}
void USkillBase::Activate(AActor* Caster)
{
	// - 애니메이션 재생 (더 느리고 강한 모션)
	if (!SourceAccessory || !SourceAccessory->GetOwningCharacter() || !Montage)
	{
		return;
	}
	AAngryCoachCharacter* Character = SourceAccessory->GetOwningCharacter();
	Character->PlayMontage(Montage);
}

void USkillBase::OnActivate(AActor* Caster)
{
	// C++ 로직 or Lua 이벤트
}
	