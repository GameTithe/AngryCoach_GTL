#include "pch.h"
#include "ShapeComponent.h"
#include "OBB.h"
#include "World.h"
#include "WorldPartitionManager.h"
#include "BVHierarchy.h"
#include "GameObject.h"
#include "Collision.h"

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

    //UpdateOverlaps();
    Super::OnTransformUpdated();
}

void UShapeComponent::TickComponent(float DeltaSeconds)
{
    if (!bGenerateOverlapEvents || !bBlockComponent)
    {
        // Super::TickComponent(DeltaSeconds);
        return;
    }

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
                    HitResult.HitComponent = Other;
                    HitResult.HitActor = Other->GetOwner();
                    HitResult.bBlockingHit = true;
                    HitResult.ImpactPoint = this->GetWorldLocation();
                    HitResult.ImpactNormal = this->GetWorldTransform().Rotation.GetForwardVector();
                    
                    // 부모의 Hit 큐에 등록
                    PendingHits.Add(HitResult);
                
                    // *물리 반작용(Resolve)*: 
                    // 만약 네가 여기서 바로 밀어내기(Position correction)를 하고 싶다면:
                    AddWorldOffset(-HitResult.ImpactNormal * HitResult.PenetrationDepth);
                }
            }
            
            if (this->bGenerateOverlapEvents && Other->bGenerateOverlapEvents)
            {                
                if (Collision::CheckOverlap(this, Other))
                {
                    FHitResult HitResult;
                    HitResult.HitComponent = Other;
                    HitResult.HitActor = Other->GetOwner();
                    HitResult.bBlockingHit = false;
                    HitResult.ImpactPoint = this->GetWorldLocation();
                    HitResult.ImpactNormal = FVector::Zero();
                    
                    PendingOverlaps.Add(HitResult);
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




