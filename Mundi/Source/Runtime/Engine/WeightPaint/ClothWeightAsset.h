#pragma once
#include "ResourceBase.h"
#include "Name.h"
#include <unordered_map>

/**
 * @brief Cloth Weight 에셋 클래스
 *
 * Weight Paint 에디터에서 페인팅한 버텍스 가중치를 저장하고
 * ClothComponent에서 로드하여 NvCloth 시뮬레이션에 적용합니다.
 *
 * 가중치 의미:
 * - 0.0 = 완전 고정 (invMass = 0, 시뮬레이션에서 움직이지 않음)
 * - 1.0 = 완전 자유 (invMass = DefaultInvMass, 시뮬레이션에 완전히 참여)
 * - 0.0 ~ 1.0 = 부분적 제약 (invMass 비례 적용)
 */
class UClothWeightAsset : public UResourceBase
{
    DECLARE_CLASS(UClothWeightAsset, UResourceBase)

public:
    UClothWeightAsset() = default;
    virtual ~UClothWeightAsset() = default;

    // ============================================
    // 에셋 정보
    // ============================================

    void SetAssetName(const FName& InName) { AssetName = InName; }
    FName GetAssetName() const { return AssetName; }

    /**
     * @brief 연결된 SkeletalMesh의 경로 설정
     * Weight 에셋은 특정 메시에 종속되므로 메시 경로를 저장
     */
    void SetSkeletalMeshPath(const FString& InPath) { SkeletalMeshPath = InPath; }
    const FString& GetSkeletalMeshPath() const { return SkeletalMeshPath; }

    // ============================================
    // 버텍스 가중치 관리
    // ============================================

    /**
     * @brief 버텍스 가중치 설정
     * @param VertexIndex Cloth 버텍스 인덱스
     * @param Weight 가중치 (0.0 ~ 1.0)
     */
    void SetVertexWeight(uint32 VertexIndex, float Weight);

    /**
     * @brief 버텍스 가중치 가져오기
     * @param VertexIndex Cloth 버텍스 인덱스
     * @return 가중치 (기본값: 1.0 = 자유)
     */
    float GetVertexWeight(uint32 VertexIndex) const;

    /**
     * @brief 전체 가중치 맵 가져오기
     */
    const std::unordered_map<uint32, float>& GetVertexWeights() const { return VertexWeights; }

    /**
     * @brief 외부 맵에서 가중치 로드
     */
    void LoadFromMap(const std::unordered_map<uint32, float>& InWeights);

    /**
     * @brief 모든 가중치 초기화
     * @param DefaultWeight 기본값 (1.0 = 자유)
     */
    void ClearWeights(float DefaultWeight = 1.0f);

    /**
     * @brief 저장된 버텍스 수 반환
     */
    int32 GetVertexCount() const { return static_cast<int32>(VertexWeights.size()); }

    // ============================================
    // 에셋 직렬화
    // ============================================

    /**
     * @brief 파일에서 에셋 로드
     */
    bool Load(const FString& InFilePath, ID3D11Device* InDevice = nullptr);

    /**
     * @brief 에셋을 파일로 저장
     */
    bool SaveToFile(const FString& FilePath);

    /**
     * @brief JSON 직렬화/역직렬화
     */
    void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

private:
    // 에셋 이름
    FName AssetName;

    // 연결된 SkeletalMesh 경로
    FString SkeletalMeshPath;

    // 버텍스 인덱스 -> 가중치 맵
    // 저장되지 않은 버텍스는 기본값 1.0 (자유)
    std::unordered_map<uint32, float> VertexWeights;
};
