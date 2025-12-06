#include "pch.h"
#include "WeightPaintViewportClient.h"
#include "WeightPaintState.h"
#include "Source/Runtime/Renderer/FViewport.h"
#include "Source/Runtime/Renderer/Renderer.h"
#include "RenderManager.h"
#include "CameraActor.h"
#include "CameraComponent.h"
#include "World.h"
#include "SkeletalMeshActor.h"
#include "SkeletalMeshComponent.h"
#include "ClothComponent.h"
#include "SceneView.h"
#include "RenderSettings.h"
#include <cfloat>

FWeightPaintViewportClient::FWeightPaintViewportClient()
{
    // Perspective 카메라로 기본 설정
    ViewportType = EViewportType::Perspective;
    ViewMode = EViewMode::VMI_Lit_Phong;

    // 초기 카메라 위치 설정 (정면에서 바라보기)
    if (Camera)
    {
        Camera->SetActorLocation(FVector(200.0f, 0.0f, 75.0f));
        Camera->SetActorRotation(FVector(0.0f, 0.0f, 180.0f));
        Camera->SetCameraPitch(0.0f);
        Camera->SetCameraYaw(180.0f);
    }
}

FWeightPaintViewportClient::~FWeightPaintViewportClient()
{
}

void FWeightPaintViewportClient::Draw(FViewport* Viewport)
{
    if (!Viewport || !World)
    {
        return;
    }

    URenderer* Renderer = URenderManager::GetInstance().GetRenderer();
    if (!Renderer)
    {
        return;
    }

    // 카메라 설정 (Perspective 모드)
    Camera->GetCameraComponent()->SetProjectionMode(ECameraProjectionMode::Perspective);

    // SceneView 생성
    FSceneView RenderView(Camera->GetCameraComponent(), Viewport, &World->GetRenderSettings());

    // 뷰모드 설정
    World->GetRenderSettings().SetViewMode(ViewMode);

    // 씬 렌더링 수행
    Renderer->RenderSceneForView(World, &RenderView, Viewport);

    // Weight 시각화 및 브러시 프리뷰 렌더링 (씬 렌더링 후)
    if (PaintState)
    {
        // Weight 시각화
        if (PaintState->bShowWeightVisualization && PaintState->PreviewActor)
        {
            PaintState->DrawClothWeights(Renderer);
        }

        // 브러시 프리뷰
        DrawBrushPreview(Renderer);
    }
}

void FWeightPaintViewportClient::Tick(float DeltaTime)
{
    FViewportClient::Tick(DeltaTime);
}

void FWeightPaintViewportClient::MouseMove(FViewport* Viewport, int32 X, int32 Y)
{
    if (!PaintState)
    {
        FViewportClient::MouseMove(Viewport, X, Y);
        return;
    }

    // 마우스 위치 저장
    PaintState->LastMousePos = FVector2D(static_cast<float>(X), static_cast<float>(Y));

    // 페인팅 중이면 브러시 적용 (bIsPainting 상태만 확인 - 부모 클래스 상태와 독립적)
    if (bIsPainting)
    {
        // 버텍스 피킹 수행
        PerformVertexPicking(Viewport, X, Y);

        // 브러시가 유효하면 적용
        if (PaintState->bBrushValid && PaintState->PreviewActor)
        {
            ApplyBrushAtPosition(PaintState->BrushCenterWorld);
        }
        // 버텍스 피킹 실패시에도 마지막 유효한 위치 사용
        else if (bLastBrushValid && PaintState->PreviewActor)
        {
            ApplyBrushAtPosition(LastValidBrushPosition);
        }

        // 유효한 브러시 위치 캐시
        if (PaintState->bBrushValid)
        {
            LastValidBrushPosition = PaintState->BrushCenterWorld;
            bLastBrushValid = true;
        }
    }
    else if (PaintState->bIsMouseRightDown)
    {
        // 우클릭 드래그: 카메라 조작
        FViewportClient::MouseMove(Viewport, X, Y);
    }
    else
    {
        // 호버 상태에서 버텍스 피킹 (브러시 프리뷰용)
        PerformVertexPicking(Viewport, X, Y);
    }
}

