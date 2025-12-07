#pragma once
#include "AnimNotify.h"

class UAnimNotify_ParticleStart : public UAnimNotify
{
public:
	DECLARE_CLASS(UAnimNotify_ParticleStart, UAnimNotify)

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;

};