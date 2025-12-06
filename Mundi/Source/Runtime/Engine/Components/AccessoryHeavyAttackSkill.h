#pragma once
#include "SkillBase.h"
#include "UAccessoryHeavyAttackSkill.generated.h"

UCLASS()
class UAccessoryHeavyAttackSkill : public USkillBase
{
public:
	GENERATED_REFLECTION_BODY()

	UAccessoryHeavyAttackSkill();

	virtual void Activate(AActor* Caster) override;
};
