#include "pch.h"
#include "Collision.h"
#include "AABB.h"
#include "OBB.h"
#include "BoundingSphere.h"
#include "ShapeComponent.h"
#include "Source/Runtime/Engine/Particle/Async/ParticleSimulationContext.h"

namespace Collision
{
    bool Intersects(const FAABB& Aabb, const FOBB& Obb)
    {
        const FOBB ConvertedOBB(Aabb, FMatrix::Identity());
        return ConvertedOBB.Intersects(Obb);
    }

    bool Intersects(const FAABB& Aabb, const FBoundingSphere& Sphere)
    {
		// Real Time Rendering 4th, 22.13.2 Sphere/Box Intersection
        float Dist2 = 0.0f;

        for (int32 i = 0; i < 3; ++i)
        {
            if (Sphere.Center[i] < Aabb.Min[i])
            {
                float Delta = Sphere.Center[i] - Aabb.Min[i];
                if (Delta < -Sphere.GetRadius())
					return false;
				Dist2 += Delta * Delta;
            }
            else if (Sphere.Center[i] > Aabb.Max[i])
            {
                float Delta = Sphere.Center[i] - Aabb.Max[i];
                if (Delta > Sphere.GetRadius())
                    return false;
                Dist2 += Delta * Delta;
            }
        }
        return Dist2 <= (Sphere.Radius * Sphere.Radius);
	}

    bool Intersects(const FOBB& Obb, const FBoundingSphere& Sphere)
    {
        // Real Time Rendering 4th, 22.13.2 Sphere/Box Intersection
        float Dist2 = 0.0f;
        // OBB의 로컬 좌표계로 구 중심점 변환
        FVector LocalCenter = Obb.Center;
        for (int32 i = 0; i < 3; ++i)
        {
            FVector Axis = Obb.Axes[i];
            float Offset = FVector::Dot(Sphere.Center - Obb.Center, Axis);
            if (Offset > Obb.HalfExtent[i])
                Offset = Obb.HalfExtent[i];
            else if (Offset < -Obb.HalfExtent[i])
                Offset = -Obb.HalfExtent[i];
            LocalCenter += Axis * Offset;
        }
        for (int i = 0; i < 3; ++i)
        {
            if (LocalCenter[i] < -Obb.HalfExtent[i])
            {
                float Delta = LocalCenter[i] + Obb.HalfExtent[i];
                if (Delta < -Sphere.GetRadius())
                    return false;
                Dist2 += Delta * Delta;
            }
            else if (LocalCenter[i] > Obb.HalfExtent[i])
            {
                float Delta = LocalCenter[i] - Obb.HalfExtent[i];
                if (Delta > Sphere.GetRadius())
                    return false;
                Dist2 += Delta * Delta;
            }
        }
        return Dist2 <= (Sphere.Radius * Sphere.Radius);
	}
     
    bool OverlapSphereAndSphere(const FShape& ShapeA, const FTransform& TransformA, const FShape& ShapeB, const FTransform& TransformB)
    {
        FVector Dist = TransformA.Translation - TransformB.Translation;
        float SumRadius = ShapeA.Sphere.SphereRadius + ShapeB.Sphere.SphereRadius;

        return Dist.SizeSquared() <= SumRadius * SumRadius;
    }
    
    FVector AbsVec(const FVector& v)
    {
        return FVector(std::fabs(v.X), std::fabs(v.Y), std::fabs(v.Z));
    }

    void BuildOBB(const FShape& BoxShape, const FTransform& T, FOBB& Out)
    {
        Out.Center = T.Translation;

        const FMatrix R = T.Rotation.ToMatrix();

        // Sclae + Rotation 행렬 곱하고, Normalize
        Out.Axes[0] = FVector(R.M[0][0], R.M[0][1], R.M[0][2]).GetSafeNormal();
        Out.Axes[1] = FVector(R.M[1][0], R.M[1][1], R.M[1][2]).GetSafeNormal();
        Out.Axes[2] = FVector(R.M[2][0], R.M[2][1], R.M[2][2]).GetSafeNormal();


        // half-extent에 abs 스케일 적용
        const FVector S = AbsVec(T.Scale3D);
        const FVector HalfExtent = BoxShape.Box.BoxExtent;
        Out.HalfExtent[0] = HalfExtent.X * S.X;
        Out.HalfExtent[1] = HalfExtent.Y * S.Y;
        Out.HalfExtent[2] = HalfExtent.Z * S.Z;
    }
    
