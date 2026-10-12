// Isolated eight-player stress fixture; normal games never enable this.
// GPL-3.0-or-later with the additional EA terms.
#pragma once
#include "Common/Cnc240BattlePerformance.h"
#include "Common/Cnc240SimulationProbe.h"
#include "Common/PlayerList.h"
#include "Common/FramePacer.h"
#include "GameClient/View.h"
#include "GameClient/Mouse.h"
namespace cnc240 {
class BattleProbe {
    struct Unit {ObjectID id;int army;};
    std::vector<Unit> units;std::vector<Player*> players;
    int perArmy=32;unsigned start=0,last=0;int stage=0;Coord3D center{};DWORD wallStart=0;
    void spawn(int perArmy) {
        const char *names[]={"AmericaTankCrusader","ChinaTankBattleMaster","GLATankScorpion","AmericaInfantryRanger"};
        unsigned added=0;
        for(int a=0;a<(int)players.size();++a) {
            double angle=a*6.28318530718/players.size();
            for(int j=0;j<perArmy;++j) {
                const ThingTemplate *t=TheThingFactory->findTemplate(names[(a+j)%4]);
                Object *o=t?TheThingFactory->newObject(t,players[a]->getDefaultTeam()):nullptr;
                if(!o || !o->getAIUpdateInterface()){printf("CNC240_BATTLE_ERROR template=%s\n",names[(a+j)%4]);continue;}
                Coord3D p=center;p.x+=float(cos(angle)*850+(j%8-4)*42);p.y+=float(sin(angle)*850+(j/8-2)*42);
                p.z=TheTerrainLogic->getGroundHeight(p.x,p.y);o->setPosition(&p);o->setOrientation(float(angle+3.14159265359));
                o->getAIUpdateInterface()->chooseLocomotorSet(LOCOMOTORSET_NORMAL);
                units.push_back({o->getID(),a});++added;
            }
            printf("CNC240_BATTLE_ACTION frame=%u wall_ms=%lu action=spawn army=%d player=%d units=%d\n",TheGameLogic->getFrame(),GetTickCount()-wallStart,a,players[a]->getPlayerIndex(),perArmy);
        }
        printf("CNC240_BATTLE_SPAWN added=%u tracked=%u\n",added,(unsigned)units.size());fflush(stdout);
    }
    void command(bool combat,int wave) {
        unsigned alive=0;
        for(const Unit &u:units) {
            Object *o=TheGameLogic->findObjectByID(u.id);if(!o || o->isEffectivelyDead() || !o->getAIUpdateInterface())continue;
            Coord3D p=center;
            if(!combat){double angle=(u.army+wave)*6.28318530718/players.size();p.x+=float(cos(angle)*500);p.y+=float(sin(angle)*500);}
            p.z=TheTerrainLogic->getGroundHeight(p.x,p.y);
            if(combat)o->getAIUpdateInterface()->aiAttackMoveToPosition(&p,-1,CMD_FROM_SCRIPT);
            else o->getAIUpdateInterface()->aiMoveToPosition(&p,CMD_FROM_SCRIPT);
            ++alive;
        }
        printf("CNC240_BATTLE_ACTION frame=%u wall_ms=%lu action=%s wave=%d ordered=%u stage=%d\n",TheGameLogic->getFrame(),GetTickCount()-wallStart,combat?"attack_move":"move",wave,alive,stage);fflush(stdout);
    }
public:
    void update(bool enabled) {
        if(!enabled)return;unsigned frame=TheGameLogic->getFrame();
        if(!stage && frame>=legacyFrames(60)) {
            players.clear();for(int i=0;i<ThePlayerList->getPlayerCount();++i){Player *p=ThePlayerList->getNthPlayer(i);if(p!=ThePlayerList->getNeutralPlayer() && ThePlayerList->getSlotIndex(p->getPlayerIndex())>=0 && p->isPlayerActive())players.push_back(p);}
            Region3D r;TheTerrainLogic->getExtent(&r);center={(r.lo.x+r.hi.x)/2,(r.lo.y+r.hi.y)/2,0};
            center.z=TheTerrainLogic->getGroundHeight(center.x,center.y);
            if(players.size()!=8){printf("CNC240_BATTLE_ERROR players=%u expected=8\n",(unsigned)players.size());fflush(stdout);return;}
            const char *amount=std::getenv("CNC240_VERIFY_BATTLE_UNITS");if(amount)perArmy=std::max(8,std::min(128,atoi(amount)));
            stage=1;start=frame;wallStart=GetTickCount();spawn(perArmy);
            TheTacticalView->lookAt(&center);TheTacticalView->setHeightAboveGround(650);
            printf("CNC240_BATTLE_TIMING sim_hz=%d retry_ticks=%d retry_ms=%.3f attack_retry_ticks=%d attack_retry_ms=%.3f bump_slowdown_1s=%.6f bump_recovery_1s=%.6f\n",simulationHz(),legacyFrames(3),1000.0*legacyFrames(3)/simulationHz(),legacyFrames(2),1000.0*legacyFrames(2)/simulationHz(),pow(authoredRetention(.95f),simulationHz()),pow(authoredRetention(1.05f),simulationHz()));
            printf("CNC240_BATTLE_START frame=%u seed=12345 players=8 sim_hz=%d center_x=%.3f center_y=%.3f\n",frame,simulationHz(),center.x,center.y);
        }
        if(!stage)return;double seconds=(frame-start)/(double)simulationHz();
        if(stage==1 && seconds>=8){stage=2;command(false,1);}
        if(stage==2 && seconds>=20){stage=3;command(true,0);}
        if(stage==3 && seconds>=38){stage=4;spawn(perArmy);command(false,3);}
        if(stage==4 && seconds>=52){stage=5;command(true,0);}
        battlePerformance().stage=stage;
        if(frame-last>=simulationHz()) {
            last=frame;printf("CNC240_BATTLE_PATHSTATE frame=%u pops=%llu hash=%016llX\n",frame,battlePerformance().pathPops,battlePerformance().pathHash);
            unsigned alive=0,objects=0;double distance=0;
            for(const Unit &u:units){Object *o=TheGameLogic->findObjectByID(u.id);if(o && !o->isEffectivelyDead()){++alive;float x=o->getPosition()->x-center.x,y=o->getPosition()->y-center.y;distance+=sqrt(x*x+y*y);}}
            for(Object *o=TheGameLogic->getFirstObject();o;o=o->getNextObject())++objects;
            printf("CNC240_BATTLE_STATE frame=%u game_s=%.3f wall_ms=%lu stage=%d alive=%u tracked=%u objects=%u mean_distance=%.3f captured=%d\n",frame,seconds,GetTickCount()-wallStart,stage,alive,(unsigned)units.size(),objects,alive?distance/alive:0,TheMouse && TheMouse->isCursorCaptured());fflush(stdout);
        }
    }
};
inline void updateBattleProbe() {
    static BattleProbe probe;
    probe.update(performanceProbeEnabled() && TheGameLogic && TheGameLogic->getGameMode()==GAME_SKIRMISH && !TheGameLogic->isLoadingMap());
}
}
