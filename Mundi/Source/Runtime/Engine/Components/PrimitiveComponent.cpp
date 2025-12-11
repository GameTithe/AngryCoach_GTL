#include "pch.h"
#include "PrimitiveComponent.h"
#include "SceneComponent.h"
#include "Actor.h"
#include "Pawn.h"
#include "WorldPartitionManager.h"
#include "../Physics/BodyInstance.h"
#include "../Physics/PhysScene.h"
#include "World.h"

// IMPLEMENT_CLASS is now auto-generated in .generated.cpp
UPrimitiveComponent::UPrimitiveComponent() : bGenerateOverlapEvents(true)
{
	CollisionEnabled = (ECollisionState)CollisionEnabled_Internal;
}

UPrimitiveComponent::~UPrimitiveComponent()
{
    if (BodyInstance)
    {
        // PhysScene에서 RigidActor를 먼저 제거해야 dangling pointer 방지
        if (GWorld && !GWorld->IsTearingDown())
        {
            if (FPhysScene* PhysScene = GWorld->GetPhysScene())
            {
                // 콜백에서 이 컴포넌트 접근 방지 (WaitForSimulation 전에!)
                BodyInstance->OwnerComponent = nullptr;
                // userData도 미리 null - fetchResults 중 dangling FBodyInstance 접근 방지
                if (BodyInstance->RigidActor)
                {
                    BodyInstance->RigidActor->userData = nullptr;
                }
                // 시뮬레이션 완료 대기 후 제거
                PhysScene->WaitForSimulation();
                BodyInstance->Terminate(*PhysScene);
            }
        }
        delete BodyInstance;
        BodyInstance = nullptr;
    }
}

void UPrimitiveComponent::BeginPlay()
{
    Super::BeginPlay();

    // 필요하면 바인딩
    // OnComponentBeginOverlap.AddDynamic(this, &UPrimitiveComponent::OnBeginOverlap);
    // OnComponentEndOverlap.AddDynamic(this, &UPrimitiveComponent::OnEndOverlap);
    // OnComponentHit.AddDynamic(this, &UPrimitiveComponent::OnHit);
}

void UPrimitiveComponent::TickComponent(float DeltaTime)
{
    Super::TickComponent(DeltaTime);

    // 오버랩 비활성화 시 Queue 비우기
    if (!bGenerateOverlapEvents)
    {
        PendingOverlaps.Empty();
    }

    // begin overlap
    for (const FHitResult& HitResult : PendingOverlaps)
    {
        FOverlapInfo Info;
        Info.OtherComp = HitResult.HitComponent;
        Info.OtherActor = HitResult.HitActor;
        // 이미 겹쳐있으면 생략
        if (ActiveOverlaps.Contains(Info))
        {
            continue;
        }

        if (UWorld* World = GetWorld())
        {
            if (!World->TryMarkOverlapPair(GetOwner(), HitResult.HitActor))
            {
                
                ActiveOverlaps.Add(Info);
                continue;
            }
        }

        ActiveOverlaps.Add(Info);
        OnComponentBeginOverlap.Broadcast(this, HitResult.HitComponent, HitResult);

        if (AActor* Owner = GetOwner())
        {
            Owner->OnComponentBeginOverlap.Broadcast(this, Info.OtherComp, nullptr);
        }
    }

    // end overlap
    for (int32 i = ActiveOverlaps.Num() - 1; i >= 0; --i)
    {
        const FOverlapInfo& ActiveInfo = ActiveOverlaps[i];

        bool bStillOverlapping = false;        
        for (const FHitResult& PendingHit : PendingOverlaps)
        {
            if (PendingHit.HitComponent == ActiveInfo.OtherComp)
            {
                bStillOverlapping = true;                
                break;
            }
            
        }
        if (!bStillOverlapping)
        {
            if (UWorld* World = GetWorld())
            {
                if (!World->TryMarkOverlapPair(GetOwner(), ActiveInfo.OtherActor))
                {
                    ActiveOverlaps.RemoveAt(i);
                    continue;
                }
            }
            
            FHitResult EndResult = {};
            EndResult.HitComponent = ActiveInfo.OtherComp;
            EndResult.HitActor = ActiveInfo.OtherActor;
            EndResult.bBlockingHit = false;

            OnComponentEndOverlap.Broadcast(this, EndResult.HitComponent, EndResult);
            // 2) 액터 End - [복구된 부분]            
            if (AActor* Owner = GetOwner())
            {
                Owner->OnComponentEndOverlap.Broadcast(this, ActiveInfo.OtherComp, nullptr);
                // Owner->NotifyActorEndOverlap(ActiveInfo.OtherActor);
            }

            ActiveOverlaps.RemoveAt(i);
        }        
    }

    CurrentFrameHits.Empty();

    // hit
    for (const FHitResult& Hit : PendingHits)
    {
        {
            /*
             * Hit event를 최초 1 번만 발생하게 하는 코드
             */
            uintptr_t OtherPtr = reinterpret_cast<uintptr_t>(Hit.HitComponent);
            uint64 HitID = static_cast<uint64>(OtherPtr);
            CurrentFrameHits.Add(HitID);
            if (PreviousFrameHits.Contains(HitID))
            {
                continue;;
            }
        }
        
        OnComponentHit.Broadcast(this, Hit.HitComponent, Hit);
        
        if (AActor* Owner = GetOwner())
        {
            Owner->OnComponentHit.Broadcast(this, Hit.HitComponent, nullptr);
        }
    }

    PreviousFrameHits = CurrentFrameHits;
    PendingHits.Empty();
    PendingOverlaps.Empty();
}

