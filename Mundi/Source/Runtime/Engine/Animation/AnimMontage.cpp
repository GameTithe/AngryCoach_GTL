#include "pch.h"
#include "AnimMontage.h"
#include "AnimSequence.h"

IMPLEMENT_CLASS(UAnimMontage)

float UAnimMontage::GetPlayLength() const
{
    if (Sequence)
    {
        return Sequence->GetPlayLength();
    }
    return 0.0f;
}
