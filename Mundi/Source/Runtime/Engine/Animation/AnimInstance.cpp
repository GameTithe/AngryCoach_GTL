#include "pch.h"
#include "AnimInstance.h"
#include "SkeletalMeshComponent.h"
#include "AnimTypes.h"
#include "AnimationStateMachine.h"
#include "AnimSequence.h"
#include "AnimMontage.h"
// For notify dispatching
#include "Source/Runtime/Engine/Animation/AnimNotify.h"

IMPLEMENT_CLASS(UAnimInstance)

// ============================================================
// Initialization & Setup
// ============================================================

void UAnimInstance::Initialize(USkeletalMeshComponent* InComponent)
{
    OwningComponent = InComponent;

    if (OwningComponent && OwningComponent->GetSkeletalMesh())
    {
        CurrentSkeleton = OwningComponent->GetSkeletalMesh()->GetSkeleton();
    }
}

// ============================================================
// Update & Pose Evaluation
// ============================================================

void UAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    // 상태머신이 있으면 먼저 ProcessState 호출
    if (AnimStateMachine)
    {
        AnimStateMachine->ProcessState(DeltaSeconds);
    }

    // PoseProvider 또는 Sequence가 있어야 재생 가능
    if (!CurrentPlayState.PoseProvider && !CurrentPlayState.Sequence)
    {
        // 몽타주만 재생 중인 경우도 처리
        if (MontageState && MontageState->bPlaying)
        {
            UpdateMontage(DeltaSeconds);

            UAnimMontage* M = MontageState->Montage;
            UAnimSequence* CurrentSeq = M ? M->GetSectionSequence(MontageState->CurrentSectionIndex) : nullptr;

            if (MontageState->Weight > 0.0f && CurrentSeq)
            {
                TArray<FTransform> FinalPose;

                // 섹션 블렌딩 중이면 이전 섹션과 현재 섹션 블렌딩
                if (MontageState->bBlendingSection && MontageState->PreviousSectionIndex >= 0)
                {
                    UAnimSequence* PrevSeq = M->GetSectionSequence(MontageState->PreviousSectionIndex);
                    if (PrevSeq && CurrentSkeleton)
                    {
                        TArray<FTransform> PrevPoseRaw, CurrPoseRaw;
                        TArray<FTransform> PrevPoseMapped, CurrPoseMapped;
                        PrevSeq->EvaluatePose(MontageState->PreviousSectionEndTime, DeltaSeconds, PrevPoseRaw);
                        CurrentSeq->EvaluatePose(MontageState->Position, DeltaSeconds, CurrPoseRaw);

                        // 스켈레톤 순서로 변환
                        MapPoseToSkeleton(PrevPoseRaw, PrevSeq, PrevPoseMapped);
                        MapPoseToSkeleton(CurrPoseRaw, CurrentSeq, CurrPoseMapped);

                        float Alpha = MontageState->SectionBlendTime / FMath::Max(MontageState->SectionBlendTotalTime, 0.001f);
                        Alpha = FMath::Clamp(Alpha, 0.0f, 1.0f);

                        // 둘 다 스켈레톤 순서이므로 직접 블렌딩
                        BlendPoseArrays(PrevPoseMapped, CurrPoseMapped, Alpha, FinalPose);
                    }
                    else
                    {
                        TArray<FTransform> RawPose;
                        CurrentSeq->EvaluatePose(MontageState->Position, DeltaSeconds, RawPose);
                        MapPoseToSkeleton(RawPose, CurrentSeq, FinalPose);
                    }
                }
                else
                {
                    TArray<FTransform> RawPose;
                    CurrentSeq->EvaluatePose(MontageState->Position, DeltaSeconds, RawPose);
                    MapPoseToSkeleton(RawPose, CurrentSeq, FinalPose);
                }

                if (OwningComponent && FinalPose.Num() > 0)
                {
                    OwningComponent->SetAnimationPose(FinalPose);
                }
            }
        }
        return;
    }

    // 이전 시간 저장 (노티파이 검출용)
    PreviousPlayTime = CurrentPlayState.CurrentTime;

    // 현재 상태 시간 갱신
    AdvancePlayState(CurrentPlayState, DeltaSeconds);

    // 기본 포즈 계산
    TArray<FTransform> BasePose;
    const bool bIsBlending = (BlendTimeRemaining > 0.0f && (BlendTargetState.Sequence != nullptr || BlendTargetState.PoseProvider != nullptr));

    if (bIsBlending)
    {
        AdvancePlayState(BlendTargetState, DeltaSeconds);

        const float SafeTotalTime = FMath::Max(BlendTotalTime, 1e-6f);
        float BlendAlpha = 1.0f - (BlendTimeRemaining / SafeTotalTime);
        BlendAlpha = FMath::Clamp(BlendAlpha, 0.0f, 1.0f);

        TArray<FTransform> FromPose, FromPoseMapped;
        TArray<FTransform> TargetPose, TargetPoseMapped;
        EvaluatePoseForState(CurrentPlayState, FromPose, DeltaSeconds);
        EvaluatePoseForState(BlendTargetState, TargetPose, DeltaSeconds);

        // 스켈레톤 순서로 변환
        UAnimSequence* FromSeq = CurrentPlayState.Sequence;
        if (!FromSeq && CurrentPlayState.PoseProvider)
            FromSeq = CurrentPlayState.PoseProvider->GetDominantSequence();

        UAnimSequence* ToSeq = BlendTargetState.Sequence;
        if (!ToSeq && BlendTargetState.PoseProvider)
            ToSeq = BlendTargetState.PoseProvider->GetDominantSequence();

        MapPoseToSkeleton(FromPose, FromSeq, FromPoseMapped);
        MapPoseToSkeleton(TargetPose, ToSeq, TargetPoseMapped);

        // 이제 둘 다 스켈레톤 순서이므로 직접 인덱스 블렌딩
        BlendPoseArrays(FromPoseMapped, TargetPoseMapped, BlendAlpha, BasePose);

        BlendTimeRemaining = FMath::Max(BlendTimeRemaining - DeltaSeconds, 0.0f);
        if (BlendTimeRemaining <= 1e-4f)
        {
            CurrentPlayState = BlendTargetState;
            CurrentPlayState.BlendWeight = 1.0f;

            BlendTargetState = FAnimationPlayState();
            BlendTimeRemaining = 0.0f;
            BlendTotalTime = 0.0f;
        }
    }
    else
    {
        TArray<FTransform> RawPose;
        EvaluatePoseForState(CurrentPlayState, RawPose, DeltaSeconds);

        UAnimSequence* BaseSeq = CurrentPlayState.Sequence;
        if (!BaseSeq && CurrentPlayState.PoseProvider)
            BaseSeq = CurrentPlayState.PoseProvider->GetDominantSequence();

        MapPoseToSkeleton(RawPose, BaseSeq, BasePose);
    }

    // 몽타주 업데이트
    UpdateMontage(DeltaSeconds);

    // 최종 포즈 계산 (기본 + 몽타주 블렌드)
    TArray<FTransform> FinalPose = BasePose;

    if (MontageState && MontageState->bPlaying && MontageState->Weight > 0.0f && MontageState->Montage)
    {
        UAnimMontage* M = MontageState->Montage;
        UAnimSequence* CurrentSeq = M->GetSectionSequence(MontageState->CurrentSectionIndex);
        if (CurrentSeq)
        {
            TArray<FTransform> MontagePose;

            // 섹션 블렌딩 중이면 이전/현재 섹션 블렌드
            if (MontageState->bBlendingSection && MontageState->PreviousSectionIndex >= 0)
            {
                UAnimSequence* PrevSeq = M->GetSectionSequence(MontageState->PreviousSectionIndex);
                if (PrevSeq && CurrentSkeleton)
                {
                    TArray<FTransform> PrevPoseRaw, CurrPoseRaw;
                    TArray<FTransform> PrevPoseMapped, CurrPoseMapped;
                    PrevSeq->EvaluatePose(MontageState->PreviousSectionEndTime, DeltaSeconds, PrevPoseRaw);
                    CurrentSeq->EvaluatePose(MontageState->Position, DeltaSeconds, CurrPoseRaw);

                    // 스켈레톤 순서로 변환
                    MapPoseToSkeleton(PrevPoseRaw, PrevSeq, PrevPoseMapped);
                    MapPoseToSkeleton(CurrPoseRaw, CurrentSeq, CurrPoseMapped);

                    float Alpha = MontageState->SectionBlendTime / FMath::Max(MontageState->SectionBlendTotalTime, 0.001f);
                    Alpha = FMath::Clamp(Alpha, 0.0f, 1.0f);

                    // 이제 둘 다 스켈레톤 순서이므로 직접 인덱스 블렌딩
                    BlendPoseArrays(PrevPoseMapped, CurrPoseMapped, Alpha, MontagePose);
                }
                else
                {
                    TArray<FTransform> RawPose;
                    CurrentSeq->EvaluatePose(MontageState->Position, DeltaSeconds, RawPose);
                    MapPoseToSkeleton(RawPose, CurrentSeq, MontagePose);
                }
            }
            else
            {
                TArray<FTransform> RawPose;
                CurrentSeq->EvaluatePose(MontageState->Position, DeltaSeconds, RawPose);
                MapPoseToSkeleton(RawPose, CurrentSeq, MontagePose);
            }

            // BasePose와 MontagePose 둘 다 스켈레톤 순서이므로 직접 블렌딩
            BlendPoseArrays(BasePose, MontagePose, MontageState->Weight, FinalPose);
        }
    }

    // 최종 포즈 적용 (모두 스켈레톤 순서)
    if (OwningComponent && FinalPose.Num() > 0)
    {
        OwningComponent->SetAnimationPose(FinalPose);
    }

    // 노티파이 트리거
    TriggerAnimNotifies(DeltaSeconds);

    // 커브 업데이트
    UpdateAnimationCurves();
}


