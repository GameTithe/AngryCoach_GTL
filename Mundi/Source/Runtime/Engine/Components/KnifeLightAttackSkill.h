#pragma once
#include "SkillBase.h"
#include "UKnifeLightAttackSkill.generated.h"

UCLASS()
class UKnifeLightAttackSkill : public USkillBase
{
public:
	GENERATED_REFLECTION_BODY()

	UKnifeLightAttackSkill();

	virtual void Activate(AActor* Caster) override;
};