void FWeightPaintViewportClient::MouseButtonDown(FViewport* Viewport, int32 X, int32 Y, int32 Button)
{
    if (!PaintState)
    {
        FViewportClient::MouseButtonDown(Viewport, X, Y, Button);
        return;
    }

    if (Button == 0) // Left button - Paint
    {
        PaintState->bIsMouseDown = true;
        bIsPainting = true;

        // 즉시 브러시 적용
        PerformVertexPicking(Viewport, X, Y);
        if (PaintState->bBrushValid)
        {
            ApplyBrushAtPosition(PaintState->BrushCenterWorld);
        }
    }
    else if (Button == 1) // Right button - Camera
    {
        PaintState->bIsMouseRightDown = true;
        FViewportClient::MouseButtonDown(Viewport, X, Y, Button);
    }
    else
    {
        FViewportClient::MouseButtonDown(Viewport, X, Y, Button);
    }
}

void FWeightPaintViewportClient::MouseButtonUp(FViewport* Viewport, int32 X, int32 Y, int32 Button)
{
    if (!PaintState)
    {
        FViewportClient::MouseButtonUp(Viewport, X, Y, Button);
        return;
    }

    if (Button == 0) // Left button
    {
        PaintState->bIsMouseDown = false;
        bIsPainting = false;
    }
    else if (Button == 1) // Right button
    {
        PaintState->bIsMouseRightDown = false;
        FViewportClient::MouseButtonUp(Viewport, X, Y, Button);
    }
    else
    {
        FViewportClient::MouseButtonUp(Viewport, X, Y, Button);
    }
}

void FWeightPaintViewportClient::MouseWheel(float DeltaSeconds)
{
    FViewportClient::MouseWheel(DeltaSeconds);
}

bool FWeightPaintViewportClient::MakeRayFromViewport(FViewport* Viewport, int32 ScreenX, int32 ScreenY,
                                                      FVector& OutRayOrigin, FVector& OutRayDirection)
{
    if (!Viewport || !Camera)
        return false;

    // 뷰포트 크기
    float ViewportWidth = static_cast<float>(Viewport->GetSizeX());
    float ViewportHeight = static_cast<float>(Viewport->GetSizeY());

    if (ViewportWidth <= 0 || ViewportHeight <= 0)
        return false;

    // 스크린 좌표를 NDC로 변환 (-1 ~ 1)
    float NdcX = (2.0f * ScreenX / ViewportWidth) - 1.0f;
    float NdcY = 1.0f - (2.0f * ScreenY / ViewportHeight); // Y 반전

    // 카메라 정보
    FVector CameraPos = Camera->GetActorLocation();
    float FOV = PerspectiveCameraFov;
    float AspectRatio = ViewportWidth / ViewportHeight;

    // 뷰 매트릭스
    FMatrix ViewMatrix = GetViewMatrix();
    FMatrix InvViewMatrix = ViewMatrix.Inverse();

    // Projection matrix inverse 계산
    float TanHalfFOV = std::tan(FOV * 0.5f * 3.14159265f / 180.0f);

    // 카메라 공간에서의 레이 방향
    FVector RayDirCamera;
    RayDirCamera.X = NdcX * TanHalfFOV * AspectRatio;
    RayDirCamera.Y = NdcY * TanHalfFOV;
    RayDirCamera.Z = 1.0f; // Forward

    // 월드 공간으로 변환
    FVector RayDirWorld = InvViewMatrix.TransformVector(RayDirCamera);
    RayDirWorld.Normalize();

    OutRayOrigin = CameraPos;
    OutRayDirection = RayDirWorld;

    return true;
}

void FWeightPaintViewportClient::DrawWeightVisualization(URenderer* Renderer)
{
    if (!PaintState || !PaintState->bShowWeightVisualization || !Renderer)
        return;

    // TODO: ClothComponent에서 버텍스 데이터를 가져와서 시각화
    // 현재는 SkeletalMesh의 버텍스를 사용

    if (!PaintState->PreviewActor)
        return;

    // 메쉬 버텍스를 라인으로 시각화 (임시)
    // 실제 구현에서는 Primitive Batch 또는 별도 셰이더 사용
}

