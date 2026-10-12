// Bounded native fixtures. GPL-3.0-or-later with EA terms.
#pragma once
#include "Common/SpecialPower.h"
#include "Common/Upgrade.h"
#include "Common/RandomValue.h"
#include "GameLogic/AIPathfind.h"
#include <vector>
#include <algorithm>
#include "GameLogic/Module/SpecialAbilityUpdate.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Module/JetAIUpdate.h"
#include "GameLogic/Module/FloatUpdate.h"
#include "GameClient/Drawable.h"
namespace cnc240 {
class CombatProbe {
    ObjectID airId=INVALID_ID, fieldId=INVALID_ID, captureId=INVALID_ID, targetId=INVALID_ID, shockId=INVALID_ID;
    unsigned start=0,last=0; bool initialized=false,departed=false,returned=false,captured=false,captureRequested=false;
    struct DeathTrack {ObjectID id;Coord3D start;float range,height;AsciiString name;};
    std::vector<DeathTrack> deaths;
    float deathRange=0,deathHeight=0;
    Coord3D origin{},shockOrigin{}; float peak=0,range=0;
    Object *make(const char *name,const Coord3D &p,Player *player) {
        const ThingTemplate *t=TheThingFactory->findTemplate(name);
        Object *o=t?TheThingFactory->newObject(t,player->getDefaultTeam()):nullptr;
        if(o){
            Coord3D position=p;
            if(!o->isKindOf(KINDOF_AIRCRAFT))position.z=TheTerrainLogic->getGroundHeight(p.x,p.y);
            o->setPosition(&position);o->setOrientation(0);
            if(o->isKindOf(KINDOF_STRUCTURE)){
                TheTerrainLogic->flattenTerrain(o);position.z=TheTerrainLogic->getGroundHeight(p.x,p.y);o->setPosition(&position);
                TheAI->pathfinder()->addObjectToPathfindMap(o);
            }
        }
        else printf("CNC240_COMBAT_ERROR missing=%s\n",name);
        return o;
    }
    Coord3D flatRunway(const Coord3D &wanted) {
        Region3D bounds;TheTerrainLogic->getExtent(&bounds);
        Coord3D best=wanted;float bestScore=1e30f,bestSpread=1e30f;
        for(float x=bounds.lo.x+450;x<bounds.hi.x-700;x+=100)
        for(float y=bounds.lo.y+450;y<bounds.hi.y-450;y+=100) {
            float low=1e30f,high=-1e30f;bool valid=true;
            for(int dx=-3;dx<=3;++dx)for(int dy=-3;dy<=3;++dy) {
                float sx=x+dx*100,sy=y+dy*100;
                if(TheTerrainLogic->isUnderwater(sx,sy) || TheTerrainLogic->isCliffCell(sx,sy))valid=false;
                float z=TheTerrainLogic->getGroundHeight(sx,sy);low=min(low,z);high=max(high,z);
            }
            const float spread=high-low;
            const float score=(spread>1?spread*1000000:0)+sqr(x-wanted.x)+sqr(y-wanted.y);
            if(valid && score<bestScore){bestScore=score;bestSpread=spread;best={x,y,TheTerrainLogic->getGroundHeight(x,y)};}
        }
        printf("CNC240_RUNWAY_SITE x=%.4f y=%.4f z=%.4f height_spread=%.4f\n",best.x,best.y,best.z,bestSpread);fflush(stdout);
        return best;
    }
    void exerciseDeath(const char *name,const Coord3D &position,Player *player,DeathType deathType) {
        std::vector<ObjectID> before;
        for(Object *o=TheGameLogic->getFirstObject();o;o=o->getNextObject())before.push_back(o->getID());
        // Hold randomized fling/OCL inputs fixed at the damage boundary.
        // Native AI updates consume different RNG counts at different rates.
        InitRandom(0xC240DEADu);
        Object *o=make(name,position,player);
        if(o){if(o->getAIUpdateInterface())o->getAIUpdateInterface()->chooseLocomotorSet(LOCOMOTORSET_NORMAL);DamageInfo damage;damage.in.m_damageType=DAMAGE_EXPLOSION;damage.in.m_deathType=deathType;damage.in.m_amount=100000;damage.in.m_kill=true;o->attemptDamage(&damage);}
        unsigned count=0;
        for(Object *n=TheGameLogic->getFirstObject();n;n=n->getNextObject()) {
            if(n->getPhysics() && std::find(before.begin(),before.end(),n->getID())==before.end()) {
                deaths.push_back({n->getID(),*n->getPosition(),0,0,n->getTemplate()->getName()});++count;
            }
        }
        printf("CNC240_COMBAT_DEATH_START template=%s tracked=%u\n",name,count);fflush(stdout);
    }
public:
    void update(bool enabled) {
        if(!enabled)return;
        const unsigned frame=TheGameLogic->getFrame();
        if(!initialized && frame>=legacyFrames(60)) {
            Object *anchor=nullptr;
            for(Object *o=TheGameLogic->getFirstObject();o;o=o->getNextObject())
                if(o->getControllingPlayer()==ThePlayerList->getLocalPlayer() && o->getAIUpdateInterface()){anchor=o;break;}
            if(!anchor)return;
            initialized=true;start=frame;origin=*anchor->getPosition();origin.x+=500;origin.y+=500;
            origin.z=TheTerrainLogic->getGroundHeight(origin.x,origin.y);
            Player *player=ThePlayerList->getLocalPlayer();
            const char *name=std::getenv("CNC240_VERIFY_AIRCRAFT");
            if(name && *name) {
                const bool china=strstr(name,"China") || strstr(name,"Nuke_") || strstr(name,"Infa_");
                const Coord3D runway=flatRunway(origin);
                Object *field=make(china?"ChinaAirfield":"AmericaAirfield",runway,player);
                Coord3D p=runway;p.x+=500;p.z+=100;
                Object *air=make(name,p,player);
                if(field && air && air->getAIUpdateInterface()) {
                    fieldId=field->getID();airId=air->getID();air->setProducer(field);
                    // Isolate flight control from unrelated enemy attacks and initially empty ammo.
                    field->getBodyModule()->setIndestructible(true);air->getBodyModule()->setIndestructible(true);
                    air->getAIUpdateInterface()->chooseLocomotorSet(LOCOMOTORSET_NORMAL);
                    air->getAIUpdateInterface()->aiGetRepaired(field,CMD_FROM_SCRIPT);
                }
            }
            Coord3D visual=origin;visual.x-=100;
            make("ChinaInfantryBlackLotus",visual,player);
            Region3D bounds;TheTerrainLogic->getExtent(&bounds);bool foundWater=false;
            for(float x=bounds.lo.x+100;x<bounds.hi.x-100 && !foundWater;x+=100)
            for(float y=bounds.lo.y+100;y<bounds.hi.y-100 && !foundWater;y+=100) {
                float water=0;
                if(TheTerrainLogic->isUnderwater(x,y,&water)) {
                    Coord3D bp{x,y,water};Object *boat=make("BoatAsianFishing",bp,player);
                    if(boat){FloatUpdate *f=(FloatUpdate*)boat->findUpdateModule(NAMEKEY("FloatUpdate"));if(f)f->setEnabled(true);}
                    foundWater=true;
                }
            }
            if(!foundWater){Coord3D bp=origin;bp.x-=200;make("BoatAsianFishing",bp,player);}
            printf("CNC240_FLOAT_FIXTURE water_found=%d\n",foundWater);
            Coord3D p=origin;p.y+=350;
            Object *target=make("TechOilDerrick",p,ThePlayerList->getNeutralPlayer());
            p.x+=30;Object *unit=make("AmericaInfantryRanger",p,player);
            TheTacticalView->userLookAt(&p);
            if(target && unit){captureId=unit->getID();targetId=target->getID();
            }
            p=origin;p.y-=350;p.z+=1;shockOrigin=p;
            Object *shock=make("AmericaInfantryRanger",p,player);
            if(shock && shock->getPhysics()){shockId=shock->getID();shock->getPhysics()->setAllowToFall(true);
                shockOrigin=*shock->getPosition();shockOrigin.z+=1;shock->setPosition(&shockOrigin);
                Coord3D force{2,0,3};
#if RTS_ZEROHOUR
                shock->getPhysics()->applyShock(&force);
#else
                force.scale(cnc240::authoredStep());shock->getPhysics()->applyForce(&force);
#endif
            }
            p=origin;p.x-=250;p.y-=200;
            exerciseDeath("AmericaInfantryRanger",p,player,DEATH_EXPLODED);
            p.y-=150;exerciseDeath("AmericaTankCrusader",p,player,DEATH_NORMAL);
            if(name && *name){p.y-=150;p.z=TheTerrainLogic->getGroundHeight(p.x,p.y)+180;exerciseDeath(name,p,player,DEATH_NORMAL);}
            printf("CNC240_COMBAT_START hz=%d aircraft=%s\n",simulationHz(),name?name:"none");fflush(stdout);
        }
        if(!initialized)return;
        const double seconds=(frame-start)/(double)simulationHz();
        Object *shock=TheGameLogic->findObjectByID(shockId);
        if(shock){const Coord3D &p=*shock->getPosition();peak=max(peak,p.z-shockOrigin.z);range=max(range,fabs(p.x-shockOrigin.x));}
        for(DeathTrack &track:deaths) {
            Object *o=TheGameLogic->findObjectByID(track.id);
            if(o){const Coord3D &p=*o->getPosition();const float dx=p.x-track.start.x,dy=p.y-track.start.y;
                track.range=max(track.range,sqrt(dx*dx+dy*dy));track.height=max(track.height,p.z-track.start.z);
                deathRange=max(deathRange,track.range);deathHeight=max(deathHeight,track.height);}
        }
        Object *target=TheGameLogic->findObjectByID(targetId);
        Object *capturer=TheGameLogic->findObjectByID(captureId);
        if(!captureRequested && seconds>=1 && target && capturer) {
            captureRequested=true;
            Player *player=ThePlayerList->getLocalPlayer();
            const UpgradeTemplate *upgrade=TheUpgradeCenter->findUpgrade("Upgrade_InfantryCaptureBuilding");
            if(upgrade){player->addUpgrade(upgrade,UPGRADE_STATUS_COMPLETE);player->onUpgradeCompleted(upgrade);}
            capturer->getAIUpdateInterface()->chooseLocomotorSet(LOCOMOTORSET_NORMAL);
            const SpecialPowerTemplate *power=TheSpecialPowerStore->findSpecialPowerTemplate("SpecialAbilityRangerCaptureBuilding");
            SpecialAbilityUpdate *ability=capturer->findSpecialAbilityUpdate(SPECIAL_INFANTRY_CAPTURE_BUILDING);
            if(power && ability){
                const bool accepted=ability->initiateIntentToDoSpecialPower(power,target,nullptr,nullptr,0);
                ability->verifyCaptureContact();
                printf("CNC240_COMBAT_CAPTURE_START accepted=%d assumed_contact=1\n",accepted);
            }
        }
        if(!captured && target && target->getControllingPlayer()==ThePlayerList->getLocalPlayer()){
            captured=true;printf("CNC240_COMBAT_CAPTURE_DONE seconds=%.4f\n",seconds);fflush(stdout);}
        Object *air=TheGameLogic->findObjectByID(airId);
        Object *field=TheGameLogic->findObjectByID(fieldId);
        if(air && air->getAIUpdateInterface() && field) {
            if(seconds>=35 && !departed){departed=true;Coord3D p=origin;p.x+=700;
                air->getAIUpdateInterface()->aiMoveToPosition(&p,CMD_FROM_SCRIPT);}
            if(seconds>=50 && !returned){returned=true;air->getAIUpdateInterface()->aiGetRepaired(field,CMD_FROM_SCRIPT);}
        }
        if(frame-last>=simulationHz()) {
            last=frame;
            // Keep the selected native drawable visible in this isolated fixture.
            if(air)TheTacticalView->lookAt(air->getPosition());
            else if(capturer)TheTacticalView->lookAt(capturer->getPosition());
            printf("CNC240_COMBAT_DEATH seconds=%.4f range=%.4f height=%.4f tracked=%u\n",seconds,deathRange,deathHeight,(unsigned)deaths.size());
            for(const DeathTrack &track:deaths)printf("CNC240_DEATH_OBJECT seconds=%.4f id=%u range=%.4f height=%.4f name=%s\n",seconds,track.id,track.range,track.height,track.name.str());
            printf("CNC240_COMBAT_PHYSICS seconds=%.4f peak=%.4f range=%.4f height=%.4f\n",seconds,peak,range,shock?shock->getHeightAboveTerrain():-1);
            if(air && air->getAIUpdateInterface())printf("CNC240_COMBAT_AIR seconds=%.4f state=%s height=%.4f x=%.4f y=%.4f speed=%.4f ground=%d health=%.4f field_health=%.4f\n",seconds,
                air->getAIUpdateInterface()->getCurrentStateName().str(),air->getHeightAboveTerrain(),air->getPosition()->x,air->getPosition()->y,
                air->getPhysics()?air->getPhysics()->getVelocityMagnitude()*simulationHz():0,air->getAIUpdateInterface()->isDoingGroundMovement(),air->getBodyModule()->getHealth(),field?field->getBodyModule()->getHealth():0);
            if(capturer) {
                SpecialAbilityUpdate *ability=capturer->findSpecialAbilityUpdate(SPECIAL_INFANTRY_CAPTURE_BUILDING);
                printf("CNC240_COMBAT_CAPTURE_STATUS seconds=%.4f active=%d x=%.4f y=%.4f target_x=%.4f target_y=%.4f relationship=%d\n",seconds,ability && ability->isActive(),capturer->getPosition()->x,capturer->getPosition()->y,target?target->getPosition()->x:0,target?target->getPosition()->y:0,target?capturer->getRelationship(target):-1);
            }
            fflush(stdout);
        }
    }
};
}
