#pragma once
#include "Archive.h"
#include "Vector.h"
#include "Name.h"
#include "Color.h"
#include "PathUtils.h"
#include "Source/Runtime/Core/Misc/JsonSerializer.h" // JSON 직렬화 추가

#define  INDEX_NONE -1
// 직렬화 포맷 (FVertexDynamic와 역할이 달라서 분리됨)
struct FNormalVertex
{
    FVector pos;
    FVector normal;
    FVector2D tex;
    FVector4 Tangent;
    FVector4 color;

    friend FArchive& operator<<(FArchive& Ar, FNormalVertex& Vtx)
    {
        Ar << Vtx.pos;
        Ar << Vtx.normal;
        Ar << Vtx.Tangent;
        Ar << Vtx.color;
        Ar << Vtx.tex;
        return Ar;
    }
};

struct FMeshData
{
    TArray<FVector> Vertices;
    TArray<uint32> Indices;
    TArray<FVector4> Color;
    TArray<FVector2D> UV;
    TArray<FVector> Normal;
};

struct FVertexSimple
{
    FVector Position;
    FVector4 Color;

    void FillFrom(const FMeshData& mesh, size_t i);
    void FillFrom(const FNormalVertex& src);
};

// 런타임 포맷 (FNormalVertex와 역할이 달라서 분리됨)
struct FVertexDynamic
{
    FVector Position;
    FVector Normal;
    FVector2D UV;
    FVector4 Tangent;
    FVector4 Color;

    void FillFrom(const FMeshData& mesh, size_t i);
    void FillFrom(const FNormalVertex& src);
};

/**
* 스키닝용 정점 구조체
*/
struct FSkinnedVertex
{
    FVector Position{}; // 정점 위치
    FVector Normal{}; // 법선 벡터
    FVector2D UV{}; // 텍스처 좌표
    FVector4 Tangent{}; // 탄젠트 (w는 binormal 방향)
    FVector4 Color{}; // 정점 컬러
    uint32 BoneIndices[4]{}; // 영향을 주는 본 인덱스 (최대 4개)
    float BoneWeights[4]{}; // 각 본의 가중치 (합이 1.0)

    friend FArchive& operator<<(FArchive& Ar, FSkinnedVertex& Vertex)
    {
        Ar << Vertex.Position;
        Ar << Vertex.Normal;
        Ar << Vertex.UV;
        Ar << Vertex.Tangent;
        Ar << Vertex.Color;

        for (int i = 0; i < 4; ++i)
        {
            Ar << Vertex.BoneIndices[i];
        }

        for (int i = 0; i < 4; ++i)
        {
            Ar << Vertex.BoneWeights[i];
        }

        return Ar;
    }

    void FillFrom(const FSkinnedVertex& Src);
};

// 같은 Position인데 Normal이나 UV가 다른 vertex가 존재할 수 있음, 그래서 SkinnedVertex를 키로 구별해야해서 hash함수 정의함
template <class T>
inline void CombineHash(size_t& InSeed, const T& Vertex)
{
    std::hash<T> Hasher;

    InSeed ^= Hasher(Vertex) + 0x9e3779b9 + (InSeed << 6) + (InSeed >> 2);
}
inline bool operator==(const FSkinnedVertex& Vertex1, const FSkinnedVertex& Vertex2)
{
    if (Vertex1.Position != Vertex2.Position ||
        Vertex1.Normal != Vertex2.Normal ||
        Vertex1.UV != Vertex2.UV ||
        Vertex1.Tangent != Vertex2.Tangent ||
        Vertex1.Color != Vertex2.Color)
    {
        return false;
    }

    if (std::memcmp(Vertex1.BoneIndices, Vertex2.BoneIndices, sizeof(Vertex1.BoneIndices)) != 0)
    {
        return false;
    }

    if (std::memcmp(Vertex1.BoneWeights, Vertex2.BoneWeights, sizeof(Vertex1.BoneWeights)) != 0)
    {
        return false;
    }

    // 모든 게 같음
    return true;
}
namespace std
{
    template<>
    struct hash<FSkinnedVertex>
    {

