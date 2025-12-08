#pragma once
#include "SkillBase.h"
#include "UCloakSpecialAttackSkill.generated.h"

UCLASS()
class UCloakSpecialAttackSkill : public USkillBase
{
public:
	GENERATED_REFLECTION_BODY()

	UCloakSpecialAttackSkill();

	virtual void Activate(AActor* Caster) override;
};
