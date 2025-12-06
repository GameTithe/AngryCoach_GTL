#pragma once
#include "SkillBase.h"
#include "UDefaultLightAttackSkill.generated.h"

UCLASS()
class UDefaultLightAttackSkill : public USkillBase
{
public:
	GENERATED_REFLECTION_BODY()

	UDefaultLightAttackSkill();

	virtual void Activate(AActor* Caster) override;
};
