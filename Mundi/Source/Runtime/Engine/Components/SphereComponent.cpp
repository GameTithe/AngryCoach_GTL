#include "pch.h"
#include "SphereComponent.h"
#include "Renderer.h"
#include "Actor.h"
#include "../Physics/BodyInstance.h"
#include "../Physics/BodySetup.h"
#include "../Physics/PhysScene.h"
#include "World.h"
#include <PxPhysicsAPI.h>
using namespace physx;
// IMPLEMENT_CLASS is now auto-generated in .generated.cpp
USphereComponent::USphereComponent()
{
    SphereRadius = 0.5f;
}

void USphereComponent::OnRegister(UWorld* InWorld)
{
    Super::OnRegister(InWorld);

    // 사용자가 명시적으로 SphereRadius를 설정한 경우(기본값 0.5f가 아닌 경우) 덮어쓰지 않음
    constexpr float DefaultRadius = 0.5f;
    if (FMath::Abs(SphereRadius - DefaultRadius) > 1e-6f)
    {
        // PhysX body 생성 후 리턴
        OnCreatePhysicsState(InWorld);
        return;
    }


    if (AActor* Owner = GetOwner())
    {
        FAABB ActorBounds = Owner->GetBounds();
        FVector WorldHalfExtent = ActorBounds.GetHalfExtent();

        // World scale로 나눠서 local 값 계산
        const FTransform WorldTransform = GetWorldTransform();
        const FVector S = FVector(
            std::fabs(WorldTransform.Scale3D.X),
            std::fabs(WorldTransform.Scale3D.Y),
            std::fabs(WorldTransform.Scale3D.Z)
        );

        constexpr float Eps = 1e-6f;

        // XYZ 중 최대값을 반지름으로 사용
        float LocalRadiusX = S.X > Eps ? WorldHalfExtent.X / S.X : WorldHalfExtent.X;
        float LocalRadiusY = S.Y > Eps ? WorldHalfExtent.Y / S.Y : WorldHalfExtent.Y;
        float LocalRadiusZ = S.Z > Eps ? WorldHalfExtent.Z / S.Z : WorldHalfExtent.Z;

        SphereRadius = FMath::Max(LocalRadiusX, FMath::Max(LocalRadiusY, LocalRadiusZ));
    }

    // PhysX body 생성
    OnCreatePhysicsState(InWorld);
}

void USphereComponent::OnCreatePhysicsState(UWorld* World)
{
    if (!World)
    {
        return;
    }

    FPhysScene* PhysScene = World->GetPhysScene();
    if (!PhysScene)
    {
        return;
    }

    // Radius가 유효하지 않으면 PhysX body 생성하지 않음
    if (SphereRadius <= 0.0f)
    {
        UE_LOG("[SphereComponent] %s - Invalid radius (%.4f), skipping PhysX body creation",
            ObjectName.ToString().c_str(), SphereRadius);
        return;
    }

    if (BodyInstance)
    {
        BodyInstance->Terminate(*PhysScene);
        delete BodyInstance;
        BodyInstance = nullptr;
    }

    UBodySetup* SphereBodySetup = NewObject<UBodySetup>();
    if (!SphereBodySetup)
    {
        return;
    }

    FKSphereElem SphereElem;
    SphereElem.Center = FVector::Zero();
    SphereElem.Radius = SphereRadius;
    SphereBodySetup->AddSphere(SphereElem);

    SphereBodySetup->CollisionState = CollisionEnabled;

    BodyInstance = new FBodyInstance();
    BodyInstance->OwnerComponent = this;
    BodyInstance->BodySetup = SphereBodySetup;

    FTransform WorldTransform = GetWorldTransform();
    float Mass = 10.0f;

    BodyInstance->InitDynamic(*PhysScene, WorldTransform, Mass, WorldTransform.Scale3D, 0);

    if (BodyInstance->RigidActor)
    {
        if (PxRigidDynamic* Dyn = BodyInstance->RigidActor->is<PxRigidDynamic>())
        {
            Dyn->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true);
        }
    }
}

void USphereComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();
}

void USphereComponent::GetShape(FShape& Out) const
{
    Out.Kind = EShapeKind::Sphere;
    Out.Sphere.SphereRadius = SphereRadius;	
}

