// C&C 240 local engine changes, 2026.
// GPL-3.0-or-later with the additional EA terms in LICENSE.md.
#pragma once
#include "Common/Cnc240BattlePerformance.h"
#ifndef FLUID_ENABLE_DEVELOPMENT_PROBES
namespace cnc240 { class TimingCheck { public: bool update() { return false; } }; }
#else
#include "Common/Cnc240TrainProbe.h"
#include "GameClient/CampaignManager.h"
#include "GameClient/ChallengeGenerals.h"
#include "Common/PlayerTemplate.h"
#include "Common/MessageStream.h"
#include "GameNetwork/GameInfo.h"
#include "Common/RandomValue.h"
#include "Common/Cnc240RenderStats.h"
#include "GameClient/View.h"
#include "Common/Cnc240SimulationProbe.h"
#include "Common/Cnc240CombatProbe.h"
#include "Common/Cnc240FocusedGameplayProbe.h"
#include "Common/Cnc240DamageProbe.h"
#include "Common/Cnc240BattleProbe.h"
#include "Common/Cnc240DisplayMode.h"
#include "GameClient/ClientInstance.h"
#include "GameClient/Mouse.h"
#include "Common/Cnc240TextClock.h"
extern HWND ApplicationHWnd;
extern bool DX8Wrapper_IsWindowed;

namespace cnc240
{
    // Opt-in bounded runtime check. Normal launches never enable this.
    class TimingCheck
    {
    public:
        TimingCheck() : m_start(GetTickCount()), m_lastReport(m_start), m_renders(0), m_logic(0),
                        m_lastLogic(TheGameLogic->getFrame()), m_presentStart(getPresentedFrameCount()),
                        m_seconds(0), m_started(false), m_launchPending(false), m_crcReported(false), m_zoomCheck(false), m_zoomRequested(false), m_zoomReported(false), m_coverageStage(0)
        {
            char value[16] = {};
            const DWORD length = GetEnvironmentVariableA("CNC240_VERIFY_SECONDS", value, sizeof(value));
            if (length > 0 && length < sizeof(value))
                m_seconds = atoi(value);
            if (m_seconds < 1 || m_seconds > 120) m_seconds = 0;
            char zoomValue[4] = {};
            m_zoomCheck = m_seconds && GetEnvironmentVariableA("CNC240_VERIFY_ZOOM", zoomValue, sizeof(zoomValue)) == 1 && zoomValue[0] == '1';
            m_scenario[0] = '\0';
            if (m_seconds && GetEnvironmentVariableA("CNC240_VERIFY_SCENARIO", m_scenario, sizeof(m_scenario)) >= sizeof(m_scenario))
                m_scenario[0] = '\0';
        }

