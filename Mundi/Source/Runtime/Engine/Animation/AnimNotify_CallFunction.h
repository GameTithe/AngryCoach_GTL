#pragma once
#include "AnimNotify.h"

class UAnimNotify;

class UAnimNotify_CallFunction : public UAnimNotify
{
public:
    DECLARE_CLASS(UAnimNotify_CallFunction, UAnimNotify)

    FName FunctionName;
    void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;
};