    bool Overlap_OBB_OBB(const FOBB& A, const FOBB& B)
    {
        constexpr float EPS = 1e-6f;

        auto ProjectRadius = [](const FOBB& box, const FVector& axisUnit) -> float
            {
                // box의 세 축을 axisUnit에 투영했을 때의 합
                return  box.HalfExtent[0] * FMath::Abs(FVector::Dot(box.Axes[0], axisUnit))
                    + box.HalfExtent[1] * FMath::Abs(FVector::Dot(box.Axes[1], axisUnit))
                    + box.HalfExtent[2] * FMath::Abs(FVector::Dot(box.Axes[2], axisUnit));
            };

        auto SeparatedOnAxis = [&](const FVector& axis) -> bool
            {
                // 축이 너무 짧으면(평행·중복) 건너뜀
                const float len2 = axis.SizeSquared();
                if (len2 < EPS) return false;

                const FVector n = axis / FMath::Sqrt(len2);          // 단위축
                const float dist = FMath::Abs(FVector::Dot(B.Center - A.Center, n));
                const float ra = ProjectRadius(A, n);
                const float rb = ProjectRadius(B, n);
                return dist > (ra + rb);                             // 분리되면 true
            };

        // 1) A의 세 면 법선
        if (SeparatedOnAxis(A.Axes[0])) return false;
        if (SeparatedOnAxis(A.Axes[1])) return false;
        if (SeparatedOnAxis(A.Axes[2])) return false;

        // 2) B의 세 면 법선
        if (SeparatedOnAxis(B.Axes[0])) return false;
        if (SeparatedOnAxis(B.Axes[1])) return false;
        if (SeparatedOnAxis(B.Axes[2])) return false;

        // 3) 9개 교차축 A x B
        for (int i = 0; i < 3; ++i)
        {
            for (int j = 0; j < 3; ++j)
            {
                if (SeparatedOnAxis(FVector::Cross(A.Axes[i], B.Axes[j])))
                    return false;
            }
        }

        return true;
    }

    float UniformScaleMax(const FVector& S)
    {
        return FMath::Max(FMath::Max(std::fabs(S.X), std::fabs(S.Y)), std::fabs(S.Z));
    }
     
    // 캡슐 obb의 상단, 하단 중점을 반환 + 반지름 반환 
    void BuildCapsule(const FShape& CapsuleShape, const FTransform& TransformCapsule, FVector& OutP0, FVector& OutP1, float& OutRadius)
    {
        const float CapsuleRadius = CapsuleShape.Capsule.CapsuleRadius;
        const float CapsuleHalfHeight = CapsuleShape.Capsule.CapsuleHalfHeight;

        const FVector S = AbsVec(TransformCapsule.Scale3D);
        const float HeightScale = std::fabs(S.Z); 
        const float RadiusScale = FMath::Max(std::fabs(S.X), std::fabs(S.Y));

        const float RadiusWorld = CapsuleRadius * RadiusScale;
        const float HalfHeightWorld = FMath::Max(0.0f, CapsuleHalfHeight - CapsuleRadius) * HeightScale;

        const FVector AxisWorld = TransformCapsule.Rotation.RotateVector(FVector(0, 0, 1)).GetSafeNormal();
        const FVector Center = TransformCapsule.Translation;

        OutP0 = Center - AxisWorld * HalfHeightWorld;
        OutP1 = Center + AxisWorld * HalfHeightWorld;
        OutRadius = RadiusWorld;
    }

    // 캡슐의 가운데 OBB 반환 
    void BuildCapsuleCoreOBB(const FShape& CapsuleShape, const FTransform& Transform, FOBB& Out)
    {
        const float CapsuleRadius = CapsuleShape.Capsule.CapsuleRadius;
        const float HalfHeight= CapsuleShape.Capsule.CapsuleHalfHeight;

        const FVector S = AbsVec(Transform.Scale3D);
        const float AxisScale = std::fabs(S.Z);
        const float RadiusScale = FMath::Max(std::fabs(S.X), std::fabs(S.Y));

        const float RadiusWorld = CapsuleRadius * RadiusScale;
        const float HalfHeightLocal = FMath::Max(0.0f, HalfHeight- CapsuleRadius);
        const float HalfHeightWorld = HalfHeightLocal* AxisScale;

        Out.Center = Transform.Translation;
        const FMatrix R = Transform.Rotation.ToMatrix();
        Out.Axes[0] = FVector(R.M[0][0], R.M[0][1], R.M[0][2]).GetSafeNormal();
        Out.Axes[1] = FVector(R.M[1][0], R.M[1][1], R.M[1][2]).GetSafeNormal();
        Out.Axes[2] = FVector(R.M[2][0], R.M[2][1], R.M[2][2]).GetSafeNormal();

        Out.HalfExtent[0] = RadiusWorld;
        Out.HalfExtent[1] = RadiusWorld;
        Out.HalfExtent[2] = HalfHeightWorld;
    }
     
