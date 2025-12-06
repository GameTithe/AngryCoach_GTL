#pragma once
#include "SkillBase.h"
#include "UKnifeHeavyAttackSkill.generated.h"

UCLASS()
class UKnifeHeavyAttackSkill : public USkillBase
{
public:
	GENERATED_REFLECTION_BODY()

	UKnifeHeavyAttackSkill();

	virtual void Activate(AActor* Caster) override;
};
