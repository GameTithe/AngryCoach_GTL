#include "pch.h"
#include "ParticleModuleVelocity.h"
#include "ParticleModuleRequired.h"
#include "../ParticleEmitter.h"
#include "../ParticleHelper.h"
#include "Source/Runtime/Engine/Particle/ParticleEmitterInstance.h"

IMPLEMENT_CLASS(UParticleModuleVelocity)

UParticleModuleVelocity::UParticleModuleVelocity()
{
    bSpawnModule = true;
    bUpdateModule = true;
}

void UParticleModuleVelocity::SpawnAsync(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase, FParticleSimulationContext& Context)
{
    if (!ParticleBase)
        return;

    // 초기 속도 설정
    FVector Velocity = StartVelocity.GetValue({Owner->GetRandomFloat(), Owner->GetRandomFloat(),Owner->GetRandomFloat()});
    float Multiplier = VelocityMultiplier.GetValue(Owner->GetRandomFloat());
    Velocity *= Multiplier;

    // bUseLocalSpace가 false면 컴포넌트 회전을 적용해서 월드 방향으로 변환
    bool bUseLocalSpace = false;
    if (Owner->CachedRequiredModule)
    {
        bUseLocalSpace = Owner->CachedRequiredModule->bUseLocalSpace;
    }

    if (!bUseLocalSpace)
    {
        // 월드 스페이스: 속도 방향에 컴포넌트 회전 적용
        Velocity = Context.ComponentWorldMatrix.TransformVector(Velocity);
    }

    ParticleBase->Velocity = Velocity;
    ParticleBase->BaseVelocity = ParticleBase->Velocity;
}

void UParticleModuleVelocity::Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
    BEGIN_UPDATE_LOOP
    {
        // 이전 위치 저장
        Particle.OldLocation = Particle.Location;

        // 중력 적용
        Particle.Velocity += Gravity * DeltaTime;

        // 공기 저항 적용
        if (Damping > 0.0f)
        {
            float DampingFactor = FMath::Max(0.0f, 1.0f - Damping * DeltaTime);
            Particle.Velocity *= DampingFactor;
        }

        // 위치 업데이트
        Particle.Location += Particle.Velocity * DeltaTime;
    }
    END_UPDATE_LOOP;
}
