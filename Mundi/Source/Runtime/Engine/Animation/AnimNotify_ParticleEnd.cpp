#include "pch.h"
#include "AnimNotify_ParticleEnd.h"	 
#include "AngryCoachCharacter.h"
#include "AccessoryActor.h"
#include "SkeletalMeshComponent.h"

IMPLEMENT_CLASS(UAnimNotify_ParticleEnd)
void UAnimNotify_ParticleEnd::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{ 
	if (!MeshComp)
	{ 
		return;
	}

	if (AAngryCoachCharacter* Character = Cast<AAngryCoachCharacter>(MeshComp->GetOwner()))
	{ 
		if (AAccessoryActor* Accessory = Character->GetCurrentAccessory())
		{
			 
			Accessory->StopTryParticle();
		} 
	}
	else
	{
		UE_LOG("[AnimNotify_ParticleEnd] Owner is not AngryCoachCharacter!");
	}
}


 