FAABB USphereComponent::GetWorldAABB() const
{
    const FTransform WorldTransform = GetWorldTransform();
    const FVector Center = WorldTransform.Translation;

    // 2. 월드 스케일의 절대값 구하기 (음수 스케일 대응)
    const float AbsScaleX = std::fabs(WorldTransform.Scale3D.X);
    const float AbsScaleY = std::fabs(WorldTransform.Scale3D.Y);
    const float AbsScaleZ = std::fabs(WorldTransform.Scale3D.Z);

    // 3. 가장 큰 스케일 값을 찾음 (구의 형태를 유지하면서 감싸기 위해 Max 사용)
    const float MaxScale = FMath::Max(AbsScaleX, FMath::Max(AbsScaleY, AbsScaleZ));

    // 4. 월드 공간에서의 반지름 계산
    const float WorldRadius = SphereRadius * MaxScale;

    // 5. AABB 생성 (Center - Radius, Center + Radius)
    // FAABB 생성자가 (Min, Max)를 받는다고 가정
    return FAABB(Center - FVector(WorldRadius, WorldRadius, WorldRadius), Center + FVector(WorldRadius, WorldRadius, WorldRadius));}

void USphereComponent::RenderDebugVolume(URenderer* Renderer) const
{
    if (!bShapeIsVisible) return;
    if (GWorld->bPie)
    {
        if (bShapeHiddenInGame)
            return;
    }
    // Draw three great circles to visualize the sphere (XY, XZ, YZ planes)
    const FTransform WorldTransform = GetWorldTransform();
    const FVector Center = GetWorldLocation();
    const float AbsScaleX = std::fabs(WorldTransform.Scale3D.X);
    const float AbsScaleY = std::fabs(WorldTransform.Scale3D.Y);
    const float AbsScaleZ = std::fabs(WorldTransform.Scale3D.Z);
    const float MaxScale = FMath::Max(AbsScaleX, FMath::Max(AbsScaleY, AbsScaleZ));
    
    const float Radius = SphereRadius * MaxScale;
    constexpr int NumSegments = 16;

    TArray<FVector> StartPoints;
    TArray<FVector> EndPoints;
    TArray<FVector4> Colors;

    // XY circle (Z fixed)
    for (int i = 0; i < NumSegments; ++i)
    {
        const float a0 = (static_cast<float>(i) / NumSegments) * TWO_PI;
        const float a1 = (static_cast<float>((i + 1) % NumSegments) / NumSegments) * TWO_PI;

        const FVector p0 = Center + FVector(Radius * std::cos(a0), Radius * std::sin(a0), 0.0f);
        const FVector p1 = Center + FVector(Radius * std::cos(a1), Radius * std::sin(a1), 0.0f);

        StartPoints.Add(p0);
        EndPoints.Add(p1);
        Colors.Add(ShapeColor);
    }

    // XZ circle (Y fixed)
    for (int i = 0; i < NumSegments; ++i)
    {
        const float a0 = (static_cast<float>(i) / NumSegments) * TWO_PI;
        const float a1 = (static_cast<float>((i + 1) % NumSegments) / NumSegments) * TWO_PI;

        const FVector p0 = Center + FVector(Radius * std::cos(a0), 0.0f, Radius * std::sin(a0));
        const FVector p1 = Center + FVector(Radius * std::cos(a1), 0.0f, Radius * std::sin(a1));

        StartPoints.Add(p0);
        EndPoints.Add(p1);
        Colors.Add(ShapeColor);
    }

    // YZ circle (X fixed)
    for (int i = 0; i < NumSegments; ++i)
    {
        const float a0 = (static_cast<float>(i) / NumSegments) * TWO_PI;
        const float a1 = (static_cast<float>((i + 1) % NumSegments) / NumSegments) * TWO_PI;

        const FVector p0 = Center + FVector(0.0f, Radius * std::cos(a0), Radius * std::sin(a0));
        const FVector p1 = Center + FVector(0.0f, Radius * std::cos(a1), Radius * std::sin(a1));

        StartPoints.Add(p0);
        EndPoints.Add(p1);
        Colors.Add(ShapeColor);
    }

    Renderer->AddLines(StartPoints, EndPoints, Colors);
}
