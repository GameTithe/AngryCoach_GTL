#include "pch.h"
#include "WeightPaintState.h"
#include "ClothComponent.h"
#include "Renderer.h"
#include "VertexData.h"
#include "SkeletalMeshActor.h"
#include "SkeletalMeshComponent.h"
#include "SkeletalMesh.h"
#include <algorithm>
#include <cmath>

FWeightPaintState::FWeightPaintState()
{
}

FWeightPaintState::~FWeightPaintState()
{
}

float FWeightPaintState::GetVertexWeight(uint32 VertexIndex) const
{
    auto it = ClothVertexWeights.find(VertexIndex);
    if (it != ClothVertexWeights.end())
    {
        return it->second;
    }
    // 기본값: 1.0 (자유 - 시뮬레이션에 참여)
    return 1.0f;
}

void FWeightPaintState::SetVertexWeight(uint32 VertexIndex, float Weight)
{
    // 0.0 ~ 1.0 범위로 클램프
    Weight = std::clamp(Weight, 0.0f, 1.0f);
    ClothVertexWeights[VertexIndex] = Weight;
}

void FWeightPaintState::ApplyBrush(const FVector& BrushCenter, const FVector* VertexPositions, uint32 VertexCount)
{
    if (!VertexPositions || VertexCount == 0)
        return;

    for (uint32 i = 0; i < VertexCount; ++i)
    {
        FVector VertexPos = VertexPositions[i];
        float Distance = (VertexPos - BrushCenter).Size();

        if (Distance <= BrushRadius)
        {
            // 폴오프 계산
            float FalloffFactor = CalculateFalloff(Distance, BrushRadius, BrushFalloff);

            // 적용 강도 계산
            float ApplyStrength = BrushStrength * FalloffFactor;

            // 현재 가중치 가져오기
            float CurrentWeight = GetVertexWeight(i);

            // 브러시 모드에 따른 새 가중치 계산
            float NewWeight = CurrentWeight;

            switch (BrushMode)
            {
            case EWeightPaintMode::Paint:
                // 페인트: 목표 값으로 보간
                NewWeight = CurrentWeight + (PaintValue - CurrentWeight) * ApplyStrength;
                break;

            case EWeightPaintMode::Erase:
                // 지우기: 1.0 (자유)로 보간
                NewWeight = CurrentWeight + (1.0f - CurrentWeight) * ApplyStrength;
                break;

            case EWeightPaintMode::Smooth:
                // 스무스: 주변 값과 평균화 (간단 구현 - 추후 개선 가능)
                // 현재는 0.5로 수렴
                NewWeight = CurrentWeight + (0.5f - CurrentWeight) * ApplyStrength * 0.5f;
                break;

            case EWeightPaintMode::Fill:
                // 채우기: 즉시 목표 값으로 설정
                NewWeight = PaintValue;
                break;
            }

            SetVertexWeight(i, NewWeight);
        }
    }
}

void FWeightPaintState::ResetAllWeights(float DefaultWeight)
{
    ClothVertexWeights.clear();

    // 필요시 모든 버텍스에 기본값 설정
    // 현재는 GetVertexWeight에서 기본값 반환하므로 clear만 해도 됨
}

FVector4 FWeightPaintState::WeightToColor(float Weight)
{
    // 가중치에 따른 색상 그라데이션
    // 0.0 (고정) = Red
    // 0.5 (중간) = Yellow
    // 1.0 (자유) = Green

    float Alpha = 0.6f; // 60% 불투명도

    if (Weight < 0.5f)
    {
        // Red -> Yellow (0.0 ~ 0.5)
        float t = Weight * 2.0f; // 0 ~ 1 매핑
        return FVector4(1.0f, t, 0.0f, Alpha);
    }
    else
    {
        // Yellow -> Green (0.5 ~ 1.0)
        float t = (Weight - 0.5f) * 2.0f; // 0 ~ 1 매핑
        return FVector4(1.0f - t, 1.0f, 0.0f, Alpha);
    }
}

