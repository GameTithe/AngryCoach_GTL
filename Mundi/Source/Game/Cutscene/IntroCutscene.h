#pragma once

#include <string>

class UUICanvas;
class UUIWidget;

// 컷신 완료 콜백 타입
typedef void (*FOnCutsceneFinished)(void* Owner);

/**
 * @brief 인트로 컷신 페이즈
 *
 * 흐름: 1초 대기 → gtl Enter → gtl Exit → 1초 대기
 *       → phys/dx/power Enter → phys/dx/power Exit → 1초 대기
 *       → team_label Enter → team_label Exit → 1초 대기
 *       → mb1~4 Enter(동시) → mb1~4 Exit(동시) → 완료
 */
enum class EIntroPhase : uint8_t
{
	Idle,              // 시작 전
	InitialWait,       // 시작 1초 대기
	GtlEnter,          // gtl Enter 애니메이션
	GtlExit,           // gtl Exit 애니메이션
	GtlWait,           // 1초 대기
	TechEnter,         // phys, dx, power Enter (동시)
	TechExit,          // phys, dx, power Exit (동시)
	TechWait,          // 1초 대기
	TeamLabelEnter,    // team_label Enter
	TeamLabelExit,     // team_label Exit
	TeamLabelWait,     // 1초 대기
	MembersEnter,      // mb1~4 Enter (동시)
	MembersWait,       // Enter 후 대기
	MembersExit,       // mb1~4 Exit (동시)
	Finished           // 완료
};

/**
 * @brief 인트로 컷신 클래스
 *
 * GameMode의 Intro 상태에서 재생되는 컷신을 관리합니다.
 *
 * 위젯 구조:
 * - gtl: 부트캠프 이름
 * - team_label: 팀 라벨
 * - mb1, mb2, mb3, mb4: 팀원 이름들
 *
 * 각 위젯의 Enter/Exit Animation은 uiasset에서 설정합니다.
 */
class UIntroCutscene
{
public:
	UIntroCutscene() = default;
	~UIntroCutscene() = default;

	/**
	 * @brief 컷신 시작
	 * Intro.uiasset을 로드하고 시퀀스 재생을 시작합니다.
	 * @return 성공 여부
	 */
	bool Start();

	/**
	 * @brief 매 프레임 업데이트
	 * @param DeltaTime 프레임 시간 (초)
	 */
	void Update(float DeltaTime);

	/**
	 * @brief 컷신 즉시 종료 (스킵)
	 */
	void Skip();

	/**
	 * @brief 컷신 완료 여부
	 */
	bool IsFinished() const { return CurrentPhase == EIntroPhase::Finished; }

	/**
	 * @brief 현재 페이즈 반환
	 */
	EIntroPhase GetCurrentPhase() const { return CurrentPhase; }

	/**
	 * @brief 완료 콜백 설정
	 * 컷신이 완료되면 호출됩니다.
	 */
	void SetOnFinished(FOnCutsceneFinished Callback, void* InOwner)
	{
		OnFinished = Callback;
		Owner = InOwner;
	}

private:
	// 현재 상태
	EIntroPhase CurrentPhase = EIntroPhase::Idle;
	float PhaseTime = 0.0f;      // 현재 페이즈 경과 시간

	// UI 캔버스
	UUICanvas* Canvas = nullptr;
	std::string CanvasName = "Intro";

	// 완료 콜백
	FOnCutsceneFinished OnFinished = nullptr;
	void* Owner = nullptr;

	// === 대기 시간 설정 (초) ===
	static constexpr float WAIT_DURATION = 1.0f;          // 기본 대기 시간
	static constexpr float TECH_WAIT_DURATION = 2.0f;     // Tech (phys, dx, power) 후 대기 시간
	static constexpr float MEMBERS_WAIT_DURATION = 1.0f;  // Members Enter 후 대기 시간

	// === Skip 위젯 깜빡임 설정 ===
	static constexpr float SKIP_BLINK_SPEED = 0.7f;       // 깜빡임 속도 (Hz)
	static constexpr float SKIP_OPACITY_MIN = 0.3f;       // 최소 Opacity
	static constexpr float SKIP_OPACITY_MAX = 1.0f;       // 최대 Opacity
	float SkipBlinkTime = 0.0f;                           // 깜빡임 타이머

	// === 내부 함수 ===

	/**
	 * @brief 페이즈 전환
	 */
	void TransitionTo(EIntroPhase NewPhase);

	/**
	 * @brief 위젯 초기화 (모두 숨김)
	 */
	void InitializeWidgets();

	/**
	 * @brief 위젯 Enter 애니메이션 재생
	 */
	void PlayWidgetEnter(const std::string& WidgetName);

	/**
	 * @brief 위젯 Exit 애니메이션 재생
	 */
	void PlayWidgetExit(const std::string& WidgetName);

	/**
	 * @brief 특정 위젯의 애니메이션이 완료되었는지 확인
	 */
	bool IsWidgetAnimationDone(const std::string& WidgetName);

	/**
	 * @brief 멤버 위젯들(mb1~4)의 애니메이션이 모두 완료되었는지 확인
	 */
	bool AreMembersAnimationDone();

	/**
	 * @brief Tech 위젯들(phys, dx, power)의 애니메이션이 모두 완료되었는지 확인
	 */
	bool AreTechAnimationDone();

	/**
	 * @brief 리소스 정리
	 */
	void Cleanup();
};
