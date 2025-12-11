#include "pch.h"
#include "ShapeComponent.h"
#include "OBB.h"
#include "World.h"
#include "WorldPartitionManager.h"
#include "BVHierarchy.h"
#include "GameObject.h"
#include "Collision.h"
#include "../Physics/BodyInstance.h"
#include "../Physics/PhysicsTypes.h"
#include <PxPhysicsAPI.h>
using namespace physx;

// IMPLEMENT_CLASS is now auto-generated in .generated.cpp
UShapeComponent::UShapeComponent() : bShapeIsVisible(true), bShapeHiddenInGame(true)
{
    ShapeColor = FVector4(0.2f, 0.8f, 1.0f, 1.0f); 
    bCanEverTick = true;
}

void UShapeComponent::BeginPlay()
{
    Super::BeginPlay();
}

void UShapeComponent::OnRegister(UWorld* InWorld)
{
    Super::OnRegister(InWorld);

    GetWorldAABB();

    // 부착 정보 로그
    FString ParentName = AttachParent ? AttachParent->ObjectName.ToString() : "None";
    FString SocketName = AttachSocketName.ToString().empty() ? "None" : AttachSocketName.ToString();
    FTransform WorldTM = GetWorldTransform();
    UE_LOG("[ShapeComponent] %s OnRegister - Parent: %s, Socket: %s, WorldPos: (%.2f, %.2f, %.2f)",
        ObjectName.ToString().c_str(), ParentName.c_str(), SocketName.c_str(),
        WorldTM.Translation.X, WorldTM.Translation.Y, WorldTM.Translation.Z);

    // PhysX body 생성은 자식 클래스에서 크기 계산 후 호출
}

void UShapeComponent::OnUnregister()
{
    // PhysX body 정리
    if (BodyInstance)
    {
        UWorld* World = GetWorld();
        if (World && World->GetPhysScene())
        {
            BodyInstance->Terminate(*World->GetPhysScene());
        }
        delete BodyInstance;
        BodyInstance = nullptr;
    }

    Super::OnUnregister();
}

void UShapeComponent::OnTransformUpdated()
{
    GetWorldAABB();

    // Keep BVH up-to-date for broad phase queries
    if (UWorld* World = GetWorld())
    {
        if (UWorldPartitionManager* Partition = World->GetPartitionManager())
        {
            Partition->MarkDirty(this);
        }
    }

    // PhysX body 위치 업데이트 (Kinematic)
    if (BodyInstance && BodyInstance->RigidActor)
    {
        PxRigidDynamic* Dyn = BodyInstance->RigidActor->is<PxRigidDynamic>();
        if (Dyn && (Dyn->getRigidBodyFlags() & PxRigidBodyFlag::eKINEMATIC))
        {
            FTransform WorldTransform = GetWorldTransform();
            PxTransform PxTM = ToPx(WorldTransform);
            // setKinematicTarget은 다음 simulate()에서 적용됨
            // setGlobalPose는 즉시 적용됨
            Dyn->setGlobalPose(PxTM);
        }
    }

    //UpdateOverlaps();
    Super::OnTransformUpdated();
}

void UShapeComponent::TickComponent(float DeltaSeconds)
{
    // PhysX body가 있으면 자체 충돌 시스템 스킵 (PhysX가 처리)
    if (BodyInstance && BodyInstance->RigidActor)
    {
        Super::TickComponent(DeltaSeconds);
        return;
    }

    // if (!bGenerateOverlapEvents || !bBlockComponent)
    // {
    //     // Super::TickComponent(DeltaSeconds);
    //     return;
    // }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }
    
    for (AActor* Actor : World->GetActors())
    {
        if (!Actor || !Actor->IsActorActive())
        {
            continue;
        }

        for (USceneComponent* Comp : Actor->GetSceneComponents())
        {
            UShapeComponent* Other = Cast<UShapeComponent>(Comp);

            if (!Other || Other == this)
            {
                continue;
            }

            if (Other->GetOwner() == this->GetOwner())
            {
                continue;
            }

            if (!Other->bGenerateOverlapEvents && !Other->bBlockComponent)
            {
                continue;
            }
            
            if (this->bBlockComponent/* && Other->bBlockComponent*/)
            {
                FHitResult HitResult;
                if (Collision::ComputePenetration(this, Other, HitResult))
                {
                    // ImpactPoint, ImpactNormal, bHit, PenetrationDepth는 ComputePenetration에서 채워줌
                    HitResult.HitComponent = Other;
                    HitResult.HitActor = Other->GetOwner();                    
                    
                    // 부모의 Hit 큐에 등록
                    PendingHits.Add(HitResult);
                
                    // *물리 반작용(Resolve)*: 
                    // 만약 네가 여기서 바로 밀어내기(Position correction)를 하고 싶다면:
                    AddWorldOffset(-HitResult.ImpactNormal * HitResult.PenetrationDepth);
                }
            }
            
            if (this->bGenerateOverlapEvents /*&& Other->bGenerateOverlapEvents*/)
            {
                FHitResult HitResult;
                if (/*Collision::CheckOverlap(this, Other)*/Collision::ComputePenetration(this, Other, HitResult))
                {
                    HitResult.HitComponent = Other;
                    HitResult.HitActor = Other->GetOwner();
                    // HitResult.bBlockingHit = false;
                    // HitResult.ImpactPoint = this->GetWorldLocation();
                    // HitResult.ImpactNormal = FVector::Zero();
                    PendingOverlaps.Add(HitResult);
                    AddWorldOffset(-HitResult.ImpactNormal * HitResult.PenetrationDepth);
                }
            }
        }
    }

    // 충돌 감지 후 부모 Tick 호출해야지 감지된 충돌 이벤트가 현재 프레임에 발생된다
    Super::TickComponent(DeltaSeconds);    
}

FAABB UShapeComponent::GetWorldAABB() const
{
    if (AActor* Owner = GetOwner())
    {
        FAABB OwnerBounds = Owner->GetBounds();
        const FVector HalfExtent = OwnerBounds.GetHalfExtent();
        WorldAABB = OwnerBounds;
    }
    return WorldAABB;
}
  
void UShapeComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();
}