bool FWeightPaintState::PickVertex(const FVector& RayOrigin, const FVector& RayDirection,
                                    const FVector* VertexPositions, uint32 VertexCount,
                                    int32& OutVertexIndex, float& OutDistance) const
{
    if (!VertexPositions || VertexCount == 0)
        return false;

    // 피킹 반경: 브러시 반경의 절반
    float PickRadius = BrushRadius * 0.5f;
    float ClosestT = FLT_MAX;
    int32 HitVertexIndex = -1;

    for (uint32 i = 0; i < VertexCount; ++i)
    {
        FVector VertexPos = VertexPositions[i];

        // Ray-Sphere 교차 검사
        FVector L = VertexPos - RayOrigin;
        float tca = FVector::Dot(L, RayDirection);

        if (tca < 0.0f)
            continue; // 버텍스가 레이 뒤에 있음

        float d2 = FVector::Dot(L, L) - tca * tca;
        float r2 = PickRadius * PickRadius;

        if (d2 > r2)
            continue; // 레이가 버텍스 구를 지나치지 않음

        float thc = std::sqrt(r2 - d2);
        float t = tca - thc;

        if (t < 0.0f)
            t = tca + thc; // 레이 시작점이 구 내부에 있음

        if (t > 0.0f && t < ClosestT)
        {
            ClosestT = t;
            HitVertexIndex = static_cast<int32>(i);
        }
    }

    if (HitVertexIndex >= 0)
    {
        OutVertexIndex = HitVertexIndex;
        OutDistance = ClosestT;
        return true;
    }

    return false;
}

float FWeightPaintState::CalculateFalloff(float Distance, float Radius, float Falloff) const
{
    if (Radius <= 0.0f)
        return 0.0f;

    // 정규화된 거리 (0 ~ 1)
    float NormalizedDist = Distance / Radius;

    // 폴오프 적용
    // Falloff = 0: 하드 엣지 (전체 영역에 동일한 강도)
    // Falloff = 1: 소프트 엣지 (중심에서 멀어질수록 약해짐)
    float FalloffFactor = 1.0f - (NormalizedDist * Falloff);

    return std::clamp(FalloffFactor, 0.0f, 1.0f);
}

