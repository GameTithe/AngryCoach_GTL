#pragma once
#include "SkillBase.h" 
#include "UKnifeSpecialAttackSkill.generated.h"

UCLASS()
class UKnifeSpecialAttackSkill : public USkillBase
{
public:
	GENERATED_REFLECTION_BODY()

	UKnifeSpecialAttackSkill();

	virtual void Activate(AActor* Caster) override;
};
