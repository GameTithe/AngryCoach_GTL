#pragma once

struct FHitResult
{
    bool bHit = false;                          // 충돌했는가
    bool bBlockingHit = false;                  // 이동을 막는 충돌인가
    float Time = 1.0f;                          // Sweep 시 충돌 시간 (0~1, 1이면 충돌 없음)
    float Distance = 0.0f;                      // 충돌까지의 거리
    float PenetrationDepth = 0.0f;              // 얼마나 파고들었는가
    FVector Location = FVector::Zero();         // Sweep 충돌 시 Shape의 위치
    FVector ImpactPoint = FVector::Zero();      // 충돌 지점
    FVector ImpactNormal{0, 0, 1};              // 밀어내야 할 방향
    FVector TraceStart = FVector::Zero();       // Sweep 시작점
    FVector TraceEnd = FVector::Zero();         // Sweep 끝점
    AActor* HitActor = nullptr;                 // 충돌한 액터
    UPrimitiveComponent* HitComponent = nullptr; // 충돌한 컴포넌트

    void Reset()
    {
        bHit = false;
        bBlockingHit = false;
        Time = 1.0f;
        Distance = 0.0f;
        PenetrationDepth = 0.0f;
        Location = FVector::Zero();
        ImpactPoint = FVector::Zero();
        ImpactNormal = {0, 0, 1};
        TraceStart = FVector::Zero();
        TraceEnd = FVector::Zero();
        HitActor = nullptr;
        HitComponent = nullptr;
    }
};