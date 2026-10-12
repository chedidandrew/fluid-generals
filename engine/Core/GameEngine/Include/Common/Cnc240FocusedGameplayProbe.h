// Bounded native gameplay comparison fixtures. GPL-3.0-or-later with EA terms.
#pragma once
#include "GameClient/GadgetTextEntry.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/Gadget.h"
#include <set>
namespace cnc240 {
class FocusedGameplayProbe {
    bool started=false,stopped=false,paused=false; unsigned startFrame=0,lastFrame=0;DWORD startMs=0;
    ObjectID dragons[2]={INVALID_ID,INVALID_ID},targets[2]={INVALID_ID,INVALID_ID},fieldId=INVALID_ID;
    ObjectID jets[4]={INVALID_ID,INVALID_ID,INVALID_ID,INVALID_ID};unsigned sorties[4]={};unsigned nextFlight[4]={};
    bool airborne[4]={};std::set<ObjectID> segments;GameWindow *entry=nullptr;
    std::vector<ObjectID> auditTanks;std::vector<ObjectID> auditBuildings;
    unsigned auditSample=0;int auditPhase=-1;bool lightStarted=false,auditResume=false;
    Object *make(const char *name,Coord3D p,Player *player) {
        const ThingTemplate *t=TheThingFactory->findTemplate(name);
        Object *o=t?TheThingFactory->newObject(t,player->getDefaultTeam()):nullptr;
        if(!o){printf("CNC240_FOCUSED_ERROR missing=%s\n",name);return nullptr;}
        if(!o->isKindOf(KINDOF_AIRCRAFT))p.z=TheTerrainLogic->getGroundHeight(p.x,p.y);
        o->setPosition(&p);o->setOrientation(0);
        if(o->isKindOf(KINDOF_STRUCTURE)) {TheTerrainLogic->flattenTerrain(o);p.z=TheTerrainLogic->getGroundHeight(p.x,p.y);o->setPosition(&p);TheAI->pathfinder()->addObjectToPathfindMap(o);}
        return o;
    }
    Coord3D flatSite(Coord3D wanted) {
        Region3D bounds;TheTerrainLogic->getExtent(&bounds);Coord3D best=wanted;float scoreBest=1e30f;
        for(float x=bounds.lo.x+450;x<bounds.hi.x-700;x+=100)for(float y=bounds.lo.y+450;y<bounds.hi.y-450;y+=100) {
            float low=1e30f,high=-1e30f;bool valid=true;
            for(int dx=-3;dx<=3;++dx)for(int dy=-3;dy<=3;++dy){float px=x+dx*100,py=y+dy*100;
                if(TheTerrainLogic->isUnderwater(px,py)||TheTerrainLogic->isCliffCell(px,py))valid=false;
                float z=TheTerrainLogic->getGroundHeight(px,py);low=min(low,z);high=max(high,z);}
            float score=(high-low>1?(high-low)*1000000:0)+sqr(x-wanted.x)+sqr(y-wanted.y);
            if(valid&&score<scoreBest){scoreBest=score;best={x,y,TheTerrainLogic->getGroundHeight(x,y)};}
        }return best;
    }
public:
    void update(bool enabled) {
        if(!enabled)return;
        const char *mode=std::getenv("CNC240_VERIFY_GAMEPLAY");if(!mode)return;
        unsigned frame=TheGameLogic->getFrame();
        if(!started&&frame>=legacyFrames(60)) {
            Object *anchor=nullptr;Player *player=ThePlayerList->getLocalPlayer();
            for(Object *o=TheGameLogic->getFirstObject();o;o=o->getNextObject())if(o->getControllingPlayer()==player&&o->getAIUpdateInterface()){anchor=o;break;}
            if(!anchor)return;
            started=true;startFrame=frame;startMs=GetTickCount();Coord3D origin=*anchor->getPosition();origin.x+=500;origin.y+=500;origin=flatSite(origin);
            if(!strcmp(mode,"firewall"))for(int i=0;i<2;++i){
                Coord3D p=origin;p.y+=i*250;Object *tank=make("ChinaTankDragon",p,player);
                if(tank&&tank->getAIUpdateInterface()){
                    dragons[i]=tank->getID();tank->getBodyModule()->setIndestructible(true);
                    if(i)tank->setWeaponSetFlag(WEAPONSET_PLAYER_UPGRADE);
                    tank->setWeaponLock(SECONDARY_WEAPON,LOCKED_PERMANENTLY);
                    tank->getAIUpdateInterface()->chooseLocomotorSet(LOCOMOTORSET_NORMAL);
                    Coord3D attack=p;attack.x+=25;attack.z=TheTerrainLogic->getGroundHeight(attack.x,attack.y);
                    tank->getAIUpdateInterface()->aiAttackPosition(&attack,NO_MAX_SHOTS_LIMIT,CMD_FROM_PLAYER);
                    p.x+=50;Object *victim=make("TechOilDerrick",p,ThePlayerList->getNeutralPlayer());
                    if(victim){targets[i]=victim->getID();victim->getBodyModule()->setMaxHealth(100000);victim->getBodyModule()->setInitialHealth(100);}
                    TheTacticalView->lookAt(tank->getPosition());
                }
            }
            else if(!strcmp(mode,"raptor")) {
                Object *field=make("AmericaAirfield",origin,player);
                if(field){fieldId=field->getID();field->getBodyModule()->setIndestructible(true);
                    for(int i=0;i<4;++i){Coord3D p=origin;p.x+=600;p.y+=i*120;p.z+=180;
#if RTS_ZEROHOUR
                        Object *jet=make("AirF_AmericaJetRaptor",p,player);
#else
                        Object *jet=make("AmericaJetRaptor",p,player);
#endif
                        if(jet&&jet->getAIUpdateInterface()){jets[i]=jet->getID();jet->setProducer(field);jet->getBodyModule()->setIndestructible(true);
                            jet->getAIUpdateInterface()->chooseLocomotorSet(LOCOMOTORSET_NORMAL);
                            Weapon *weapon=jet->getWeaponInWeaponSlot(PRIMARY_WEAPON);if(weapon)weapon->setClipPercentFull(0.0f,true);
                            jet->getAIUpdateInterface()->aiEnter(field,CMD_FROM_PLAYER);}
                    }
                    TheTacticalView->lookAt(field->getPosition());
                }
            }
            else if(!strcmp(mode,"audit")) {
                for(const ThingTemplate *tt=TheThingFactory->firstTemplate();tt;tt=tt->friend_getNextTemplate()) {
                    const char *name=tt->getName().str();
                    if(!tt->isKindOf(KINDOF_VEHICLE) || !strstr(name,"Tank") || strstr(name,"Husk") || strstr(name,"Debris"))continue;
                    const unsigned i=auditTanks.size();Coord3D p=origin;p.x+=(i%7)*90;p.y+=(i/7)*100;
                    Object *tank=make(name,p,player);if(!tank || !tank->getAIUpdateInterface())continue;
                    tank->getBodyModule()->setIndestructible(true);tank->getAIUpdateInterface()->chooseLocomotorSet(LOCOMOTORSET_NORMAL);
                    auditTanks.push_back(tank->getID());
                    printf("CNC240_TANK_INVENTORY id=%u name=%s\n",tank->getID(),name);
                }
                for(int upgrade=0;upgrade<3;++upgrade) {
                    Coord3D p=origin;p.x+=upgrade*150;p.y-=200;
                    Object *tank=make("GLATankScorpion",p,player);if(!tank)continue;
                    tank->setWeaponSetFlag(WEAPONSET_PLAYER_UPGRADE);
                    if(upgrade)tank->setWeaponSetFlag(upgrade==1?WEAPONSET_CRATEUPGRADE_ONE:WEAPONSET_CRATEUPGRADE_TWO);
                    tank->getBodyModule()->setIndestructible(true);tank->getAIUpdateInterface()->chooseLocomotorSet(LOCOMOTORSET_NORMAL);
                    auditTanks.push_back(tank->getID());
                    Weapon *w=tank->getWeaponInWeaponSlot(SECONDARY_WEAPON);
                    printf("CNC240_SCORPION_UPGRADE id=%u scrap=%d secondary=%d\n",tank->getID(),upgrade,w!=nullptr);
                }
                const char *buildings[]={"AmericaPowerPlant","ChinaPowerPlant","GLABarracks","AmericaWarFactory","ChinaWarFactory","GLAArmsDealer"};
                for(int i=0;i<6;++i) {
                    Coord3D p=origin;p.x+=i*160;p.y-=450;Object *o=make(buildings[i],p,player);
                    if(o){o->getBodyModule()->setIndestructible(true);auditBuildings.push_back(o->getID());}
                }
                Coord3D center=origin;center.x+=300;TheTacticalView->lookAt(&center);TheTacticalView->setZoomLimited(false);TheTacticalView->setZoom(3.5f);
                printf("CNC240_AUDIT_INVENTORY tanks=%u buildings=%u\n",(unsigned)auditTanks.size(),(unsigned)auditBuildings.size());
            }
            else if(!strcmp(mode,"caret")) {
                EntryData data={};data.maxTextLen=50;WinInstanceData inst;inst.init();inst.m_style=GWS_ENTRY_FIELD|GWS_MOUSE_TRACK;
                entry=TheWindowManager->gogoGadgetTextEntry(nullptr,WIN_STATUS_ENABLED|WIN_STATUS_IMAGE,150,100,400,30,&inst,&data,nullptr,TRUE);
                if(entry){GadgetTextEntrySetText(entry,UnicodeString(L"Saved game blink verification"));TheWindowManager->winSetFocus(entry);}
                else printf("CNC240_FOCUSED_ERROR missing=text_entry\n");
            }
            printf("CNC240_FOCUSED_START mode=%s hz=%d frame=%u\n",mode,simulationHz(),frame);fflush(stdout);
        }
        if(!started)return;
        const double seconds=(frame-startFrame)/(double)simulationHz();
        const double wallSeconds=(GetTickCount()-startMs)/1000.0;
        if(!strcmp(mode,"firewall")) {
            if(!stopped&&seconds>=12){stopped=true;for(ObjectID id:dragons){Object *tank=TheGameLogic->findObjectByID(id);if(tank&&tank->getAIUpdateInterface())tank->getAIUpdateInterface()->aiIdle(CMD_FROM_SCRIPT);}printf("CNC240_FIREWALL_STOP seconds=%.4f\n",seconds);}
            unsigned alive=0;for(Object *o=TheGameLogic->getFirstObject();o;o=o->getNextObject())if(strstr(o->getTemplate()->getName().str(),"FireWallSegment")){++alive;segments.insert(o->getID());}
            if(frame-lastFrame>=simulationHz()){
                lastFrame=frame;printf("CNC240_FIREWALL_FIELD seconds=%.4f alive=%u created=%u\n",seconds,alive,(unsigned)segments.size());
                for(int i=0;i<2;++i){Object *tank=TheGameLogic->findObjectByID(dragons[i]);Object *victim=TheGameLogic->findObjectByID(targets[i]);float angle=0,pitch=0;
                    if(tank&&tank->getAIUpdateInterface())tank->getAIUpdateInterface()->getTurretRotAndPitch(TURRET_MAIN,&angle,&pitch);
                    printf("CNC240_FIREWALL_SAMPLE seconds=%.4f upgraded=%d angle=%.5f damage=%.4f\n",seconds,i,angle,victim?100000-victim->getBodyModule()->getHealth():-1);}
                fflush(stdout);
            }
        }
        else if(!strcmp(mode,"raptor")) {
            Object *field=TheGameLogic->findObjectByID(fieldId);
            for(int i=0;i<4;++i){Object *jet=TheGameLogic->findObjectByID(jets[i]);if(!jet||!jet->getAIUpdateInterface())continue;
                Weapon *weapon=jet->getWeaponInWeaponSlot(PRIMARY_WEAPON);JetAIUpdate *ai=(JetAIUpdate*)jet->getAIUpdateInterface();
                const bool full=weapon&&weapon->getRemainingAmmo()==weapon->getClipSize();
                if(field&&jet->getHeightAboveTerrain()<1&&ai->isIdle()&&full&&!airborne[i]&&frame>=nextFlight[i]&&seconds>15&&seconds<75){
                    Coord3D p=*field->getPosition();p.x+=900;p.y+=(i-1)*180;
                    ai->aiMoveToPosition(&p,CMD_FROM_SCRIPT);airborne[i]=true;nextFlight[i]=frame+legacyFrames(300);
                    printf("CNC240_RAPTOR_ACTION seconds=%.4f index=%d action=depart cycle=%u\n",seconds,i,sorties[i]);
                }
                else if(airborne[i]&&frame>=nextFlight[i]){
                    if(weapon)weapon->setClipPercentFull(0.0f,true);
                    ai->aiEnter(field,CMD_FROM_PLAYER);airborne[i]=false;nextFlight[i]=frame+legacyFrames(450);++sorties[i];
                    printf("CNC240_RAPTOR_ACTION seconds=%.4f index=%d action=return cycle=%u\n",seconds,i,sorties[i]);
                }
            }
            if(frame-lastFrame>=simulationHz()) {lastFrame=frame;
                for(int i=0;i<4;++i){Object *jet=TheGameLogic->findObjectByID(jets[i]);if(!jet||!jet->getAIUpdateInterface())continue;
                    Weapon *w=jet->getWeaponInWeaponSlot(PRIMARY_WEAPON);
                    printf("CNC240_RAPTOR_SAMPLE seconds=%.4f index=%d state=%d x=%.4f y=%.4f z=%.4f height=%.4f speed=%.4f ground=%d ammo=%d clip=%d cycle=%u\n",seconds,i,jet->getAIUpdateInterface()->getCurrentStateID(),jet->getPosition()->x,jet->getPosition()->y,jet->getPosition()->z,jet->getHeightAboveTerrain(),jet->getPhysics()?jet->getPhysics()->getVelocityMagnitude()*simulationHz():0,jet->getAIUpdateInterface()->isDoingGroundMovement(),w?w->getRemainingAmmo():-1,w?w->getClipSize():-1,sorties[i]);}
                fflush(stdout);
            }
        }
        else if(!strcmp(mode,"audit")) {
            if(!lightStarted && seconds>=5) {
                lightStarted=true;RGBColor color={1,0.5f,0.1f};Object *o=TheGameLogic->findObjectByID(auditBuildings[0]);
                if(o)TheDisplay->createLightPulse(o->getPosition(),&color,1,136,legacyFrames(3),legacyFrames(9));
                printf("CNC240_AUDIT_LIGHT seconds=%.4f\n",seconds);
            }
            if(!paused && lightStarted && seconds>=5.1) {paused=true;TheGameLogic->setGamePaused(true);printf("CNC240_AUDIT_PAUSE wall=%.4f game=%.4f\n",wallSeconds,seconds);}
            if(paused && !auditResume && wallSeconds>=6.3) {auditResume=true;TheGameLogic->setGamePaused(false);printf("CNC240_AUDIT_RESUME wall=%.4f game=%.4f\n",wallSeconds,seconds);}
            const int phase=static_cast<int>(seconds/3);
            if(phase!=auditPhase && phase<7) {
                auditPhase=phase;
                for(ObjectID id:auditTanks){Object *tank=TheGameLogic->findObjectByID(id);if(!tank || !tank->getAIUpdateInterface())continue;
                    AIUpdateInterface *ai=tank->getAIUpdateInterface();ai->aiIdle(CMD_FROM_SCRIPT);
                    if(phase==4)tank->setOrientation(PI/2);
                    const float directions[]={PI/2,PI-0.0174533f,-PI+0.0174533f,0,PI/2,PI/2,0};
                    Coord3D target=*tank->getPosition();target.x+=cos(directions[phase])*120;target.y+=sin(directions[phase])*120;
                    for(int tur=0;tur<MAX_TURRETS;++tur)ai->setTurretTargetPosition(static_cast<WhichTurretType>(tur),&target);
                    if(phase==5){Coord3D dest=*tank->getPosition();dest.x+=100;ai->aiMoveToPosition(&dest,CMD_FROM_SCRIPT);}
                    if(phase==6)ai->aiAttackPosition(&target,NO_MAX_SHOTS_LIMIT,CMD_FROM_PLAYER);
                }
                if(phase==3)for(ObjectID id:auditBuildings){Object *o=TheGameLogic->findObjectByID(id);if(o)o->getBodyModule()->setInitialHealth(40);}
                printf("CNC240_TANK_ACTION seconds=%.4f phase=%d\n",seconds,phase);
            }
            if(frame-auditSample>=static_cast<unsigned>(simulationHz()/10)) {
                auditSample=frame;
                for(ObjectID id:auditTanks){Object *tank=TheGameLogic->findObjectByID(id);if(!tank || !tank->getAIUpdateInterface())continue;
                    AIUpdateInterface *ai=tank->getAIUpdateInterface();
                    for(int tur=0;tur<MAX_TURRETS;++tur){float angle=0,pitch=0;bool exists=ai->getTurretRotAndPitch(static_cast<WhichTurretType>(tur),&angle,&pitch);if(!exists)continue;
                        printf("CNC240_TANK_SAMPLE seconds=%.4f id=%u name=%s turret=%d angle=%.6f rate=%.6f hull=%.6f phase=%d\n",seconds,id,tank->getTemplate()->getName().str(),tur,angle,ai->getTurretTurnRate(static_cast<WhichTurretType>(tur))*simulationHz(),tank->getOrientation(),phase);}
                }fflush(stdout);
            }
        }
        else if(!strcmp(mode,"caret")) {
            if(!paused&&wallSeconds>=3){paused=true;TheGameLogic->setGamePaused(true);printf("CNC240_CARET_PAUSED seconds=%.4f\n",wallSeconds);fflush(stdout);}
        }
    }
};
}
