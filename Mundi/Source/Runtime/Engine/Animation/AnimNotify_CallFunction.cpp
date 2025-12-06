#include "pch.h"
#include "AnimNotify_CallFunction.h"
#include "Actor.h"
#include "SkeletalMeshComponent.h"

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
        Owner->ProcessEvent(FunctionName);
    }
    
}
