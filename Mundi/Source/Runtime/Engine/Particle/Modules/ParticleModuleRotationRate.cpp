#include "pch.h"
#include "ParticleModuleRotationRate.h"
#include "ParticleModuleRequired.h"
#include "../ParticleEmitter.h"
#include "../ParticleEmitterInstance.h"
#include "../ParticleHelper.h"

IMPLEMENT_CLASS(UParticleModuleRotationRate)

UParticleModuleRotationRate::UParticleModuleRotationRate()
{
    ModuleType = EParticleModuleType::Spawn;
    bSpawnModule = true;
    bUpdateModule = true;
}

void UParticleModuleRotationRate::SpawnAsync(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase, FParticleSimulationContext& Context)
{
    if (!ParticleBase)
        return;

    // 랜덤 값 생성 (0~1 범위)
    float RandomValue = Owner->GetRandomFloat();

    // 초기 회전 각도 설정 (라디안 단위)
    FVector RotationValue = InitialRotation.GetValue(RandomValue);

    // bUseLocalSpace가 false면 컴포넌트 회전을 적용
    bool bUseLocalSpace = false;
    if (Owner->CachedRequiredModule)
    {
        bUseLocalSpace = Owner->CachedRequiredModule->bUseLocalSpace;
    }

    if (!bUseLocalSpace)
    {
        // 파티클 로컬 회전을 Quaternion으로 변환
        FQuat LocalRotQuat = FQuat::MakeFromEulerZYX(FVector(
            RadiansToDegrees(RotationValue.X),
            RadiansToDegrees(RotationValue.Y),
            RadiansToDegrees(RotationValue.Z)
        ));

        // 컴포넌트 회전과 결합
        FQuat CombinedRot = Context.ComponentRotation * LocalRotQuat;

        // 다시 Euler(라디안)로 변환
        FVector EulerDeg = CombinedRot.ToEulerZYXDeg();
        RotationValue = FVector(
            DegreesToRadians(EulerDeg.X),
            DegreesToRadians(EulerDeg.Y),
            DegreesToRadians(EulerDeg.Z)
        );
    }

    ParticleBase->Rotation = RotationValue;
    ParticleBase->BaseRotation = RotationValue;

    // 초기 회전 속도 설정 (라디안/초 단위)
    FVector RotationRateValue = StartRotationRate.GetValue(RandomValue);
    ParticleBase->RotationRate = RotationRateValue;
    ParticleBase->BaseRotationRate = RotationRateValue;
}

void UParticleModuleRotationRate::Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
    // BEGIN_UPDATE_LOOP 매크로 사용 - 모든 활성 파티클 순회
    BEGIN_UPDATE_LOOP
    {
        // Rotation += RotationRate * dt
        // 각속도를 시간에 적분하여 회전각 업데이트
        Particle.Rotation += Particle.RotationRate * DeltaTime;
    }
    END_UPDATE_LOOP;
}
