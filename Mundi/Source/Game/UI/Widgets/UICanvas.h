#pragma once

#include "UIWidget.h"
#include <unordered_map>
#include <memory>
#include <vector>
#include <string>

class UProgressBarWidget;
class UTextureWidget;
class UButtonWidget;

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

    /**
     * @brief 버튼 위젯 생성
     * @param Name 위젯 이름
     * @param TexturePath 기본 텍스처 경로
     * @param X, Y 캔버스 내 상대 좌표
     * @param W, H 크기
     * @param D2DContext D2D 디바이스 컨텍스트
     * @return 성공 여부
     */
    bool CreateButton(const std::string& Name, const std::string& TexturePath,
                      float X, float Y, float W, float H,
                      ID2D1DeviceContext* D2DContext);

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
    void SetWidgetOpacity(const std::string& Name, float Opacity);
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
    // 버튼 설정 함수
    // ============================================

    /**
     * @brief 버튼 활성화/비활성화
     */
    void SetButtonInteractable(const std::string& Name, bool bInteractable);

    /**
     * @brief 버튼 호버 스케일 효과 설정
     */
    void SetButtonHoverScale(const std::string& Name, float Scale, float Duration = 0.1f);

    /**
     * @brief 버튼 상태별 텍스처 설정
     */
    bool SetButtonHoveredTexture(const std::string& Name, const std::string& Path, ID2D1DeviceContext* Context);
    bool SetButtonPressedTexture(const std::string& Name, const std::string& Path, ID2D1DeviceContext* Context);
    bool SetButtonDisabledTexture(const std::string& Name, const std::string& Path, ID2D1DeviceContext* Context);

    /**
     * @brief 버튼 상태별 틴트 색상 설정
     */
    void SetButtonNormalTint(const std::string& Name, float R, float G, float B, float A);
    void SetButtonHoveredTint(const std::string& Name, float R, float G, float B, float A);
    void SetButtonPressedTint(const std::string& Name, float R, float G, float B, float A);
    void SetButtonDisabledTint(const std::string& Name, float R, float G, float B, float A);

    // ============================================
    // 마우스 입력 처리
    // ============================================

    /**
     * @brief 마우스 입력 처리
     * @param MouseX 마우스 X 좌표 (뷰포트 기준)
     * @param MouseY 마우스 Y 좌표 (뷰포트 기준)
     * @param bLeftDown 왼쪽 마우스 버튼 눌림 여부
     * @param bLeftPressed 이번 프레임에 눌렸는지 (down edge)
     * @param bLeftReleased 이번 프레임에 뗐는지 (up edge)
     * @return 입력이 소비되었으면 true (버튼 위에서 처리됨)
     */
    bool ProcessMouseInput(float MouseX, float MouseY, bool bLeftDown, bool bLeftPressed, bool bLeftReleased);

    /**
     * @brief 마우스가 캔버스 영역 안에 있는지 확인
     */
    bool ContainsPoint(float PosX, float PosY) const;

    // ============================================
    // 키보드/게임패드 포커스 시스템
    // ============================================

    /**
     * @brief 키보드/게임패드 입력 처리
     * @param bUp 위 방향 눌림 (W, 위 화살표, D-pad Up)
     * @param bDown 아래 방향 눌림 (S, 아래 화살표, D-pad Down)
     * @param bConfirm 확인 눌림 (Enter, Space, A 버튼)
     * @return 입력이 소비되었으면 true
     */
    bool ProcessKeyboardInput(bool bUp, bool bDown, bool bConfirm);

    /**
     * @brief 특정 버튼에 포커스 설정
     */
    void SetFocus(UButtonWidget* Button);

    /**
     * @brief 이름으로 버튼에 포커스 설정
     */
    void SetFocusByName(const std::string& ButtonName);

    /**
     * @brief 다음 버튼으로 포커스 이동 (아래/오른쪽)
     */
    void MoveFocusNext();

    /**
     * @brief 이전 버튼으로 포커스 이동 (위/왼쪽)
     */
    void MoveFocusPrev();

    /**
     * @brief 현재 포커스된 버튼 클릭 트리거
     * @return 클릭이 실행되었으면 true
     */
    bool TriggerFocusedClick();

    /**
     * @brief 현재 포커스된 버튼 반환
     */
    UButtonWidget* GetFocusedButton() const { return FocusedButton; }

    /**
     * @brief 포커스 가능한 버튼이 있는지
     * @note dirty 상태면 자동으로 목록 재구축
     */
    bool HasFocusableButtons()
    {
        if (bFocusListDirty)
        {
            RebuildFocusableButtons();
        }
        return !FocusableButtons.empty();
    }

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

    // 마우스 입력 상태
    UButtonWidget* HoveredButton = nullptr;   // 현재 호버중인 버튼
    UButtonWidget* PressedButton = nullptr;   // 현재 눌린 버튼

    // 키보드/게임패드 포커스 상태
    UButtonWidget* FocusedButton = nullptr;   // 현재 포커스된 버튼
    std::vector<UButtonWidget*> FocusableButtons;  // 포커스 가능한 버튼 목록 (Y좌표 정렬)
    int32_t FocusIndex = -1;                  // 현재 포커스 인덱스
    bool bFocusListDirty = true;              // 포커스 목록 갱신 필요

    // 정렬 갱신
    void UpdateWidgetSortOrder();

    /**
     * @brief 포커스 가능한 버튼 목록 갱신 (Y좌표 기준 정렬)
     */
    void RebuildFocusableButtons();

    /**
     * @brief 좌표에서 가장 위에 있는 버튼 찾기 (Z-order 고려)
     */
    UButtonWidget* FindButtonAtPosition(float LocalX, float LocalY);
};
