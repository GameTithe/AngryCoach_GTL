#pragma once

#include "ActorComponent.h"
#include "USkillBase.generated.h"

class AAccessoryActor;

class USkillBase: public UObject
{
	public:
	GENERATED_REFLECTION_BODY()

	USkillBase();

	virtual void Activate(AActor* Caster);
	void SetSourceAccessory(AAccessoryActor* InAccessory) { SourceAccessory = InAccessory; };

protected:
	AAccessoryActor* SourceAccessory = nullptr;
	
	void OnActivate(AActor* Caster);
};