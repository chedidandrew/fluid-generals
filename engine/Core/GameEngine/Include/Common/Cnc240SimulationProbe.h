// Isolated variable-rate simulation validation. GPL-3.0-or-later plus EA terms.
#pragma once
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/Object.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Module/ProductionUpdate.h"

namespace cnc240 {
class SimulationProbe {
public:
    SimulationProbe() : m_id(INVALID_ID), m_airId(INVALID_ID), m_producerId(INVALID_ID), m_startFrame(0), m_lastShot(0), m_shots(0), m_stage(0), m_productionDone(false), m_start{}, m_airStart{} {}
    void update(bool enabled) {
        if (!enabled) return;
        const unsigned frame = TheGameLogic->getFrame();
        if (m_stage == 0 && frame >= legacyFrames(60)) {
            Object *anchor = nullptr;
            for (Object *obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject()) {
                if (obj->getControllingPlayer() == ThePlayerList->getLocalPlayer() && obj->getAIUpdateInterface()) {
                    anchor = obj; break;
                }
            }
            if (!anchor) return;
            const ThingTemplate *templ = TheThingFactory->findTemplate("AmericaTankCrusader");
            Object *obj = templ ? TheThingFactory->newObject(templ, ThePlayerList->getLocalPlayer()->getDefaultTeam()) : nullptr;
            if (!obj || !obj->getAIUpdateInterface()) {
                printf("CNC240_SIM_PROBE_ERROR cannot create controlled tank\n"); m_stage = 3; return;
            }
            m_start = *anchor->getPosition();
            m_start.x += 130.0f; m_start.y += 130.0f;
            m_start.z = TheTerrainLogic->getGroundHeight(m_start.x, m_start.y);
            obj->setPosition(&m_start);
            obj->setOrientation(0.0f);
            obj->getAIUpdateInterface()->chooseLocomotorSet(LOCOMOTORSET_NORMAL);
            Coord3D destination = m_start;
            destination.x += 600.0f;
            destination.z = TheTerrainLogic->getGroundHeight(destination.x, destination.y);
            obj->getAIUpdateInterface()->aiMoveToPosition(&destination, CMD_FROM_SCRIPT);
            m_id = obj->getID(); m_startFrame = frame; m_stage = 1;
            Weapon *weapon = obj->getWeaponInWeaponSlot(PRIMARY_WEAPON);
            WeaponBonus bonus;
            printf("CNC240_SIM_UNITS sim_hz=%d build_seconds=%.4f clip_reload_seconds=%.4f legacy_wait_seconds=%.4f start_x=%.3f start_y=%.3f\n",
                simulationHz(), templ->calcTimeToBuild(ThePlayerList->getLocalPlayer()) / (double)simulationHz(), weapon ? weapon->getClipReloadTime(obj) / (double)simulationHz() : -1.0,
                legacyFrames(90) / (double)simulationHz(), m_start.x, m_start.y);
            if (weapon) printf("CNC240_SIM_WEAPON_DATA delay_seconds=%.4f\n", weapon->getTemplate()->getDelayBetweenShots(bonus)/(double)simulationHz());
            const ThingTemplate *airTemplate = TheThingFactory->findTemplate("AmericaVehicleChinook");
            Object *air = airTemplate ? TheThingFactory->newObject(airTemplate, ThePlayerList->getLocalPlayer()->getDefaultTeam()) : nullptr;
            if (air && air->getAIUpdateInterface()) {
                m_airStart = m_start; m_airStart.y += 150.0f;
                m_airStart.z = TheTerrainLogic->getGroundHeight(m_airStart.x, m_airStart.y) + 100.0f;
                air->setPosition(&m_airStart); air->setOrientation(0.0f);
                air->getAIUpdateInterface()->chooseLocomotorSet(LOCOMOTORSET_NORMAL);
                Coord3D airDestination = m_airStart; airDestination.x += 600.0f;
                air->getAIUpdateInterface()->aiMoveToPosition(&airDestination, CMD_FROM_SCRIPT);
                m_airId = air->getID();
            } else printf("CNC240_SIM_PROBE_ERROR cannot create helicopter\n");
            for (Object *factory = TheGameLogic->getFirstObject(); factory; factory = factory->getNextObject()) {
                if (factory->getControllingPlayer() != ThePlayerList->getLocalPlayer()) continue;
                ProductionUpdateInterface *production = ProductionUpdate::getProductionUpdateInterfaceFromObject(factory);
                const ThingTemplate *dozer = TheThingFactory->findTemplate("AmericaVehicleDozer");
                if (production && dozer && production->getProductionCount() == 0 && production->queueCreateUnit(dozer, production->requestUniqueUnitID())) {
                    m_producerId = factory->getID();
                    printf("CNC240_SIM_PRODUCTION_START expected_seconds=%.4f\n", dozer->calcTimeToBuild(ThePlayerList->getLocalPlayer())/(double)simulationHz());
                    break;
                }
            }
            fflush(stdout);
        }
        if (m_stage > 0 && m_stage < 3) {
            Object *obj = TheGameLogic->findObjectByID(m_id);
            Weapon *weapon = obj ? obj->getWeaponInWeaponSlot(PRIMARY_WEAPON) : nullptr;
            if (weapon && weapon->getStatus() == READY_TO_FIRE && m_shots < 8) {
                Coord3D aim = *obj->getPosition(); aim.y += 50.0f;
                weapon->fireWeapon(obj, &aim);
                const unsigned shot = weapon->getLastShotFrame();
                if (shot != m_lastShot) {
                    printf("CNC240_SIM_SHOT index=%d interval_seconds=%.4f frame=%u\n", m_shots, m_lastShot ? (shot-m_lastShot)/(double)simulationHz() : 0.0, shot);
                    m_lastShot = shot; ++m_shots; fflush(stdout);
                }
            }
            Object *factory = TheGameLogic->findObjectByID(m_producerId);
            ProductionUpdateInterface *production = factory ? ProductionUpdate::getProductionUpdateInterfaceFromObject(factory) : nullptr;
            if (production && !m_productionDone && production->getProductionCount() == 0) {
                printf("CNC240_SIM_PRODUCTION_DONE game_seconds=%.4f\n", (frame-m_startFrame)/(double)simulationHz());
                m_productionDone = true; fflush(stdout);
            }
        }
        if (m_stage == 1 && frame >= m_startFrame + legacyFrames(180)) {
            Object *obj = TheGameLogic->findObjectByID(m_id);
            if (obj) {
                const Coord3D &pos = *obj->getPosition();
                const double dx = pos.x - m_start.x, dy = pos.y - m_start.y;
                printf("CNC240_SIM_MOVEMENT sim_hz=%d game_seconds=%.4f distance=%.4f end_x=%.3f end_y=%.3f speed_per_second=%.4f\n",
                    simulationHz(), (frame-m_startFrame)/(double)simulationHz(), std::sqrt(dx*dx+dy*dy),
                    pos.x, pos.y, obj->getAIUpdateInterface()->getCurLocomotorSpeed()*simulationHz());
                Object *factory = TheGameLogic->findObjectByID(m_producerId);
                ProductionUpdateInterface *production = factory ? ProductionUpdate::getProductionUpdateInterfaceFromObject(factory) : nullptr;
                if (production && production->firstProduction()) printf("CNC240_SIM_PRODUCTION_PROGRESS game_seconds=%.4f percent=%.4f\n", (frame-m_startFrame)/(double)simulationHz(), production->firstProduction()->getPercentComplete());
                Object *air = TheGameLogic->findObjectByID(m_airId);
                if (air) {
                    const Coord3D &airPos = *air->getPosition();
                    const double ax = airPos.x-m_airStart.x, ay = airPos.y-m_airStart.y;
                    printf("CNC240_SIM_AIR game_seconds=%.4f distance=%.4f height=%.4f speed_per_second=%.4f\n",
                        (frame-m_startFrame)/(double)simulationHz(), std::sqrt(ax*ax+ay*ay),
                        airPos.z-TheTerrainLogic->getGroundHeight(airPos.x, airPos.y),
                        air->getAIUpdateInterface()->getCurLocomotorSpeed()*simulationHz());
                } else printf("CNC240_SIM_PROBE_ERROR helicopter disappeared\n");
            } else printf("CNC240_SIM_PROBE_ERROR tank disappeared\n");
            fflush(stdout); m_stage = 2;
        }
    }
private:
    ObjectID m_id;
    ObjectID m_airId;
    ObjectID m_producerId;
    unsigned m_startFrame, m_lastShot;
    int m_shots;
    int m_stage;
    bool m_productionDone;
    Coord3D m_start;
    Coord3D m_airStart;
};
}