void UPrimitiveComponent::OnRegister(UWorld* InWorld)
{
    Super::OnRegister(InWorld);

    // UStaticMeshComponent라면 World Partition에 추가. (null 체크는 Register 내부에서 수행)
    if (InWorld)
    {
        if (UWorldPartitionManager* Partition = InWorld->GetPartitionManager())
        {
            Partition->Register(this);
        }
    }
}

void UPrimitiveComponent::OnUnregister()
{
    // 피직스 바디를 먼저 정리 (시뮬레이션 콜백에서 dangling pointer 방지)
    if (BodyInstance && GWorld && !GWorld->IsTearingDown())
    {
        if (FPhysScene* PhysScene = GWorld->GetPhysScene())
        {
            // 콜백에서 이 컴포넌트 접근 방지 (WaitForSimulation 전에!)
            BodyInstance->OwnerComponent = nullptr;
            // userData도 미리 null - fetchResults 중 dangling FBodyInstance 접근 방지
            if (BodyInstance->RigidActor)
            {
                BodyInstance->RigidActor->userData = nullptr;
            }
            // 시뮬레이션 완료 대기 (시뮬레이션 중 removeActor 호출 방지)
            PhysScene->WaitForSimulation();
            BodyInstance->Terminate(*PhysScene);
        }
        delete BodyInstance;
        BodyInstance = nullptr;
    }

    // PIE 종료 시 World가 이미 파괴 중일 수 있으므로
    // GWorld가 유효하고 파괴 중이 아닐 때만 Partition 접근
    if (GWorld && !GWorld->IsTearingDown())
    {
        if (UWorldPartitionManager* Partition = GWorld->GetPartitionManager())
        {
            Partition->Unregister(this);
        }
    }

    Super::OnUnregister();
}

void UPrimitiveComponent::SetMaterialByName(uint32 InElementIndex, const FString& InMaterialName)
{
    SetMaterial(InElementIndex, UResourceManager::GetInstance().Load<UMaterial>(InMaterialName));
} 
 
void UPrimitiveComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();
	CollisionEnabled = (ECollisionState)CollisionEnabled_Internal;
}

void UPrimitiveComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);

    if (bInIsLoading)
    {
        FJsonSerializer::ReadBool(InOutHandle, "bOverrideCollisionSetting", bOverrideCollisionSetting, bOverrideCollisionSetting, false);
        FJsonSerializer::ReadInt32(InOutHandle, "CollisionEnabled_Internal", CollisionEnabled_Internal, CollisionEnabled_Internal, false);
        CollisionEnabled = (ECollisionState)CollisionEnabled_Internal;
    }

	if (!bInIsLoading)
	{
        InOutHandle["bOverrideCollisionSetting"] = bOverrideCollisionSetting;
        CollisionEnabled_Internal = (int32)CollisionEnabled;
        InOutHandle["CollisionEnabled_Internal"] = CollisionEnabled_Internal;
	}
}

void UPrimitiveComponent::OnCreatePhysicsState()
{

}

void UPrimitiveComponent::SetCollisionEnabled(ECollisionState NewState)
{
    bOverrideCollisionSetting = true;
    CollisionEnabled = NewState;

    // 기존 physics body가 있으면 shape flag 업데이트
    if (BodyInstance && BodyInstance->RigidActor)
    {
        UWorld* World = GetWorld();
        if (!World || !World->GetPhysScene())
            return;

        PxScene* PxScenePtr = World->GetPhysScene()->GetScene();
        if (!PxScenePtr)
            return;

        SCOPED_PHYSX_WRITE_LOCK(*PxScenePtr);

        PxRigidActor* Actor = BodyInstance->RigidActor;
        PxU32 NumShapes = Actor->getNbShapes();
        std::vector<PxShape*> Shapes(NumShapes);
        Actor->getShapes(Shapes.data(), NumShapes);

        for (PxShape* Shape : Shapes)
        {
            if (!Shape) continue;

            switch (NewState)
            {
                case ECollisionState::NoCollision:
                    Shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, false);
                    Shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
                    Shape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, false);
                    break;
                case ECollisionState::QueryOnly:
                    Shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
                    Shape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, true);
                    Shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, true);
                    break;
                case ECollisionState::PhysicsOnly:
                    Shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, false);
                    Shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, true);
                    Shape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, false);
                    break;
                case ECollisionState::QueryAndPhysics:
                    Shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, false);
                    Shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, true);
                    Shape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, true);
                    break;
            }
        }
    }
}

void UPrimitiveComponent::OnBeginOverlap(UPrimitiveComponent* A, UPrimitiveComponent* B, const FHitResult& HitResult)
{
    UE_LOG("OnBeginOverlap");
}

void UPrimitiveComponent::OnEndOverlap(UPrimitiveComponent* A, UPrimitiveComponent* B, const FHitResult& HitResult)
{
    UE_LOG("OnEndOverlap");
}

void UPrimitiveComponent::OnHit(UPrimitiveComponent* A, UPrimitiveComponent* B, const FHitResult& HitResult)
{
    UE_LOG("OnHit");
}

bool UPrimitiveComponent::IsOverlappingActor(const AActor* Other) const
{
    if (!Other)
    {
        return false;
    }

    const TArray<FOverlapInfo>& Infos = GetOverlapInfos();
    for (const FOverlapInfo& Info : Infos)
    {
        if (Info.OtherComp)
        {
            if (AActor* Owner = Info.OtherComp->GetOwner())
            {
                if (Owner == Other)
                {
                    return true;
                }
            }
        }
    }
    return false;
}
 