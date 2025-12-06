#pragma once
#include "SkillBase.h"
#include "UCloakLightAttackSkill.generated.h"

UCLASS()
class UCloakLightAttackSkill : public USkillBase
{
public:
	GENERATED_REFLECTION_BODY()

	UCloakLightAttackSkill();

	virtual void Activate(AActor* Caster) override;
};
