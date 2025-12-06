#pragma once
#include "SkillBase.h"
#include "UDefaultHeavyAttackSkill.generated.h"

UCLASS()
class UDefaultHeavyAttackSkill : public USkillBase
{
public:
	GENERATED_REFLECTION_BODY()

	UDefaultHeavyAttackSkill();

	virtual void Activate(AActor* Caster) override;
};