void UAnimInstance::EvaluatePose(TArray<FTransform>& OutPose)
{
    EvaluatePoseForState(CurrentPlayState, OutPose);
}
// ============================================================
// Playback API
// ============================================================

void UAnimInstance::PlaySequence(UAnimSequence* Sequence, bool bLoop, float InPlayRate)
{
    if (!Sequence)
    {
        UE_LOG("UAnimInstance::PlaySequence - Invalid sequence");
        return;
    }

    CurrentPlayState.Sequence = Sequence;
    CurrentPlayState.PoseProvider = Sequence;  // IAnimPoseProvider로 설정
    CurrentPlayState.CurrentTime = 0.0f;
    CurrentPlayState.PlayRate = InPlayRate;
    CurrentPlayState.bIsLooping = bLoop;
    CurrentPlayState.bIsPlaying = true;
    CurrentPlayState.BlendWeight = 1.0f;

    PreviousPlayTime = 0.0f;

    UE_LOG("UAnimInstance::PlaySequence - Playing: %s (Loop: %d, PlayRate: %.2f)",
        Sequence->ObjectName.ToString().c_str(), bLoop, InPlayRate);
}

void UAnimInstance::PlaySequence(UAnimSequence* Sequence, EAnimLayer Layer, bool bLoop, float InPlayRate)
{
    if (!Sequence)
    {
        UE_LOG("UAnimInstance::PlaySequence - Invalid sequence");
        return;
    }

    int32 LayerIndex = (int32)Layer;

    // 해당 레이어의 상태를 덮어쓴다.
    Layers[LayerIndex].Sequence = Sequence;
    Layers[LayerIndex].PoseProvider = Sequence;  // IAnimPoseProvider로 설정
    Layers[LayerIndex].CurrentTime = 0.0f;
    Layers[LayerIndex].PlayRate = InPlayRate;
    Layers[LayerIndex].bIsLooping = bLoop;
    Layers[LayerIndex].bIsPlaying = true;
    Layers[LayerIndex].BlendWeight = 1.0f;

    LayerBlendTimeRemaining[LayerIndex] = 0.0f;
}