    // 캡슐 VS Sphere
    bool OverlapCapsuleAndSphere(const FShape& Capsule, const FTransform& TransformCapsule,
        const FShape& Sphere, const FTransform& TransformSphere)
    {
        // 캡슐 코어 obb생성
        FOBB Core{};
        BuildCapsuleCoreOBB(Capsule, TransformCapsule, Core);

        // 캡슐 상하단 중점 + Radius 생성
        FVector TopCenter, BottomCenter; float rCapsule = 0.0f;
        BuildCapsule(Capsule, TransformCapsule, BottomCenter, TopCenter, rCapsule);  

        const float SphereRadius = Sphere.Sphere.SphereRadius * UniformScaleMax(AbsVec(TransformSphere.Scale3D));
        const FVector C = TransformSphere.Translation;

        // 1) Sphere vs OBB
        if (Overlap_Sphere_OBB(C, SphereRadius, Core))
            return true;

        // 2) Sphere vs top/bottom Spheres
        auto SphereSphere = [](const FVector& Pos0, float Radius0, const FVector& Pos1, float Radius1) -> bool
        {
            const FVector d = Pos0 - Pos1; 
            const float rs = Radius0 + Radius1; 
            
            return d.SizeSquared() <= rs * rs;
        };

        if (SphereSphere(C, SphereRadius, TopCenter, rCapsule)) return true;
        if (SphereSphere(C, SphereRadius, BottomCenter, rCapsule)) return true;

        return false;
    }

    // 캡슐 VS Box 
    bool OverlapCapsuleAndBox(const FShape& Capsule, const FTransform& TransformCapsule,
        const FShape& Box, const FTransform& TransformBox)
    { 
        // 캡슐 몸통 부분
        FOBB Core{};
        BuildCapsuleCoreOBB(Capsule, TransformCapsule, Core);

        // 비교 대상 OBB 
        FOBB B{};
        BuildOBB(Box, TransformBox, B);

        // 1) 캡슐 몸통 vs OBB
        if (Overlap_OBB_OBB(Core, B))
            return true;

        // 2) Top/Bottom 반구 vs OBB 
        FVector BottomCenter, TopCenter; float CapsuleRadius = 0.0f;
        BuildCapsule(Capsule, TransformCapsule, BottomCenter, TopCenter, CapsuleRadius);
        if (Overlap_Sphere_OBB(TopCenter, CapsuleRadius, B)) return true;
        if (Overlap_Sphere_OBB(BottomCenter, CapsuleRadius, B)) return true;

        return false;
    }

    // Capsuel - Box 
    // 캡슐은 구 - OBB - 구로 구성되었다고 가정했다. 
    bool OverlapBoxAndCapsule(const FShape& Box, const FTransform& TransformBox,
        const FShape& Capsule, const FTransform& TransformCapsule)
    {
        return OverlapCapsuleAndBox(Capsule, TransformCapsule, Box, TransformBox);
    }
    
    // Capsule - Capsule
    // 캡슐은 구 - OBB - 구로 구성되었다고 가정했다. 
    bool OverlapCapsuleAndCapsule(const FShape& CapsuleA, const FTransform& TransformA,
        const FShape& CapsuleB, const FTransform& TransformB)
    {
        FOBB CoreA{}, CoreB{}; 
        BuildCapsuleCoreOBB(CapsuleA, TransformA, CoreA);
        BuildCapsuleCoreOBB(CapsuleB, TransformB, CoreB);
        
        FVector ABottom, ATop; 
        float RadiusA = 0.0f; 
        BuildCapsule(CapsuleA, TransformA, ABottom, ATop, RadiusA);
        
        FVector BBottom, BTop; 
        float RadiusB = 0.0f; 
        BuildCapsule(CapsuleB, TransformB, BBottom, BTop, RadiusB);

        auto SphereSphere = [](const FVector& Pos0, float Radius0, const FVector& Pos1, float Radius1) -> bool
            {
                const FVector d = Pos0 - Pos1;
                const float rs = Radius0 + Radius1;

                return d.SizeSquared() <= rs * rs;
            };

        // 1) Core vs Core
        if (Overlap_OBB_OBB(CoreA, CoreB)) return true;

        // 2) Core vs Sphere
        if (Overlap_Sphere_OBB(BTop, RadiusB, CoreA)) return true;
        if (Overlap_Sphere_OBB(BBottom, RadiusB, CoreA)) return true;
        if (Overlap_Sphere_OBB(ATop, RadiusA, CoreB)) return true;
        if (Overlap_Sphere_OBB(ABottom, RadiusA, CoreB)) return true;

        // 3) Sphere Sphere 
        if (SphereSphere(ATop, RadiusA, BTop, RadiusB)) return true;
        if (SphereSphere(ATop, RadiusA, BBottom, RadiusB)) return true;
        if (SphereSphere(ABottom, RadiusA, BTop, RadiusB)) return true;
        if (SphereSphere(ABottom, RadiusA, BBottom, RadiusB)) return true;

        return false;
    }

    // Capsuel - Sphere
    // 캡슐은 구 - OBB - 구로 구성되었다고 가정했다. 
    bool OverlapSphereAndCapsule(const FShape& Sphere, const FTransform& TransformSphere,
        const FShape& Capsule, const FTransform& TransformCapsule)
    {
        return OverlapCapsuleAndSphere(Capsule, TransformCapsule, Sphere, TransformSphere);
    }

