#pragma once

struct FHitResult;

class UGameplayStatics
{
public:
    static float ApplyDamage(AActor* DamagedActor, float BaseDamage, AActor* DamageCauser, const FHitResult& HitResult);
};