void UAnimInstance::StopSequence()
{
    CurrentPlayState.bIsPlaying = false;
    CurrentPlayState.CurrentTime = 0.0f;

    UE_LOG("UAnimInstance::StopSequence - Stopped");
}

void UAnimInstance::BlendTo(UAnimSequence* Sequence, bool bLoop, float InPlayRate, float BlendTime)
{
    if (!Sequence)
    {
        UE_LOG("UAnimInstance::BlendTo - Invalid sequence");
        return;
    }

    BlendTargetState.Sequence = Sequence;
    BlendTargetState.PoseProvider = Sequence;  // IAnimPoseProvider로 설정
    BlendTargetState.CurrentTime = 0.0f;
    BlendTargetState.PlayRate = InPlayRate;
    BlendTargetState.bIsLooping = bLoop;
    BlendTargetState.bIsPlaying = true;
    BlendTargetState.BlendWeight = 0.0f;

    BlendTimeRemaining = FMath::Max(BlendTime, 0.0f);
    BlendTotalTime = BlendTimeRemaining;

    if (BlendTimeRemaining <= 0.0f)
    {
        // 즉시 전환
        CurrentPlayState = BlendTargetState;
        CurrentPlayState.BlendWeight = 1.0f;
        BlendTargetState = FAnimationPlayState();
    }

    UE_LOG("UAnimInstance::BlendTo - Blending to: %s (BlendTime: %.2f)",
        Sequence->ObjectName.ToString().c_str(), BlendTime);
}

void UAnimInstance::BlendTo(UAnimSequence* Sequence, EAnimLayer Layer, bool bLoop, float InPlayRate, float BlendTime)
{
    int32 LayerIndex = (int32)Layer;

    BlendTargets[LayerIndex].Sequence = Sequence;
    BlendTargets[LayerIndex].PoseProvider = Sequence;  // IAnimPoseProvider로 설정
    BlendTargets[LayerIndex].CurrentTime = 0.0f;
    BlendTargets[LayerIndex].PlayRate = InPlayRate;
    BlendTargets[LayerIndex].bIsLooping = bLoop;
    BlendTargets[LayerIndex].bIsPlaying = true;
    BlendTargets[LayerIndex].BlendWeight = 0.0f;

    LayerBlendTimeRemaining[LayerIndex] = FMath::Max(BlendTime, 0.0f);
    LayerBlendTotalTime[LayerIndex] = LayerBlendTimeRemaining[LayerIndex];
}

void UAnimInstance::PlayPoseProvider(IAnimPoseProvider* Provider, bool bLoop, float InPlayRate)
{
    if (!Provider)
    {
        UE_LOG("UAnimInstance::PlayPoseProvider - Invalid provider");
        return;
    }

    CurrentPlayState.Sequence = nullptr;  // Sequence는 없음
    CurrentPlayState.PoseProvider = Provider;
    CurrentPlayState.CurrentTime = 0.0f;
    CurrentPlayState.PlayRate = InPlayRate;
    CurrentPlayState.bIsLooping = bLoop;
    CurrentPlayState.bIsPlaying = true;
    CurrentPlayState.BlendWeight = 1.0f;

    PreviousPlayTime = 0.0f;

    UE_LOG("UAnimInstance::PlayPoseProvider - Playing PoseProvider (Loop: %d, PlayRate: %.2f)",
        bLoop, InPlayRate);
}

void UAnimInstance::BlendToPoseProvider(IAnimPoseProvider* Provider, bool bLoop, float InPlayRate, float BlendTime)
{
    if (!Provider)
    {
        UE_LOG("UAnimInstance::BlendToPoseProvider - Invalid provider");
        return;
    }

    BlendTargetState.Sequence = nullptr;  // Sequence는 없음
    BlendTargetState.PoseProvider = Provider;
    BlendTargetState.CurrentTime = 0.0f;
    BlendTargetState.PlayRate = InPlayRate;
    BlendTargetState.bIsLooping = bLoop;
    BlendTargetState.bIsPlaying = true;
    BlendTargetState.BlendWeight = 0.0f;

    BlendTimeRemaining = FMath::Max(BlendTime, 0.0f);
    BlendTotalTime = BlendTimeRemaining;

    if (BlendTimeRemaining <= 0.0f)
    {
        // 즉시 전환
        CurrentPlayState = BlendTargetState;
        CurrentPlayState.BlendWeight = 1.0f;
        BlendTargetState = FAnimationPlayState();
    }

    UE_LOG("UAnimInstance::BlendToPoseProvider - Blending to PoseProvider (BlendTime: %.2f)",
        BlendTime);
}


// ============================================================
// Notify & Curve Processing
// ============================================================

