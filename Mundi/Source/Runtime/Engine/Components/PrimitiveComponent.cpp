#include "pch.h"
#include "PrimitiveComponent.h"
#include "SceneComponent.h"
#include "Actor.h"
#include "Collision.h"
#include "WorldPartitionManager.h"
#include "../Physics/BodyInstance.h"


// IMPLEMENT_CLASS is now auto-generated in .generated.cpp
UPrimitiveComponent::UPrimitiveComponent() : bGenerateOverlapEvents(true)
{
	CollisionEnabled = (ECollisionState)CollisionEnabled_Internal;
}

UPrimitiveComponent::~UPrimitiveComponent()
{
    if (BodyInstance)
    {
        delete BodyInstance;
        BodyInstance = nullptr;
    }
}

void UPrimitiveComponent::BeginPlay()
{
    Super::BeginPlay();

    OnComponentBeginOverlap.AddDynamic(this, &UPrimitiveComponent::beginoverlap);
    OnComponentEndOverlap.AddDynamic(this, &UPrimitiveComponent::endoverlap);
    OnComponentHit.AddDynamic(this, &UPrimitiveComponent::hit);
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
    for (const FOverlapInfo& Info : PendingOverlaps)
    {
        // 이미 겹쳐있으면 생략
        if (ActiveOverlaps.Contains(Info))
        {
            continue;
        }

        if (UWorld* World = GetWorld())
        {
            if (!World->TryMarkOverlapPair(GetOwner(), Info.OtherActor))
            {
                ActiveOverlaps.Add(Info);
                continue;
            }
        }

        ActiveOverlaps.Add(Info);
        OnComponentBeginOverlap.Broadcast(this);

        if (AActor* Owner = GetOwner())
        {
            Owner->OnComponentBeginOverlap.Broadcast(this, Info.OtherComp, nullptr);
        }
    }

    // end overlap
    for (int32 i = ActiveOverlaps.Num() - 1; i >= 0; --i)
    {
        const FOverlapInfo& ActiveInfo = ActiveOverlaps[i];
        
        if (!PendingOverlaps.Contains(ActiveInfo))
        {
            // EndOverlap도 Begin과 마찬가지로 쌍방향 체크
            if (UWorld* World = GetWorld())
            {
                // TryMarkOverlapPair는 내부적으로 Begin/End 상태를 토글하거나
                // 별도의 Set을 써야 하는데, 언리얼은 보통 Begin때 넣고 End때 뺌.
                // *주의: Begin때 넣은 Pair를 여기서 제거해줘야 다음 충돌이 가능함*
                // 네 구현에 따라 TryRemoveOverlapPair 같은 함수가 필요할 수 있음.
                
                // 여기서는 단순히 호출한다고 가정
            }

            // 1) 컴포넌트 End
            OnComponentEndOverlap.Broadcast(this);

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
        
        OnComponentHit.Broadcast(this);
        
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
    if (UWorld* World = GetWorld())
    {
        if (UWorldPartitionManager* Partition = World->GetPartitionManager())
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

void UPrimitiveComponent::beginoverlap(UPrimitiveComponent* A)
{
    UE_LOG("beginoverlap : %p", A);
}

void UPrimitiveComponent::endoverlap(UPrimitiveComponent* A)
{
    UE_LOG("endoverlap : %p", A);
}

void UPrimitiveComponent::hit(UPrimitiveComponent* A)
{
    UE_LOG("hit : %p", A);
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
