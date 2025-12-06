#include "pch.h"
#include "ClothWeightAsset.h"
#include <algorithm>
#include <fstream>

IMPLEMENT_CLASS(UClothWeightAsset)

void UClothWeightAsset::SetVertexWeight(uint32 VertexIndex, float Weight)
{
    // 0.0 ~ 1.0 범위로 클램프
    Weight = std::clamp(Weight, 0.0f, 1.0f);
    VertexWeights[VertexIndex] = Weight;
}

float UClothWeightAsset::GetVertexWeight(uint32 VertexIndex) const
{
    auto It = VertexWeights.find(VertexIndex);
    if (It != VertexWeights.end())
    {
        return It->second;
    }
    // 기본값: 1.0 (자유 - 시뮬레이션에 완전히 참여)
    return 1.0f;
}

void UClothWeightAsset::LoadFromMap(const std::unordered_map<uint32, float>& InWeights)
{
    VertexWeights = InWeights;
}

void UClothWeightAsset::ClearWeights(float DefaultWeight)
{
    VertexWeights.clear();
    // 기본값은 GetVertexWeight에서 반환하므로 clear만 해도 됨
}

bool UClothWeightAsset::Load(const FString& InFilePath, ID3D11Device* InDevice)
{
    JSON Root;
    if (!FJsonSerializer::LoadJsonFromFile(Root, UTF8ToWide(InFilePath)))
    {
        UE_LOG("[UClothWeightAsset] Failed to load file: %s", InFilePath.c_str());
        return false;
    }

    Serialize(true, Root);
    SetFilePath(NormalizePath(InFilePath));

    UE_LOG("[UClothWeightAsset] Loaded successfully: %s (Vertices: %d)",
           InFilePath.c_str(), GetVertexCount());
    return true;
}

bool UClothWeightAsset::SaveToFile(const FString& FilePath)
{
    JSON JsonData = JSON::Make(JSON::Class::Object);

    Serialize(false, JsonData);

    std::ofstream OutFile(FilePath, std::ios::out);
    if (!OutFile.is_open())
    {
        UE_LOG("[UClothWeightAsset] Failed to open file for writing: %s", FilePath.c_str());
        return false;
    }

    OutFile << JsonData.dump(4); // Pretty print
    OutFile.close();

    UE_LOG("[UClothWeightAsset] Saved to: %s (Vertices: %d)",
           FilePath.c_str(), GetVertexCount());
    return true;
}

void UClothWeightAsset::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);

    if (bInIsLoading)
    {
        // =========================================================
        // [LOAD] 역직렬화
        // =========================================================
        if (!InOutHandle.hasKey("Type") || InOutHandle["Type"].ToString() != "ClothWeightAsset")
        {
            return;
        }

        // 에셋 이름 로드
        FString NameStr;
        if (FJsonSerializer::ReadString(InOutHandle, "Name", NameStr))
        {
            AssetName = FName(NameStr);
        }

        // SkeletalMesh 경로 로드
        FJsonSerializer::ReadString(InOutHandle, "SkeletalMeshPath", SkeletalMeshPath);

        // 버텍스 가중치 로드
        VertexWeights.clear();
        if (InOutHandle.hasKey("VertexWeights"))
        {
            JSON& WeightsArray = InOutHandle["VertexWeights"];
            for (int32 i = 0; i < WeightsArray.size(); ++i)
            {
                JSON& Item = WeightsArray.at(i);

                int32 VertexIndex = 0;
                float Weight = 1.0f;

                FJsonSerializer::ReadInt32(Item, "Index", VertexIndex);
                FJsonSerializer::ReadFloat(Item, "Weight", Weight);

                if (VertexIndex >= 0)
                {
                    VertexWeights[static_cast<uint32>(VertexIndex)] = std::clamp(Weight, 0.0f, 1.0f);
                }
            }
        }
    }
    else
    {
        // =========================================================
        // [SAVE] 직렬화
        // =========================================================
        InOutHandle["Type"] = "ClothWeightAsset";
        InOutHandle["Name"] = AssetName.ToString().c_str();
        InOutHandle["SkeletalMeshPath"] = SkeletalMeshPath.c_str();

        // 버텍스 가중치 저장
        // 1.0 (기본값)이 아닌 값만 저장하여 파일 크기 최적화
        JSON WeightsArray = JSON::Make(JSON::Class::Array);

        for (const auto& Pair : VertexWeights)
        {
            // 기본값 1.0과 다른 경우만 저장 (또는 모든 값 저장 - 선택)
            // 여기서는 모든 페인팅된 값을 저장 (작업 보존을 위해)
            JSON WeightItem = JSON::Make(JSON::Class::Object);
            WeightItem["Index"] = static_cast<int32>(Pair.first);
            WeightItem["Weight"] = Pair.second;
            WeightsArray.append(WeightItem);
        }

        InOutHandle["VertexWeights"] = WeightsArray;

        // 메타 정보
        InOutHandle["VertexCount"] = GetVertexCount();
    }
}