        size_t operator()(const FSkinnedVertex& Vertex) const
        {
            size_t Seed = 0;

            CombineHash(Seed, Vertex.Position.X); CombineHash(Seed, Vertex.Position.Y); CombineHash(Seed, Vertex.Position.Z);
            CombineHash(Seed, Vertex.Normal.X); CombineHash(Seed, Vertex.Normal.Y); CombineHash(Seed, Vertex.Normal.Z);
            CombineHash(Seed, Vertex.Tangent.X); CombineHash(Seed, Vertex.Tangent.Y); CombineHash(Seed, Vertex.Tangent.Z); CombineHash(Seed, Vertex.Tangent.W);
            CombineHash(Seed, Vertex.Color.X); CombineHash(Seed, Vertex.Color.Y); CombineHash(Seed, Vertex.Color.Z); CombineHash(Seed, Vertex.Color.W);
            CombineHash(Seed, Vertex.UV.X); CombineHash(Seed, Vertex.UV.Y);

            for (int Index = 0; Index < 4; Index++)
            {
                CombineHash(Seed, Vertex.BoneIndices[Index]);
                CombineHash(Seed, Vertex.BoneWeights[Index]);
            }
            return Seed;
        }
    };
}

struct FBillboardVertexInfo {
    FVector WorldPosition;
    FVector2D CharSize;//char scale
    FVector4 UVRect;//uv start && uv size
};

struct FBillboardVertexInfo_GPU
{
    float Position[3];
    float CharSize[2];
    float UVRect[4];

    void FillFrom(const FMeshData& mesh, size_t i); 
    void FillFrom(const FNormalVertex& src);
};

// 빌보드 전용: 위치 + UV만 있으면 충분
struct FBillboardVertex
{
    FVector WorldPosition;  // 정점 위치 (로컬 좌표, -0.5~0.5 기준 쿼드)
    FVector2D UV;        // 텍스처 좌표 (0~1)

    void FillFrom(const FMeshData& mesh, size_t i);
    void FillFrom(const FNormalVertex& src);
};

struct FGroupInfo
{
    uint32 StartIndex = 0;
    uint32 IndexCount = 0;
    FString InitialMaterialName; // obj 파일 자체에 맵핑된 material 이름

    friend FArchive& operator<<(FArchive& Ar, FGroupInfo& Info)
    {
        Ar << Info.StartIndex;
        Ar << Info.IndexCount;

        if (Ar.IsSaving())
            Serialization::WriteString(Ar, Info.InitialMaterialName);
        else if (Ar.IsLoading())
            Serialization::ReadString(Ar, Info.InitialMaterialName);

        return Ar;
    }
};

struct FStaticMesh
{
    FString PathFileName;
    FString CacheFilePath;  // 캐시된 소스 경로 (예: DerivedDataCache/cube.obj.bin)

    TArray<uint32> Indices;
    TArray<FNormalVertex> Vertices;
    TArray<FGroupInfo> GroupInfos; // 각 group을 render 하기 위한 정보

    bool bHasMaterial;

    friend FArchive& operator<<(FArchive& Ar, FStaticMesh& Mesh)
    {
        if (Ar.IsSaving())
        {
            Serialization::WriteString(Ar, Mesh.PathFileName);
            Serialization::WriteArray(Ar, Mesh.Vertices);
            Serialization::WriteArray(Ar, Mesh.Indices);

            uint32_t gCount = (uint32_t)Mesh.GroupInfos.size();
            Ar << gCount;
            for (auto& g : Mesh.GroupInfos) Ar << g;

            Ar << Mesh.bHasMaterial;
        }
        else if (Ar.IsLoading())
        {
            Serialization::ReadString(Ar, Mesh.PathFileName);
            Serialization::ReadArray(Ar, Mesh.Vertices);
            Serialization::ReadArray(Ar, Mesh.Indices);

            uint32_t gCount;
            Ar << gCount;
            Mesh.GroupInfos.resize(gCount);
            for (auto& g : Mesh.GroupInfos) Ar << g;

            Ar << Mesh.bHasMaterial;
        }
        return Ar;
    }
};

struct FBone
{
    FString Name; // 본 이름
    int32 ParentIndex; // 부모 본 인덱스 (-1이면 루트)
    FMatrix BindPose; // Bind Pose 변환 행렬
    FMatrix InverseBindPose; // Inverse Bind Pose (스키닝용)

