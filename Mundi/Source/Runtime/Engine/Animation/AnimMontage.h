#pragma once
#include "AnimSequenceBase.h"

class UAnimSequence;

/**
 * @brief 몽타주 런타임 재생 상태
 */
struct FMontagePlayState
{
    class UAnimMontage* Montage = nullptr;

    float Position = 0.0f;       // 현재 재생 위치
    float PlayRate = 1.0f;       // 재생 속도
    float Weight = 0.0f;         // 블렌드 가중치 (0~1)

    bool bPlaying = false;
    bool bBlendingOut = false;

    float BlendTime = 0.0f;      // 현재 블렌드 진행 시간

    void Reset()
    {
        Montage = nullptr;
        Position = 0.0f;
        PlayRate = 1.0f;
        Weight = 0.0f;
        bPlaying = false;
        bBlendingOut = false;
        BlendTime = 0.0f;
    }
};

/**
 * @brief 애니메이션 몽타주
 *
 * 기존 애니메이션 위에 오버레이로 재생되는 일회성 애니메이션.
 * 블렌드 인/아웃을 통해 부드럽게 전환됩니다.
 *
 * @example 사용법:
 *
 * // 1. 몽타주 생성
 * UAnimMontage* AttackMontage = NewObject<UAnimMontage>();
 * AttackMontage->Sequence = AttackAnimSequence;
 * AttackMontage->BlendInTime = 0.1f;
 * AttackMontage->BlendOutTime = 0.2f;
 *
 * // 2. 재생
 * float Duration = AnimInstance->PlayMontage(AttackMontage, 1.0f);
 *
 * // 3. 인터럽트 (필요시)
 * AnimInstance->StopMontage(0.1f);
 *
 * // 4. 상태 체크
 * if (!AnimInstance->IsPlayingMontage())
 * {
 *     // 다음 행동 가능
 * }
 */
class UAnimMontage : public UAnimSequenceBase
{
    DECLARE_CLASS(UAnimMontage, UAnimSequenceBase)

public:
    UAnimMontage() = default;
    virtual ~UAnimMontage() = default;

    /** 재생할 애니메이션 시퀀스 */
    UAnimSequence* Sequence = nullptr;

    /** 블렌드 인 시간 (초) */
    float BlendInTime = 0.2f;

    /** 블렌드 아웃 시간 (초) */
    float BlendOutTime = 0.2f;

    /** 루프 여부 */
    bool bLoop = false;

    /** 재생 길이 반환 */
    virtual float GetPlayLength() const override;
};