    // Spherer - OBB 충돌 처리 
    bool Overlap_Sphere_OBB(const FVector& Center, float Radius, const FOBB& B)
    {
        const FVector Dist = Center - B.Center;
        const float Dist0 = FVector::Dot(Dist, B.Axes[0]);
        const float Dist1 = FVector::Dot(Dist, B.Axes[1]);
        const float Dist2 = FVector::Dot(Dist, B.Axes[2]);

        const float CenterX = FMath::Clamp(Dist0, -B.HalfExtent[0], B.HalfExtent[0]);
        const float CenterY = FMath::Clamp(Dist1, -B.HalfExtent[1], B.HalfExtent[1]);
        const float CenterZ = FMath::Clamp(Dist2, -B.HalfExtent[2], B.HalfExtent[2]);

        const float DiffX = Dist0 - CenterX;
        const float DiffY = Dist1 - CenterY;
        const float DiffZ = Dist2 - CenterZ;

        return (DiffX * DiffX + DiffY * DiffY + DiffZ * DiffZ) <= Radius * Radius;
    }

    bool OverlapSphereAndBox(const FShape& ShapeA, const FTransform& TransformA,
        const FShape& ShapeB, const FTransform& TransformB)
    {
     
        // 구 중심과 반지름(비등방 스케일 상계 적용)
        const FVector SphereCenter = TransformA.Translation;
        const float SphereRadius = ShapeA.Sphere.SphereRadius* UniformScaleMax(AbsVec(TransformA.Scale3D));

        FOBB B{}; 
        BuildOBB(ShapeB, TransformB, B);

        return Overlap_Sphere_OBB(SphereCenter, SphereRadius, B);
    }

    bool OverlapBoxAndSphere(const FShape& ShapeA, const FTransform& TransformA,
        const FShape& ShapeB, const FTransform& TransformB)
    { 
        FOBB B{}; 
        BuildOBB(ShapeA, TransformA, B);

        // 구 중심과 반지름(비등방 스케일 상계 적용)
        const FVector SphereCenter = TransformB.Translation;
        const float SphereRadius = ShapeB.Sphere.SphereRadius * UniformScaleMax(AbsVec(TransformB.Scale3D));

        return Overlap_Sphere_OBB(SphereCenter, SphereRadius, B);
    }

    bool OverlapBoxAndBox(const FShape& ShapeA, const FTransform& TransformA, const FShape& ShapeB, const FTransform& TransformB)
    {
        // 두 입력 모두 OBB로 구성
        FOBB A{}, B{};
        BuildOBB(ShapeA, TransformA, A);
        BuildOBB(ShapeB, TransformB, B);
        return Overlap_OBB_OBB(A, B);
        return true;

    }
    // 0: Box ,1: Sphere, 2: Capsule
    OverlapFunc OverlapLUT[3][3] =
    {
        /* Box    */   { &OverlapBoxAndBox,      &OverlapBoxAndSphere,   &OverlapBoxAndCapsule },
        /* Sphere */   { &OverlapSphereAndBox,   &OverlapSphereAndSphere,&OverlapSphereAndCapsule },
        /* Capsule*/   { &OverlapCapsuleAndBox,  &OverlapCapsuleAndSphere,&OverlapCapsuleAndCapsule }
    };


    bool CheckOverlap(const UShapeComponent* A, const UShapeComponent* B)
    {
        FShape ShapeA, ShapeB;
        A->GetShape(ShapeA);
        B->GetShape(ShapeB);

        return OverlapLUT[(int)ShapeA.Kind][(int)ShapeB.Kind](ShapeA, A->GetWorldTransform(), ShapeB, B->GetWorldTransform());
    }

