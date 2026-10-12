// Bounded damage fixtures. GPL-3.0-or-later with additional EA terms.
#pragma once
#include "GameLogic/Armor.h"
#include "GameLogic/ArmorSet.h"
namespace cnc240 {
class DamageProbe {
    bool started=false;unsigned start=0,sample=0;ObjectID attackers[4]={},victims[4]={},poison=INVALID_ID,burn=INVALID_ID,fall=INVALID_ID;
    Object *make(const char *name,Coord3D p,Player *player) {
        const ThingTemplate *tt=TheThingFactory->findTemplate(name);Object *o=tt?TheThingFactory->newObject(tt,player->getDefaultTeam()):nullptr;
        if(!o){printf("CNC240_DAMAGE_ERROR name=%s\n",name);return nullptr;}
        p.z=TheTerrainLogic->getGroundHeight(p.x,p.y);o->setPosition(&p);o->setOrientation(0);
        if(o->isKindOf(KINDOF_STRUCTURE)){TheTerrainLogic->flattenTerrain(o);p.z=TheTerrainLogic->getGroundHeight(p.x,p.y);o->setPosition(&p);TheAI->pathfinder()->addObjectToPathfindMap(o);}
        return o;
    }
    std::vector<Coord3D> sites;
    Coord3D flatPair(Coord3D wanted) {
        Region3D bounds;TheTerrainLogic->getExtent(&bounds);Coord3D best=wanted;float bestScore=1e30f;
        for(float x=bounds.lo.x+450;x<bounds.hi.x-550;x+=50)for(float y=bounds.lo.y+450;y<bounds.hi.y-450;y+=50) {
            bool valid=true;float low=1e30f,high=-1e30f;
            for(const Coord3D &p:sites)if(sqr(x-p.x)+sqr(y-p.y)<250*250)valid=false;
            for(int dx=-1;dx<=5;++dx)for(int dy=-1;dy<=1;++dy){float px=x+dx*25,py=y+dy*25;
                if(TheTerrainLogic->isUnderwater(px,py)||TheTerrainLogic->isCliffCell(px,py))valid=false;
                float z=TheTerrainLogic->getGroundHeight(px,py);low=min(low,z);high=max(high,z);}
            float score=(high-low)*1000000+sqr(x-wanted.x)+sqr(y-wanted.y);
            if(valid&&score<bestScore){bestScore=score;best={x,y,low};}
        }sites.push_back(best);return best;
    }
public:
    void update(bool enabled) {
        if(!enabled)return;const char *mode=std::getenv("CNC240_VERIFY_GAMEPLAY");if(!mode||strcmp(mode,"damage"))return;
        unsigned frame=TheGameLogic->getFrame();
        if(!started&&frame>=legacyFrames(60)) {
            Player *player=ThePlayerList->getLocalPlayer();Object *anchor=nullptr;
            for(Object *o=TheGameLogic->getFirstObject();o;o=o->getNextObject())if(o->getControllingPlayer()==player&&o->getAIUpdateInterface()){anchor=o;break;}
            if(!anchor)return;started=true;start=frame;
            TheWeaponStore->verifyDamageTemplates();
            unsigned units=0,armorCases=0;
            for(const ThingTemplate *tt=TheThingFactory->firstTemplate();tt;tt=tt->friend_getNextTemplate()) {
                if(!tt->isKindOf(KINDOF_VEHICLE)&&!tt->isKindOf(KINDOF_INFANTRY)&&!tt->isKindOf(KINDOF_STRUCTURE))continue;
                unsigned checksum=2166136261u;
                for(int mask=0;mask<(1<<ARMORSET_COUNT);++mask){ArmorSetFlags flags;flags.clear();
                    for(int bit=0;bit<ARMORSET_COUNT;++bit)if(mask&(1<<bit))flags.set(static_cast<ArmorSetType>(bit));
                    const ArmorTemplateSet *as=tt->findArmorTemplateSet(flags);const ArmorTemplate *a=as?as->getArmorTemplate():nullptr;
                    for(int type=0;type<DAMAGE_NUM_TYPES;++type){union {float f;unsigned u;} value;
                        value.f=a?a->adjustDamage(static_cast<DamageType>(type),100):100;checksum=(checksum^value.u)*16777619u;++armorCases;}
                }
                printf("CNC240_DAMAGE_ARMOR name=%s checksum=%08x states=%u\n",tt->getName().str(),checksum,1u<<ARMORSET_COUNT);++units;
            }
            printf("CNC240_DAMAGE_COVERAGE units=%u armor_cases=%u hz=%d\n",units,armorCases,simulationHz());
            Coord3D origin=*anchor->getPosition();origin.x+=500;origin.y+=500;
            Player *enemy=nullptr;
            for(int n=0;n<ThePlayerList->getPlayerCount();++n){Player *p=ThePlayerList->getNthPlayer(n);if(p!=player&&p!=ThePlayerList->getNeutralPlayer()&&ThePlayerList->getSlotIndex(p->getPlayerIndex())>=0&&p->isPlayerActive()){enemy=p;break;}}
            if(!enemy){printf("CNC240_DAMAGE_ERROR missing=enemy_player\n");return;}
            const char *names[]={"ChinaTankBattleMaster","GLATankScorpion","ChinaTankGattling","AmericaInfantryRanger"};
            for(int i=0;i<4;++i){Coord3D p=origin;p.y+=i*350;p=flatPair(p);Object *a=make(names[i],p,player);p.x+=100;
                Object *v=make(i<2?"ChinaTankBattleMaster":"TechOilDerrick",p,enemy);
                if(a&&v){attackers[i]=a->getID();victims[i]=v->getID();a->getBodyModule()->setIndestructible(true);
                    v->getBodyModule()->setMaxHealth(500000);v->getBodyModule()->setInitialHealth(100);
                    for(int slot=0;slot<WEAPONSLOT_COUNT;++slot){Weapon *w=a->getWeaponInWeaponSlot(static_cast<WeaponSlotType>(slot));if(w)w->loadAmmoNow(a);}
                    a->getAIUpdateInterface()->chooseLocomotorSet(LOCOMOTORSET_NORMAL);a->getAIUpdateInterface()->aiAttackObject(v,NO_MAX_SHOTS_LIMIT,CMD_FROM_PLAYER);
                    printf("CNC240_DAMAGE_PAIR index=%d attacker=%u victim=%u name=%s victim_name=%s\n",i,a->getID(),v->getID(),names[i],v->getTemplate()->getName().str());}
            }
            const DamageType types[]={DAMAGE_ARMOR_PIERCING,DAMAGE_SMALL_ARMS,DAMAGE_GATTLING,DAMAGE_FLAME,DAMAGE_POISON,DAMAGE_FALLING,DAMAGE_UNRESISTABLE,DAMAGE_EXPLOSION,DAMAGE_LASER,DAMAGE_LAND_MINE};
            const char *bodyNames[]={"AmericaInfantryRanger","GLATankScorpion","TechOilDerrick","AmericaJetRaptor"};
            for(int i=0;i<4;++i){Coord3D p=origin;p.x+=400;p.y+=i*200;Object *v=make(bodyNames[i],p,ThePlayerList->getNeutralPlayer());if(!v)continue;
                BodyModuleInterface *body=v->getBodyModule();body->setMaxHealth(10000);
                for(DamageType type:types){body->setInitialHealth(100);DamageInfo d;d.in.m_damageType=type;d.in.m_deathType=DEATH_NORMAL;d.in.m_sourceID=anchor->getID();d.in.m_amount=100;
                    body->attemptDamage(&d);
                    printf("CNC240_DAMAGE_HIT name=%s type=%d input=100 dealt=%.6f clipped=%.6f health=%.6f\n",bodyNames[i],(int)type,d.out.m_actualDamageDealt,d.out.m_actualDamageClipped,body->getHealth());}
                body->setIndestructible(true);
            }
            for(int i=0;i<2;++i){Coord3D p=origin;p.x+=750;p.y+=i*250;Object *v=make("AmericaInfantryRanger",p,ThePlayerList->getNeutralPlayer());if(!v)continue;
                v->getBodyModule()->setMaxHealth(10000);v->getBodyModule()->setInitialHealth(100);
                DamageInfo d;d.in.m_damageType=i?DAMAGE_FLAME:DAMAGE_POISON;d.in.m_deathType=DEATH_NORMAL;d.in.m_sourceID=anchor->getID();d.in.m_amount=25;v->attemptDamage(&d);
                if(i)burn=v->getID();else poison=v->getID();
            }
            Coord3D drop=origin;drop.x+=1050;Object *f=make("AmericaInfantryRanger",drop,ThePlayerList->getNeutralPlayer());
            if(f&&f->getPhysics()){fall=f->getID();f->getBodyModule()->setMaxHealth(10000);f->getBodyModule()->setInitialHealth(100);
                drop=*f->getPosition();drop.z+=200;f->setPosition(&drop);f->getPhysics()->setAllowToFall(true);f->getPhysics()->setImmuneToFallingDamage(false);f->getPhysics()->setAllowBouncing(false);}
            printf("CNC240_DAMAGE_START frame=%u hz=%d\n",start,simulationHz());fflush(stdout);
        }
        if(started&&frame-start>=sample*(unsigned)simulationHz()) {
            for(int i=0;i<4;++i){Object *a=TheGameLogic->findObjectByID(attackers[i]);Object *v=TheGameLogic->findObjectByID(victims[i]);
                if(a&&v)printf("CNC240_DAMAGE_BATTLE index=%d seconds=%.6f damage=%.6f weapon=%s status=%d x=%.3f y=%.3f vx=%.3f vy=%.3f\n",i,(frame-start)/(double)simulationHz(),500000-v->getBodyModule()->getHealth(),a->getCurrentWeapon()?a->getCurrentWeapon()->getName().str():"NONE",a->getCurrentWeapon()?(int)a->getCurrentWeapon()->getStatus():-1,a->getPosition()->x,a->getPosition()->y,v->getPosition()->x,v->getPosition()->y);}
            for(int i=0;i<2;++i){Object *v=TheGameLogic->findObjectByID(i?burn:poison);if(v)printf("CNC240_DAMAGE_PERIODIC kind=%s seconds=%.6f damage=%.6f\n",i?"burn":"poison",(frame-start)/(double)simulationHz(),10000-v->getBodyModule()->getHealth());}
            Object *f=TheGameLogic->findObjectByID(fall);if(f)printf("CNC240_DAMAGE_FALL seconds=%.6f damage=%.6f height=%.6f\n",(frame-start)/(double)simulationHz(),10000-f->getBodyModule()->getHealth(),f->getPosition()->z-TheTerrainLogic->getGroundHeight(f->getPosition()->x,f->getPosition()->y));
            ++sample;fflush(stdout);
        }
    }
};
}
