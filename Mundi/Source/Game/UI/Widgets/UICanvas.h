#pragma once

#include "UIWidget.h"
#include <unordered_map>
#include <memory>
#include <vector>
#include <string>

class UProgressBarWidget;
class UTextureWidget;

/**
 * @brief UI 캔버스 클래스
 *
 * 위젯들을 그룹으로 관리하는 컨테이너입니다.
 * UE의 UMG Canvas Panel과 유사한 역할을 합니다.
 *
 * 특징:
 * - 캔버스 단위로 Show/Hide 가능
 * - 캔버스 단위로 ZOrder 설정 (캔버스 간 레이어링)
 * - 위젯 좌표는 캔버스 기준 상대 좌표
 */
class UUICanvas
{
public:
    UUICanvas();
    ~UUICanvas() = default;

    // === 캔버스 식별자 ===
    std::string Name;

    // === 위치 및 크기 ===
    float X = 0.0f;         // 캔버스 시작 위치 (뷰포트 기준)
    float Y = 0.0f;
    float Width = 0.0f;     // 0이면 뷰포트 전체
    float Height = 0.0f;

    // === 렌더링 순서 (캔버스 레벨) ===
    int32_t ZOrder = 0;     // 낮을수록 먼저 그려짐 (뒤에)

    // === 가시성 ===
    bool bVisible = true;   // false면 모든 하위 위젯 숨김

    // ============================================
    // 위젯 생성 함수
    // ============================================

    /**
     * @brief 프로그레스 바 위젯 생성
     * @param Name 위젯 이름
     * @param X, Y 캔버스 내 상대 좌표
     * @param W, H 크기
     * @return 성공 여부
     */
    bool CreateProgressBar(const std::string& Name, float X, float Y, float W, float H);

    /**
     * @brief 텍스처 위젯 생성
     */
    bool CreateTextureWidget(const std::string& Name, const std::string& TexturePath,
                             float X, float Y, float W, float H,
                             ID2D1DeviceContext* D2DContext);

    /**
     * @brief 사각형 위젯 생성
     */
    bool CreateRect(const std::string& Name, float X, float Y, float W, float H);

    // ============================================
    // 위젯 관리
    // ============================================

    /**
     * @brief 이름으로 위젯 찾기
     */
    UUIWidget* FindWidget(const std::string& Name);

    /**
     * @brief 위젯 삭제
     */
    void RemoveWidget(const std::string& Name);

    /**
     * @brief 모든 위젯 삭제
     */
    void RemoveAllWidgets();

    // ============================================
    // 위젯 속성 설정 (편의 함수)
    // ============================================

    void SetWidgetProgress(const std::string& Name, float Value);
    void SetWidgetPosition(const std::string& Name, float X, float Y);
    void SetWidgetSize(const std::string& Name, float W, float H);
    void SetWidgetVisible(const std::string& Name, bool bVisible);
    void SetWidgetZOrder(const std::string& Name, int32_t Z);
    void SetWidgetForegroundColor(const std::string& Name, float R, float G, float B, float A);
    void SetWidgetBackgroundColor(const std::string& Name, float R, float G, float B, float A);
    void SetWidgetRightToLeft(const std::string& Name, bool bRTL);

    // ProgressBar 텍스처 설정
    bool SetProgressBarForegroundTexture(const std::string& Name, const std::string& Path, ID2D1DeviceContext* Context);
    bool SetProgressBarBackgroundTexture(const std::string& Name, const std::string& Path, ID2D1DeviceContext* Context);
    bool SetProgressBarLowTexture(const std::string& Name, const std::string& Path, ID2D1DeviceContext* Context);
    void SetProgressBarTextureOpacity(const std::string& Name, float Opacity);
    void ClearProgressBarTextures(const std::string& Name);

    // ============================================
    // SubUV 설정 함수
    // ============================================

    // TextureWidget용 SubUV
    void SetTextureSubUVGrid(const std::string& Name, int32_t NX, int32_t NY);
    void SetTextureSubUVFrame(const std::string& Name, int32_t FrameIndex);
    void SetTextureSubUV(const std::string& Name, int32_t FrameIndex, int32_t NX, int32_t NY);

