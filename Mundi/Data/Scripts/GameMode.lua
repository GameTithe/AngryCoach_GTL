-- GameMode.lua
-- 게임 흐름을 제어하는 스크립트
-- GameModeBase Actor에 LuaScriptComponent로 붙여서 사용

-- ============================================
-- 게임 흐름:
-- [최초 실행]
-- 1. PIE 시작 → OnMatchStart → OnIntroStart (인트로 컷신)
-- 2. 인트로 완료 → EndIntro() → OnStartPageStart (게임 스타트 화면)
-- 3. 버튼 클릭 → EndStartPage() → OnCharacterSelectStart (캐릭터 선택)
-- 4. [첫 매치만] Select UI 표시 → 라운드 선택 (1/2/3) → bFirstMatch = false
-- 5. 선택 완료 → EndCharacterSelect() → OnRoundStart (3-2-1 Ready Go)
-- 6. BattleUI 표시 + 전투 시작 (타이머 카운트다운)
-- 7. 승자 결정 → EndRound() → OnRoundEnd (GameSet)
-- 8. GameSet 표시 후 → StartCharacterSelect()
--    → bFirstMatch=false이므로 Select UI 스킵 → 바로 5번으로
--
-- [전투 루프] 5 → 6 → 7 → 8 → 5 → ... (누군가 RoundsToWin 달성까지)
-- [게임 종료] RoundsToWin 달성 시 → OnGameOver
-- ============================================

local bGameStarted = false
local bBattleActive = false  -- 전투 중인지 여부

-- UI Canvas 이름 (FindCanvas로 찾을 때 사용)
local introCanvasName = "Intro"
local startPageCanvasName = "Start_Page"
local tutorialCanvasName = "Tutorial"
local selectCanvasName = "Select"
local characterSelectCanvasName = "character_select"
local readyGoCanvasName = "ready_go"
local battleUICanvasName = "BattleUI"
local gameOverCanvasName = "game_over"
local gameSetCanvasName = "GameSet"

-- 첫 매치인지 여부 (라운드 선택 UI 표시 결정)
local bFirstMatch = true

-- 타이머 관련
local lastDisplayedTime = -1  -- 마지막으로 표시한 시간 (초 단위)
local bTimerShaking = false   -- 타이머 진동 중인지 여부
local TIMER_SHAKE_THRESHOLD = 10  -- 진동 시작 기준 시간 (초)

-- 애니메이션 지속 시간 (초)
local FADE_DURATION = 3.0

-- 라운드 표시 관련
local currentRoundsToWin = 2  -- 현재 설정된 우승 목표 라운드
local p1Wins = 0              -- P1 현재 승리 횟수
local p2Wins = 0              -- P2 현재 승리 횟수

-- 플레이어 체력 관련
local MAX_HP = 100
local p1HP = MAX_HP
local p2HP = MAX_HP
local currentRoundNumber = 0  -- 현재 라운드 번호

-- Portrait 진동용 이전 체력 비율 저장
local prevP1HealthRatio = 1.0
local prevP2HealthRatio = 1.0

-- ============================================
-- 악세사리 선택 관련
-- ============================================

-- 악세사리 목록 (인덱스 → 프리팹 경로)
local AccessoryList = {
    [1] = { name = "Knife",   prefab = "Data/Prefabs/FlowerKnife.prefab" },
    [2] = { name = "Cloak",   prefab = "Data/Prefabs/CloakAcce.prefab" },
    [3] = { name = "Gorilla", prefab = "Data/Prefabs/Gorilla.prefab" }
}

-- 현재 선택된 악세사리 인덱스 (1~3)
local p1SelectedAccessory = 1
local p2SelectedAccessory = 1

-- 플레이어 Ready 상태
local p1Ready = false
local p2Ready = false

-- 선택 단계 (false = 악세사리 선택, true = 라운드 선택)
local bRoundSelectionPhase = false

-- 코루틴 대기 함수 (스케줄러가 "wait_time" 태그를 기대함)
function WaitForSeconds(seconds)
    return "wait_time", seconds
end

-- ============================================
-- 라운드 표시 UI 관련 함수
-- ============================================

-- 새 매치 시작 시 승리 카운트 초기화
function ResetRoundWins()
    p1Wins = 0
    p2Wins = 0
    currentRoundNumber = 0
    print("[GameMode] Round wins reset: P1=0, P2=0, RoundNum=0")
end

-- 라운드 시작 시 플레이어 체력 초기화
function ResetPlayerHP()
    ResetPlayersHP()  -- C++ 함수 호출
    ResetPrevHealthRatios()  -- Portrait 진동용 이전 체력 비율 리셋
    print("[GameMode] Player HP reset")
end

-- 라운드 표시 위젯 설정 (BattleUI 로드 시마다 호출)
-- 현재 승리 상태를 위젯에 반영
function SetupRoundIndicators(canvas, roundsToWin)
    if not canvas then return end

    print("[GameMode] SetupRoundIndicators: roundsToWin=" .. roundsToWin .. ", P1Wins=" .. p1Wins .. ", P2Wins=" .. p2Wins)

    -- 모든 라운드 표시 위젯 설정
    for i = 1, 3 do
        local p1Name = "P1_R" .. i
        local p2Name = "P2_R" .. i

        if i <= roundsToWin then
            -- 활성화
            canvas:SetWidgetVisible(p1Name, true)
            canvas:SetWidgetVisible(p2Name, true)

            -- P1 승리 상태 반영 (i번째까지 이겼으면 Frame 1)
            if i <= p1Wins then
                canvas:SetTextureSubUVFrame(p1Name, 1)
            else
                canvas:SetTextureSubUVFrame(p1Name, 0)
            end

            -- P2 승리 상태 반영
            if i <= p2Wins then
                canvas:SetTextureSubUVFrame(p2Name, 1)
            else
                canvas:SetTextureSubUVFrame(p2Name, 0)
            end
        else
            -- 비활성화
            canvas:SetWidgetVisible(p1Name, false)
            canvas:SetWidgetVisible(p2Name, false)
        end
    end
end

-- 라운드 승리 표시 업데이트
-- winnerIndex: 0 = P1, 1 = P2
function UpdateRoundIndicator(canvas, winnerIndex)
    if not canvas then return end

    local widgetName = nil

    if winnerIndex == 0 then
        -- P1 승리: P1_R1 → P1_R2 → P1_R3 순서로 채움
        p1Wins = p1Wins + 1
        widgetName = "P1_R" .. p1Wins
        print("[GameMode] P1 wins round! " .. widgetName .. " -> Frame 1")
    elseif winnerIndex == 1 then
        -- P2 승리: P2_R1 → P2_R2 → P2_R3 순서로 채움
        p2Wins = p2Wins + 1
        widgetName = "P2_R" .. p2Wins
        print("[GameMode] P2 wins round! " .. widgetName .. " -> Frame 1")
    end

    if widgetName then
        -- SubUV 프레임 변경 (승리 상태로)
        canvas:SetTextureSubUVFrame(widgetName, 1)

        -- 깜빡임 + 크기 펄스 효과
        PlayRoundWinEffect(canvas, widgetName)
    end
end

-- 라운드 승리 위젯 효과 (페이드 깜빡임 + 진동)
-- NOTE: 크기 변경하면 원래 크기를 모르므로 페이드/진동만 사용
function PlayRoundWinEffect(canvas, widgetName)
    if not canvas then return end

    -- 진동 효과 (0.3초간)
    canvas:ShakeWidget(widgetName, 5, 0.3, 20, true)

    -- 코루틴으로 깜빡임 시퀀스 실행
    StartCoroutine(function()
        -- 깜빡임 3회
        for i = 1, 3 do
            local c = UI.FindCanvas(battleUICanvasName)
            if c then
                c:FadeWidget(widgetName, 0.2, 0.08, "Linear")
            end

            coroutine.yield(WaitForSeconds(0.08))

            c = UI.FindCanvas(battleUICanvasName)
            if c then
                c:FadeWidget(widgetName, 1.0, 0.08, "Linear")
            end

            coroutine.yield(WaitForSeconds(0.08))
        end

        print("[GameMode] Round win effect complete: " .. widgetName)
    end)