    bool ComputePenetration(const UShapeComponent* A, const UShapeComponent* B, FHitResult& OutHit)
    {
        FShape ShapeA, ShapeB;
        A->GetShape(ShapeA);
        B->GetShape(ShapeB);

        FTransform TransA = A->GetWorldTransform();
        FTransform TransB = B->GetWorldTransform();

        // 편의를 위한 람다: A와 B를 뒤집어서 호출한 뒤 결과의 Normal을 반전시킴
        auto SwapAndCall = [&](auto Func) -> bool
        {
            if (Func(B, A, OutHit))
            {
                OutHit.ImpactNormal *= -1.0f; // 방향 반전 (B->A를 A->B로)
                // HitActor/Component는 호출자가 수정하거나, 여기서 스왑해야 함
                std::swap(OutHit.HitActor, OutHit.HitComponent); // (주의: HitResult 구조에 따라 다름)
                // 여기서는 OutHit에 A, B 포인터가 명시적으로 들어가지 않고 결과만 나오므로
                // ImpactNormal만 뒤집으면 됩니다. (HitResult에 Actor/Comp 포인터 채우는건 호출자 책임 권장)
                OutHit.HitActor = B->GetOwner();
                OutHit.HitComponent = const_cast<UPrimitiveComponent*>((const UPrimitiveComponent*)B);
                return true;
            }
            return false;
        };

        // 1. Sphere vs ...
        if (ShapeA.Kind == EShapeKind::Sphere)
        {
            FVector CenterA = TransA.Translation;
            float RadiusA = ShapeA.Sphere.SphereRadius * UniformScaleMax(AbsVec(TransA.Scale3D));
            
            // Sphere vs Sphere
            if (ShapeB.Kind == EShapeKind::Sphere)
            {
                FVector CenterB = TransB.Translation;
                float RadiusB = ShapeB.Sphere.SphereRadius * UniformScaleMax(AbsVec(TransB.Scale3D));
                return ComputeSphereToSpherePenetration(CenterA, RadiusA, CenterB, RadiusB, OutHit);
            }
            // Sphere vs Box
            else if (ShapeB.Kind == EShapeKind::Box)
            {
                FOBB BoxB; BuildOBB(ShapeB, TransB, BoxB);
                return ComputeSphereToBoxPenetration(CenterA, RadiusA, BoxB, OutHit);
            }
            // Sphere vs Capsule
            else if (ShapeB.Kind == EShapeKind::Capsule)
            {
                FVector CapP0, CapP1; float CapR;
                BuildCapsule(ShapeB, TransB, CapP0, CapP1, CapR);
                return ComputeSphereToCapsulePenetration(CenterA, RadiusA, CapP0, CapP1, CapR, OutHit);
            }
        }
        
        // 2. Box vs ...
        else if (ShapeA.Kind == EShapeKind::Box)
        {
            FOBB BoxA; BuildOBB(ShapeA, TransA, BoxA);

            // Box vs Sphere (Swap)
            if (ShapeB.Kind == EShapeKind::Sphere)
            {
                FVector CenterB = TransB.Translation;
                float RadiusB = ShapeB.Sphere.SphereRadius * UniformScaleMax(AbsVec(TransB.Scale3D));
                bool bHit = ComputeSphereToBoxPenetration(CenterB, RadiusB, BoxA, OutHit);
                if (bHit) OutHit.ImpactNormal *= -1.0f; // Sphere->Box를 Box->Sphere로
                return bHit;
            }
            // Box vs Box (SAT)
            else if (ShapeB.Kind == EShapeKind::Box)
            {
                FOBB BoxB; BuildOBB(ShapeB, TransB, BoxB);
                return ComputeBoxToBoxPenetration(BoxA, BoxB, OutHit);
            }
            // Box vs Capsule
            else if (ShapeB.Kind == EShapeKind::Capsule)
            {
                FVector CapP0, CapP1; float CapR;
                BuildCapsule(ShapeB, TransB, CapP0, CapP1, CapR);
                return ComputeBoxToCapsulePenetration(BoxA, CapP0, CapP1, CapR, OutHit);
            }
        }

        // 3. Capsule vs ...
        else if (ShapeA.Kind == EShapeKind::Capsule)
        {
            FVector CapA_P0, CapA_P1; float CapA_R;
            BuildCapsule(ShapeA, TransA, CapA_P0, CapA_P1, CapA_R);

            // Capsule vs Sphere (Swap)
            if (ShapeB.Kind == EShapeKind::Sphere)
            {
                FVector CenterB = TransB.Translation;
                float RadiusB = ShapeB.Sphere.SphereRadius * UniformScaleMax(AbsVec(TransB.Scale3D));
                bool bHit = ComputeSphereToCapsulePenetration(CenterB, RadiusB, CapA_P0, CapA_P1, CapA_R, OutHit);
                if (bHit) OutHit.ImpactNormal *= -1.0f;
                return bHit;
            }
            // Capsule vs Box (Swap)
            else if (ShapeB.Kind == EShapeKind::Box)
            {
                FOBB BoxB; BuildOBB(ShapeB, TransB, BoxB);
                bool bHit = ComputeBoxToCapsulePenetration(BoxB, CapA_P0, CapA_P1, CapA_R, OutHit);
                if (bHit) OutHit.ImpactNormal *= -1.0f;
                return bHit;
            }
            // Capsule vs Capsule
            else if (ShapeB.Kind == EShapeKind::Capsule)
            {
                FVector CapB_P0, CapB_P1; float CapB_R;
                BuildCapsule(ShapeB, TransB, CapB_P0, CapB_P1, CapB_R);
                return ComputeCapsuleToCapsulePenetration(CapA_P0, CapA_P1, CapA_R, CapB_P0, CapB_P1, CapB_R, OutHit);
            }
        }

        return false;
    }

    bool ComputeSphereToShapePenetration(const FVector& SpherePos, float SphereR, const FColliderProxy& Proxy, FHitResult& OutHit)
    {
        switch (Proxy.Type)
        {
        case EShapeKind::Box:
            return ComputeSphereToBoxPenetration(SpherePos, SphereR, Proxy.Box, OutHit);

        case EShapeKind::Capsule:
            return ComputeSphereToCapsulePenetration(SpherePos, SphereR, 
                Proxy.Capsule.PosA, Proxy.Capsule.PosB, Proxy.Capsule.Radius, OutHit);

        case EShapeKind::Sphere:
            return ComputeSphereToSpherePenetration(SpherePos, SphereR, 
                Proxy.Sphere.Center, Proxy.Sphere.Radius, OutHit);
        }
        return false;
    }

