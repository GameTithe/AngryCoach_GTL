#pragma once
#include "SkillBase.h"
#include "UAccessoryLightAttackSkill.generated.h"

UCLASS()
class UAccessoryLightAttackSkill : public USkillBase
{
public:
	GENERATED_REFLECTION_BODY()

	UAccessoryLightAttackSkill();

	virtual void Activate(AActor* Caster) override;
};