end

-- ============================================
-- 타이머 UI 업데이트
-- ============================================
function UpdateTimerUI(seconds)
    local canvas = UI.FindCanvas(battleUICanvasName)
    if not canvas then
        return
    end

    -- 소수점 버림
    local timeInt = math.floor(seconds)
    if timeInt < 0 then timeInt = 0 end
    if timeInt > 99 then timeInt = 99 end

    -- 십의 자리, 일의 자리 분리
    local tens = math.floor(timeInt / 10)
    local ones = timeInt % 10

    -- SubUV 프레임 설정 (0-9)
    canvas:SetTextureSubUVFrame("timer_tens", tens)
    canvas:SetTextureSubUVFrame("timer_ones", ones)
end

-- ============================================
-- 체력바 UI 업데이트
-- ============================================
function UpdateHealthBars()
    -- C++ 캐릭터에서 실제 체력 퍼센트 읽기
    local p1Ratio = GetP1HealthPercent()
    local p2Ratio = GetP2HealthPercent()

    local canvas = UI.FindCanvas(battleUICanvasName)
    if canvas then
        canvas:SetProgress("P1_HP", p1Ratio)
        canvas:SetProgress("P2_HP", p2Ratio)
    end

    return p1Ratio, p2Ratio
end

-- ============================================
-- Portrait 진동 효과 (피격 시)
-- 카메라 셰이크와 비슷하게 데미지에 비례한 진동
-- ============================================

-- 데미지 비율에 따른 Portrait 진동 강도 계산
-- 카메라 셰이크: StartCameraShake(0.3, 0.3, 0.3, DamageAmount)
-- DamageAmount: Light=5, Heavy=10, Skill=15
-- damageRatio: 0.05 (5%), 0.10 (10%), 0.15 (15%) 등
function GetPortraitShakeParams(damageRatio)
    -- 데미지 비율을 실제 데미지 수치로 환산 (MAX_HP 기준)
    local damageAmount = damageRatio * MAX_HP

    -- 카메라 셰이크와 비슷한 비율로 설정
    -- 진동 강도: 데미지에 비례 (5~15 데미지 → 3~10 픽셀)
    local intensity = math.max(3, math.min(15, damageAmount * 0.8))

    -- 지속 시간: 카메라와 동일하게 0.3초 기본, 큰 데미지는 좀 더 길게
    local duration = 0.3 + (damageAmount * 0.01)

    -- 진동 빈도: 데미지에 비례 (카메라 셰이크처럼)
    local frequency = math.max(10, math.min(25, damageAmount + 5))

    return intensity, duration, frequency
end

-- Portrait 진동 트리거
-- playerIndex: 1 = P1, 2 = P2
-- damageRatio: 받은 데미지 비율 (0.0 ~ 1.0)
function ShakePortraitOnDamage(playerIndex, damageRatio)
    local canvas = UI.FindCanvas(battleUICanvasName)
    if not canvas then return end

    -- 최소 데미지 임계값 (너무 작은 데미지는 무시)
    if damageRatio < 0.01 then return end

    local widgetName = (playerIndex == 1) and "P1_Portrait" or "P2_Portrait"
    local intensity, duration, frequency = GetPortraitShakeParams(damageRatio)

    -- 진동 시작 (감쇠 적용)
    canvas:ShakeWidget(widgetName, intensity, duration, frequency, true)

    print(string.format("[GameMode] Portrait shake: P%d, damage=%.1f%%, intensity=%.1f, duration=%.2f, freq=%.1f",
        playerIndex, damageRatio * 100, intensity, duration, frequency))
end

-- 이전 체력 비율 리셋 (라운드 시작 시 호출)
function ResetPrevHealthRatios()
    prevP1HealthRatio = 1.0
    prevP2HealthRatio = 1.0
end

-- ============================================
-- 타이머 진동 효과
-- 시간이 줄어들수록 강도 증가
-- ============================================
function GetTimerShakeIntensity(seconds)
    if seconds >= TIMER_SHAKE_THRESHOLD then
        return 0  -- 10초 이상이면 진동 없음
    elseif seconds >= 7 then
        return 3  -- 10~7초: 약한 진동
    elseif seconds >= 4 then
        return 6  -- 7~4초: 중간 진동
    else
        return 10 -- 4초 이하: 강한 진동
    end
end

function StartTimerShake(canvas, intensity)
    if not canvas then return end
    -- 무한 진동, 감쇠 없음 (계속 유지)
    canvas:ShakeWidget("timer_tens", intensity, 0, 15, false)
    canvas:ShakeWidget("timer_ones", intensity, 0, 15, false)
    bTimerShaking = true
    print("[GameMode] Timer shake started! Intensity: " .. intensity)
end

function UpdateTimerShake(canvas, intensity)
    if not canvas then return end
    -- 기존 진동 중지 후 새 강도로 재시작
    canvas:StopShake("timer_tens")
    canvas:StopShake("timer_ones")
    canvas:ShakeWidget("timer_tens", intensity, 0, 15, false)
    canvas:ShakeWidget("timer_ones", intensity, 0, 15, false)
end

function StopTimerShake(canvas)
    if not canvas then return end
    if bTimerShaking then
        canvas:StopShake("timer_tens")
        canvas:StopShake("timer_ones")
        bTimerShaking = false
        print("[GameMode] Timer shake stopped")
    end
end

-- ============================================
-- 카운트다운 숫자 표시 (잔상 효과 포함)
-- ============================================
function ShowCountdownNumber(canvas, number)
    if not canvas then return end

    -- 숫자 프레임 설정 (SubUV: 0=0, 1=1, 2=2, 3=3, ...)
    local frame = number

    -- 메인 숫자 설정 및 표시
    canvas:SetTextureSubUVFrame("countdown_main", frame)
    canvas:SetWidgetVisible("countdown_main", true)
    canvas:SetWidgetOpacity("countdown_main", 1.0)  -- 즉시 보이게
    canvas:SetWidgetSize("countdown_main", 100, 100)
    canvas:ResizeWidget("countdown_main", 140, 140, 0.15, "EaseOut")  -- 커지기

    -- 잔상1: 즉시 opacity 설정 후 커지면서 페이드아웃
    canvas:SetTextureSubUVFrame("countdown_ghost1", frame)
    canvas:SetWidgetVisible("countdown_ghost1", true)
    canvas:SetWidgetOpacity("countdown_ghost1", 0.6)  -- 즉시 설정
    canvas:SetWidgetSize("countdown_ghost1", 100, 100)
    canvas:ResizeWidget("countdown_ghost1", 200, 200, 0.4, "EaseOut")
    canvas:FadeWidget("countdown_ghost1", 0, 0.4, "EaseIn")  -- 페이드아웃

    -- 잔상2: 더 크게 커지면서 페이드아웃
    canvas:SetTextureSubUVFrame("countdown_ghost2", frame)
    canvas:SetWidgetVisible("countdown_ghost2", true)
    canvas:SetWidgetOpacity("countdown_ghost2", 0.3)  -- 즉시 설정
    canvas:SetWidgetSize("countdown_ghost2", 100, 100)
    canvas:ResizeWidget("countdown_ghost2", 280, 280, 0.5, "EaseOut")
    canvas:FadeWidget("countdown_ghost2", 0, 0.5, "EaseIn")  -- 페이드아웃
end

function HideCountdownNumber(canvas)
    if not canvas then return end

    -- 메인 숫자 작아지면서 페이드아웃
    canvas:ResizeWidget("countdown_main", 80, 80, 0.2, "EaseIn")
    canvas:FadeWidget("countdown_main", 0, 0.2, "EaseIn")
end