    bool ComputeSphereToBoxPenetration(const FVector& SpherePos, float SphereR, const FOBB& Box, FHitResult& OutHit)
    {
        FVector Diff = SpherePos - Box.Center;

        // OBB의 각 축(Axis)에 투영(Project)하여 "로컬 거리" 구하기
        float DistX = FVector::Dot(Diff, Box.Axes[0]);
        float DistY = FVector::Dot(Diff, Box.Axes[1]);
        float DistZ = FVector::Dot(Diff, Box.Axes[2]);

        // Box Extent 내로 클램핑 (박스 표면 상의 가장 가까운 점 찾기)
        float ClampedX = FMath::Clamp(DistX, -Box.HalfExtent[0], Box.HalfExtent[0]);
        float ClampedY = FMath::Clamp(DistY, -Box.HalfExtent[1], Box.HalfExtent[1]);
        float ClampedZ = FMath::Clamp(DistZ, -Box.HalfExtent[2], Box.HalfExtent[2]);

        // Closest Point 복원 (World Space)
        FVector ClosestPoint = Box.Center + (Box.Axes[0] * ClampedX) + (Box.Axes[1] * ClampedY) + (Box.Axes[2] * ClampedZ);

        // 거리 체크
        FVector PushVec = SpherePos - ClosestPoint;
        float DistSq = PushVec.SizeSquared();

        // [충돌 판정 A] 구의 중심이 박스 밖에 있을 때
        if (DistSq > 1e-6f)
        {
            if (DistSq > SphereR * SphereR) return false; // 안 닿음

            float Distance = FMath::Sqrt(DistSq);
            OutHit.bHit = true;
            OutHit.ImpactPoint = ClosestPoint;
            OutHit.ImpactNormal = PushVec / Distance; // 박스 표면 -> 구 중심 방향
            OutHit.PenetrationDepth = SphereR - Distance;
            return true;
        }

        // [충돌 판정 B] 구의 중심이 박스 안에 있을 때
        OutHit.bHit = true;
        OutHit.ImpactPoint = ClosestPoint;

        // 각 면까지의 거리 계산 (양수: 안쪽 깊이)
        float DepthX = Box.HalfExtent[0] - FMath::Abs(DistX);
        float DepthY = Box.HalfExtent[1] - FMath::Abs(DistY);
        float DepthZ = Box.HalfExtent[2] - FMath::Abs(DistZ);

        // 가장 작은 깊이(가장 가까운 탈출구) 찾기
        if (DepthX < DepthY && DepthX < DepthZ)
        {
            // X축 방향으로 탈출
            float Sign = (DistX >= 0.0f) ? 1.0f : -1.0f;
            OutHit.ImpactNormal = Box.Axes[0] * Sign;
            OutHit.PenetrationDepth = DepthX + SphereR;
        }
        else if (DepthY < DepthZ)
        {
            // Y축 방향으로 탈출
            float Sign = (DistY >= 0.0f) ? 1.0f : -1.0f;
            OutHit.ImpactNormal = Box.Axes[1] * Sign;
            OutHit.PenetrationDepth = DepthY + SphereR;
        }
        else
        {
            // Z축 방향으로 탈출
            float Sign = (DistZ >= 0.0f) ? 1.0f : -1.0f;
            OutHit.ImpactNormal = Box.Axes[2] * Sign;
            OutHit.PenetrationDepth = DepthZ + SphereR;
        }

        return true;
    }

    bool ComputeSphereToCapsulePenetration(const FVector& SpherePos, float SphereR, const FVector& PosA, const FVector& PosB, float CapsuleR, FHitResult& OutHit)
    {
        // 캡슐의 뼈대 벡터
        FVector SegVec = PosB - PosA;
        float SegLenSq = SegVec.SizeSquared();

        // 투영을 통해 선분 위에서 가장 가까운 위치 비율(t) 찾기
        float t = 0.0f;
        if (SegLenSq > 0.0f)
        {
            t = FVector::Dot(SpherePos - PosA, SegVec) / SegLenSq;
            
            // t <= 0 : 하단 구체(PosA) 쪽
            // t >= 1 : 상단 구체(PosB) 쪽
            // 0 < t < 1 : 원기둥 몸통 쪽
            t = FMath::Clamp(t, 0.0f, 1.0f);
        }

        // 가장 가까운 점 좌표 계산
        FVector ClosestOnSeg = PosA + (SegVec * t);

        // 구 vs ClosestOnSeg에 있는 가상의 구
        return ComputeSphereToSpherePenetration(
            SpherePos, SphereR,     // 내 파티클
            ClosestOnSeg,           // 타겟 구 중심 (선분 위의 점)
            CapsuleR,          // 타겟 구 반지름 (캡슐 두께)
            OutHit                  // 결과 담기
        );
    }

