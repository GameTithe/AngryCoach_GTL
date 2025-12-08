#include "pch.h"
#include "AnimNotify_ParticleStart.h"
#include "AngryCoachCharacter.h"
#include "AccessoryActor.h"
#include "SkeletalMeshComponent.h"

IMPLEMENT_CLASS(UAnimNotify_ParticleStart)
void UAnimNotify_ParticleStart::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
	// UE_LOG("[AnimNotify_ParticleStart] Notify called");

	if (!MeshComp)
	{
		UE_LOG("[AnimNotify_ParticleStart] MeshComp is null!");
		return;
	}

	if (AAngryCoachCharacter* Character = Cast<AAngryCoachCharacter>(MeshComp->GetOwner()))
	{
		// UE_LOG("[AnimNotify_ParticleStart] Character found");
		if (AAccessoryActor* Accessory = Character->GetCurrentAccessory())
		{
			// UE_LOG("[AnimNotify_ParticleStart] Calling PlayTryParticle");
			Accessory->PlayTryParticle();
		}
		else
		{
			UE_LOG("[AnimNotify_ParticleStart] No accessory equipped!");
		}
	}
	else
	{
		UE_LOG("[AnimNotify_ParticleStart] Owner is not AngryCoachCharacter!");
	}
}