-- ============================================
-- ReadyGo 애니메이션 시퀀스 (코루틴)
-- ============================================
function ReadyGoSequence()
    print("[GameMode] ReadyGo Sequence started")

    -- 플레이어 체력 초기화
    ResetPlayerHP()

    -- BattleUI 먼저 로드 (카운트다운 중에도 체력바/타이머/라운드표시 보여야 함)
    local battleCanvas = UI.FindCanvas(battleUICanvasName)
    local bNewlyLoaded = false

    if not battleCanvas then
        battleCanvas = UI.LoadUIAsset("Data/UI/BattleUI.uiasset")
        bNewlyLoaded = true
        print("[GameMode] BattleUI newly loaded!")
    else
        print("[GameMode] BattleUI reused (already exists)")
    end

    if battleCanvas then
        -- 프리로드된 캔버스가 숨겨져 있을 수 있으므로 visible 설정
        battleCanvas:SetVisible(true)
        -- 항상 Enter 애니메이션 재생 (프리로드된 경우에도 위젯들이 화면 밖에 있을 수 있음)
        battleCanvas:PlayAllEnterAnimations()

        -- 라운드 표시 위젯 설정 (현재 승리 상태 반영)
        SetupRoundIndicators(battleCanvas, currentRoundsToWin)

        -- 타이머를 라운드 시간으로 초기화 (카운트다운 중에도 표시)
        local roundDuration = GetRoundDuration()  -- 라운드 총 시간
        UpdateTimerUI(roundDuration)
        lastDisplayedTime = math.floor(roundDuration)

        -- 체력바 UI 초기화 (100% 표시)
        UpdateHealthBars()
    end

    local canvas = UI.FindCanvas(readyGoCanvasName)
    if not canvas then
        print("[GameMode] ERROR: readyGoCanvas is nil!")
        return
    end

    -- === 3-2-1 카운트다운 ===
    -- 3
    print("[GameMode] Countdown: 3")
    ShowCountdownNumber(canvas, 3)
    coroutine.yield(WaitForSeconds(0.15))  -- 커지는 시간
    canvas = UI.FindCanvas(readyGoCanvasName)
    if canvas then
        canvas:ResizeWidget("countdown_main", 100, 100, 0.2, "EaseIn")  -- 원래 크기로
    end
    coroutine.yield(WaitForSeconds(0.65))  -- 나머지 대기
    canvas = UI.FindCanvas(readyGoCanvasName)
    HideCountdownNumber(canvas)
    coroutine.yield(WaitForSeconds(0.2))

    -- 2
    print("[GameMode] Countdown: 2")
    canvas = UI.FindCanvas(readyGoCanvasName)
    ShowCountdownNumber(canvas, 2)
    coroutine.yield(WaitForSeconds(0.15))
    canvas = UI.FindCanvas(readyGoCanvasName)
    if canvas then
        canvas:ResizeWidget("countdown_main", 100, 100, 0.2, "EaseIn")
    end
    coroutine.yield(WaitForSeconds(0.65))
    canvas = UI.FindCanvas(readyGoCanvasName)
    HideCountdownNumber(canvas)
    coroutine.yield(WaitForSeconds(0.2))

    -- 1
    print("[GameMode] Countdown: 1")
    canvas = UI.FindCanvas(readyGoCanvasName)
    ShowCountdownNumber(canvas, 1)
    coroutine.yield(WaitForSeconds(0.15))
    canvas = UI.FindCanvas(readyGoCanvasName)
    if canvas then
        canvas:ResizeWidget("countdown_main", 100, 100, 0.2, "EaseIn")
    end
    coroutine.yield(WaitForSeconds(0.65))
    canvas = UI.FindCanvas(readyGoCanvasName)
    HideCountdownNumber(canvas)
    coroutine.yield(WaitForSeconds(0.2))

    -- 카운트다운 위젯들 숨기기
    canvas = UI.FindCanvas(readyGoCanvasName)
    if canvas then
        canvas:SetWidgetVisible("countdown_main", false)
        canvas:SetWidgetVisible("countdown_ghost1", false)
        canvas:SetWidgetVisible("countdown_ghost2", false)
    end

    -- === Ready / Go ===
    -- Ready 등장
    print("[GameMode] Playing ready enter animation")
    canvas = UI.FindCanvas(readyGoCanvasName)
    if canvas then
        canvas:SetWidgetVisible("ready", true)
        canvas:PlayEnterAnimation("ready")
    end

    coroutine.yield(WaitForSeconds(0.7))

    -- Go 등장
    print("[GameMode] Playing go enter animation")
    canvas = UI.FindCanvas(readyGoCanvasName)
    if canvas then
        canvas:SetWidgetVisible("go", true)
        canvas:PlayEnterAnimation("go")
    end

    coroutine.yield(WaitForSeconds(0.7))

    -- 둘 다 퇴장
    print("[GameMode] Playing exit animations")
    canvas = UI.FindCanvas(readyGoCanvasName)
    if canvas then
        canvas:PlayExitAnimation("ready")
        canvas:PlayExitAnimation("go")
    end

    -- Exit 애니메이션 완료 대기 후 캔버스 제거
    coroutine.yield(WaitForSeconds(0.5))
    UI.RemoveCanvas(readyGoCanvasName)
    print("[GameMode] ReadyGo canvas removed")

    -- 전투 시작!
    print("[GameMode] Battle Start!")

    -- 마우스 커서 숨기기 (전투 중에는 필요 없음)
    InputManager:SetCursorVisible(false)
    print("[GameMode] Mouse cursor hidden for battle")

    -- 실제 전투 시작 (InProgress 상태로 전환 + 타이머 시작)
    BeginBattle()
    print("[GameMode] BeginBattle called - Timer started!")

    -- BattleUI는 이미 시퀀스 시작 시 로드됨, 타이머만 초기화
    local battleCanvas = UI.FindCanvas(battleUICanvasName)
    if battleCanvas then
        local timeRemaining = GetRoundTimeRemaining()
        UpdateTimerUI(timeRemaining)
        lastDisplayedTime = math.floor(timeRemaining)
        bBattleActive = true
    end

    print("[GameMode] ReadyGo Sequence complete")
end

-- ============================================
-- 캐릭터 선택 시퀀스 (코루틴) - 테스트용
-- ============================================
function CharacterSelectSequence()
    print("[GameMode] CharacterSelect Sequence started")

    -- 테스트: 2초 후 자동으로 선택 완료
    -- 실제 구현에서는 플레이어 입력을 기다림
    coroutine.yield(WaitForSeconds(2.0))

    print("[GameMode] CharacterSelect auto-complete (test)")
    EndCharacterSelect()  -- C++ 함수 호출 → OnCharacterSelectEnd → StartCountDown
end

-- ============================================
-- Lua 기본 콜백
-- ============================================
function BeginPlay()
    print("[GameMode] BeginPlay - Game initializing...")

    -- 기존 GlobalConfig 초기화 (호환성)
    GlobalConfig.GameState = "Playing"
    GlobalConfig.PlayerState = "Alive"

    bGameStarted = true
    bBattleActive = false
    bTimerShaking = false
    lastDisplayedTime = -1

    print("[GameMode] Game Started!")
end

