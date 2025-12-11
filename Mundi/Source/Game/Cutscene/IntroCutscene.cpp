#include "pch.h"
#include "IntroCutscene.h"
#include "Source/Game/UI/GameUIManager.h"
#include "Source/Game/UI/Widgets/UICanvas.h"
#include "Source/Game/UI/Widgets/UIWidget.h"
#include "Source/Game/UI/Widgets/TextureWidget.h"

bool UIntroCutscene::Start()
{
	// UI 에셋 로드
	Canvas = UGameUIManager::Get().LoadUIAsset("Data/UI/Intro.uiasset");
	if (!Canvas)
	{
		// 로드 실패 시 바로 완료 처리
		TransitionTo(EIntroPhase::Finished);
		return false;
	}

	// 위젯 초기화
	InitializeWidgets();

	// 첫 페이즈로 전환 (1초 대기)
	TransitionTo(EIntroPhase::InitialWait);

	return true;
}

void UIntroCutscene::Update(float DeltaTime)
{
	if (CurrentPhase == EIntroPhase::Idle || CurrentPhase == EIntroPhase::Finished)
	{
		return;
	}

	PhaseTime += DeltaTime;

	// Skip 위젯 깜빡임 업데이트
	SkipBlinkTime += DeltaTime;
	if (Canvas)
	{
		// 사인파로 부드러운 깜빡임 (0.3 ~ 1.0 범위)
		float BlinkAlpha = (sinf(SkipBlinkTime * SKIP_BLINK_SPEED * 2.0f * 3.14159f) + 1.0f) * 0.5f;
		float Opacity = SKIP_OPACITY_MIN + BlinkAlpha * (SKIP_OPACITY_MAX - SKIP_OPACITY_MIN);
		Canvas->SetWidgetOpacity("Skip", Opacity);
	}

	// 페이즈별 처리
	switch (CurrentPhase)
	{
	// === 대기 페이즈들 ===
	case EIntroPhase::InitialWait:
		if (PhaseTime >= WAIT_DURATION)
		{
			TransitionTo(EIntroPhase::GtlEnter);
		}
		break;

	case EIntroPhase::GtlWait:
		if (PhaseTime >= WAIT_DURATION)
		{
			TransitionTo(EIntroPhase::TechEnter);
		}
		break;

	case EIntroPhase::TechWait:
		if (PhaseTime >= TECH_WAIT_DURATION)
		{
			TransitionTo(EIntroPhase::TeamLabelEnter);
		}
		break;

	case EIntroPhase::TeamLabelWait:
		if (PhaseTime >= WAIT_DURATION)
		{
			TransitionTo(EIntroPhase::MembersEnter);
		}
		break;

	// === 애니메이션 페이즈들 (완료까지 대기) ===
	case EIntroPhase::GtlEnter:
		if (IsWidgetAnimationDone("gtl"))
		{
			TransitionTo(EIntroPhase::GtlExit);
		}
		break;

	case EIntroPhase::GtlExit:
		if (IsWidgetAnimationDone("gtl"))
		{
			TransitionTo(EIntroPhase::GtlWait);
		}
		break;

	case EIntroPhase::TechEnter:
		if (AreTechAnimationDone())
		{
			TransitionTo(EIntroPhase::TechExit);
		}
		break;

	case EIntroPhase::TechExit:
		if (AreTechAnimationDone())
		{
			TransitionTo(EIntroPhase::TechWait);
		}
		break;

	case EIntroPhase::TeamLabelEnter:
		if (IsWidgetAnimationDone("team_label"))
		{
			TransitionTo(EIntroPhase::TeamLabelExit);
		}
		break;

	case EIntroPhase::TeamLabelExit:
		if (IsWidgetAnimationDone("team_label"))
		{
			TransitionTo(EIntroPhase::TeamLabelWait);
		}
		break;

	case EIntroPhase::MembersEnter:
		if (AreMembersAnimationDone())
		{
			TransitionTo(EIntroPhase::MembersWait);
		}
		break;

	case EIntroPhase::MembersWait:
		if (PhaseTime >= MEMBERS_WAIT_DURATION)
		{
			TransitionTo(EIntroPhase::MembersExit);
		}
		break;

	case EIntroPhase::MembersExit:
		if (AreMembersAnimationDone())
		{
			TransitionTo(EIntroPhase::Finished);
		}
		break;

	default:
		break;
	}
}

void UIntroCutscene::Skip()
{
	Cleanup();
	TransitionTo(EIntroPhase::Finished);
}