    friend FArchive& operator<<(FArchive& Ar, FBone& Bone)
    {
        if (Ar.IsSaving())
        {
            Serialization::WriteString(Ar, Bone.Name);
            Ar << Bone.ParentIndex;
            Ar << Bone.BindPose;
            Ar << Bone.InverseBindPose;
        }
        else if (Ar.IsLoading())
        {
            Serialization::ReadString(Ar, Bone.Name);
            Ar << Bone.ParentIndex;
            Ar << Bone.BindPose;
            Ar << Bone.InverseBindPose;
        }
        return Ar;
    }
};

/**
 * @brief 스켈레탈 메시 소켓 - 본에 부착되는 가상의 부착점
 * 무기, 이펙트 등을 특정 본에 상대적인 위치에 부착할 때 사용
 */
struct FSkeletalMeshSocket
{
    FString SocketName;         // 소켓 이름
    FString BoneName;           // 부착할 본 이름
    FVector RelativeLocation;   // 본 기준 상대 위치
    FQuat RelativeRotation;     // 본 기준 상대 회전
    FVector RelativeScale;      // 상대 스케일

    FSkeletalMeshSocket()
        : RelativeLocation(FVector::Zero())
        , RelativeRotation(FQuat::Identity())
        , RelativeScale(FVector::One())
    {}

    friend FArchive& operator<<(FArchive& Ar, FSkeletalMeshSocket& Socket)
    {
        if (Ar.IsSaving())
        {
            Serialization::WriteString(Ar, Socket.SocketName);
            Serialization::WriteString(Ar, Socket.BoneName);
            Ar << Socket.RelativeLocation;
            Ar << Socket.RelativeRotation;
            Ar << Socket.RelativeScale;
        }
        else if (Ar.IsLoading())
        {
            Serialization::ReadString(Ar, Socket.SocketName);
            Serialization::ReadString(Ar, Socket.BoneName);
            Ar << Socket.RelativeLocation;
            Ar << Socket.RelativeRotation;
            Ar << Socket.RelativeScale;
        }
        return Ar;
    }
};

// JSON 직렬화를 위한 헬퍼 함수들
inline void to_json(JSON& j, const FVector& P) {
    j = JSON::Make(JSON::Class::Object);
    j["X"] = P.X;
    j["Y"] = P.Y;
    j["Z"] = P.Z;
}
inline void from_json(const JSON& j, FVector& P) {
    P.X = static_cast<float>(j.at("X").ToFloat());
    P.Y = static_cast<float>(j.at("Y").ToFloat());
    P.Z = static_cast<float>(j.at("Z").ToFloat());
}

inline void to_json(JSON& j, const FQuat& Q) {
    j = JSON::Make(JSON::Class::Object);
    j["X"] = Q.X;
    j["Y"] = Q.Y;
    j["Z"] = Q.Z;
    j["W"] = Q.W;
}
inline void from_json(const JSON& j, FQuat& Q) {
    Q.X = static_cast<float>(j.at("X").ToFloat());
    Q.Y = static_cast<float>(j.at("Y").ToFloat());
    Q.Z = static_cast<float>(j.at("Z").ToFloat());
    Q.W = static_cast<float>(j.at("W").ToFloat());
}

inline void to_json(JSON& j, const FSkeletalMeshSocket& S) {
    j = JSON::Make(JSON::Class::Object);
    j["SocketName"] = S.SocketName;
    j["BoneName"] = S.BoneName;

    JSON loc, rot, scale;
    to_json(loc, S.RelativeLocation);
    to_json(rot, S.RelativeRotation);
    to_json(scale, S.RelativeScale);

    j["RelativeLocation"] = loc;
    j["RelativeRotation"] = rot;
    j["RelativeScale"] = scale;
}

inline void from_json(const JSON& j, FSkeletalMeshSocket& S) {
    S.SocketName = j.at("SocketName").ToString();
    S.BoneName = j.at("BoneName").ToString();
    from_json(j.at("RelativeLocation"), S.RelativeLocation);
    from_json(j.at("RelativeRotation"), S.RelativeRotation);
    from_json(j.at("RelativeScale"), S.RelativeScale);
}