function Tick(Delta)
    if not bGameStarted then
        return
    end

    -- 전투 중일 때만 처리
    if bBattleActive then
        local timeRemaining = GetRoundTimeRemaining()
        local currentTime = math.floor(timeRemaining)

        -- 초가 바뀌었을 때만 타이머 UI 업데이트 (성능 최적화)
        if currentTime ~= lastDisplayedTime then
            UpdateTimerUI(timeRemaining)
            lastDisplayedTime = currentTime

            local canvas = UI.FindCanvas(battleUICanvasName)

            -- 시간이 0이 되면 진동 중지
            -- (EndRound는 C++에서 자동 호출됨 → OnRoundEnd에서 GameSet 처리)
            if currentTime <= 0 then
                StopTimerShake(canvas)
            else
                -- 타이머 진동 체크 (0초 제외)
                local shakeIntensity = GetTimerShakeIntensity(currentTime)

                if shakeIntensity > 0 then
                    if not bTimerShaking then
                        -- 진동 시작 (처음 threshold 미만이 됐을 때)
                        StartTimerShake(canvas, shakeIntensity)
                    else
                        -- 강도 업데이트 (구간이 바뀌었을 때만)
                        local prevIntensity = GetTimerShakeIntensity(currentTime + 1)
                        if shakeIntensity ~= prevIntensity then
                            UpdateTimerShake(canvas, shakeIntensity)
                            print("[GameMode] Timer shake intensity changed: " .. shakeIntensity)
                        end
                    end
                end
            end
        end

        -- 체력바 UI 업데이트 및 KO 체크
        local p1Ratio, p2Ratio = UpdateHealthBars()

        -- Portrait 진동 체크 (체력이 감소했을 때)
        if p1Ratio < prevP1HealthRatio then
            local damageRatio = prevP1HealthRatio - p1Ratio
            ShakePortraitOnDamage(1, damageRatio)
        end
        if p2Ratio < prevP2HealthRatio then
            local damageRatio = prevP2HealthRatio - p2Ratio
            ShakePortraitOnDamage(2, damageRatio)
        end

        -- 이전 체력 비율 업데이트
        prevP1HealthRatio = p1Ratio
        prevP2HealthRatio = p2Ratio

        -- KO 체크 (한 쪽이라도 0 이하가 되면)
        if p1Ratio <= 0 or p2Ratio <= 0 then
            -- 전투 종료
            bBattleActive = false

            -- KO 슬로모션 효과 (3초간 50% 속도)
            SetSlomo(3.0, 0.5)
            print("[GameMode] KO Slow-motion activated! (3s at 50% speed)")

            -- 타이머 진동 중지
            local canvas = UI.FindCanvas(battleUICanvasName)
            StopTimerShake(canvas)

            -- 승자 결정 (0 = P1, 1 = P2)
            local winnerIndex
            if p1Ratio <= 0 and p2Ratio <= 0 then
                -- 동시 KO: 체력 많이 남은 쪽 승리, 같으면 P1 유리
                if p1Ratio >= p2Ratio then
                    winnerIndex = 0
                else
                    winnerIndex = 1
                end
            elseif p1Ratio <= 0 then
                winnerIndex = 1  -- P2 승리
            else
                winnerIndex = 0  -- P1 승리
            end

            print("[GameMode] KO! Winner=" .. winnerIndex)

            -- C++ EndRound 호출 → RoundState 변경 + OnRoundEnd Lua 콜백 자동 호출
            -- (라운드 승리 카운트, 매치 종료 여부는 C++에서 처리됨)
            EndRound(winnerIndex)
        end
    end
end


-- ============================================
-- UI 프리로드 시스템
-- ============================================

-- 프리로드할 UI 에셋 목록
local UI_ASSETS_TO_PRELOAD = {
    "Data/UI/Start_Page.uiasset",
    "Data/UI/Tutorial.uiasset",
    "Data/UI/Select.uiasset",
    "Data/UI/ready_go.uiasset",
    "Data/UI/BattleUI.uiasset",
    "Data/UI/GameSet.uiasset",
    "Data/UI/game_over.uiasset"
}

-- 모든 UI 에셋 프리로드 (PIE 시작 시 호출)
function PreloadAllUIAssets()
    print("[GameMode] Preloading all UI assets...")

    for _, assetPath in ipairs(UI_ASSETS_TO_PRELOAD) do
        local canvas = UI.LoadUIAsset(assetPath)
        if canvas then
            canvas:SetVisible(false)  -- 로드 후 숨김
            print("[GameMode] Preloaded: " .. assetPath)
        else
            print("[GameMode] WARNING: Failed to preload: " .. assetPath)
        end
    end

    print("[GameMode] UI preload complete!")
end

-- ============================================
-- GameMode 콜백 (C++에서 호출됨)
-- ============================================

-- 매치 시작 시 (전체 게임 시작)
function OnMatchStart()
    print("[GameMode Callback] ========== Match Started! ==========")

    -- UI 프리로드
    PreloadAllUIAssets()
end

-- 매치 종료 시
function OnMatchEnd()
    print("[GameMode Callback] ========== Match Ended! ==========")
end

-- ============================================
-- 인트로 컷신 (최초 1회)
-- 인트로 시퀀스는 C++ UIntroCutscene 클래스에서 처리됨
-- ============================================
function OnIntroStart()
    print("[GameMode Callback] >>> Intro Started <<<")
    -- C++에서 IntroCutscene이 자동으로 시작됨
    -- Lua에서는 추가 처리 불필요
end

function OnIntroEnd()
    print("[GameMode Callback] >>> Intro Ended <<<")
end

-- ============================================
-- 게임 스타트 화면 (최초 1회)
-- ============================================
function OnStartPageStart()
    print("[GameMode Callback] >>> Start Page Started <<<")

    -- Start_Page UI 로드
    print("[GameMode] Loading Start_Page.uiasset...")
    local canvas = UI.LoadUIAsset("Data/UI/Start_Page.uiasset")
    if canvas then
        print("[GameMode] Start_Page loaded!")
        canvas:PlayAllEnterAnimations()

        -- Ghost 위젯들 명시적으로 숨김 (PlayAllEnterAnimations 후에도 안 보이게)
        canvas:SetWidgetVisible("GameStart_ghost1", false)
        canvas:SetWidgetVisible("GameStart_ghost2", false)
        canvas:SetWidgetOpacity("GameStart_ghost1", 0)
        canvas:SetWidgetOpacity("GameStart_ghost2", 0)

        -- GameStart 버튼 클릭 이벤트 설정
        canvas:SetOnClick("GameStart", function()
            print("[GameMode] GameStart button clicked!")

            -- 버튼 클릭 시 잔상 효과
            local c = UI.FindCanvas(startPageCanvasName)
            if c then
                ShowButtonAfterimage(c, "GameStart")
            end

            -- 코루틴으로 대기 후 전환
            StartCoroutine(function()
                coroutine.yield(WaitForSeconds(0.5))

                -- Start_Page 제거
                UI.RemoveCanvas(startPageCanvasName)

                -- 게임 스타트 화면 완료 → 캐릭터 선택으로
                EndStartPage()
            end)
        end)

        -- Tutorial 버튼 클릭 이벤트 설정
        canvas:SetOnClick("Tutorial", function()
            print("[GameMode] Tutorial button clicked!")
            -- 코루틴으로 지연시켜서 콜백 실행 중 캔버스 삭제 방지
            StartCoroutine(function()
                coroutine.yield(WaitForSeconds(0.01))
                UI.RemoveCanvas(startPageCanvasName)
                OpenTutorial()
            end)
        end)

        -- Exit 버튼 클릭 이벤트 설정
        canvas:SetOnClick("Exit", function()
            print("[GameMode] Exit button clicked!")
            -- 코루틴으로 지연 후 종료 (콜백 실행 중 종료 방지)
            StartCoroutine(function()
                coroutine.yield(WaitForSeconds(0.01))
                ExitGame()  -- Editor: PIE 종료, StandAlone: exe 종료
            end)
        end)

        print("[GameMode] GameStart button callback set")
    else
        print("[GameMode] WARNING: Failed to load Start_Page.uiasset, auto-starting...")
        -- 폴백: Start_Page가 없으면 자동 시작
        EndStartPage()
    end
end

function OnStartPageEnd()
    print("[GameMode Callback] >>> Start Page Ended <<<")
end

-- ============================================
-- 튜토리얼 화면 (프리로드 사용)
-- ============================================
function OpenTutorial()
    print("[GameMode] Opening Tutorial...")

    -- 프리로드된 캔버스 찾기
    local canvas = UI.FindCanvas(tutorialCanvasName)
    if not canvas then
        print("[GameMode] Tutorial not preloaded, loading now...")
        canvas = UI.LoadUIAsset("Data/UI/Tutorial.uiasset")
    end

    if canvas then
        canvas:SetVisible(true)
        canvas:PlayAllEnterAnimations()
        print("[GameMode] Tutorial shown!")

        -- 뒤로가기/닫기 버튼 클릭 이벤트 설정
        canvas:SetOnClick("back", function()
            print("[GameMode] Tutorial back button clicked!")
            CloseTutorial()
        end)

        print("[GameMode] Tutorial back button callback set")
    else
        print("[GameMode] WARNING: Failed to find Tutorial.uiasset")
    end
