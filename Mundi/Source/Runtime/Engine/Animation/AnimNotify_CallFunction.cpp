#include "pch.h"
#include "AnimNotify_CallFunction.h"
#include "Actor.h"
#include "Pawn.h"
#include "SkeletalMeshComponent.h"
#include <AccessoryActor.h>

IMPLEMENT_CLASS(UAnimNotify_CallFunction)
void UAnimNotify_CallFunction::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
    AActor* Owner = MeshComp->GetOwner();
    if (!Owner)
    {
        return;
    }

    if (FunctionName.IsValid())
    {
        if (APawn* Pawn = Cast<APawn>(Owner))
        {            
            Pawn->ProcessEvent(FunctionName);
        }
        if (AAccessoryActor* AccessoryActor = Cast< AAccessoryActor>(Owner))
        {
            AccessoryActor->ProcessEvent(FunctionName);
        }
    }
    
}