void UIntroCutscene::TransitionTo(EIntroPhase NewPhase)
{

	CurrentPhase = NewPhase;
	PhaseTime = 0.0f;

	// 페이즈 진입 시 액션
	switch (NewPhase)
	{
	case EIntroPhase::InitialWait:
		break;

	case EIntroPhase::GtlEnter:
		PlayWidgetEnter("gtl");
		// SubUV 위젯도 함께 Enter Animation + SubUV 자동 재생 시작
		PlayWidgetEnter("SubUV");
		if (Canvas)
		{
			if (UUIWidget* SubUVWidget = Canvas->FindWidget("SubUV"))
			{
				if (UTextureWidget* TexWidget = dynamic_cast<UTextureWidget*>(SubUVWidget))
				{
					TexWidget->StartSubUVAnimation(10.0f, true);  // 10 FPS, 루프
				}
			}
		}
		break;

	case EIntroPhase::GtlExit:
		PlayWidgetExit("gtl");
		// SubUV 위젯도 함께 Exit Animation
		PlayWidgetExit("SubUV");
		break;

	case EIntroPhase::GtlWait:
		// SubUV 위젯 숨기고 애니메이션 정지
		if (Canvas)
		{
			if (UUIWidget* SubUVWidget = Canvas->FindWidget("SubUV"))
			{
				if (UTextureWidget* TexWidget = dynamic_cast<UTextureWidget*>(SubUVWidget))
				{
					TexWidget->StopSubUVAnimation();
				}
			}
			Canvas->SetWidgetVisible("SubUV", false);
		}
		break;

	case EIntroPhase::TechEnter:
		// phys, dx, power 동시 Enter
		PlayWidgetEnter("phys");
		PlayWidgetEnter("dx");
		PlayWidgetEnter("power");
		break;

	case EIntroPhase::TechExit:
		// phys, dx, power 동시 Exit
		PlayWidgetExit("phys");
		PlayWidgetExit("dx");
		PlayWidgetExit("power");
		break;

	case EIntroPhase::TechWait:
		// Tech 위젯들 숨기기
		if (Canvas)
		{
			Canvas->SetWidgetVisible("phys", false);
			Canvas->SetWidgetVisible("dx", false);
			Canvas->SetWidgetVisible("power", false);
		}
		break;

	case EIntroPhase::TeamLabelEnter:
		PlayWidgetEnter("team_label");
		break;

	case EIntroPhase::TeamLabelExit:
		break;

	case EIntroPhase::TeamLabelWait:
		break;

	case EIntroPhase::MembersEnter:
		// mb1 ~ mb4 동시 Enter
		PlayWidgetEnter("mb1");
		PlayWidgetEnter("mb2");
		PlayWidgetEnter("mb3");
		PlayWidgetEnter("mb4");
		break;

	case EIntroPhase::MembersWait:
		break;

	case EIntroPhase::MembersExit:
		// mb1 ~ mb4 동시 Exit
		PlayWidgetExit("team_label");
		PlayWidgetExit("mb1");
		PlayWidgetExit("mb2");
		PlayWidgetExit("mb3");
		PlayWidgetExit("mb4");
		break;

	case EIntroPhase::Finished:
		Cleanup();
		if (OnFinished)
		{
			OnFinished(Owner);
		}
		break;

	default:
		break;
	}
}

void UIntroCutscene::InitializeWidgets()
{
	if (!Canvas) return;

	// 모든 위젯 숨기기
	Canvas->SetWidgetVisible("gtl", false);
	Canvas->SetWidgetVisible("SubUV", false);
	Canvas->SetWidgetVisible("phys", false);
	Canvas->SetWidgetVisible("dx", false);
	Canvas->SetWidgetVisible("power", false);
	Canvas->SetWidgetVisible("team_label", false);
	Canvas->SetWidgetVisible("mb1", false);
	Canvas->SetWidgetVisible("mb2", false);
	Canvas->SetWidgetVisible("mb3", false);
	Canvas->SetWidgetVisible("mb4", false);

	// Skip 위젯은 처음부터 보이게 (깜빡임 시작)
	Canvas->SetWidgetVisible("Skip", true);
	SkipBlinkTime = 0.0f;
}

void UIntroCutscene::PlayWidgetEnter(const std::string& WidgetName)
{
	if (!Canvas) return;

	UUIWidget* Widget = Canvas->FindWidget(WidgetName);
	if (Widget)
	{
		Widget->bVisible = true;
		Widget->PlayEnterAnimation();
	}
}

void UIntroCutscene::PlayWidgetExit(const std::string& WidgetName)
{
	if (!Canvas) return;

	UUIWidget* Widget = Canvas->FindWidget(WidgetName);
	if (Widget)
	{
		Widget->PlayExitAnimation();
	}
}

bool UIntroCutscene::IsWidgetAnimationDone(const std::string& WidgetName)
{
	if (!Canvas)
	{
		return true;  // 캔버스 없으면 완료로 처리
	}

	UUIWidget* Widget = Canvas->FindWidget(WidgetName);
	if (!Widget)
	{
		return true;  // 위젯 없으면 완료로 처리
	}

	return !Widget->IsAnimating();
}

bool UIntroCutscene::AreMembersAnimationDone()
{
	return IsWidgetAnimationDone("mb1") &&
	       IsWidgetAnimationDone("mb2") &&
	       IsWidgetAnimationDone("mb3") &&
	       IsWidgetAnimationDone("mb4");
}

bool UIntroCutscene::AreTechAnimationDone()
{
	return IsWidgetAnimationDone("phys") &&
	       IsWidgetAnimationDone("dx") &&
	       IsWidgetAnimationDone("power");
}

void UIntroCutscene::Cleanup()
{
	if (Canvas)
	{
		UGameUIManager::Get().RemoveCanvas(CanvasName);
		Canvas = nullptr;
	}
}
