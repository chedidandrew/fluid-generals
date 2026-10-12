// Native locomotive regression fixture. GPL-3.0-or-later plus EA terms.
#pragma once
#include "Common/Cnc240SimulationRate.h"
#include "Common/MapObject.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/ThingFactory.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"

namespace cnc240 {
inline void updateTrainProbe(bool enabled) {
    static bool started=false;
    if (!enabled || started || TheGameLogic->getFrame()<legacyFrames(60)) return;
    started=true;
    const ThingTemplate *templ=TheThingFactory->findTemplate("TrainEngine");
    if (!templ) {printf("CNC240_TRAIN_ERROR template missing\n");return;}
    Region3D area;TheTerrainLogic->getExtent(&area);
    Coord3D points[4]={{area.lo.x+200,area.lo.y+200,0},{area.lo.x+800,area.lo.y+200,0},{area.lo.x+800,area.lo.y+800,0},{area.lo.x+200,area.lo.y+800,0}};
    for (int i=0;i<4;++i) {
        points[i].z=TheTerrainLogic->getGroundHeight(points[i].x,points[i].y);
        MapObject *map=newInstance(MapObject)(points[i],"Waypoint",0,0,nullptr,nullptr);
        map->setIsWaypoint();map->setWaypointID(700001+i);
        map->setWaypointName(i==2 ? "Cnc240TrainStation" : "Cnc240TrainTrack");
        TheTerrainLogic->addWaypoint(map);deleteInstance(map);
    }
    for (int i=0;i<4;++i) TheTerrainLogic->addWaypointLink(700001+i,700001+(i+1)%4);
    Object *train=TheThingFactory->newObject(templ,ThePlayerList->getLocalPlayer()->getDefaultTeam());
    if (!train) {printf("CNC240_TRAIN_ERROR creation failed\n");return;}
    train->setPosition(&points[0]);train->setOrientation(0);
    printf("CNC240_TRAIN_CREATED id=%u frame=%u hz=%u\n",train->getID(),TheGameLogic->getFrame(),simulationHz());fflush(stdout);
}
}