void UAnimInstance::EnableUpperBodySplit(FName BoneName)
{
    if (!CurrentSkeleton) return;
    int32 RootBoneIdx = CurrentSkeleton->FindBoneIndex(BoneName);
    if (RootBoneIdx == INDEX_NONE) return;

    const int32 NumBones = CurrentSkeleton->GetNumBones();
    UpperBodyMask = { false };

    // BFS로 자식 bone을 모두 True로 변경

    TArray<int32> BoneQueue;
    BoneQueue.Add(RootBoneIdx);
     
    while (BoneQueue.Num() > 0)
    {
        int32 CurrentIdx = BoneQueue.Pop();

        UpperBodyMask[CurrentIdx] = true;
            
        //// TODO: 자식 본 찾기
        //for (int32 i = CurrentIdx + 1; i < NumBones; ++i)
        //{
        //    if (CurrentSkeleton->GetParentIndex(i) == CurrentIdx)
        //    {
        //        BoneQueue.Add(i);
        //    }
        //}
    }
    bUseUpperBody = true; 
}

void UAnimInstance::TriggerAnimNotifies(float DeltaSeconds)
{
    // 노티파이를 처리할 시퀀스 결정
    UAnimSequence* NotifySequence = CurrentPlayState.Sequence;

    // Sequence가 없지만 PoseProvider가 있는 경우 (예: BlendSpace1D)
    // PoseProvider에서 지배적 시퀀스를 가져옴
    if (!NotifySequence && CurrentPlayState.PoseProvider)
    {
        NotifySequence = CurrentPlayState.PoseProvider->GetDominantSequence();
    }

    if (!NotifySequence)
    {
        return;
    }

    // UAnimSequenceBase의 GetAnimNotify를 사용하여 노티파이 수집
    TArray<FPendingAnimNotify> PendingNotifies;

    // PoseProvider가 있으면 그 시간을 사용 (BlendSpace1D의 경우 내부 시간 추적 사용)
    // 그렇지 않으면 AnimInstance의 시간 사용
    float PrevTime = PreviousPlayTime;
    float DeltaMove = DeltaSeconds * CurrentPlayState.PlayRate;

    if (CurrentPlayState.PoseProvider && !CurrentPlayState.Sequence)
    {
        // BlendSpace 등의 경우 내부 시간 추적 사용
        PrevTime = CurrentPlayState.PoseProvider->GetPreviousPlayTime();
        // DeltaMove는 그대로 사용 (실제 시간 진행량)
    }

    NotifySequence->GetAnimNotify(PrevTime, DeltaMove, PendingNotifies);

    // 수집된 노티파이 처리
    for (const FPendingAnimNotify& Pending : PendingNotifies)
    {
        const FAnimNotifyEvent& Event = *Pending.Event;

        //UE_LOG("AnimNotify Triggered: %s at %.2f (Type: %d)",
        //    Event.NotifyName.ToString().c_str(), Event.TriggerTime, (int)Pending.Type);

        // Dispatch to notifies using the same policy as SkeletalMeshComponent
        if (OwningComponent)
        {
            switch (Pending.Type)
            {
            case EPendingNotifyType::Trigger:
                if (Event.Notify)
                {
                    Event.Notify->Notify(OwningComponent, NotifySequence);
                }
                break;
            case EPendingNotifyType::StateBegin:
                if (Event.NotifyState)
                {
                    Event.NotifyState->NotifyBegin(OwningComponent, NotifySequence, Event.Duration);
                }
                break;
            case EPendingNotifyType::StateTick:
                if (Event.NotifyState)
                {
                    Event.NotifyState->NotifyTick(OwningComponent, NotifySequence, Event.Duration);
                }
                break;
            case EPendingNotifyType::StateEnd:
                if (Event.NotifyState)
                {
                    Event.NotifyState->NotifyEnd(OwningComponent, NotifySequence, Event.Duration);
                }
                break;
            default:
                break;
            }
        }

        // TODO: 노티파이 타입별 처리
        // switch (Pending.Type)
        // {
        //     case EAnimNotifyEventType::Begin:
        //         // 노티파이 시작
        //         break;
        //     case EAnimNotifyEventType::End:
        //         // 노티파이 종료
        //         break;
        // }
    }

}

void UAnimInstance::UpdateAnimationCurves()
{
    if (!CurrentPlayState.Sequence)
    {
        return;
    }

    // TODO: 애니메이션 커브 구현
    // UAnimDataModel에 CurveData가 추가되면 아래와 같이 구현:
    /*
    UAnimDataModel* DataModel = CurrentPlayState.Sequence->GetDataModel();
    const FAnimationCurveData& CurveData = DataModel->GetCurveData();

    for (const auto& Curve : CurveData.Curves)
    {
        float CurveValue = Curve.Evaluate(CurrentPlayState.CurrentTime);
        // 커브 값을 컴포넌트나 다른 시스템에 전달
    }
    */
}

// ============================================================
// State Machine & Parameters
// ============================================================

