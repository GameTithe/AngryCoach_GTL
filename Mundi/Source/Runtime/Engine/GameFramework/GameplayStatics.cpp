#include "pch.h"
#include "GameplayStatics.h"

float UGameplayStatics::ApplyDamage(AActor* DamagedActor, float BaseDamage, AActor* DamageCauser,
    const FHitResult& HitResult)
{
    if (!DamagedActor || BaseDamage <= 0.0f)
    {
        return 0.0f;
    }

    if (DamagedActor == DamageCauser)
    {
        return 0.0f;
    }

    return DamagedActor->TakeDamage(BaseDamage, HitResult, DamageCauser);
}
