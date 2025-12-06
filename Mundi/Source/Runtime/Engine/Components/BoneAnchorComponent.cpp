#include "pch.h"
#include "BoneAnchorComponent.h"
#include "SelectionManager.h"
#include "Source/Runtime/Core/Misc/VertexData.h"

IMPLEMENT_CLASS(UBoneAnchorComponent)

void UBoneAnchorComponent::SetTarget(USkeletalMeshComponent* InTarget, int32 InBoneIndex)
{
    Target = InTarget;
    BoneIndex = InBoneIndex;
    SocketIndex = -1;
    UpdateAnchorFromBone();
}

void UBoneAnchorComponent::SetSocketTarget(USkeletalMeshComponent* InTarget, int32 InSocketIndex)
{
    Target = InTarget;
    SocketIndex = InSocketIndex;
    BoneIndex = -1;
    UpdateAnchorFromBone();
}

void UBoneAnchorComponent::UpdateAnchorFromBone()
{
    if (!Target)
        return;

    // 소켓 모드
    if (SocketIndex >= 0)
    {
        USkeletalMesh* Mesh = Target->GetSkeletalMesh();
        if (!Mesh) return;

        const FSkeleton* Skeleton = Mesh->GetSkeleton();
        if (!Skeleton || SocketIndex >= static_cast<int32>(Skeleton->Sockets.size()))
            return;

        const FSkeletalMeshSocket& Socket = Skeleton->Sockets[SocketIndex];

        // 소켓이 부착된 본의 인덱스 찾기
        int32 ParentBoneIndex = Skeleton->FindBoneIndex(FName(Socket.BoneName));
        if (ParentBoneIndex < 0)
            return;

        // 본의 월드 트랜스폼
        FTransform BoneWorld = Target->GetBoneWorldTransform(ParentBoneIndex);

        // 소켓의 상대 트랜스폼 적용
        FTransform SocketRelative;
        SocketRelative.Translation = Socket.RelativeLocation;
        SocketRelative.Rotation = Socket.RelativeRotation;
        SocketRelative.Scale3D = Socket.RelativeScale;

        // SocketWorld = BoneWorld * SocketRelative (부모 * 자식)
        FTransform SocketWorld = BoneWorld.GetWorldTransform(SocketRelative);
        SetWorldTransform(SocketWorld);
    }
    // 본 모드
    else if (BoneIndex >= 0)
    {
        const FTransform BoneWorld = Target->GetBoneWorldTransform(BoneIndex);
        SetWorldTransform(BoneWorld);
    }
}

void UBoneAnchorComponent::OnTransformUpdated()
{
    Super::OnTransformUpdated();

    if (!Target)
        return;

    const FTransform AnchorWorld = GetWorldTransform();

    // 소켓 모드 - 소켓의 상대 트랜스폼 업데이트
    if (SocketIndex >= 0)
    {
        USkeletalMesh* Mesh = Target->GetSkeletalMesh();
        if (!Mesh) return;

        FSkeleton* Skeleton = const_cast<FSkeleton*>(Mesh->GetSkeleton());
        if (!Skeleton || SocketIndex >= static_cast<int32>(Skeleton->Sockets.size()))
            return;

        FSkeletalMeshSocket& Socket = Skeleton->Sockets[SocketIndex];

        // 소켓이 부착된 본의 인덱스 찾기
        int32 ParentBoneIndex = Skeleton->FindBoneIndex(FName(Socket.BoneName));
        if (ParentBoneIndex < 0)
            return;

        // 본의 월드 트랜스폼
        FTransform BoneWorld = Target->GetBoneWorldTransform(ParentBoneIndex);

        // 월드 → 본 로컬 변환 (GetRelativeTransform 사용)
        FTransform SocketRelative = BoneWorld.GetRelativeTransform(AnchorWorld);

        Socket.RelativeLocation = SocketRelative.Translation;
        Socket.RelativeRotation = SocketRelative.Rotation;
        Socket.RelativeScale = SocketRelative.Scale3D;
    }
    // 본 모드
    else if (BoneIndex >= 0)
    {
        Target->SetBoneWorldTransform(BoneIndex, AnchorWorld);
    }
}
