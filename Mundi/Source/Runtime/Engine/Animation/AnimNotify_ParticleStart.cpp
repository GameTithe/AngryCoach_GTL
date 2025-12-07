#include "pch.h"
#include "AnimNotify_ParticleStart.h"
#include "AngryCoachCharacter.h"
#include "AccessoryActor.h"
#include "SkeletalMeshComponent.h"

IMPLEMENT_CLASS(UAnimNotify_ParticleStart)
void UAnimNotify_ParticleStart::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
	if (!MeshComp) return;

	if (AAngryCoachCharacter* Character = Cast<AAngryCoachCharacter>(MeshComp->GetOwner()))
	{
		if (AAccessoryActor* Accessory = Character->GetCurrentAccessory())
		{
			Accessory->PlayTryParticle();
		}
	}
}