    bool ComputeSphereToSpherePenetration(const FVector& SpherePos, float SphereR, const FVector& TargetCenter, float TargetR, FHitResult& OutHit)
    {
        FVector Dir = SpherePos - TargetCenter;
        float DistSq = Dir.SizeSquared();
        float SumRadius = SphereR + TargetR;

        // 거리가 반지름 합보다 멀면 충돌 X
        if (DistSq >= SumRadius * SumRadius) { return false; }

        float Distance = FMath::Sqrt(DistSq);
        OutHit.bHit = true;

        // 중심점이 완전히 같을 때 (0으로 나누기 방지)
        if (Distance < 1e-4f)
        {
            OutHit.ImpactNormal = {0, 0, 1}; // 임의의 위쪽 방향
            OutHit.PenetrationDepth = SumRadius;
            OutHit.ImpactPoint = TargetCenter + (OutHit.ImpactNormal * TargetR); 
        }
        // 일반적인 경우
        else
        {
            OutHit.ImpactNormal = Dir / Distance;
            OutHit.PenetrationDepth = SumRadius - Distance;
            OutHit.ImpactPoint = TargetCenter + (OutHit.ImpactNormal * TargetR);
        }

        return true;
    }

    bool ComputeBoxToBoxPenetration(const FOBB& A, const FOBB& B, FHitResult& OutHit)
    {
        float MinPenetration = FLT_MAX;
        FVector BestAxis = FVector::Zero();

        // 15개의 분리축 테스트 (A의 3축 + B의 3축 + Cross Product 9축)
        // SAT는 하나라도 겹치지 않는 축이 있으면 충돌 아님.
        // 모든 축에서 겹친다면, 겹침(Penetration)이 가장 작은 축이 밀어낼 방향임.

        auto TestAxis = [&](const FVector& Axis) -> bool
        {
            if (Axis.SizeSquared() < 1e-6f) return true; // 축이 너무 작으면 패스(평행)
            
            FVector UnitAxis = Axis.GetSafeNormal();

            // A 투영
            float RA = A.HalfExtent[0] * FMath::Abs(FVector::Dot(A.Axes[0], UnitAxis)) +
                       A.HalfExtent[1] * FMath::Abs(FVector::Dot(A.Axes[1], UnitAxis)) +
                       A.HalfExtent[2] * FMath::Abs(FVector::Dot(A.Axes[2], UnitAxis));

            // B 투영
            float RB = B.HalfExtent[0] * FMath::Abs(FVector::Dot(B.Axes[0], UnitAxis)) +
                       B.HalfExtent[1] * FMath::Abs(FVector::Dot(B.Axes[1], UnitAxis)) +
                       B.HalfExtent[2] * FMath::Abs(FVector::Dot(B.Axes[2], UnitAxis));

            // 중심 거리 투영
            float Dist = FMath::Abs(FVector::Dot(B.Center - A.Center, UnitAxis));

            // 분리되었는가?
            float Penetration = (RA + RB) - Dist;
            if (Penetration <= 0.0f) return false; // 분리됨 -> 충돌 X

            // 최소 침투 깊이 갱신
            if (Penetration < MinPenetration)
            {
                MinPenetration = Penetration;
                BestAxis = UnitAxis;
                
                // Normal 방향 보정 (A -> B 방향이 되도록)
                if (FVector::Dot(B.Center - A.Center, BestAxis) < 0.0f)
                {
                    BestAxis = -BestAxis;
                }
            }
            return true;
        };

        // A의 3축
        for (int i = 0; i < 3; ++i) if (!TestAxis(A.Axes[i])) return false;
        // B의 3축
        for (int i = 0; i < 3; ++i) if (!TestAxis(B.Axes[i])) return false;
        // Cross Product 9축 (Edge vs Edge)
        for (int i = 0; i < 3; ++i)
        {
            for (int j = 0; j < 3; ++j)
            {
                FVector CrossAxis = FVector::Cross(A.Axes[i], B.Axes[j]);
                if (!TestAxis(CrossAxis)) return false;
            }
        }

        // 여기까지 왔으면 충돌임
        OutHit.bHit = true;
        OutHit.ImpactNormal = -BestAxis; // A를 밀어내는 방향이어야 하므로 (B->A) 반전 필요 여부 확인. 
                                         // 위 로직에서 BestAxis는 A->B임. 따라서 A를 밀어내려면 -BestAxis (B->A)
        OutHit.PenetrationDepth = MinPenetration;
        // ImpactPoint는 SAT로 정확히 구하기 어려움(면, 선, 점 가능). 대략적으로 중간 지점 설정
        OutHit.ImpactPoint = A.Center + (BestAxis * (A.HalfExtent[0])); // 근사치임. 정밀 계산하려면 Clipping 필요.
        
        return true;
    }