end

function CloseTutorial()
    print("[GameMode] Closing Tutorial...")

    -- Tutorial UI 숨김
    local canvas = UI.FindCanvas(tutorialCanvasName)
    if canvas then
        canvas:SetVisible(false)
    end
    print("[GameMode] Tutorial hidden")

    -- Start_Page 다시 보이기
    OnStartPageStart()
end

-- ============================================
-- 캐릭터/악세사리/라운드 선택
-- ============================================
-- 라운드 선택 관련 변수
local selectedRoundIndex = 2  -- 기본값: 2 (3판 2선승)

function OnCharacterSelectStart()
    print("[GameMode Callback] >>> Character Select Started <<<")

    -- 전투 비활성화 (새 라운드 시작)
    bBattleActive = false
    lastDisplayedTime = -1

    -- Camera Fade In 효과 (검은 화면에서 5초간 밝아짐)
    local camManager = GetCameraManager()
    if camManager then
        camManager:StartFade(5.0, 1.0, 0.0)  -- Duration=5초, Alpha 1→0
    end

    -- 첫 매치일 때만 Select UI 표시
    if not bFirstMatch then
        -- 첫 매치가 아니면 바로 전투로 (선택 스킵)
        print("[GameMode] Skipping Select UI (not first match), going to battle...")
        EndCharacterSelect()
        return
    end

    -- Select UI 로드 - 첫 매치일 때만
    print("[GameMode] Loading Select.uiasset (first match)...")
    local canvas = UI.LoadUIAsset("Data/UI/Select.uiasset")
    if canvas then
        canvas:PlayAllEnterAnimations()
        print("[GameMode] Select UI loaded!")

        -- 초기값 설정
        p1SelectedAccessory = 1
        p2SelectedAccessory = 1
        p1Ready = false
        p2Ready = false
        bRoundSelectionPhase = false
        selectedRoundIndex = 2  -- 기본값: 2선승

        -- 초기 UI 상태 설정
        UpdateAccessoryUI(canvas)
        UpdateReadyUI(canvas)  -- Ready 위젯 초기 상태 (둘 다 false이므로 숨김)
        canvas:SetTextureSubUVFrame("round_to_win", selectedRoundIndex)

        -- 초기 악세사리 미리보기 장착
        EquipAccessoryToPlayer(1, AccessoryList[p1SelectedAccessory].prefab)
        EquipAccessoryToPlayer(2, AccessoryList[p2SelectedAccessory].prefab)

        -- 라운드 선택 화살표는 처음에 숨김 (둘 다 Ready 후에 표시)
        canvas:SetWidgetVisible("left_arrow", false)
        canvas:SetWidgetVisible("right_arrow", false)

        -- 키 입력 처리 코루틴 시작
        StartCoroutine(SelectionInputLoop)

        print("[GameMode] Selection started - P1:A/D+T, P2:Arrows+Numpad1")
    else
        print("[GameMode] WARNING: Failed to load Select.uiasset, using default")
        SetRoundsToWin(2)
        SetMaxRounds(3)
        EndCharacterSelect()
    end
end

-- ============================================
-- 통합 선택 UI (악세사리 + 라운드)
-- ============================================

-- 악세사리 UI 업데이트 (선택 상태 표시)
-- 인디케이터 위젯 (P1_Ind_1~3, P2_Ind_1~3) 사용: 선택된 인덱스만 visible
function UpdateAccessoryUI(canvas)
    if not canvas then return end

    -- P1 인디케이터: 선택된 인덱스만 visible
    for i = 1, 3 do
        local indName = "P1_Ind_" .. i
        canvas:SetWidgetVisible(indName, i == p1SelectedAccessory)
    end

    -- P2 인디케이터: 선택된 인덱스만 visible
    for i = 1, 3 do
        local indName = "P2_Ind_" .. i
        canvas:SetWidgetVisible(indName, i == p2SelectedAccessory)
    end
end

-- Ready 상태 UI 업데이트
-- P1_Ready, P2_Ready 위젯의 visible = ready 상태
function UpdateReadyUI(canvas)
    if not canvas then return end

    -- P1 Ready 표시 (visible = p1Ready)
    canvas:SetWidgetVisible("P1_Ready", p1Ready)

    -- P2 Ready 표시 (visible = p2Ready)
    canvas:SetWidgetVisible("P2_Ready", p2Ready)

    -- 둘 다 Ready면 라운드 선택 화살표 표시 (인덱스에 따라)
    if p1Ready and p2Ready then
        UpdateRoundArrows(canvas)
    end
end

-- 라운드 선택 화살표 업데이트 (인덱스에 따라 visible 설정)
function UpdateRoundArrows(canvas)
    if not canvas then return end
    -- left_arrow: 감소 가능할 때만 표시 (index > 1)
    -- right_arrow: 증가 가능할 때만 표시 (index < 3)
    canvas:SetWidgetVisible("left_arrow", selectedRoundIndex > 1)
    canvas:SetWidgetVisible("right_arrow", selectedRoundIndex < 3)
end

