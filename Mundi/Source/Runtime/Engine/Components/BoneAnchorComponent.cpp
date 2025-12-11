#include "pch.h"
#include "BoneAnchorComponent.h"
#include "SelectionManager.h"
#include "StaticMeshComponent.h"
#include "Source/Runtime/Core/Misc/VertexData.h"

IMPLEMENT_CLASS(UBoneAnchorComponent)

void UBoneAnchorComponent::SetTarget(USkeletalMeshComponent* InTarget, int32 InBoneIndex)
{
    Target = InTarget;
    BoneIndex = InBoneIndex;
    SocketIndex = -1;
    PreviewMesh = nullptr;

    // UpdateAnchorFromBone 중 writeback 방지 (SetWorldTransform → OnTransformUpdated 호출됨)
    bool bWasEditable = bIsEditable;
    bIsEditable = false;
    UpdateAnchorFromBone();
    bIsEditable = bWasEditable;
}

void UBoneAnchorComponent::SetSocketTarget(USkeletalMeshComponent* InTarget, int32 InSocketIndex)
{
    Target = InTarget;
    SocketIndex = InSocketIndex;
    BoneIndex = -1;
    PreviewMesh = nullptr;

    // UpdateAnchorFromBone 중 writeback 방지 (SetWorldTransform → OnTransformUpdated 호출됨)
    bool bWasEditable = bIsEditable;
    bIsEditable = false;
    UpdateAnchorFromBone();
    bIsEditable = bWasEditable;
}

void UBoneAnchorComponent::SetPreviewMeshTarget(UStaticMeshComponent* InPreviewMesh)
{
    PreviewMesh = InPreviewMesh;
    BoneIndex = -1;
    SocketIndex = -1;

    // UpdateAnchorFromBone 중 writeback 방지 (SetWorldTransform → OnTransformUpdated 호출됨)
    bool bWasEditable = bIsEditable;
    bIsEditable = false;
    UpdateAnchorFromBone();
    bIsEditable = bWasEditable;
}

void UBoneAnchorComponent::UpdateAnchorFromBone()
{
    // 프리뷰 메시 모드
    if (PreviewMesh)
    {
        SetWorldTransform(PreviewMesh->GetWorldTransform());
        return;
    }

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

    // 편집 모드가 아니면 writeback 하지 않음 (다른 곳에서 트랜스폼이 변경되어도 소켓/본 값 유지)
    if (!bIsEditable)
    {
        return;
    }

    const FTransform AnchorWorld = GetWorldTransform();

    // 프리뷰 메시 모드 - 프리뷰 메시의 상대 트랜스폼 업데이트
    if (PreviewMesh)
    {
        // 기즈모 월드 트랜스폼 -> 프리뷰 메시의 부모 기준 상대 트랜스폼
        USceneComponent* Parent = PreviewMesh->GetAttachParent();
        if (Parent)
        {
            FTransform ParentWorld = Parent->GetWorldTransform();

            // 소켓에 붙어있으면 소켓 트랜스폼 가져오기
            FName SocketName = PreviewMesh->GetAttachSocketName();
            if (!SocketName.ToString().empty())
            {
                if (USkeletalMeshComponent* SkelParent = Cast<USkeletalMeshComponent>(Parent))
                {
                    if (SkelParent->DoesSocketExist(SocketName))
                    {
                        ParentWorld = SkelParent->GetSocketTransform(SocketName);
                    }
                }
            }

            FTransform RelativeTransform = ParentWorld.GetRelativeTransform(AnchorWorld);
            PreviewMesh->SetRelativeLocation(RelativeTransform.Translation);
            PreviewMesh->SetRelativeRotation(RelativeTransform.Rotation);
            // 스케일은 기즈모로 변경하지 않음 (UI에서만 변경)
        }
        return;
    }

    if (!Target)
        return;

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
        // 스케일은 기즈모로 변경하지 않음 (UI에서만 변경, 부동소수점 오차 방지)

        // 이 소켓에 붙은 자식 컴포넌트들 트랜스폼 갱신 트리거
        FName SocketName(Socket.SocketName);
        for (USceneComponent* Child : Target->GetAttachChildren())
        {
            if (Child && Child->GetAttachSocketName() == SocketName)
            {
                // OnTransformUpdated는 USceneComponent에서 public
                Child->OnTransformUpdated();
            }
        }
    }
    // 본 모드
    else if (BoneIndex >= 0)
    {
        Target->SetBoneWorldTransform(BoneIndex, AnchorWorld);

        // 이 본에 붙은 소켓들의 자식 컴포넌트들도 트랜스폼 갱신
        USkeletalMesh* Mesh = Target->GetSkeletalMesh();
        if (Mesh)
        {
            const FSkeleton* Skeleton = Mesh->GetSkeleton();
            if (Skeleton)
            {
                // 본 이름 가져오기
                FString BoneName = Skeleton->GetBoneName(BoneIndex).ToString();

                // 이 본에 붙은 소켓들 찾기
                for (const FSkeletalMeshSocket& Socket : Skeleton->Sockets)
                {
                    if (Socket.BoneName == BoneName)
                    {
                        FName SocketName(Socket.SocketName);
                        // 이 소켓에 붙은 자식들 업데이트
                        for (USceneComponent* Child : Target->GetAttachChildren())
                        {
                            if (Child && Child->GetAttachSocketName() == SocketName)
                            {
                                Child->OnTransformUpdated();
                            }
                        }
                    }
                 }
            }
        }
    }
}
