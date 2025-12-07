#pragma once
#include "PrimitiveComponent.h"
#include "DamageTypes.h"
struct FAABB;
struct FOBB;
struct FBoundingSphere;
struct FShape;
struct FColliderProxy;

class UShapeComponent;

class AActor;
class UPrimitiveComponent;

namespace Collision
{
    bool Intersects(const FAABB& Aabb, const FOBB& Obb);

    bool Intersects(const FAABB& Aabb, const FBoundingSphere& Sphere);

	bool Intersects(const FOBB& Obb, const FBoundingSphere& Sphere);

    // ㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡShapeComponent Helper 함수ㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡ
    FVector AbsVec(const FVector& v);
    float UniformScaleMax(const FVector& S);

    void BuildOBB(const FShape& BoxShape, const FTransform& T, FOBB& Out);
    bool Overlap_OBB_OBB(const FOBB& A, const FOBB& B);
    bool Overlap_Sphere_OBB(const FVector& Center, float Radius, const FOBB& B);

    bool OverlapSphereAndSphere(const FShape& ShapeA, const FTransform& TransformA, const FShape& ShapeB, const FTransform& TransformB);
    
    void BuildCapsule(const FShape& CapsuleShape, const FTransform& TransformCapsule, FVector& OutP0, FVector& OutP1, float& OutRadius);
    void BuildCapsuleCoreOBB(const FShape& CapsuleShape, const FTransform& Transform, FOBB& Out);

    bool OverlapCapsuleAndSphere(const FShape& Capsule, const FTransform& TransformCapsule, const FShape& Sphere, const FTransform& TransformSphere);

    bool OverlapCapsuleAndBox(const FShape& Capsule, const FTransform& TransformCapsule, const FShape& Box, const FTransform& TransformBox);

    bool OverlapCapsuleAndCapsule(const FShape& CapsuleA, const FTransform& TransformA, const FShape& CapsuleB, const FTransform& TransformB);
       
    using OverlapFunc = bool(*) (const FShape&, const FTransform&, const FShape&, const FTransform&);

    extern OverlapFunc OverlapLUT[3][3];
    
    
    bool CheckOverlap(const UShapeComponent* A, const UShapeComponent* B);

    bool ComputePenetration(const UShapeComponent* A, const UShapeComponent* B, FHitResult& OutHit);
    
    // ㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡParticle Collision Helper 함수ㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡ
    bool ComputeSphereToShapePenetration(const FVector& SpherePos, float SphereR, const FColliderProxy& Proxy, FHitResult& OutHit);
    bool ComputeSphereToBoxPenetration(const FVector& SpherePos, float SphereR, const FOBB& Box, FHitResult& OutHit);
    bool ComputeSphereToCapsulePenetration(const FVector& SpherePos, float SphereR, const FVector& PosA, const FVector& PosB, float CapsuleR, FHitResult& OutHit);
    bool ComputeSphereToSpherePenetration(const FVector& SpherePos, float SphereR, const FVector& TargetCenter, float TargetR, FHitResult& OutHit);

    bool ComputeBoxToBoxPenetration(const FOBB& A, const FOBB& B, FHitResult& OutHit);
    bool ComputeCapsuleToCapsulePenetration(const FVector& PA_0, const FVector& PA_1, float RadiusA, 
                                            const FVector& PB_0, const FVector& PB_1, float RadiusB, FHitResult& OutHit);
    bool ComputeBoxToCapsulePenetration(const FOBB& Box, const FVector& CapP0, const FVector& CapP1, float CapRadius, FHitResult& OutHit);
    
    
}