void UAnimInstance::SetStateMachine(UAnimationStateMachine* InStateMachine)
{
    AnimStateMachine = InStateMachine;

    if (AnimStateMachine)
    {
        AnimStateMachine->Initialize(this);
        UE_LOG("AnimInstance: StateMachine set and initialized");
    }
}
void UAnimInstance::EvaluatePoseForState(const FAnimationPlayState& PlayState, TArray<FTransform>& OutPose, float DeltaTime) const
{
    OutPose.Empty();

    // PoseProvider가 있으면 그것을 사용 (BlendSpace 등)
    if (PlayState.PoseProvider)
    {
        const int32 NumBones = PlayState.PoseProvider->GetNumBoneTracks();
        OutPose.SetNum(NumBones);

        // const_cast 필요: EvaluatePose가 non-const (내부 상태 변경 가능)
        const_cast<IAnimPoseProvider*>(PlayState.PoseProvider)->EvaluatePose(
            PlayState.CurrentTime, DeltaTime, OutPose);
        return;
    }

    // 기존 방식: Sequence 직접 사용
    if (!PlayState.Sequence)
    {
        return;
    }

    UAnimDataModel* DataModel = PlayState.Sequence->GetDataModel();
    if (!DataModel)
    {
        return;
    }

    const int32 NumBones = DataModel->GetNumBoneTracks();
    OutPose.SetNum(NumBones);

    FAnimExtractContext ExtractContext(PlayState.CurrentTime, PlayState.bIsLooping);
    FPoseContext PoseContext(NumBones);
    PlayState.Sequence->GetAnimationPose(PoseContext, ExtractContext);

    OutPose = PoseContext.Pose;
}

void UAnimInstance::AdvancePlayState(FAnimationPlayState& PlayState, float DeltaSeconds)
{
    // PoseProvider 또는 Sequence가 있어야 재생 가능
    if (!PlayState.bIsPlaying)
    {
        return;
    }

    // PoseProvider가 없고 Sequence도 없으면 리턴
    if (!PlayState.PoseProvider && !PlayState.Sequence)
    {
        return;
    }

    PlayState.CurrentTime += DeltaSeconds * PlayState.PlayRate;

    // PlayLength는 PoseProvider 또는 Sequence에서 가져옴
    float PlayLength = 0.0f;
    if (PlayState.PoseProvider)
    {
        PlayLength = PlayState.PoseProvider->GetPlayLength();
    }
    else if (PlayState.Sequence)
    {
        PlayLength = PlayState.Sequence->GetPlayLength();
    }

    if (PlayLength <= 0.0f)
    {
        return;
    }

    if (PlayState.bIsLooping)
    {
        if (PlayState.CurrentTime >= PlayLength)
        {
            PlayState.CurrentTime = FMath::Fmod(PlayState.CurrentTime, PlayLength);
        }
    }
    else if (PlayState.CurrentTime >= PlayLength)
    {
        PlayState.CurrentTime = PlayLength;
        PlayState.bIsPlaying = false;
    }
}

void UAnimInstance::BlendPoseArrays(const TArray<FTransform>& FromPose, const TArray<FTransform>& ToPose, float Alpha, TArray<FTransform>& OutPose) const
{
    const int32 NumBones = FMath::Min(FromPose.Num(), ToPose.Num());
    if (NumBones == 0)
    {
        OutPose = FromPose;
        return;
    }

    const float ClampedAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
    OutPose.SetNum(NumBones);

    for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
    {
        const FTransform& From = FromPose[BoneIndex];
        const FTransform& To = ToPose[BoneIndex];

        FTransform Result;
        //Result.Lerp(From,To,ClampedAlpha);
        Result.Translation = FMath::Lerp(From.Translation, To.Translation, ClampedAlpha);
        Result.Scale3D = FMath::Lerp(From.Scale3D, To.Scale3D, ClampedAlpha);
        Result.Rotation = FQuat::Slerp(From.Rotation, To.Rotation, ClampedAlpha);
        Result.Rotation.Normalize();

        OutPose[BoneIndex] = Result;
    }
}

void UAnimInstance::GetPoseForLayer(int32 LayerIndex, TArray<FTransform>& OutPose,float DeltaSeconds)
{
    if (!Layers[LayerIndex].Sequence) return;

    // 시간 갱신
    AdvancePlayState(Layers[LayerIndex], DeltaSeconds);

    const bool bIsBlending = (BlendTimeRemaining > 0.0f && (BlendTargetState.Sequence != nullptr || BlendTargetState.PoseProvider != nullptr));

    if (bIsBlending)
    {
        AdvancePlayState(BlendTargetState, DeltaSeconds);

        const float SafeTotalTime = FMath::Max(BlendTotalTime, 1e-6f);
        float BlendAlpha = 1.0f - (BlendTimeRemaining / SafeTotalTime);
        BlendAlpha = FMath::Clamp(BlendAlpha, 0.0f, 1.0f);

        TArray<FTransform> FromPose;
        TArray<FTransform> TargetPose;
        EvaluatePoseForState(CurrentPlayState, FromPose, DeltaSeconds);
        EvaluatePoseForState(BlendTargetState, TargetPose, DeltaSeconds);

        TArray<FTransform> BlendedPose;
        BlendPoseArrays(FromPose, TargetPose, BlendAlpha, BlendedPose);

        if (OwningComponent && BlendedPose.Num() > 0)
        {
            // 포즈 크기가 스켈레톤과 다르면 본 이름으로 매핑
            UAnimSequence* MappingSeq = CurrentPlayState.Sequence;
            if (!MappingSeq && CurrentPlayState.PoseProvider)
                MappingSeq = CurrentPlayState.PoseProvider->GetDominantSequence();

            if (CurrentSkeleton && MappingSeq && BlendedPose.Num() != CurrentSkeleton->Bones.Num())
            {
                TArray<FTransform> MappedPose;
                MapPoseToSkeleton(BlendedPose, MappingSeq, MappedPose);
                OwningComponent->SetAnimationPose(MappedPose);
            }
            else
            {
                OwningComponent->SetAnimationPose(BlendedPose);
            }
        }

        BlendTimeRemaining = FMath::Max(BlendTimeRemaining - DeltaSeconds, 0.0f);
        if (BlendTimeRemaining <= 1e-4f)
        {
            CurrentPlayState = BlendTargetState;
            CurrentPlayState.BlendWeight = 1.0f;

            BlendTargetState = FAnimationPlayState();
            BlendTimeRemaining = 0.0f;
            BlendTotalTime = 0.0f;
        }
    }
    else if (OwningComponent)
    {
        TArray<FTransform> Pose;
        EvaluatePoseForState(CurrentPlayState, Pose, DeltaSeconds);

        if (Pose.Num() > 0)
        {
            // 포즈 크기가 스켈레톤과 다르면 본 이름으로 매핑
            UAnimSequence* MappingSeq = CurrentPlayState.Sequence;
            if (!MappingSeq && CurrentPlayState.PoseProvider)
                MappingSeq = CurrentPlayState.PoseProvider->GetDominantSequence();

            if (CurrentSkeleton && MappingSeq && Pose.Num() != CurrentSkeleton->Bones.Num())
            {
                TArray<FTransform> MappedPose;
                MapPoseToSkeleton(Pose, MappingSeq, MappedPose);
                OwningComponent->SetAnimationPose(MappedPose);
            }
            else
            {
                OwningComponent->SetAnimationPose(Pose);
            }
        }
    }

}