-- 통합 선택 입력 루프
function SelectionInputLoop()
    print("[GameMode] SelectionInputLoop started")

    -- 입력 쿨다운 (프레임 단위, ~60fps 기준 5프레임 ≈ 0.08초)
    local INPUT_COOLDOWN_FRAMES = 5
    local READY_COOLDOWN_FRAMES = 15  -- Ready 버튼은 더 긴 쿨다운 (토글 방지)
    local p1InputCooldown = 0
    local p2InputCooldown = 0
    local p1ReadyCooldown = 0
    local p2ReadyCooldown = 0
    local roundInputCooldown = 0

    while UI.FindCanvas(selectCanvasName) do
        local canvas = UI.FindCanvas(selectCanvasName)
        if not canvas then break end

        -- 쿨다운 감소 (매 프레임 1씩)
        if p1InputCooldown > 0 then p1InputCooldown = p1InputCooldown - 1 end
        if p2InputCooldown > 0 then p2InputCooldown = p2InputCooldown - 1 end
        if p1ReadyCooldown > 0 then p1ReadyCooldown = p1ReadyCooldown - 1 end
        if p2ReadyCooldown > 0 then p2ReadyCooldown = p2ReadyCooldown - 1 end
        if roundInputCooldown > 0 then roundInputCooldown = roundInputCooldown - 1 end

        -- ==========================================
        -- Phase 1: 악세사리 선택 (둘 다 Ready 전)
        -- ==========================================
        if not bRoundSelectionPhase then
            -- P1 악세사리 선택: A/D (Ready 전에만, 쿨다운 적용)
            if not p1Ready and p1InputCooldown <= 0 then
                local p1Changed = false
                if InputManager:IsKeyPressed("A") then
                    if p1SelectedAccessory > 1 then
                        p1SelectedAccessory = p1SelectedAccessory - 1
                        p1Changed = true
                    end
                elseif InputManager:IsKeyPressed("D") then
                    if p1SelectedAccessory < 3 then
                        p1SelectedAccessory = p1SelectedAccessory + 1
                        p1Changed = true
                    end
                end

                if p1Changed then
                    print("[GameMode] P1 accessory: " .. AccessoryList[p1SelectedAccessory].name)
                    UpdateAccessoryUI(canvas)
                    EquipAccessoryToPlayer(1, AccessoryList[p1SelectedAccessory].prefab)
                    p1InputCooldown = INPUT_COOLDOWN_FRAMES
                end
            end

            -- P2 악세사리 선택: 화살표 (Ready 전에만, 쿨다운 적용)
            if not p2Ready and p2InputCooldown <= 0 then
                local p2Changed = false
                if InputManager:IsKeyPressed(37) then  -- VK_LEFT
                    if p2SelectedAccessory > 1 then
                        p2SelectedAccessory = p2SelectedAccessory - 1
                        p2Changed = true
                    end
                elseif InputManager:IsKeyPressed(39) then  -- VK_RIGHT
                    if p2SelectedAccessory < 3 then
                        p2SelectedAccessory = p2SelectedAccessory + 1
                        p2Changed = true
                    end
                end

                if p2Changed then
                    print("[GameMode] P2 accessory: " .. AccessoryList[p2SelectedAccessory].name)
                    UpdateAccessoryUI(canvas)
                    EquipAccessoryToPlayer(2, AccessoryList[p2SelectedAccessory].prefab)
                    p2InputCooldown = INPUT_COOLDOWN_FRAMES
                end
            end

            -- P1 Ready: T키 (쿨다운 적용)
            local tKeyPressed = InputManager:IsKeyPressed("T")
            local numpad1Pressed = InputManager:IsKeyPressed(97)

            if p1ReadyCooldown <= 0 and tKeyPressed then
                p1Ready = not p1Ready
                print("[GameMode] P1 Ready toggled by T key: " .. tostring(p1Ready))
                print("[GameMode]   - T pressed: " .. tostring(tKeyPressed) .. ", Numpad1 pressed: " .. tostring(numpad1Pressed))
                UpdateReadyUI(canvas)
                p1ReadyCooldown = READY_COOLDOWN_FRAMES

                -- 둘 다 Ready면 라운드 선택 단계로
                if p1Ready and p2Ready then
                    bRoundSelectionPhase = true
                    print("[GameMode] Both players ready! Round selection enabled.")
                end
            end

            -- P2 Ready: Numpad 1 (VK_NUMPAD1 = 97, 쿨다운 적용)
            if p2ReadyCooldown <= 0 and numpad1Pressed then
                p2Ready = not p2Ready
                print("[GameMode] P2 Ready toggled by Numpad1: " .. tostring(p2Ready))
                print("[GameMode]   - T pressed: " .. tostring(tKeyPressed) .. ", Numpad1 pressed: " .. tostring(numpad1Pressed))
                UpdateReadyUI(canvas)
                p2ReadyCooldown = READY_COOLDOWN_FRAMES

                -- 둘 다 Ready면 라운드 선택 단계로
                if p1Ready and p2Ready then
                    bRoundSelectionPhase = true
                    print("[GameMode] Both players ready! Round selection enabled.")
                end
            end

        -- ==========================================
        -- Phase 2: 라운드 선택 (둘 다 Ready 후)
        -- ==========================================
        else
            -- 라운드 선택: A/D 또는 화살표 (쿨다운 적용)
            if roundInputCooldown <= 0 then
                local leftPressed = InputManager:IsKeyPressed("A") or InputManager:IsKeyPressed(37)
                local rightPressed = InputManager:IsKeyPressed("D") or InputManager:IsKeyPressed(39)

                if leftPressed and selectedRoundIndex > 1 then
                    selectedRoundIndex = selectedRoundIndex - 1
                    canvas:SetTextureSubUVFrame("round_to_win", selectedRoundIndex)
                    UpdateRoundArrows(canvas)
                    print("[GameMode] Round to win: " .. selectedRoundIndex)
                    roundInputCooldown = INPUT_COOLDOWN_FRAMES
                elseif rightPressed and selectedRoundIndex < 3 then
                    selectedRoundIndex = selectedRoundIndex + 1
                    canvas:SetTextureSubUVFrame("round_to_win", selectedRoundIndex)
                    UpdateRoundArrows(canvas)
                    print("[GameMode] Round to win: " .. selectedRoundIndex)
                    roundInputCooldown = INPUT_COOLDOWN_FRAMES
                end
            end

            -- 확정: Space 또는 Enter
            if InputManager:IsKeyPressed("Space") or InputManager:IsKeyPressed(13) then
                print("[GameMode] Selection confirmed!")
                FinishSelection()
                break
            end
        end

        coroutine.yield(WaitForSeconds(0.016))  -- ~60fps
    end

    print("[GameMode] SelectionInputLoop ended")
end

-- 선택 완료
function FinishSelection()
    print("[GameMode] FinishSelection called")
    print("[GameMode] P1 accessory: " .. AccessoryList[p1SelectedAccessory].name)
    print("[GameMode] P2 accessory: " .. AccessoryList[p2SelectedAccessory].name)
    print("[GameMode] Rounds to win: " .. selectedRoundIndex)

    -- 라운드 설정
    currentRoundsToWin = selectedRoundIndex
    SetRoundsToWin(selectedRoundIndex)
    SetMaxRounds(2 * selectedRoundIndex - 1)  -- 1->1, 2->3, 3->5

    -- 새 매치 시작 - 승리 카운트 초기화
    ResetRoundWins()

    -- 선택된 악세사리 장착
    EquipAccessoryToPlayer(1, AccessoryList[p1SelectedAccessory].prefab)
    EquipAccessoryToPlayer(2, AccessoryList[p2SelectedAccessory].prefab)

    -- Select UI 제거
    if UI.FindCanvas(selectCanvasName) then
        UI.RemoveCanvas(selectCanvasName)
    end

    -- 캐릭터 선택 완료 → 전투로
    EndCharacterSelect()
end

-- 캐릭터 선택 종료 시 (선택 완료 후 Ready/Go 전에)
function OnCharacterSelectEnd()
    print("[GameMode Callback] >>> Character Select Ended <<<")

    -- Select UI 제거 (혹시 남아있다면)
    if UI.FindCanvas(selectCanvasName) then
        UI.RemoveCanvas(selectCanvasName)
    end

    -- 캐릭터 선택 UI 제거
    if UI.FindCanvas(characterSelectCanvasName) then
        UI.RemoveCanvas(characterSelectCanvasName)
    end

    -- 첫 매치 완료 표시 (다음 라운드부터는 Select UI 스킵)
    bFirstMatch = false
end

-- 라운드 시작 시 (카운트다운 후)
function OnRoundStart(roundNumber)
    print("[GameMode Callback] Round " .. roundNumber .. " Started!")

    -- R키 카운트 리셋
    RKeyPressCount = 0

    -- 이전 캔버스 정리 (혹시 남아있을 경우)
    if UI.FindCanvas(readyGoCanvasName) then
        print("[GameMode] Removing old ready_go canvas...")
        UI.RemoveCanvas(readyGoCanvasName)
    end

    -- ReadyGo 캔버스 로드
    print("[GameMode] Loading ready_go.uiasset...")
    local canvas = UI.LoadUIAsset("Data/UI/ready_go.uiasset")
    print("[GameMode] LoadUIAsset returned: " .. tostring(canvas))
    if canvas then
        print("[GameMode] ready_go canvas loaded")

        -- 모든 위젯들을 처음에 숨김 (코루틴에서 순차적으로 등장시킴)
        canvas:SetWidgetVisible("ready", false)
        canvas:SetWidgetVisible("go", false)
        canvas:SetWidgetVisible("countdown_main", false)
        canvas:SetWidgetVisible("countdown_ghost1", false)
        canvas:SetWidgetVisible("countdown_ghost2", false)

        -- 코루틴으로 ReadyGo 시퀀스 시작 (3-2-1 카운트다운 포함)
        print("[GameMode] Starting ReadyGo coroutine...")
        StartCoroutine(ReadyGoSequence)
    else
        print("[GameMode] ERROR: Failed to load ready_go.uiasset!")
    end
end