    bool ComputeCapsuleToCapsulePenetration(const FVector& PA_0, const FVector& PA_1, float RadiusA,
                                            const FVector& PB_0, const FVector& PB_1, float RadiusB, FHitResult& OutHit)
    {
        // 두 선분 사이의 최단 거리를 구하는 알고리즘
        FVector u = PA_1 - PA_0;
        FVector v = PB_1 - PB_0;
        FVector w = PA_0 - PB_0;

        float a = FVector::Dot(u, u);
        float b = FVector::Dot(u, v);
        float c = FVector::Dot(v, v);
        float d = FVector::Dot(u, w);
        float e = FVector::Dot(v, w);
        float D = a * c - b * b;

        float sc, sN, sD = D;
        float tc, tN, tD = D;

        // 평행 여부 체크
        if (D < 1e-6f)
        {
            sN = 0.0f;
            sD = 1.0f;
            tN = e;
            tD = c;
        }
        else
        {
            sN = (b * e - c * d);
            tN = (a * e - b * d);
            if (sN < 0.0f)
            {
                sN = 0.0f;
                tN = e;
                tD = c;
            }
            else if (sN > sD)
            {
                sN = sD;
                tN = e + b;
                tD = c;
            }
        }

        if (tN < 0.0f)
        {
            tN = 0.0f;
            if (-d < 0.0f) sN = 0.0f;
            else if (-d > a) sN = sD;
            else
            {
                sN = -d;
                sD = a;
            }
        }
        else if (tN > tD)
        {
            tN = tD;
            if ((-d + b) < 0.0f) sN = 0.0f;
            else if ((-d + b) > a) sN = sD;
            else
            {
                sN = (-d + b);
                sD = a;
            }
        }

        sc = (FMath::Abs(sN) < 1e-6f) ? 0.0f : sN / sD;
        tc = (FMath::Abs(tN) < 1e-6f) ? 0.0f : tN / tD;

        FVector ClosestA = PA_0 + (u * sc);
        FVector ClosestB = PB_0 + (v * tc);

        // 이제 구 vs 구 문제로 변환
        return ComputeSphereToSpherePenetration(ClosestA, RadiusA, ClosestB, RadiusB, OutHit);
    }

    bool ComputeBoxToCapsulePenetration(const FOBB& Box, const FVector& CapP0, const FVector& CapP1, float CapRadius,
        FHitResult& OutHit)
    {
        // [최적화 1] 캡슐의 기본 데이터 미리 계산
    FVector SegVec = CapP1 - CapP0;
    float SegLenSq = SegVec.SizeSquared();
    
    // 임시 결과 저장소
    FHitResult BestHit;
    float MaxPenetration = -FLT_MAX;
    bool bHasHit = false;

    // 람다: 히트 결과를 갱신하는 헬퍼 함수
    auto ProcessHit = [&](const FVector& TestPoint)
    {
        FHitResult TempHit;
        // 이미 구현된 SphereToBox 활용
        if (ComputeSphereToBoxPenetration(TestPoint, CapRadius, Box, TempHit))
        {
            if (TempHit.PenetrationDepth > MaxPenetration)
            {
                MaxPenetration = TempHit.PenetrationDepth;
                BestHit = TempHit;
                bHasHit = true;
            }
            return true;
        }
        return false;
    };

    // 1. [양 끝점 검사] (가장 싸고 확률 높음)
    bool bHitP0 = ProcessHit(CapP0);
    bool bHitP1 = ProcessHit(CapP1);

    // [최적화 2] 조기 종료 (Early Exit)
    // 만약 양 끝점 중 하나가 충분히 깊게 박혀 있다면, 중간 검사 생략 가능.
    // (예: 양쪽 다 박혀있으면, 어차피 캡슐 전체가 박힌 것임)
    if (bHitP0 && bHitP1)
    {
        OutHit = BestHit;
        return true;
    }

    // 2. [스마트 중간점 검사] (Smart Mid-Point Heuristic)
    // 기존의 '박스 중심 투영' 대신 '박스 표면 유도 투영'을 사용해 거대 박스 문제를 해결
    if (SegLenSq > 1e-6f)
    {
        // 2-1. 캡슐의 기하학적 중심
        FVector CapsuleCenter = (CapP0 + CapP1) * 0.5f;

        // 2-2. 캡슐 중심에서 박스 표면(혹은 내부)의 가장 가까운 점 찾기 (Closest Point on OBB)
        // (이 로직은 ComputeSphereToBox 내부 로직의 일부를 활용하거나 별도 함수로 분리 권장)
        // 여기서는 개념적으로 인라인 구현:
        FVector Diff = CapsuleCenter - Box.Center;
        FVector ClosestOnBox = Box.Center;
        
        for (int i = 0; i < 3; ++i)
        {
            float Dist = FVector::Dot(Diff, Box.Axes[i]);
            Dist = FMath::Clamp(Dist, -Box.HalfExtent[i], Box.HalfExtent[i]);
            ClosestOnBox += Box.Axes[i] * Dist;
        }

        // 2-3. '박스 표면 점'을 다시 '캡슐 선분'에 투영
        // 이 과정이 "박스 모서리 쪽으로 검사 지점을 당겨오는" 핵심 역할을 함
        float t = FVector::Dot(ClosestOnBox - CapP0, SegVec) / SegLenSq;
        t = FMath::Clamp(t, 0.0f, 1.0f);
        
        FVector SmartTestPoint = CapP0 + (SegVec * t);

        // 2-4. 계산된 스마트 지점 검사
        // (이미 검사한 끝점과 너무 가까우면 생략하는 미세 최적화도 가능)
        ProcessHit(SmartTestPoint);
    }

    if (bHasHit)
    {
        OutHit = BestHit;
        return true;
    }

    return false;
    }
}