// ============================================================
// Montage API
// ============================================================

float UAnimInstance::PlayMontage(UAnimMontage* Montage, float PlayRate)
{
    if (!Montage || !Montage->HasSections())
    {
        UE_LOG("UAnimInstance::PlayMontage - Invalid montage or no sections");
        return 0.0f;
    }

    MontageState->Montage = Montage;
    MontageState->Position = 0.0f;
    MontageState->PlayRate = PlayRate;
    MontageState->Weight = 0.0f;
    MontageState->bPlaying = true;
    MontageState->bBlendingOut = false;
    MontageState->BlendTime = 0.0f;
    MontageState->CurrentSectionIndex = 0;

    // 섹션 블렌딩 상태 리셋
    MontageState->bBlendingSection = false;
    MontageState->SectionBlendTime = 0.0f;
    MontageState->SectionBlendTotalTime = 0.0f;
    MontageState->PreviousSectionIndex = -1;
    MontageState->PreviousSectionEndTime = 0.0f;

    // 노티파이 트래킹 초기화
    PreviousMontagePlayTime = 0.0f;

    float Duration = Montage->GetPlayLength() / PlayRate;

    UE_LOG("UAnimInstance::PlayMontage - Playing montage (BlendIn: %.2f, BlendOut: %.2f, Duration: %.2f)",
        Montage->BlendInTime, Montage->BlendOutTime, Duration);

    return Duration;
}

void UAnimInstance::StopMontage(float BlendOutTime)
{
    if (!MontageState || !MontageState->bPlaying || MontageState->bBlendingOut)
    {
        return;
    }

    MontageState->bBlendingOut = true;
    MontageState->BlendTime = 0.0f;

    // -1이면 몽타주 기본값 사용
    if (BlendOutTime >= 0.0f && MontageState->Montage)
    {
        MontageState->Montage->BlendOutTime = BlendOutTime;
    }

    UE_LOG("UAnimInstance::StopMontage - Stopping montage with blend out: %.2f",
        MontageState->Montage ? MontageState->Montage->BlendOutTime : BlendOutTime);
}

UAnimMontage* UAnimInstance::GetCurrentMontage() const
{
    if (MontageState && MontageState->bPlaying)
    {
        return MontageState->Montage;
    }
    return nullptr;
}

bool UAnimInstance::IsPlayingMontage() const
{
    return MontageState && MontageState->bPlaying;
}

bool UAnimInstance::JumpToSection(const FString& SectionName)
{
    if (!MontageState || !MontageState->bPlaying || !MontageState->Montage)
    {
        return false;
    }

    int32 Index = MontageState->Montage->FindSectionIndex(SectionName);
    if (Index < 0)
    {
        UE_LOG("UAnimInstance::JumpToSection - Section not found: %s", SectionName.c_str());
        return false;
    }

    MontageState->CurrentSectionIndex = Index;
    MontageState->Position = 0.0f;
    PreviousMontagePlayTime = 0.0f;

    UE_LOG("UAnimInstance::JumpToSection - Jumped to section: %s (index %d)", SectionName.c_str(), Index);
    return true;
}

bool UAnimInstance::JumpToNextSection()
{
    if (!MontageState || !MontageState->bPlaying || !MontageState->Montage)
    {
        return false;
    }

    int32 NextIndex = MontageState->CurrentSectionIndex + 1;
    if (NextIndex >= MontageState->Montage->GetNumSections())
    {
        return false;
    }

    MontageState->CurrentSectionIndex = NextIndex;
    MontageState->Position = 0.0f;
    PreviousMontagePlayTime = 0.0f;

    UE_LOG("UAnimInstance::JumpToNextSection - Jumped to section index %d", NextIndex);
    return true;
}

int32 UAnimInstance::GetCurrentSectionIndex() const
{
    if (MontageState && MontageState->bPlaying)
    {
        return MontageState->CurrentSectionIndex;
    }
    return -1;
}

float UAnimInstance::GetMontagePosition() const
{
    if (MontageState && MontageState->bPlaying && MontageState->Montage)
    {
        // 전체 몽타주 위치 계산 (이전 섹션들 길이 + 현재 섹션 내 위치)
        float TotalPos = 0.0f;
        UAnimMontage* M = MontageState->Montage;
        for (int32 i = 0; i < MontageState->CurrentSectionIndex; ++i)
        {
            UAnimSequence* Seq = M->GetSectionSequence(i);
            TotalPos += Seq ? Seq->GetPlayLength() : 0.0f;
        }
        return TotalPos + MontageState->Position;
    }
    return 0.0f;
}