struct FSkeleton
{
    FString Name; // 스켈레톤 이름
    TArray<FBone> Bones; // 본 배열
    TArray<FSkeletalMeshSocket> Sockets; // 소켓 배열
    TMap <FString, int32> BoneNameToIndex; // 이름으로 본 검색

    /**
     * @brief 본 이름으로 본 인덱스를 찾기
     * @param BoneName 찾을 본 이름
     * @return 본 인덱스, 없으면 INDEX_NONE(-1)
     */
    int32 FindBoneIndex(const FName& BoneName) const
    {
        auto It = BoneNameToIndex.find(BoneName.ToString());
        if (It != BoneNameToIndex.end())
        {
            return It->second;
        }
        return INDEX_NONE;
    }

    /**
     * @brief 인덱스로 본 이름 가져오기
     * @param BoneIndex 본 인덱스
     * @return 본 이름
     */
    FName GetBoneName(int32 BoneIndex) const
    {
        if (BoneIndex >= 0 && BoneIndex < static_cast<int32>(Bones.size()))
        {
            return FName(Bones[BoneIndex].Name);
        }
        return FName();
    }

    /**
     * @brief 소켓 이름으로 소켓 찾기
     * @param SocketName 찾을 소켓 이름
     * @return 소켓 포인터, 없으면 nullptr
     */
    const FSkeletalMeshSocket* FindSocket(const FName& SocketName) const
    {
        for (const auto& Socket : Sockets)
        {
            if (Socket.SocketName == SocketName.ToString())
            {
                return &Socket;
            }
        }
        return nullptr;
    }

    FSkeletalMeshSocket* FindSocket(const FName& SocketName)
    {
        for (auto& Socket : Sockets)
        {
            if (Socket.SocketName == SocketName.ToString())
            {
                return &Socket;
            }
        }
        return nullptr;
    }

    /**
     * @brief 소켓 추가
     */
    void AddSocket(const FSkeletalMeshSocket& Socket)
    {
        Sockets.Add(Socket);
    }

    /**
     * @brief 소켓 제거
     */
    bool RemoveSocket(const FName& SocketName)
    {
        for (int32 i = 0; i < static_cast<int32>(Sockets.size()); ++i)
        {
            if (Sockets[i].SocketName == SocketName.ToString())
            {
                Sockets.RemoveAt(i);
                return true;
            }
        }
        return false;
    }

    friend FArchive& operator<<(FArchive& Ar, FSkeleton& Skeleton)
    {
        if (Ar.IsSaving())
        {
            Serialization::WriteString(Ar, Skeleton.Name);

            uint32 boneCount = static_cast<uint32>(Skeleton.Bones.size());
            Ar << boneCount;
            for (auto& bone : Skeleton.Bones)
            {
                Ar << bone;
            }

            // 소켓 저장은 이제 JSON으로 별도 관리하므로 제거
            // uint32 socketCount = static_cast<uint32>(Skeleton.Sockets.size());
            // Ar << socketCount;
            // for (auto& socket : Skeleton.Sockets)
            // {
            //     Ar << socket;
            // }
        }
        else if (Ar.IsLoading())
        {
            Serialization::ReadString(Ar, Skeleton.Name);

            uint32 boneCount;
            Ar << boneCount;
            Skeleton.Bones.resize(boneCount);
            for (auto& bone : Skeleton.Bones)
            {
                Ar << bone;
            }

            // BoneNameToIndex 재구축
            Skeleton.BoneNameToIndex.clear();
            for (int32 i = 0; i < static_cast<int32>(Skeleton.Bones.size()); ++i)
            {
                Skeleton.BoneNameToIndex[Skeleton.Bones[i].Name] = i;
            }

            // 소켓 로딩은 이제 JSON에서 별도 처리하므로 제거
            Skeleton.Sockets.clear();
            // try
            // {
            //     uint32 socketCount;
            //     Ar << socketCount;
            //     // 소켓 개수가 비정상적으로 크면 기존 포맷으로 간주
            //     if (socketCount > 10000)
            //     {
            //         Skeleton.Sockets.clear();
            //     }
            //     else
            //     {
            //         Skeleton.Sockets.resize(socketCount);
            //         for (auto& socket : Skeleton.Sockets)
            //         {
            //             Ar << socket;
            //         }
            //     }
            // }
            // catch (...)
            // {
            //     // 기존 캐시 파일은 소켓 데이터가 없음 - 무시
            //     Skeleton.Sockets.clear();
            // }
        }
        return Ar;
    }