-- ============================================
-- 라운드 종료 시퀀스 (코루틴)
-- GameSet 표시 후 1초 대기 → matchResult에 따라 다음 라운드 또는 GameOver
-- matchResult: -1 = 다음 라운드, 0 = P0 매치 승리, 1 = P1 매치 승리, 2 = 무승부
-- ============================================
function RoundEndSequence(roundNumber, winnerIndex, matchResult)
    print("[GameMode] RoundEnd Sequence started - Round " .. roundNumber .. ", matchResult=" .. matchResult)

    -- NOTE: BattleUI는 GameOver까지 유지 (삭제하지 않음)
    -- 라운드 표시 위젯이 계속 보여야 함

    -- GameSet UI 로드 및 표시 (BattleUI 위에 표시됨)
    local canvas = UI.LoadUIAsset("Data/UI/GameSet.uiasset")
    if canvas then
        canvas:PlayAllEnterAnimations()
        print("[GameMode] GameSet UI loaded!")
    else
        print("[GameMode] ERROR: Failed to load GameSet.uiasset!")
    end

    -- 1초 대기 (GameSet 표시)
    print("[GameMode] [1] Before first yield (1.0s)")
    coroutine.yield(WaitForSeconds(1.0))
    print("[GameMode] [2] After first yield")

    -- GameSet Exit 애니메이션
    canvas = UI.FindCanvas(gameSetCanvasName)
    if canvas then
        canvas:PlayAllExitAnimations()
        print("[GameMode] GameSet exit animation playing")
    end

    -- Exit 애니메이션 완료 대기
    print("[GameMode] [3] Before second yield (0.5s)")
    coroutine.yield(WaitForSeconds(0.5))
    print("[GameMode] [4] After second yield")

    -- GameSet 캔버스 제거
    UI.RemoveCanvas(gameSetCanvasName)
    print("[GameMode] GameSet canvas removed")

    -- 1초 대기 후 다음 처리
    print("[GameMode] [5] Before third yield (1.0s)")
    coroutine.yield(WaitForSeconds(1.0))
    print("[GameMode] [6] After third yield")

    -- matchResult에 따라 분기
    if matchResult >= 0 then
        -- 매치 종료: GameOver 처리
        -- matchResult: 0 = P0 Win, 1 = P1 Win (Lose), 2 = Draw
        local gameResult
        if matchResult == 0 then
            gameResult = 1  -- EGameResult::Win
            print("[GameMode] Match ended - P0 WINS! Showing GameOver...")
        elseif matchResult == 1 then
            gameResult = 2  -- EGameResult::Lose
            print("[GameMode] Match ended - P1 WINS! Showing GameOver...")
        else
            gameResult = 3  -- EGameResult::Draw
            print("[GameMode] Match ended - DRAW! Showing GameOver...")
        end
        -- Lua에서 직접 OnGameOver 호출 (C++ HandleGameOver 대신)
        OnGameOver(gameResult)
    else
        -- 다음 라운드로 진행
        print("[GameMode] Starting next round via StartCharacterSelect...")
        StartCharacterSelect()
        print("[GameMode] [7] StartCharacterSelect called!")
    end
end

-- 라운드 종료 시
-- matchResult: -1 = 다음 라운드, 0 = P0 매치 승리, 1 = P1 매치 승리, 2 = 무승부
function OnRoundEnd(roundNumber, winnerIndex, matchResult)
    print("[GameMode] OnRoundEnd called! Round=" .. roundNumber .. " Winner=" .. winnerIndex .. " MatchResult=" .. matchResult)

    -- 전투 비활성화
    bBattleActive = false

    -- 마우스 커서 다시 보이게 (UI 조작 필요)
    InputManager:SetCursorVisible(true)
    print("[GameMode] Mouse cursor shown for UI")

    -- 타이머 진동 중지
    local battleCanvas = UI.FindCanvas(battleUICanvasName)
    StopTimerShake(battleCanvas)

    -- 라운드 승리 표시 업데이트 (BattleUI에 표시)
    if battleCanvas and winnerIndex >= 0 then
        UpdateRoundIndicator(battleCanvas, winnerIndex)
    end

    -- RoundEnd 시퀀스 코루틴 시작 (GameSet 표시 → 대기 → 다음 라운드 또는 GameOver)
    print("[GameMode] Starting RoundEndSequence coroutine...")
    local handle = StartCoroutine(function()
        print("[GameMode] Wrapper coroutine started!")
        RoundEndSequence(roundNumber, winnerIndex, matchResult)
        print("[GameMode] Wrapper coroutine finished!")
    end)
    print("[GameMode] StartCoroutine returned, handle=" .. tostring(handle))
end

-- 게임 오버 시
-- result: 1=Win(P1), 2=Lose(P2), 3=Draw
function OnGameOver(result)
    print("[GameMode Callback] ========== Game Over! Result: " .. result .. " ==========")

    -- 전투 비활성화
    bBattleActive = false

    -- 캔버스 정리 (BattleUI는 유지 - 라운드 표시가 보여야 함)
    if UI.FindCanvas(characterSelectCanvasName) then
        UI.RemoveCanvas(characterSelectCanvasName)
    end
    if UI.FindCanvas(readyGoCanvasName) then
        UI.RemoveCanvas(readyGoCanvasName)
    end

    -- 게임 오버 UI 로드
    local canvas = UI.LoadUIAsset("Data/UI/game_over.uiasset")
    if canvas then
        print("[GameMode] Game Over UI loaded!")

        -- 모든 위젯 숨김 (시퀀스에서 순차적으로 표시)
        canvas:SetWidgetVisible("game", false)
        canvas:SetWidgetVisible("over", false)
        canvas:SetWidgetVisible("P1", false)
        canvas:SetWidgetVisible("P2", false)
        canvas:SetWidgetVisible("Win", false)
        canvas:SetWidgetVisible("restart_btn", false)

        -- Ghost 위젯들도 숨김
        canvas:SetWidgetVisible("P1_ghost1", false)
        canvas:SetWidgetVisible("P1_ghost2", false)
        canvas:SetWidgetVisible("P2_ghost1", false)
        canvas:SetWidgetVisible("P2_ghost2", false)
        canvas:SetWidgetVisible("Win_ghost1", false)
        canvas:SetWidgetVisible("Win_ghost2", false)

        -- 재시작 버튼 클릭 이벤트 설정 (미리 설정)
        canvas:SetOnClick("restart_btn", function()
            print("[GameMode] Restart button clicked!")

            StartCoroutine(function()
                coroutine.yield(WaitForSeconds(0.01))

                -- Exit animation 재생
                local goCanvas = UI.FindCanvas(gameOverCanvasName)
                if goCanvas then
                    goCanvas:PlayAllExitAnimations()
                    print("[GameMode] GameOver exit animation playing")
                end

                local battleCanvas = UI.FindCanvas(battleUICanvasName)
                if battleCanvas then
                    battleCanvas:PlayAllExitAnimations()
                    print("[GameMode] BattleUI exit animation playing")
                end

                -- Exit animation 완료 대기
                coroutine.yield(WaitForSeconds(0.5))

                -- GameOver UI 제거
                UI.RemoveCanvas(gameOverCanvasName)
                -- BattleUI 제거
                if UI.FindCanvas(battleUICanvasName) then
                    UI.RemoveCanvas(battleUICanvasName)
                end
                -- 매치 재시작
                RestartMatch()
            end)
        end)

        -- 커스텀 시퀀스 시작
        StartCoroutine(function()
            GameOverSequence(result)
        end)
    else
        print("[GameMode] WARNING: Failed to load game_over.uiasset")
    end
end

-- ============================================
-- 잔상 효과 함수 (범용)
-- ============================================

-- 버튼/위젯에 잔상 효과 적용 (ghost1, ghost2 위젯 필요)
-- ghostPrefix: 잔상 위젯 접두어
-- scaleFactor1, scaleFactor2: 확대 배율 (기본값 1.5, 2.0)
function ShowButtonAfterimage(canvas, ghostPrefix, scaleFactor1, scaleFactor2)
    if not canvas then return end
    scaleFactor1 = scaleFactor1 or 1.5
    scaleFactor2 = scaleFactor2 or 2.0

    -- 잔상1: 원본 상태로 복원 후 중점 기준으로 커지면서 페이드아웃
    local ghost1 = ghostPrefix .. "_ghost1"
    canvas:RestoreWidgetOriginal(ghost1)  -- uiasset 로드 시점의 위치/크기로 복원 (스케일링 포함)
    canvas:SetWidgetVisible(ghost1, true)
    canvas:SetWidgetOpacity(ghost1, 0.6)
    -- ScaleWidgetCentered: 현재 크기의 배수로 확대 (스케일링 문제 없음)
    canvas:ScaleWidgetCentered(ghost1, scaleFactor1, scaleFactor1, 0.4, "EaseOut")
    canvas:FadeWidget(ghost1, 0, 0.4, "EaseIn")

    -- 잔상2: 원본 상태로 복원 후 중점 기준으로 더 크게 커지면서 페이드아웃
    local ghost2 = ghostPrefix .. "_ghost2"
    canvas:RestoreWidgetOriginal(ghost2)
    canvas:SetWidgetVisible(ghost2, true)
    canvas:SetWidgetOpacity(ghost2, 0.3)
    canvas:ScaleWidgetCentered(ghost2, scaleFactor2, scaleFactor2, 0.5, "EaseOut")
    canvas:FadeWidget(ghost2, 0, 0.5, "EaseIn")

    print("[GameMode] Button afterimage effect played for " .. ghostPrefix)