void FWeightPaintState::DrawClothWeights(URenderer* Renderer) const
{
    if (!Renderer || !bShowWeightVisualization)
    {
        return;
    }

    // Week13 패턴: PreviewActor에서 SkeletalMeshComponent 가져오기
    if (!PreviewActor)
    {
        UE_LOG("[WeightPaintState] DrawClothWeights: PreviewActor is NULL");
        return;
    }

    USkeletalMeshComponent* SkelComp = PreviewActor->GetSkeletalMeshComponent();
    if (!SkelComp)
    {
        UE_LOG("[WeightPaintState] DrawClothWeights: SkelComp is NULL");
        return;
    }

    USkeletalMesh* SkelMesh = SkelComp->GetSkeletalMesh();
    if (!SkelMesh || !SkelMesh->GetSkeletalMeshData())
    {
        UE_LOG("[WeightPaintState] DrawClothWeights: SkelMesh or MeshData is NULL");
        return;
    }

    const FSkeletalMeshData* MeshData = SkelMesh->GetSkeletalMeshData();
    const TArray<FSkinnedVertex>& Vertices = MeshData->Vertices;
    const TArray<uint32>& Indices = MeshData->Indices;

    if (Vertices.Num() == 0 || Indices.Num() < 3)
    {
        UE_LOG("[WeightPaintState] DrawClothWeights: No vertices or indices");
        return;
    }

    // SkeletalMeshComponent의 월드 변환
    FTransform WorldTransform = SkelComp->GetWorldTransform();

    // Weight에 따른 색상 계산 람다 (반투명하게)
    auto GetWeightColor = [](float Weight) -> FVector4 {
        // 0(Red/고정) -> 0.5(Yellow) -> 1(Green/자유)
        float Alpha = 0.6f;  // 반투명
        if (Weight < 0.5f)
        {
            float t = Weight * 2.0f;
            return FVector4(1.0f, t, 0.0f, Alpha);
        }
        else
        {
            float t = (Weight - 0.5f) * 2.0f;
            return FVector4(1.0f - t, 1.0f, 0.0f, Alpha);
        }
    };

    // FMeshData로 삼각형 메시 생성
    FMeshData ClothMeshData;

    // 삼각형 단위로 순회 (3개씩)
    uint32 OutputVertexIndex = 0;
    for (int32 i = 0; i + 2 < Indices.Num(); i += 3)
    {
        uint32 Idx0 = Indices[i];
        uint32 Idx1 = Indices[i + 1];
        uint32 Idx2 = Indices[i + 2];

        if (Idx0 >= (uint32)Vertices.Num() || Idx1 >= (uint32)Vertices.Num() || Idx2 >= (uint32)Vertices.Num())
            continue;

        // 버텍스 위치 (월드 좌표로 변환)
        FVector P0 = WorldTransform.TransformPosition(Vertices[Idx0].Position);
        FVector P1 = WorldTransform.TransformPosition(Vertices[Idx1].Position);
        FVector P2 = WorldTransform.TransformPosition(Vertices[Idx2].Position);

        // 노멀 계산 (면 노멀)
        FVector Edge1 = P1 - P0;
        FVector Edge2 = P2 - P0;
        FVector Normal = FVector::Cross(Edge1, Edge2).GetSafeNormal();

        // 각 버텍스의 Weight 가져오기
        // 메시 버텍스 인덱스 → Cloth 파티클 인덱스로 변환 후 조회
        float W0 = 1.0f, W1 = 1.0f, W2 = 1.0f;

        if (ClothComponent)
        {
            // ClothComponent의 매핑을 사용하여 올바른 Cloth 파티클 인덱스 획득
            int32 ClothIdx0 = ClothComponent->FindClothVertexByMeshVertex(Idx0);
            int32 ClothIdx1 = ClothComponent->FindClothVertexByMeshVertex(Idx1);
            int32 ClothIdx2 = ClothComponent->FindClothVertexByMeshVertex(Idx2);

            if (ClothIdx0 >= 0)
            {
                auto It0 = ClothVertexWeights.find(static_cast<uint32>(ClothIdx0));
                if (It0 != ClothVertexWeights.end()) W0 = It0->second;
            }
            if (ClothIdx1 >= 0)
            {
                auto It1 = ClothVertexWeights.find(static_cast<uint32>(ClothIdx1));
                if (It1 != ClothVertexWeights.end()) W1 = It1->second;
            }
            if (ClothIdx2 >= 0)
            {
                auto It2 = ClothVertexWeights.find(static_cast<uint32>(ClothIdx2));
                if (It2 != ClothVertexWeights.end()) W2 = It2->second;
            }
        }
        else
        {
            // ClothComponent가 없으면 기존 방식 (fallback)
            auto It0 = ClothVertexWeights.find(Idx0);
            auto It1 = ClothVertexWeights.find(Idx1);
            auto It2 = ClothVertexWeights.find(Idx2);
            if (It0 != ClothVertexWeights.end()) W0 = It0->second;
            if (It1 != ClothVertexWeights.end()) W1 = It1->second;
            if (It2 != ClothVertexWeights.end()) W2 = It2->second;
        }

        // 각 버텍스의 색상
        FVector4 C0 = GetWeightColor(W0);
        FVector4 C1 = GetWeightColor(W1);
        FVector4 C2 = GetWeightColor(W2);

        // 삼각형 버텍스 추가
        ClothMeshData.Vertices.Add(P0);
        ClothMeshData.Vertices.Add(P1);
        ClothMeshData.Vertices.Add(P2);

        ClothMeshData.Color.Add(C0);
        ClothMeshData.Color.Add(C1);
        ClothMeshData.Color.Add(C2);

        ClothMeshData.Normal.Add(Normal);
        ClothMeshData.Normal.Add(Normal);
        ClothMeshData.Normal.Add(Normal);

        ClothMeshData.UV.Add(FVector2D(0, 0));
        ClothMeshData.UV.Add(FVector2D(1, 0));
        ClothMeshData.UV.Add(FVector2D(0.5f, 1));

        // 인덱스 추가
        ClothMeshData.Indices.Add(OutputVertexIndex);
        ClothMeshData.Indices.Add(OutputVertexIndex + 1);
        ClothMeshData.Indices.Add(OutputVertexIndex + 2);

        OutputVertexIndex += 3;
    }

    if (ClothMeshData.Vertices.Num() > 0)
    {
        Renderer->BeginPrimitiveBatch();
        Renderer->AddPrimitiveData(ClothMeshData, FMatrix::Identity());  // 이미 월드 좌표로 변환됨
        Renderer->EndPrimitiveBatch();
    }
}
