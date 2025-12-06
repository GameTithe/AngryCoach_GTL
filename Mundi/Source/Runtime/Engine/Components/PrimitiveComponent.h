#pragma once

#include "SceneComponent.h"
#include "Material.h"
#include "UPrimitiveComponent.generated.h"
#include "DamageTypes.h"

struct FBodyInstance;
struct FSceneCompData;

class URenderer;
struct FMeshBatchElement;
class FSceneView;

struct FOverlapInfo
{
    AActor* OtherActor = nullptr;
    UPrimitiveComponent* OtherComp = nullptr;

    bool operator==(const FOverlapInfo& Other) const
    {
        return OtherComp == Other.OtherComp;
    }
};

UCLASS(DisplayName="프리미티브 컴포넌트", Description="렌더링 가능한 기본 컴포넌트입니다")
class UPrimitiveComponent :public USceneComponent
{
public:

    GENERATED_REFLECTION_BODY();

    UPrimitiveComponent();
    virtual ~UPrimitiveComponent();

    void BeginPlay() override;
    void TickComponent(float DeltaTime) override;

    void OnRegister(UWorld* InWorld) override;
    void OnUnregister() override;

    virtual FAABB GetWorldAABB() const { return FAABB(); }

    // 이 프리미티브를 렌더링하는 데 필요한 FMeshBatchElement를 수집합니다.
    virtual void CollectMeshBatches(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View) {}

    virtual UMaterialInterface* GetMaterial(uint32 InElementIndex) const
    {
        // 기본 구현: UPrimitiveComponent 자체는 머티리얼을 소유하지 않으므로 nullptr 반환
        return nullptr;
    }
    virtual void SetMaterial(uint32 InElementIndex, UMaterialInterface* InNewMaterial)
    {
        // 기본 구현: 아무것도 하지 않음 (머티리얼을 지원하지 않거나 설정 불가)
    }

    // 내부적으로 ResourceManager를 통해 UMaterial*를 찾아 SetMaterial을 호출합니다.
    void SetMaterialByName(uint32 InElementIndex, const FString& MaterialName);

    void SetCulled(bool InCulled)
    {
        bIsCulled = InCulled;
    }

    bool GetCulled() const
    {
        return bIsCulled;
    }

    // ───── 충돌 관련 ──────────────────────────── 
    bool IsOverlappingActor(const AActor* Other) const;
    virtual const TArray<FOverlapInfo>& GetOverlapInfos() const { static TArray<FOverlapInfo> Empty; return Empty; }

    // BodySetup 설정을 무시하고 싶을 때
    bool bOverrideCollisionSetting = false;
    ECollisionState CollisionEnabled = ECollisionState::QueryAndPhysics;
    
    // ───── 복사 관련 ────────────────────────────
    void DuplicateSubObjects() override;

    // Overlap event generation toggle API
    void SetGenerateOverlapEvents(bool bEnable) { bGenerateOverlapEvents = bEnable; }
    bool GetGenerateOverlapEvents() const { return bGenerateOverlapEvents; }

    void SetBlockComponent(bool bEnable) { bBlockComponent = bEnable; }
    bool GetBlockComponent() const { return bBlockComponent; }

    // ───── Cartoon Rendering ────────────────────────────
    void SetUseCartoonShading(bool bInUseCartoon) { bUseCartoonShading = bInUseCartoon; }
    bool IsUsingCartoonShading() const { return bUseCartoonShading; }

    void SetCartoonOutlineThreshold(float InThreshold) { CartoonOutlineThreshold = InThreshold; }
    float GetCartoonOutlineThreshold() const { return CartoonOutlineThreshold; }

    void SetCartoonShadingLevels(int32 InLevels) { CartoonShadingLevels = InLevels; }
    int32 GetCartoonShadingLevels() const { return CartoonShadingLevels; }

    void SetCartoonSpecularThreshold(float InThreshold) { CartoonSpecularThreshold = InThreshold; }
    float GetCartoonSpecularThreshold() const { return CartoonSpecularThreshold; }

    void SetCartoonRimIntensity(float InIntensity) { CartoonRimIntensity = InIntensity; }
    float GetCartoonRimIntensity() const { return CartoonRimIntensity; }

    // ───── 직렬화 ────────────────────────────
    void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;


    virtual void OnCreatePhysicsState();

    DECLARE_DELEGATE(OnComponentBeginOverlap, UPrimitiveComponent*, UPrimitiveComponent*, const FHitResult&);    
    DECLARE_DELEGATE(OnComponentEndOverlap, UPrimitiveComponent*, UPrimitiveComponent*, const FHitResult&);    
    DECLARE_DELEGATE(OnComponentHit, UPrimitiveComponent*, UPrimitiveComponent*, const FHitResult&);    
    void OnBeginOverlap(UPrimitiveComponent* A, UPrimitiveComponent* B, const FHitResult& HitResult);
    void OnEndOverlap(UPrimitiveComponent* A, UPrimitiveComponent* B, const FHitResult& HitResult);
    void OnHit(UPrimitiveComponent* A, UPrimitiveComponent* B, const FHitResult& HitResult);

protected:
    bool bIsCulled = false;

    // ───── Cartoon Rendering Parameters ────────────────────────────
    UPROPERTY(EditAnywhere, Category = "Render")
    bool bUseCartoonShading = false;

    UPROPERTY(EditAnywhere, Category = "Render")
    float CartoonOutlineThreshold = 0.55f;  // Outline detection threshold (0.0 ~ 1.0)

    UPROPERTY(EditAnywhere, Category = "Render")
    int32 CartoonShadingLevels = 3;  // Number of cel shading levels (2 ~ 10)

    UPROPERTY(EditAnywhere, Category = "Render")
    float CartoonSpecularThreshold = 0.5f;  // Specular highlight threshold (0.0 ~ 1.0)

    UPROPERTY(EditAnywhere, Category = "Render")
    float CartoonRimIntensity = 0.3f;  // Rim light intensity (0.0 ~ 1.0)
     
    // ───── 충돌 관련 ────────────────────────────
    UPROPERTY(EditAnywhere, Category="Shape")
    bool bGenerateOverlapEvents;

    UPROPERTY(EditAnywhere, Category="Shape")
    bool bBlockComponent;

    FBodyInstance* BodyInstance;

    // 현재 프레임 감지된 충돌 후보 큐
    TArray<FHitResult> PendingOverlaps;
    TArray<FHitResult> PendingHits;
    
    // 겹쳐있는 상태의 컴포넌트 목록(EndOverlap, 중복 진입 방지)
    TArray<FOverlapInfo> ActiveOverlaps;

    TSet<uint64> CurrentFrameHits;
    TSet<uint64> PreviousFrameHits;

private:
    UPROPERTY()
    int32 CollisionEnabled_Internal = 2; // ECollisionState::QueryAndPhysics;
};