void UAnimInstance::UpdateMontage(float DeltaTime)
{
    if (!MontageState || !MontageState->bPlaying || !MontageState->Montage)
    {
        return;
    }

    UAnimMontage* M = MontageState->Montage;

    // 블렌드 인 처리
    if (!MontageState->bBlendingOut && MontageState->Weight < 1.0f)
    {
        MontageState->BlendTime += DeltaTime;
        if (M->BlendInTime > 0.0f)
        {
            MontageState->Weight = FMath::Clamp(MontageState->BlendTime / M->BlendInTime, 0.0f, 1.0f);
        }
        else
        {
            MontageState->Weight = 1.0f;
        }
    }

    // 블렌드 아웃 처리
    if (MontageState->bBlendingOut)
    {
        MontageState->BlendTime += DeltaTime;
        if (M->BlendOutTime > 0.0f)
        {
            MontageState->Weight = 1.0f - FMath::Clamp(MontageState->BlendTime / M->BlendOutTime, 0.0f, 1.0f);
        }
        else
        {
            MontageState->Weight = 0.0f;
        }

        if (MontageState->Weight <= 0.0f)
        {
            MontageState->bPlaying = false;
            MontageState->Montage = nullptr;
            UE_LOG("UAnimInstance::UpdateMontage - Montage finished");
            return;
        }
    }

    // 노티파이 트리거 (시간 진행 전에)
    TriggerMontageNotifies(DeltaTime);

    // 이전 시간 저장
    PreviousMontagePlayTime = MontageState->Position;

    // 현재 섹션 시퀀스 가져오기
    UAnimSequence* CurrentSeq = M->GetSectionSequence(MontageState->CurrentSectionIndex);

    // 섹션별 재생 속도 적용 (몽타주 PlayRate * 섹션 PlayRate)
    float SectionPlayRate = 1.0f;
    if (M->HasSections() && MontageState->CurrentSectionIndex < M->GetNumSections())
    {
        SectionPlayRate = M->Sections[MontageState->CurrentSectionIndex].PlayRate;
    }
    float EffectivePlayRate = MontageState->PlayRate * SectionPlayRate;

    // 시간 진행
    MontageState->Position += DeltaTime * EffectivePlayRate;
    float Length = CurrentSeq ? CurrentSeq->GetPlayLength() : M->GetPlayLength();

    // 섹션 블렌딩 진행
    if (MontageState->bBlendingSection)
    {
        MontageState->SectionBlendTime += DeltaTime;
        if (MontageState->SectionBlendTime >= MontageState->SectionBlendTotalTime)
        {
            MontageState->bBlendingSection = false;
            MontageState->PreviousSectionIndex = -1;
        }
    }

    if (MontageState->Position >= Length)
    {
        // 섹션이 있으면 다음 섹션으로 자동 진행
        if (M->HasSections() && MontageState->CurrentSectionIndex + 1 < M->GetNumSections())
        {
            int32 NextSection = MontageState->CurrentSectionIndex + 1;
            float NextBlendTime = M->Sections[NextSection].BlendInTime;

            // 블렌딩 시작
            if (NextBlendTime > 0.0f)
            {
                MontageState->bBlendingSection = true;
                MontageState->SectionBlendTime = 0.0f;
                MontageState->SectionBlendTotalTime = NextBlendTime;
                MontageState->PreviousSectionIndex = MontageState->CurrentSectionIndex;
                MontageState->PreviousSectionEndTime = Length;
            }

            MontageState->CurrentSectionIndex = NextSection;
            MontageState->Position = 0.0f;
            PreviousMontagePlayTime = 0.0f;
            UE_LOG("UAnimInstance::UpdateMontage - Section %d -> %d (Blend: %.2f)", MontageState->PreviousSectionIndex, NextSection, NextBlendTime);
        }
        else if (M->bLoop)
        {
            MontageState->Position = FMath::Fmod(MontageState->Position, Length);
            if (M->HasSections())
            {
                MontageState->CurrentSectionIndex = 0;
            }
        }
        else
        {
            if (!MontageState->bBlendingOut)
            {
                MontageState->bBlendingOut = true;
                MontageState->BlendTime = 0.0f;
                UE_LOG("UAnimInstance::UpdateMontage - Montage finished");
            }
        }
    }
}

void UAnimInstance::TriggerMontageNotifies(float DeltaSeconds)
{
    if (!MontageState || !MontageState->bPlaying || !MontageState->Montage)
    {
        return;
    }

    UAnimMontage* Montage = MontageState->Montage;

    // 몽타주 자체 노티파이 수집 (UAnimSequenceBase 상속)
    TArray<FPendingAnimNotify> PendingNotifies;
    float DeltaMove = DeltaSeconds * MontageState->PlayRate;
    Montage->GetAnimNotify(PreviousMontagePlayTime, DeltaMove, PendingNotifies);

    if (PendingNotifies.Num() == 0)
    {
        return;
    }

    UAnimSequence* CurrentSeq = Montage->GetSectionSequence(MontageState->CurrentSectionIndex);

    // 노티파이 처리
    for (const FPendingAnimNotify& Pending : PendingNotifies)
    {
        const FAnimNotifyEvent& Event = *Pending.Event;

        //UE_LOG("Montage Notify Triggered: %s at %.2f",
        //    Event.NotifyName.ToString().c_str(), Event.TriggerTime);

        if (OwningComponent)
        {
            switch (Pending.Type)
            {
            case EPendingNotifyType::Trigger:
                if (Event.Notify)
                {
                    Event.Notify->Notify(OwningComponent, CurrentSeq);
                }
                break;
            case EPendingNotifyType::StateBegin:
                if (Event.NotifyState)
                {
                    Event.NotifyState->NotifyBegin(OwningComponent, CurrentSeq, Event.Duration);
                }
                break;
            case EPendingNotifyType::StateTick:
                if (Event.NotifyState)
                {
                    Event.NotifyState->NotifyTick(OwningComponent, CurrentSeq, Event.Duration);
                }
                break;
            case EPendingNotifyType::StateEnd:
                if (Event.NotifyState)
                {
                    Event.NotifyState->NotifyEnd(OwningComponent, CurrentSeq, Event.Duration);
                }
                break;
            default:
                break;
            }
        }
    }
}