    // ProgressBar용 SubUV
    void SetProgressBarForegroundSubUV(const std::string& Name, int32_t FrameIndex, int32_t NX, int32_t NY);
    void SetProgressBarBackgroundSubUV(const std::string& Name, int32_t FrameIndex, int32_t NX, int32_t NY);

    // 블렌드 모드
    void SetTextureBlendMode(const std::string& Name, EUIBlendMode Mode);
    void SetTextureAdditive(const std::string& Name, bool bAdditive);

    // ============================================
    // 위젯 애니메이션 함수
    // ============================================

    /**
     * @brief 위젯 이동 애니메이션
     * @param Name 위젯 이름
     * @param TargetX, TargetY 목표 위치
     * @param Duration 애니메이션 시간 (초)
     * @param Easing 이징 함수 타입
     */
    void MoveWidget(const std::string& Name, float TargetX, float TargetY,
                    float Duration, EEasingType Easing = EEasingType::Linear);

    /**
     * @brief 위젯 크기 변경 애니메이션
     */
    void ResizeWidget(const std::string& Name, float TargetW, float TargetH,
                      float Duration, EEasingType Easing = EEasingType::Linear);

    /**
     * @brief 위젯 회전 애니메이션
     */
    void RotateWidget(const std::string& Name, float TargetAngle,
                      float Duration, EEasingType Easing = EEasingType::Linear);

    /**
     * @brief 위젯 페이드 애니메이션
     */
    void FadeWidget(const std::string& Name, float TargetOpacity,
                    float Duration, EEasingType Easing = EEasingType::Linear);

    /**
     * @brief 위젯 애니메이션 중지
     */
    void StopWidgetAnimation(const std::string& Name);

    /**
     * @brief 위젯 진동 애니메이션 시작
     * @param Name 위젯 이름
     * @param Intensity 진동 강도 (픽셀)
     * @param Duration 지속 시간 (0이면 무한)
     * @param Frequency 진동 빈도 (Hz)
     * @param bDecay 시간에 따라 감쇠할지
     */
    void ShakeWidget(const std::string& Name, float Intensity, float Duration = 0.0f,
                     float Frequency = 15.0f, bool bDecay = true);

    /**
     * @brief 위젯 진동 애니메이션 중지
     */
    void StopWidgetShake(const std::string& Name);

    /**
     * @brief 모든 위젯 Enter 애니메이션 재생
     */
    void PlayAllEnterAnimations();

    /**
     * @brief 모든 위젯 Exit 애니메이션 재생
     */
    void PlayAllExitAnimations();

    // ============================================
    // 캔버스 속성 설정
    // ============================================

    void SetPosition(float InX, float InY) { X = InX; Y = InY; }
    void SetSize(float W, float H) { Width = W; Height = H; }
    void SetVisible(bool bVis) { bVisible = bVis; }
    void SetZOrder(int32_t Z) { ZOrder = Z; }

    // ============================================
    // 업데이트 및 렌더링
    // ============================================

    /**
     * @brief 캔버스 및 모든 위젯 업데이트
     * @param DeltaTime 프레임 시간
     */
    void Update(float DeltaTime);

    /**
     * @brief 캔버스 및 모든 위젯 렌더링
     * @param D2DContext Direct2D 디바이스 컨텍스트
     * @param ViewportOffsetX 뷰포트 오프셋 X
     * @param ViewportOffsetY 뷰포트 오프셋 Y
     */
    void Render(ID2D1DeviceContext* D2DContext, float ViewportOffsetX, float ViewportOffsetY);

    /**
     * @brief 위젯 개수 반환
     */
    size_t GetWidgetCount() const { return Widgets.size(); }

private:
    // 위젯 컨테이너
    std::unordered_map<std::string, std::unique_ptr<UUIWidget>> Widgets;

    // Z순서로 정렬된 렌더링 순서
    std::vector<UUIWidget*> SortedWidgets;
    bool bWidgetsSortDirty = false;

    // 정렬 갱신
    void UpdateWidgetSortOrder();
};