        bool update()
        {
            if (!m_seconds) return false;
            static bool inputReported = false;
            if (!inputReported && boundedInputIsolated()) {
                inputReported = true;
                printf("CNC240_INPUT_ISOLATED enabled=1 captured=%d\n", TheMouse && TheMouse->isCursorCaptured());
                fflush(stdout);
            }
            if(m_launchPending && TheGameLogic->isInGame() && !TheGameLogic->isLoadingMap() && TheGameLogic->getFirstObject()) {
                m_launchPending=false;m_started=true;
                m_start=m_lastReport=GetTickCount();m_renders=m_logic=0;m_lastLogic=TheGameLogic->getFrame();m_presentStart=getPresentedFrameCount();
                printf("CNC240_SCENE_STARTED campaign=%s mission=%s map=%s speed=%d actual_hz=%d\n",m_scenario,
                    TheCampaignManager->getCurrentMission()->m_name.str(),TheCampaignManager->getCurrentMap().str(),
                    TheFramePacer->getLogicTimeScaleFps(),TheFramePacer->getActualLogicTimeScaleFps());fflush(stdout);
            }
            if (!m_started && !m_launchPending && m_scenario[0] && GetTickCount() - m_start >= 1000)
            {
                m_started = true;
                if (strcmp(m_scenario, "skirmish") == 0)
                {
                    if (!TheSkirmishGameInfo) TheSkirmishGameInfo = NEW SkirmishGameInfo;
                    TheSkirmishGameInfo->init();
                    TheSkirmishGameInfo->clearSlotList();
                    TheSkirmishGameInfo->enterGame();
                    TheSkirmishGameInfo->setLocalIP(0);
                    TheSkirmishGameInfo->setSeed(12345);
                    GameSlot *human = TheSkirmishGameInfo->getSlot(0);
                    human->setState(SLOT_PLAYER, UnicodeString(L"Cnc240 verification"));
                    human->setPlayerTemplate(2); // Stock FactionAmerica, after civilian/observer.
                    human->setColor(0);
                    human->setStartPos(0);
                    human->setAccept();
                    human->setMapAvailability(TRUE);
                    const bool battle=performanceProbeEnabled();
                    GameSlot *ai = TheSkirmishGameInfo->getSlot(1);
                    ai->setState(SLOT_MED_AI);
                    ai->setPlayerTemplate(3); // Stock FactionChina.
                    ai->setColor(1);
                    ai->setStartPos(1);
                    if(battle)for(int i=1;i<8;++i){GameSlot *slot=TheSkirmishGameInfo->getSlot(i);slot->setState(SLOT_MED_AI);slot->setPlayerTemplate(2+i%3);slot->setColor(i);slot->setStartPos(i);}
                    TheSkirmishGameInfo->setMap(battle?"Maps\\Twilight Flame\\Twilight Flame.map":"Maps\\Alpine Assault\\Alpine Assault.map");
                    TheSkirmishGameInfo->startGame(0);
                    TheWritableGlobalData->m_mapName = TheSkirmishGameInfo->getMap();
                    InitRandom(12345);
                    TheGameLogic->prepareNewGame(GAME_SKIRMISH, DIFFICULTY_NORMAL, 0);
                }
                else
                {
                    const char *mission=std::getenv("CNC240_VERIFY_MISSION");
                    if(mission && *mission){AsciiString selected(mission);selected.toLower();TheCampaignManager->setCampaignAndMission(m_scenario,selected);}
                    else TheCampaignManager->setCampaign(m_scenario);
                    if (TheCampaignManager->getCurrentMap().isEmpty())
                    {
                        printf("CNC240_SCENARIO_ERROR unknown campaign=%s\n", m_scenario);
                        fflush(stdout);
                        return true;
                    }
                    TheCampaignManager->setGameDifficulty(DIFFICULTY_NORMAL);
                    TheWritableGlobalData->m_pendingFile = TheCampaignManager->getCurrentMap();
                    InitRandom(0);
                    if(std::getenv("CNC240_VERIFY_SCENE") && boundedInputIsolated()) {
#if RTS_ZEROHOUR
                        if(TheCampaignManager->getCurrentCampaign()->isChallengeCampaign()) {
                            if(!TheChallengeGameInfo)TheChallengeGameInfo=NEW SkirmishGameInfo;
                            TheChallengeGameInfo->init();TheChallengeGameInfo->clearSlotList();TheChallengeGameInfo->reset();TheChallengeGameInfo->enterGame();TheChallengeGameInfo->setSeed(0);
                            // Mirror the actual Challenge menu's persona selection. Campaign
                            // PlayerFaction metadata is not populated for every general.
                            const GeneralPersona *persona=TheChallengeGenerals->getPlayerGeneralByCampaignName(m_scenario);
                            const int num=persona?ThePlayerTemplateStore->getTemplateNumByName(persona->getPlayerTemplateName()):-1;
                            const PlayerTemplate *player=ThePlayerTemplateStore->getNthPlayerTemplate(num);
                            if(!player){printf("CNC240_SCENARIO_ERROR challenge player\n");return true;}
                            GameSlot slot;slot.setState(SLOT_PLAYER,player->getDisplayName());slot.setPlayerTemplate(num);slot.setStartPos(0);slot.setColor(0);
                            TheChallengeGameInfo->setSlot(0,slot);TheChallengeGameInfo->setMap(TheCampaignManager->getCurrentMap());
                            TheChallengeGenerals->setCurrentPlayerTemplateNum(num);
                            TheChallengeGenerals->setCurrentDifficulty(DIFFICULTY_NORMAL);
                        }
#endif
                        GameMessage *msg=TheMessageStream->appendMessage(GameMessage::MSG_NEW_GAME);
                        msg->appendIntegerArgument(GAME_SINGLE_PLAYER);msg->appendIntegerArgument(DIFFICULTY_NORMAL);msg->appendIntegerArgument(0);
                        msg->appendIntegerArgument(BaseFps);
                        m_launchPending=true;m_started=false;return false;
                    }
                    TheGameLogic->prepareNewGame(GAME_SINGLE_PLAYER, DIFFICULTY_NORMAL, 0);
                }
                TheGameLogic->startNewGame(FALSE);
                printf("CNC240_SCENARIO started=%s\n", m_scenario);
                fflush(stdout);
                m_start = m_lastReport = GetTickCount();
                m_renders = m_logic = 0;
                m_lastLogic = TheGameLogic->getFrame();
                m_presentStart = getPresentedFrameCount();
            }
            // The battle fixture runs on native simulation boundaries in GameLogic.
            m_simProbe.update(m_started && strcmp(m_scenario, "skirmish") == 0 && std::getenv("CNC240_VERIFY_SIMULATION") != nullptr);
            m_damageProbe.update(m_started && strcmp(m_scenario,"skirmish")==0 && boundedInputIsolated());
            m_focusedGameplay.update(m_started && strcmp(m_scenario,"skirmish")==0 && boundedInputIsolated() && std::getenv("CNC240_VERIFY_GAMEPLAY"));
            m_combatProbe.update(m_started && strcmp(m_scenario,"skirmish")==0 && boundedInputIsolated() && std::getenv("CNC240_VERIFY_COMBAT"));
            updateTrainProbe(m_started && strcmp(m_scenario,"skirmish")==0 && std::getenv("CNC240_VERIFY_TRAIN"));
            if (m_started && boundedInputIsolated() && std::getenv("CNC240_VERIFY_BACKGROUND") && ApplicationHWnd && cnc240::displayMode()!=cnc240::Fullscreen) {
                static bool deactivated=false,reported=false;static unsigned startFrame=0;static DWORD startTime=0;
                if(!deactivated && GetTickCount()-m_start>=3000) {
                    deactivated=true;startFrame=TheGameLogic->getFrame();startTime=GetTickCount();
                    SendMessage(ApplicationHWnd,WM_ACTIVATEAPP,FALSE,0);
                    SendMessage(ApplicationHWnd,WM_ACTIVATE,WA_INACTIVE,0);
                    SendMessage(ApplicationHWnd,WM_KILLFOCUS,0,0);
                }
                if(deactivated && !reported && GetTickCount()-startTime>=3000) {
                    reported=true;
                    printf("CNC240_BACKGROUND elapsed_ms=%lu ticks=%u iconic=%d foreground=%d device_windowed=%d captured=%d\n",
                        GetTickCount()-startTime,TheGameLogic->getFrame()-startFrame,IsIconic(ApplicationHWnd),GetForegroundWindow()==ApplicationHWnd,DX8Wrapper_IsWindowed,TheMouse && TheMouse->isCursorCaptured());fflush(stdout);
                }
            }
            // Exercise the same W3D device-reset entry point used by Options,
            // with no physical input or foreground activation in disposable tests.
            if (m_started && boundedInputIsolated() && std::getenv("CNC240_VERIFY_DISPLAY_SWITCH") && ApplicationHWnd) {
                static int next=0, pending=-1;
                static DWORD changedAt=0;
                static unsigned changedFrame=0;
                const int modes[]={cnc240::Windowed,cnc240::Borderless,cnc240::Windowed,
                    cnc240::Borderless,cnc240::Windowed};
                if (pending>=0 && GetTickCount()-changedAt>=1000) {
                    RECT client={};GetClientRect(ApplicationHWnd,&client);
                    printf("CNC240_DISPLAY_SWITCH_SETTLED index=%d mode=%d device_windowed=%d caption=%d iconic=%d ticks=%u captured=%d client_width=%ld client_height=%ld foreground=%d\n",
                        pending,cnc240::displayMode(),DX8Wrapper_IsWindowed,
                        (GetWindowLong(ApplicationHWnd,GWL_STYLE)&WS_CAPTION)!=0,IsIconic(ApplicationHWnd),
                        TheGameLogic->getFrame()-changedFrame,TheMouse && TheMouse->isCursorCaptured(),
                        client.right,client.bottom,GetForegroundWindow()==ApplicationHWnd);
                    fflush(stdout);pending=-1;
                }
                if (next<5 && pending<0 && GetTickCount()-m_start>=static_cast<DWORD>(3000+next*2000)) {
                    const int previous=cnc240::displayMode(),requested=modes[next];
                    cnc240::displayMode()=requested;
                    const bool ok=TheDisplay->setDisplayMode(TheDisplay->getWidth(),TheDisplay->getHeight(),TheDisplay->getBitDepth(),requested!=cnc240::Fullscreen);
                    if(!ok)cnc240::displayMode()=previous;
                    printf("CNC240_DISPLAY_SWITCH index=%d from=%d requested=%d ok=%d device_windowed=%d mode=%d\n",
                        next,previous,requested,ok,DX8Wrapper_IsWindowed,cnc240::displayMode());fflush(stdout);
                    pending=next++;changedAt=GetTickCount();changedFrame=TheGameLogic->getFrame();
                    if(requested!=cnc240::Fullscreen) {
                        SendMessage(ApplicationHWnd,WM_ACTIVATEAPP,FALSE,0);
                        SendMessage(ApplicationHWnd,WM_ACTIVATE,WA_INACTIVE,0);
                        SendMessage(ApplicationHWnd,WM_KILLFOCUS,0,0);
                    }
                }
            }
            ++m_renders;
            if (!m_started && std::getenv("CNC240_VERIFY_SETTINGS") && GetTickCount()-m_start>=3000)
                verifyAdaptiveDisplaySettings();
            if (m_started && strcmp(m_scenario, "skirmish")==0 && std::getenv("CNC240_VERIFY_CAMERA"))
                verifyAdaptiveCameraClocks();
            const unsigned int frame = TheGameLogic->getFrame();
            if (m_zoomCheck && m_started && frame >= cnc240::legacyFrames(60) && !m_zoomRequested)
            {
                m_zoomRequested = true;
                const Real originalHeight = TheTacticalView->getHeightAboveGround();
                TheTacticalView->setUserControlled(FALSE);
                TheTacticalView->setHeightAboveGround(10000.0f);
                const Real scriptedLimit = TheTacticalView->getHeightAboveGround();
                TheTacticalView->setHeightAboveGround(originalHeight);
                const Bool accepted = TheTacticalView->userZoom(10000.0f);
                printf("CNC240_ZOOM accepted=%d scripted_limit=%.2f user_limit=%.2f\n", accepted, scriptedLimit, TheTacticalView->getHeightAboveGround());
                fflush(stdout);
            }
            if (m_zoomRequested && !m_zoomReported && frame >= cnc240::legacyFrames(180))
            {
                m_zoomReported = true;
                printf("CNC240_ZOOM_SETTLED actual_height=%.2f\n", TheTacticalView->getCurrentHeightAboveGround());
                checkTerrainCoverage();
                verifyMinimapDrag();
                verifyCameraSpeed();
                m_originalAngle = TheTacticalView->getAngle();
                m_originalPosition = TheTacticalView->getPosition2D();
                fflush(stdout);
            }
            if (m_zoomReported && m_coverageStage == 0 && frame >= cnc240::legacyFrames(210))
            {
                TheTacticalView->userSetAngle(m_originalAngle + 1.5707963f);
                m_coverageStage = 1;
            }
            if (m_zoomReported && m_coverageStage == 1 && frame >= cnc240::legacyFrames(240))
            {
                printf("CNC240_TERRAIN_CASE rotated\n");
                checkTerrainCoverage();
                Coord3D edge{0.0f, 0.0f, 0.0f};
                TheTacticalView->userLookAt(&edge);
                m_coverageStage = 2;
            }
            if (m_zoomReported && m_coverageStage == 2 && frame >= cnc240::legacyFrames(270))
            {
                printf("CNC240_TERRAIN_CASE map_edge\n");
                checkTerrainCoverage();
                TheTacticalView->userSetAngle(m_originalAngle);
                TheTacticalView->setPosition2D(m_originalPosition);
                m_coverageStage = 3;
            }
            if (m_started && !m_crcReported && frame >= cnc240::legacyFrames(300))
            {
                printf("CNC240_STATE frame=%u crc=%08X\n", frame, TheGameLogic->getCRC(CRC_RECALC, AsciiString::TheEmptyString));
                fflush(stdout);
                m_crcReported = true;
            }
            if (frame >= m_lastLogic) m_logic += frame - m_lastLogic;
            m_lastLogic = frame;
            const DWORD now = GetTickCount();
            const bool finished = now - m_start >= static_cast<DWORD>(m_seconds * 1000);
            if (now - m_lastReport >= 1000 || finished)
            {
                printf("CNC240_TIMING elapsed_ms=%lu render_frames=%u logic_frames=%u logic_fps=%d render_cap=%d presented_frames=%lu\n",
                       now - m_start, m_renders, m_logic, TheFramePacer->getActualLogicTimeScaleFps(),
                       TheFramePacer->getActualFramesPerSecondLimit(), getPresentedFrameCount() - m_presentStart);
                fflush(stdout);
                m_lastReport = now;
                if (std::getenv("CNC240_VERIFY_CAMERA") && TheTacticalView) {
                    const Coord3D p=TheTacticalView->getPosition();
                    printf("CNC240_CAMERA_OBSERVE elapsed_ms=%lu x=%.3f y=%.3f angle=%.5f height=%.3f scripted=%d\n",
                        now-m_start,p.x,p.y,TheTacticalView->getAngle(),TheTacticalView->getCurrentHeightAboveGround(),TheTacticalView->isDoingScriptedCamera());
                    fflush(stdout);
                    if (ApplicationHWnd) {
                        RECT client={},window={}; GetClientRect(ApplicationHWnd,&client); GetWindowRect(ApplicationHWnd,&window);
                        int x=client.right-1,y=client.bottom-1;
                        cnc240::mapClientPoint(ApplicationHWnd,TheDisplay->getWidth(),TheDisplay->getHeight(),x,y);
                        printf("CNC240_DISPLAY device_windowed=%d mode=%d render_width=%u render_height=%u client_width=%ld client_height=%ld window_width=%ld window_height=%ld caption=%d mapped_right=%d mapped_bottom=%d foreground=%d iconic=%d maximize_box=%d instance=%u profile=%s\n",
                            DX8Wrapper_IsWindowed,cnc240::displayMode(),TheDisplay->getWidth(),TheDisplay->getHeight(),client.right,client.bottom,
                            window.right-window.left,window.bottom-window.top,(GetWindowLong(ApplicationHWnd,GWL_STYLE)&WS_CAPTION)!=0,x,y,GetForegroundWindow()==ApplicationHWnd,IsIconic(ApplicationHWnd),(GetWindowLong(ApplicationHWnd,GWL_STYLE)&WS_MAXIMIZEBOX)!=0,rts::ClientInstance::getInstanceId(),TheGlobalData->getPath_UserData().str());
                        fflush(stdout);
                    }
                }
            }
            return finished;
        }
    private:
        BattleProbe m_battleProbe;
        SimulationProbe m_simProbe;
        FocusedGameplayProbe m_focusedGameplay;
        DamageProbe m_damageProbe;
        CombatProbe m_combatProbe;
        DWORD m_start, m_lastReport;
        unsigned int m_renders, m_logic, m_lastLogic;
        unsigned long m_presentStart;
        int m_seconds;
        char m_scenario[64];
        bool m_started, m_launchPending, m_crcReported;
        bool m_zoomCheck, m_zoomRequested, m_zoomReported;
        int m_coverageStage;
        Real m_originalAngle;
        Coord2D m_originalPosition;
    };
}

#endif // FLUID_ENABLE_DEVELOPMENT_PROBES
