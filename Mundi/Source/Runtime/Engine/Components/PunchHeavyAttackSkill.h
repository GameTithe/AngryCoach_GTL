#pragma once
#include "SkillBase.h"
#include "UPunchHeavyAttackSkill.generated.h"

UCLASS()
class UPunchHeavyAttackSkill : public USkillBase
{
public:
	GENERATED_REFLECTION_BODY()

	UPunchHeavyAttackSkill();

	virtual void Activate(AActor* Caster) override;
};
