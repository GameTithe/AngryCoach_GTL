#pragma once
#include "SkillBase.h"
#include "UPunchLightAttackSkill.generated.h"

UCLASS()
class UPunchLightAttackSkill : public USkillBase
{
public:
	GENERATED_REFLECTION_BODY()

	UPunchLightAttackSkill();

	virtual void Activate(AActor* Caster) override;
};
