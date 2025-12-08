#pragma once
#include "SkillBase.h"
#include "UPunchSpecialAttackSkill.generated.h"

UCLASS()
class UPunchSpecialAttackSkill : public USkillBase
{
public:
	GENERATED_REFLECTION_BODY()

	UPunchSpecialAttackSkill();
	 
	virtual void Activate(AActor* Caster) override;
};