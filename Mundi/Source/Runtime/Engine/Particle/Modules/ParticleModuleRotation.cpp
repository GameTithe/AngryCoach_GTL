#include "pch.h"
#include "ParticleModuleRotation.h"
#include "ParticleModuleRequired.h"
#include "../ParticleEmitterInstance.h"
#include "../ParticleHelper.h"

IMPLEMENT_CLASS(UParticleModuleRotation)

UParticleModuleRotation::UParticleModuleRotation()
{
    ModuleType = EParticleModuleType::Spawn;
    bSpawnModule = true;
}

void UParticleModuleRotation::SpawnAsync(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase, FParticleSimulationContext& Context)
{
    if (!ParticleBase)
        return;

    // 랜덤 값 생성 (0~1 범위)
    float RandomValue = Owner->GetRandomFloat();

    // 초기 회전각 설정 (라디안 단위)
    FVector RotationValue = StartRotation.GetValue(FVector(RandomValue, RandomValue, RandomValue));

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
}