end

-- ============================================
-- 게임 오버 잔상 효과 함수
-- ============================================
-- 위젯별 크기 (하드코딩)
local WINNER_WIDGET_SIZES = {
    P1 = { w = 400, h = 200 },
    P2 = { w = 400, h = 200 },
    Win = { w = 600, h = 200 }
}

function ShowWinnerAfterimage(canvas, widgetName, ghostPrefix)
    if not canvas then return end

    -- 원본 위젯의 크기 (하드코딩)
    local size = WINNER_WIDGET_SIZES[widgetName]
    local origW = size and size.w or 400
    local origH = size and size.h or 200

    -- 잔상1: opacity 0.6으로 시작, 커지면서 페이드아웃
    local ghost1 = ghostPrefix .. "_ghost1"
    canvas:SetWidgetVisible(ghost1, true)
    canvas:SetWidgetOpacity(ghost1, 0.6)
    canvas:SetWidgetSize(ghost1, origW, origH)
    canvas:ResizeWidget(ghost1, origW * 1.5, origH * 1.5, 0.4, "EaseOut")
    canvas:FadeWidget(ghost1, 0, 0.4, "EaseIn")

    -- 잔상2: opacity 0.3으로 시작, 더 크게 커지면서 페이드아웃
    local ghost2 = ghostPrefix .. "_ghost2"
    canvas:SetWidgetVisible(ghost2, true)
    canvas:SetWidgetOpacity(ghost2, 0.3)
    canvas:SetWidgetSize(ghost2, origW, origH)
    canvas:ResizeWidget(ghost2, origW * 2.0, origH * 2.0, 0.5, "EaseOut")
    canvas:FadeWidget(ghost2, 0, 0.5, "EaseIn")

    print("[GameMode] Afterimage effect played for " .. widgetName)
end

-- 게임 오버 ghost 위젯들 초기화 (숨기기)
function InitGameOverGhosts(canvas)
    if not canvas then return end

    canvas:SetWidgetVisible("P1_ghost1", false)
    canvas:SetWidgetVisible("P1_ghost2", false)
    canvas:SetWidgetVisible("P2_ghost1", false)
    canvas:SetWidgetVisible("P2_ghost2", false)
    canvas:SetWidgetVisible("Win_ghost1", false)
    canvas:SetWidgetVisible("Win_ghost2", false)
end

-- 게임 오버 애니메이션 시퀀스
function GameOverSequence(result)
    local canvas = UI.FindCanvas(gameOverCanvasName)
    if not canvas then return end

    print("[GameMode] GameOver Sequence started")

    -- Ghost 위젯들 초기화 (숨기기)
    InitGameOverGhosts(canvas)

    -- 1. "game"과 "over" Enter Animation
    canvas:SetWidgetVisible("game", true)
    canvas:SetWidgetVisible("over", true)
    canvas:PlayEnterAnimation("game")
    canvas:PlayEnterAnimation("over")
    print("[GameMode] game/over enter animation playing")

    -- Enter animation 완료 대기
    coroutine.yield(WaitForSeconds(2.0))

    -- 2. 3번 반짝임
    for i = 1, 3 do
        canvas = UI.FindCanvas(gameOverCanvasName)
        if not canvas then return end
        canvas:SetWidgetVisible("game", false)
        canvas:SetWidgetVisible("over", false)
        coroutine.yield(WaitForSeconds(0.15))

        canvas = UI.FindCanvas(gameOverCanvasName)
        if not canvas then return end
        canvas:SetWidgetVisible("game", true)
        canvas:SetWidgetVisible("over", true)
        coroutine.yield(WaitForSeconds(0.15))
    end
    print("[GameMode] game/over blink 3 times done")

    -- 3. "game"과 "over" Exit Animation
    canvas = UI.FindCanvas(gameOverCanvasName)
    if not canvas then return end
    canvas:PlayExitAnimation("game")
    canvas:PlayExitAnimation("over")
    print("[GameMode] game/over exit animation playing")

    -- Exit animation 완료 대기
    coroutine.yield(WaitForSeconds(2.0))

    -- 4. P1 또는 P2 + Win Enter Animation
    canvas = UI.FindCanvas(gameOverCanvasName)
    if not canvas then return end

    -- result: 1=P1 승리(왼쪽), 2=P2 승리(오른쪽), 3=Draw
    local winnerWidget = nil
    if result == 1 then
        -- P1 승리 (왼쪽)
        canvas:SetWidgetVisible("P1", true)
        canvas:SetWidgetVisible("P2", false)
        canvas:SetWidgetVisible("Win", true)
        canvas:PlayEnterAnimation("P1")
        canvas:PlayEnterAnimation("Win")
        winnerWidget = "P1"
        print("[GameMode] P1 + Win enter animation playing")
    elseif result == 2 then
        -- P2 승리 (오른쪽)
        canvas:SetWidgetVisible("P1", false)
        canvas:SetWidgetVisible("P2", true)
        canvas:SetWidgetVisible("Win", true)
        canvas:PlayEnterAnimation("P2")
        canvas:PlayEnterAnimation("Win")
        winnerWidget = "P2"
        print("[GameMode] P2 + Win enter animation playing")
    else
        -- Draw (둘 다 표시)
        canvas:SetWidgetVisible("P1", true)
        canvas:SetWidgetVisible("P2", true)
        canvas:SetWidgetVisible("Win", false)
        canvas:PlayEnterAnimation("P1")
        canvas:PlayEnterAnimation("P2")
        print("[GameMode] Draw - P1 + P2 enter animation playing")
    end

    -- Enter animation 완료 + 0.3초 여유
    coroutine.yield(WaitForSeconds(0.6))

    -- 4.5. 잔상 효과 (Enter 애니메이션 끝난 후)
    canvas = UI.FindCanvas(gameOverCanvasName)
    if canvas then
        if winnerWidget then
            ShowWinnerAfterimage(canvas, winnerWidget, winnerWidget)
            ShowWinnerAfterimage(canvas, "Win", "Win")
        else
            -- Draw: P1, P2 둘 다 잔상
            ShowWinnerAfterimage(canvas, "P1", "P1")
            ShowWinnerAfterimage(canvas, "P2", "P2")
        end
    end

    -- 잔상 효과 완료 대기
    coroutine.yield(WaitForSeconds(1.5))

    -- 5. Restart 버튼 Enter Animation
    canvas = UI.FindCanvas(gameOverCanvasName)
    if not canvas then return end

    canvas:SetWidgetVisible("restart_btn", true)
    canvas:PlayEnterAnimation("restart_btn")
    print("[GameMode] restart_btn enter animation playing")

    print("[GameMode] GameOver Sequence complete")

    -- C++ 게임 상태 업데이트 (EndMatch 호출)
    EndMatch()
    print("[GameMode] EndMatch called - GameState set to GameOver")
end

-- 매치 재시작 시 (GameOver 후 재시작 버튼 클릭)
function OnMatchRestart()
    print("[GameMode Callback] ========== Match Restarted! ==========")
    -- 상태 초기화
    bBattleActive = false
    bTimerShaking = false
    lastDisplayedTime = -1

    -- 새 매치이므로 Select UI 다시 표시하도록 플래그 리셋
    bFirstMatch = true

    -- 승리 카운트 초기화
    ResetRoundWins()
end
