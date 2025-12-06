#pragma once

#include "SceneComponent.h"
#include "SkeletalMeshComponent.h"

// A single anchor component that represents the transform of a selected bone or socket.
// The viewer selects this component so the editor gizmo latches onto it.
class UBoneAnchorComponent : public USceneComponent
{
public:
    DECLARE_CLASS(UBoneAnchorComponent, USceneComponent)

    // 본 타겟 설정
    void SetTarget(USkeletalMeshComponent* InTarget, int32 InBoneIndex);
    void SetBoneIndex(int32 InBoneIndex) { BoneIndex = InBoneIndex; SocketIndex = -1; }
    int32 GetBoneIndex() const { return BoneIndex; }
    USkeletalMeshComponent* GetTarget() const { return Target; }

    // 소켓 타겟 설정
    void SetSocketTarget(USkeletalMeshComponent* InTarget, int32 InSocketIndex);
    void SetSocketIndex(int32 InSocketIndex) { SocketIndex = InSocketIndex; BoneIndex = -1; }
    int32 GetSocketIndex() const { return SocketIndex; }
    bool IsSocketMode() const { return SocketIndex >= 0; }

    // Updates this anchor's world transform from the target bone/socket's current transform
    void UpdateAnchorFromBone();

    // When user moves gizmo, write back to the bone/socket
    void OnTransformUpdated() override;

private:
    USkeletalMeshComponent* Target = nullptr;
    int32 BoneIndex = -1;
    int32 SocketIndex = -1;
};
