#pragma once
#include "SkillBase.h"
#include "UGorillaSpecialAttackSkill.generated.h"

UCLASS()
class UGorillaSpecialAttackSkill : public USkillBase
{
public:
	GENERATED_REFLECTION_BODY()

	UGorillaSpecialAttackSkill();

	virtual void Activate(AActor* Caster) override;
};