    /**
    * @brief 전체 본 개수 반환
    */
    int32 GetNumBones() const
    {
        return static_cast<int32>(Bones.size());
    }

    /**
    * @brief 특정 본의 부모 인덱스 반환
    */
    int32 GetParentIndex(int32 BoneIndex) const
    {
        if (BoneIndex >= 0 && BoneIndex < static_cast<int32>(Bones.size()))
        {
            return Bones[BoneIndex].ParentIndex;
        }
       return INDEX_NONE;
    }

    /**
    * @brief 전체 소켓 개수 반환
    */
    int32 GetNumSockets() const
    {
        return static_cast<int32>(Sockets.size());
    }
};

struct FVertexWeight
{
    uint32 VertexIndex; // 정점 인덱스
    float Weight; // 가중치
};

struct FSkeletalMeshData
{
    FString PathFileName;
    FString CacheFilePath;
    
    TArray<FSkinnedVertex> Vertices; // 정점 배열
    TArray<uint32> Indices; // 인덱스 배열
    FSkeleton Skeleton; // 스켈레톤 정보
    TArray<FGroupInfo> GroupInfos; // 머티리얼 그룹 (기존 시스템 재사용)
    bool bHasMaterial = false;

    friend FArchive& operator<<(FArchive& Ar, FSkeletalMeshData& Data)
    {
        if (Ar.IsSaving())
        {
            // 1. Vertices 저장
            Serialization::WriteArray(Ar, Data.Vertices);

            // 2. Indices 저장
            Serialization::WriteArray(Ar, Data.Indices);

            // 3. Skeleton 저장
            Ar << Data.Skeleton;

            // 4. GroupInfos 저장
            uint32 gCount = static_cast<uint32>(Data.GroupInfos.size());
            Ar << gCount;
            for (auto& g : Data.GroupInfos)
            {
                Ar << g;
            }

            // 5. Material 플래그 저장
            Ar << Data.bHasMaterial;

            // 6. CacheFilePath 저장
            Serialization::WriteString(Ar, Data.CacheFilePath);
        }
        else if (Ar.IsLoading())
        {
            // 1. Vertices 로드
            Serialization::ReadArray(Ar, Data.Vertices);

            // 2. Indices 로드
            Serialization::ReadArray(Ar, Data.Indices);

            // 3. Skeleton 로드
            Ar << Data.Skeleton;

            // 4. GroupInfos 로드
            uint32 gCount;
            Ar << gCount;
            Data.GroupInfos.resize(gCount);
            for (auto& g : Data.GroupInfos)
            {
                Ar << g;
            }

            // 5. Material 플래그 로드
            Ar << Data.bHasMaterial;

            // 6. CacheFilePath 로드
            Serialization::ReadString(Ar, Data.CacheFilePath);
        }
        return Ar;
    }
};

struct FParticleSpriteVertex
{
    FVector Position;      // 12 bytes (offset 0)
    FVector2D Corner;      // 8 bytes (offset 12)
    FVector2D Size;        // 8 bytes (offset 20)
    FLinearColor Color;    // 16 bytes (offset 28)
    float Rotation;        // 4 bytes (offset 44)
    float SubImageIndex;   // 4 bytes (offset 48) - SubUV 애니메이션용 float 프레임 인덱스
    FVector Velocity;
};

struct FParticleInstanceData
{
    FVector Position;  
    FVector2D Size;    
    FLinearColor Color;
    float Rotation;
    FVector Velocity;
};

struct FMeshParticleInstanceData
{
    FMatrix      WorldMatrix;
    FMatrix      WorldInverseTranspose;
    FLinearColor Color;
};

struct FParticleBeamVertex
{
    FVector Position;       // 월드 위치
    FVector2D UV;          // 텍스처 좌표
    FLinearColor Color;    // 파티클 색상
};