// ============================================================
// Bone Name Mapping Helpers
// ============================================================

void UAnimInstance::MapPoseToSkeleton(const TArray<FTransform>& InPose, UAnimSequence* InSequence, TArray<FTransform>& OutPose) const
{
    if (!CurrentSkeleton || !InSequence)
    {
        OutPose = InPose;
        return;
    }

    UAnimDataModel* DataModel = InSequence->GetDataModel();
    if (!DataModel)
    {
        OutPose = InPose;
        return;
    }

    const int32 NumSkeletonBones = CurrentSkeleton->Bones.Num();
    const TArray<FBoneAnimationTrack>& Tracks = DataModel->GetBoneAnimationTracks();

    // 스켈레톤 본 개수로 초기화 (Identity로, 애니메이션이 덮어씀)
    OutPose.SetNum(NumSkeletonBones);
    for (int32 i = 0; i < NumSkeletonBones; ++i)
    {
        OutPose[i] = FTransform();
    }

    // 애니메이션 트랙을 본 이름으로 매핑
    for (int32 TrackIdx = 0; TrackIdx < Tracks.Num() && TrackIdx < InPose.Num(); ++TrackIdx)
    {
        int32 BoneIdx = CurrentSkeleton->FindBoneIndex(Tracks[TrackIdx].Name);
        if (BoneIdx != INDEX_NONE && BoneIdx < NumSkeletonBones)
        {
            OutPose[BoneIdx] = InPose[TrackIdx];
        }
    }
}

void UAnimInstance::BlendPosesByBoneName(
    const TArray<FTransform>& FromPose, UAnimSequence* FromSeq,
    const TArray<FTransform>& ToPose, UAnimSequence* ToSeq,
    float Alpha, TArray<FTransform>& OutPose) const
{
    if (!CurrentSkeleton)
    {
        BlendPoseArrays(FromPose, ToPose, Alpha, OutPose);
        return;
    }

    const int32 NumSkeletonBones = CurrentSkeleton->Bones.Num();
    const float ClampedAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);

    // 스켈레톤 본 개수로 초기화 (Identity로, 애니메이션이 덮어씀)
    OutPose.SetNum(NumSkeletonBones);
    for (int32 i = 0; i < NumSkeletonBones; ++i)
    {
        OutPose[i] = FTransform();
    }

    // From 애니메이션 트랙 정보
    UAnimDataModel* FromModel = FromSeq ? FromSeq->GetDataModel() : nullptr;
    const TArray<FBoneAnimationTrack>* FromTracks = FromModel ? &FromModel->GetBoneAnimationTracks() : nullptr;

    // To 애니메이션 트랙 정보
    UAnimDataModel* ToModel = ToSeq ? ToSeq->GetDataModel() : nullptr;
    const TArray<FBoneAnimationTrack>* ToTracks = ToModel ? &ToModel->GetBoneAnimationTracks() : nullptr;

    // 각 스켈레톤 본에 대해 블렌딩
    for (int32 BoneIdx = 0; BoneIdx < NumSkeletonBones; ++BoneIdx)
    {
        const FString& BoneName = CurrentSkeleton->Bones[BoneIdx].Name;
        FTransform FromTransform = FTransform();
        FTransform ToTransform = FTransform();
        bool bFoundFrom = false;
        bool bFoundTo = false;

        // From 애니메이션에서 본 찾기
        if (FromTracks)
        {
            for (int32 t = 0; t < FromTracks->Num() && t < FromPose.Num(); ++t)
            {
                if ((*FromTracks)[t].Name.ToString() == BoneName)
                {
                    FromTransform = FromPose[t];
                    bFoundFrom = true;
                    break;
                }
            }
        }

        // To 애니메이션에서 본 찾기
        if (ToTracks)
        {
            for (int32 t = 0; t < ToTracks->Num() && t < ToPose.Num(); ++t)
            {
                if ((*ToTracks)[t].Name.ToString() == BoneName)
                {
                    ToTransform = ToPose[t];
                    bFoundTo = true;
                    break;
                }
            }
        }

        // 블렌딩
        if (bFoundFrom && bFoundTo)
        {
            OutPose[BoneIdx].Translation = FMath::Lerp(FromTransform.Translation, ToTransform.Translation, ClampedAlpha);
            OutPose[BoneIdx].Rotation = FQuat::Slerp(FromTransform.Rotation, ToTransform.Rotation, ClampedAlpha);
            OutPose[BoneIdx].Rotation.Normalize();
            OutPose[BoneIdx].Scale3D = FMath::Lerp(FromTransform.Scale3D, ToTransform.Scale3D, ClampedAlpha);
        }
        else if (bFoundTo)
        {
            OutPose[BoneIdx] = ToTransform;
        }
        else if (bFoundFrom)
        {
            OutPose[BoneIdx] = FromTransform;
        }
        // 둘 다 없으면 바인드 포즈 유지
    }
}