void FWeightPaintViewportClient::DrawBrushPreview(URenderer* Renderer)
{
    if (!PaintState || !Renderer)
        return;

    if (!PaintState->bBrushValid)
        return;

    // 브러시 원 그리기 (와이어프레임)
    const int32 NumSegments = 32;
    const float AngleStep = 2.0f * 3.14159265f / NumSegments;
    const float Radius = PaintState->BrushRadius;
    const FVector Center = PaintState->BrushCenterWorld;

    // 브러시 색상 (페인트 모드에 따라)
    FVector4 BrushColor;
    switch (PaintState->BrushMode)
    {
    case EWeightPaintMode::Paint:
        BrushColor = FVector4(1.0f, 0.5f, 0.0f, 1.0f); // Orange
        break;
    case EWeightPaintMode::Erase:
        BrushColor = FVector4(0.0f, 0.5f, 1.0f, 1.0f); // Blue
        break;
    case EWeightPaintMode::Smooth:
        BrushColor = FVector4(0.5f, 1.0f, 0.5f, 1.0f); // Light Green
        break;
    case EWeightPaintMode::Fill:
        BrushColor = FVector4(1.0f, 1.0f, 0.0f, 1.0f); // Yellow
        break;
    }

    Renderer->BeginLineBatch();

    // 카메라를 향하는 평면에 원 그리기
    FVector CameraPos = Camera ? Camera->GetActorLocation() : FVector::Zero();
    FVector ToCamera = (CameraPos - Center);
    ToCamera.Normalize();

    // 원을 그리기 위한 직교 벡터 계산
    FVector WorldUp(0.0f, 0.0f, 1.0f);
    FVector Right = FVector::Cross(WorldUp, ToCamera);
    if (Right.SizeSquared() < 0.001f)
        Right = FVector(0.0f, 1.0f, 0.0f);
    Right.Normalize();

    FVector Up = FVector::Cross(ToCamera, Right);
    Up.Normalize();

    for (int32 i = 0; i < NumSegments; ++i)
    {
        float Angle1 = i * AngleStep;
        float Angle2 = (i + 1) * AngleStep;

        FVector P1 = Center + (Right * std::cos(Angle1) + Up * std::sin(Angle1)) * Radius;
        FVector P2 = Center + (Right * std::cos(Angle2) + Up * std::sin(Angle2)) * Radius;

        Renderer->AddLine(P1, P2, BrushColor);
    }

    // 내부 폴오프 원 (옵션)
    float InnerRadius = Radius * (1.0f - PaintState->BrushFalloff);
    if (InnerRadius > 0.1f)
    {
        FVector4 InnerColor = BrushColor;
        InnerColor.W = 0.5f; // 반투명

        for (int32 i = 0; i < NumSegments; ++i)
        {
            float Angle1 = i * AngleStep;
            float Angle2 = (i + 1) * AngleStep;

            FVector P1 = Center + (Right * std::cos(Angle1) + Up * std::sin(Angle1)) * InnerRadius;
            FVector P2 = Center + (Right * std::cos(Angle2) + Up * std::sin(Angle2)) * InnerRadius;

            Renderer->AddLine(P1, P2, InnerColor);
        }
    }

    Renderer->EndLineBatch(FMatrix::Identity());
}

