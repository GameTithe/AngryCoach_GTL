#pragma once
#include "AnimSequenceBase.h"

class UAnimSequence;

/**
 * @brief 몽타주 섹션
 */
struct FMontageSection
{
    FString Name;
    UAnimSequence* Sequence = nullptr;
    float BlendInTime = 0.1f;
    float PlayRate = 1.0f;  // 섹션별 재생 속도

    FMontageSection() = default;
    FMontageSection(const FString& InName, UAnimSequence* InSeq, float InBlendIn = 0.1f, float InPlayRate = 1.0f)
        : Name(InName), Sequence(InSeq), BlendInTime(InBlendIn), PlayRate(InPlayRate) {}
};

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
    int32 CurrentSectionIndex = 0;  // 현재 섹션 인덱스

    // 섹션 블렌딩
    bool bBlendingSection = false;
    float SectionBlendTime = 0.0f;
    float SectionBlendTotalTime = 0.0f;
    int32 PreviousSectionIndex = -1;
    float PreviousSectionEndTime = 0.0f;  // 이전 섹션 마지막 시간

    void Reset()
    {
        Montage = nullptr;
        Position = 0.0f;
        PlayRate = 1.0f;
        Weight = 0.0f;
        bPlaying = false;
        bBlendingOut = false;
        BlendTime = 0.0f;
        CurrentSectionIndex = 0;
        bBlendingSection = false;
        SectionBlendTime = 0.0f;
        SectionBlendTotalTime = 0.0f;
        PreviousSectionIndex = -1;
        PreviousSectionEndTime = 0.0f;
    }
};

/**
 * @brief 애니메이션 몽타주
 *
 * 기존 애니메이션 위에 오버레이로 재생되는 일회성 애니메이션.
 * 블렌드 인/아웃을 통해 부드럽게 전환됩니다.
 *
 * @example 사용법 1: 팩토리 함수로 생성
 *
 * UAnimSequence* AttackSeq = RESOURCE.Get<UAnimSequence>("Data/Attack.fbx_Attack");
 * UAnimMontage* AttackMontage = UAnimMontage::Create(AttackSeq, 0.1f, 0.2f);
 * AnimInstance->PlayMontage(AttackMontage, 1.0f);
 *
 *
 * @example 사용법 2: JSON 파일로 저장/로드
 *
 * // 저장 (에디터에서 한 번)
 * UAnimMontage* Montage = UAnimMontage::Create(AttackSeq, 0.1f, 0.2f);
 * Montage->Save("Data/Montages/Attack.montage.json");
 *
 * // 로드 (런타임에서)
 * UAnimMontage* LoadedMontage = NewObject<UAnimMontage>();
 * LoadedMontage->Load("Data/Montages/Attack.montage.json");
 * AnimInstance->PlayMontage(LoadedMontage, 1.0f);
 *
 *
 * @example 사용법 3: 콤보 공격
 *
 * TArray<UAnimMontage*> ComboMontages; // 미리 로드해둠
 * int32 ComboIndex = 0;
 *
 * void OnAttackInput()
 * {
 *     if (!AnimInstance->IsPlayingMontage())
 *     {
 *         ComboIndex = 0;
 *     }
 *     AnimInstance->PlayMontage(ComboMontages[ComboIndex], 1.0f);
 *     ComboIndex = (ComboIndex + 1) % ComboMontages.Num();
 * }
 */
class UAnimMontage : public UAnimSequenceBase
{
    DECLARE_CLASS(UAnimMontage, UAnimSequenceBase)

public:
    UAnimMontage() = default;
    virtual ~UAnimMontage() = default;

    /** 블렌드 인 시간 (초) */
    float BlendInTime = 0.2f;

    /** 블렌드 아웃 시간 (초) */
    float BlendOutTime = 0.2f;

    /** 루프 여부 */
    bool bLoop = false;

    /** 전체 재생 속도 (기본 1.0) */
    float PlayRate = 1.0f;

    /** 섹션 목록 */
    TArray<FMontageSection> Sections;

    /** 재생 길이 반환 */
    virtual float GetPlayLength() const override;

    // ============================================================
    // Section API
    // ============================================================

    /** 섹션 추가 */
    void AddSection(const FString& Name, UAnimSequence* Seq, float BlendIn = 0.1f);

    /** 섹션 인덱스로 시퀀스 가져오기 */
    UAnimSequence* GetSectionSequence(int32 Index) const;

    /** 섹션 이름으로 인덱스 찾기 */
    int32 FindSectionIndex(const FString& Name) const;

    /** 섹션 개수 */
    int32 GetNumSections() const { return Sections.Num(); }

    /** 섹션이 있는지 확인 */
    bool HasSections() const { return Sections.Num() > 0; }

    // ============================================================
    // Serialization (Save/Load)
    // ============================================================

    /**
     * @brief 몽타주를 JSON 파일로 저장
     * @param FilePath 저장할 파일 경로 (예: "Data/Montages/Attack.montage.json")
     * @return 성공 여부
     */
    bool Save(const FString& FilePath) const;

    /**
     * @brief JSON 파일에서 몽타주 로드
     * @param FilePath 로드할 파일 경로
     * @return 성공 여부
     */
    bool Load(const FString& FilePath);

    /**
     * @brief ResourceManager 호환 로드 (Device 파라미터 무시)
     */
    bool Load(const FString& FilePath, struct ID3D11Device* Device) { return Load(FilePath); }

    /**
     * @brief 팩토리 함수 - 몽타주 생성
     * @param InSequence 재생할 시퀀스
     * @param InBlendInTime 블렌드 인 시간
     * @param InBlendOutTime 블렌드 아웃 시간
     * @param InbLoop 루프 여부
     * @return 생성된 몽타주
     */
    static UAnimMontage* Create(UAnimSequence* InSequence, float InBlendInTime = 0.2f, float InBlendOutTime = 0.2f, bool InbLoop = false);
};
