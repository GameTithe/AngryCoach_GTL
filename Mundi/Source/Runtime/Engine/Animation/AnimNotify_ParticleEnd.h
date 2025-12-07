#pragma once
#include "AnimNotify.h"

class UAnimNotify_ParticleEnd : public UAnimNotify
{
public: 
	DECLARE_CLASS(UAnimNotify_ParticleEnd, UAnimNotify)
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;

};