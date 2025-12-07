#pragma once
#include "SkillBase.h"
#include "UCloakHeavyAttackSkill.generated.h"

UCLASS()
class UCloakHeavyAttackSkill : public USkillBase
{
public:
	GENERATED_REFLECTION_BODY()

	UCloakHeavyAttackSkill();

	virtual void Activate(AActor* Caster) override;
};
