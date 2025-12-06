#pragma once

#include "Vector.h"
#include <unordered_map>
#include <vector>

class ASkeletalMeshActor;
class USkeletalMeshComponent;
class UClothComponent;
class FViewport;
class FViewportClient;
class UWorld;
class USkeletalMesh;
class URenderer;
struct FMeshData;

/**
 * @brief Weight Paint 브러시 모드
 */
enum class EWeightPaintMode : uint8
{
    Paint,      // 가중치 페인팅
    Erase,      // 지우기 (1.0으로 설정)
    Smooth,     // 주변 값과 평균화
    Fill        // 전체 채우기
};

/**
 * @brief Weight Paint 에디터 상태 관리 클래스
 *
 * Cloth Weight Paint 기능을 위한 상태와 브러시 파라미터를 관리합니다.
 */
class FWeightPaintState
{
public:
    FWeightPaintState();
    ~FWeightPaintState();

    // ============================================
    // 뷰포트 및 월드
    // ============================================
    UWorld* World = nullptr;
    FViewport* Viewport = nullptr;
    FViewportClient* Client = nullptr;

    // ============================================
    // 프리뷰 액터
    // ============================================
    ASkeletalMeshActor* PreviewActor = nullptr;
    USkeletalMesh* CurrentMesh = nullptr;
    UClothComponent* ClothComponent = nullptr;  // Cloth Paint 대상
    FString LoadedMeshPath;

    // ============================================
    // 브러시 파라미터
    // ============================================

    // 브러시 반경 (월드 유닛, 1-100 cm)
    float BrushRadius = 5.0f;

    // 브러시 강도 (0.0 ~ 1.0)
    float BrushStrength = 0.5f;

    // 브러시 폴오프 (0.0 = 하드 엣지, 1.0 = 소프트)
    float BrushFalloff = 0.5f;

    // 페인트 값 (0.0 = 고정, 1.0 = 자유)
    float PaintValue = 0.0f;

    // 현재 브러시 모드
    EWeightPaintMode BrushMode = EWeightPaintMode::Paint;

    // ============================================
    // 버텍스 가중치 데이터
    // ============================================

    // Cloth 버텍스 인덱스 -> 가중치 (0.0 = 고정, 1.0 = 자유)
    std::unordered_map<uint32, float> ClothVertexWeights;

    // 현재 호버 중인 버텍스 인덱스 (-1 = 없음)
    int32 HoveredVertexIndex = -1;

    // 현재 브러시 중심 위치 (월드 좌표)
    FVector BrushCenterWorld = FVector::Zero();

    // 브러시가 유효한 위치에 있는지
    bool bBrushValid = false;

    // ============================================
    // 시각화 옵션
    // ============================================

    // 가중치 시각화 표시 여부
    bool bShowWeightVisualization = true;

    // 메쉬 표시 여부
    bool bShowMesh = true;

    // 본 표시 여부
    bool bShowBones = false;

    // 와이어프레임 오버레이
    bool bShowWireframe = false;

    // ============================================
    // UI 상태
    // ============================================

    // 마우스 버튼 상태
    bool bIsMouseDown = false;
    bool bIsMouseRightDown = false;

    // 마지막 마우스 위치
    FVector2D LastMousePos = FVector2D(0.0f, 0.0f);

    // ============================================
    // 메서드
    // ============================================

    /**
     * @brief 버텍스 가중치 가져오기
     * @param VertexIndex 버텍스 인덱스
     * @return 가중치 값 (없으면 1.0 반환 - 기본값은 자유)
     */
    float GetVertexWeight(uint32 VertexIndex) const;

    /**
     * @brief 버텍스 가중치 설정
     * @param VertexIndex 버텍스 인덱스
     * @param Weight 가중치 값 (0.0 ~ 1.0)
     */
    void SetVertexWeight(uint32 VertexIndex, float Weight);

    /**
     * @brief 브러시 적용
     * @param BrushCenter 브러시 중심 위치 (월드 좌표)
     * @param VertexPositions 버텍스 위치 배열
     * @param VertexCount 버텍스 개수
     */
    void ApplyBrush(const FVector& BrushCenter, const FVector* VertexPositions, uint32 VertexCount);

    /**
     * @brief 모든 가중치 초기화
     * @param DefaultWeight 기본 가중치 값
     */
    void ResetAllWeights(float DefaultWeight = 1.0f);

    /**
     * @brief 가중치를 색상으로 변환 (시각화용)
     * @param Weight 가중치 값
     * @return RGBA 색상 (Red = 0.0/고정, Green = 1.0/자유)
     */
    static FVector4 WeightToColor(float Weight);

    /**
     * @brief 레이와 버텍스 교차 검사
     * @param RayOrigin 레이 시작점
     * @param RayDirection 레이 방향
     * @param VertexPositions 버텍스 위치 배열
     * @param VertexCount 버텍스 개수
     * @param OutVertexIndex 충돌한 버텍스 인덱스 (출력)
     * @param OutDistance 충돌 거리 (출력)
     * @return 충돌 여부
     */
    bool PickVertex(const FVector& RayOrigin, const FVector& RayDirection,
                    const FVector* VertexPositions, uint32 VertexCount,
                    int32& OutVertexIndex, float& OutDistance) const;

    /**
     * @brief Weight 시각화 그리기 (Week13 PAE 방식)
     * @param Renderer 렌더러
     */
    void DrawClothWeights(URenderer* Renderer) const;

private:
    /**
     * @brief 폴오프 계산
     * @param Distance 중심으로부터의 거리
     * @param Radius 브러시 반경
     * @param Falloff 폴오프 값
     * @return 폴오프 팩터 (0.0 ~ 1.0)
     */
    float CalculateFalloff(float Distance, float Radius, float Falloff) const;
};
