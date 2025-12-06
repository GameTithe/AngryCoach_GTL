#pragma once
#include "SkillBase.h"
#include "UGorillaHeavyAttackSkill.generated.h"

UCLASS()
class UGorillaHeavyAttackSkill : public USkillBase
{
public:
	GENERATED_REFLECTION_BODY()

	UGorillaHeavyAttackSkill();

	virtual void Activate(AActor* Caster) override;
};