void FWeightPaintViewportClient::PerformVertexPicking(FViewport* Viewport, int32 X, int32 Y)
{
    if (!PaintState || !PaintState->PreviewActor)
    {
        PaintState->bBrushValid = false;
        return;
    }

    // 레이 생성
    FVector RayOrigin, RayDirection;
    if (!MakeRayFromViewport(Viewport, X, Y, RayOrigin, RayDirection))
    {
        PaintState->bBrushValid = false;
        return;
    }

    CachedRayOrigin = RayOrigin;
    CachedRayDirection = RayDirection;
    bRayValid = true;

    // ClothComponent에서 메시 데이터 가져오기
    UClothComponent* ClothComp = PaintState->ClothComponent;
    if (!ClothComp)
    {
        PaintState->bBrushValid = false;
        return;
    }

    int32 VertexCount = ClothComp->GetClothVertexCount();
    const TArray<uint32>& Indices = ClothComp->GetClothIndices();

    if (VertexCount == 0 || Indices.Num() < 3)
    {
        PaintState->bBrushValid = false;
        return;
    }

    // 버텍스 위치 가져오기
    TArray<FVector> Positions;
    Positions.SetNum(VertexCount);
    for (int32 i = 0; i < VertexCount; ++i)
    {
        Positions[i] = ClothComp->GetClothVertexPosition(i);
    }

    // 레이-트라이앵글 교차 검사 (Möller–Trumbore 알고리즘)
    float ClosestT = FLT_MAX;
    FVector ClosestHitPoint;
    bool bHit = false;

    const float EPSILON = 0.0000001f;
    int32 TriangleCount = Indices.Num() / 3;

    for (int32 i = 0; i < TriangleCount; ++i)
    {
        uint32 i0 = Indices[i * 3 + 0];
        uint32 i1 = Indices[i * 3 + 1];
        uint32 i2 = Indices[i * 3 + 2];

        if (i0 >= static_cast<uint32>(VertexCount) ||
            i1 >= static_cast<uint32>(VertexCount) ||
            i2 >= static_cast<uint32>(VertexCount))
            continue;

        const FVector& V0 = Positions[i0];
        const FVector& V1 = Positions[i1];
        const FVector& V2 = Positions[i2];

        FVector Edge1 = V1 - V0;
        FVector Edge2 = V2 - V0;
        FVector H = FVector::Cross(RayDirection, Edge2);
        float A = FVector::Dot(Edge1, H);

        // 레이가 삼각형과 평행한지 확인
        if (A > -EPSILON && A < EPSILON)
            continue;

        float F = 1.0f / A;
        FVector S = RayOrigin - V0;
        float U = F * FVector::Dot(S, H);

        if (U < 0.0f || U > 1.0f)
            continue;

        FVector Q = FVector::Cross(S, Edge1);
        float V = F * FVector::Dot(RayDirection, Q);

        if (V < 0.0f || U + V > 1.0f)
            continue;

        // t 계산 (레이 파라미터)
        float T = F * FVector::Dot(Edge2, Q);

        if (T > EPSILON && T < ClosestT)
        {
            ClosestT = T;
            ClosestHitPoint = RayOrigin + RayDirection * T;
            bHit = true;
        }
    }

    if (bHit)
    {
        PaintState->BrushCenterWorld = ClosestHitPoint;
        PaintState->bBrushValid = true;
    }
    else
    {
        // 메시와 교차하지 않으면 가장 가까운 버텍스 기반 fallback
        float MinDist = FLT_MAX;
        int32 ClosestVertexIndex = -1;

        for (int32 i = 0; i < VertexCount; ++i)
        {
            // 레이에서 버텍스까지의 최단 거리 계산
            FVector ToVertex = Positions[i] - RayOrigin;
            float Projection = FVector::Dot(ToVertex, RayDirection);

            if (Projection <= 0.0f)
                continue;

            FVector ClosestPointOnRay = RayOrigin + RayDirection * Projection;
            float Distance = (Positions[i] - ClosestPointOnRay).Size();

            if (Distance < MinDist)
            {
                MinDist = Distance;
                ClosestVertexIndex = i;
            }
        }

        // 합리적인 거리 내에 버텍스가 있으면 그 위치 사용
        if (ClosestVertexIndex >= 0 && MinDist < PaintState->BrushRadius * 2.0f)
        {
            PaintState->BrushCenterWorld = Positions[ClosestVertexIndex];
            PaintState->bBrushValid = true;
        }
        else
        {
            PaintState->bBrushValid = false;
        }
    }
}

void FWeightPaintViewportClient::ApplyBrushAtPosition(const FVector& WorldPosition)
{
    if (!PaintState)
        return;

    UClothComponent* ClothComp = PaintState->ClothComponent;
    if (!ClothComp)
        return;

    int32 VertexCount = ClothComp->GetClothVertexCount();
    if (VertexCount == 0)
        return;

    // ClothComponent에서 버텍스 위치 가져오기
    TArray<FVector> Positions;
    Positions.SetNum(VertexCount);
    for (int32 i = 0; i < VertexCount; ++i)
    {
        Positions[i] = ClothComp->GetClothVertexPosition(i);
    }

    // 브러시 적용 (WeightPaintState에서 처리)
    PaintState->ApplyBrush(WorldPosition, Positions.GetData(), VertexCount);

    // ClothComponent에 가중치 동기화
    for (const auto& Pair : PaintState->ClothVertexWeights)
    {
        ClothComp->SetVertexWeight(Pair.first, Pair.second);
    }

    // NvCloth 파티클에 weight 적용 (invMass 업데이트)
    ClothComp->ApplyPaintedWeights();

    // Weight 시각화 업데이트 (버텍스 색상 갱신)
    ClothComp->UpdateVertexColorsFromWeights();
}
