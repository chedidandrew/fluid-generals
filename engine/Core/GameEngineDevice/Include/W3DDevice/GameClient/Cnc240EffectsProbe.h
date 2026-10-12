// Bounded, opt-in visual regression probe. GPL-3.0-or-later plus EA terms.
#pragma once
#include "Common/FramePacer.h"
#include "Common/PlayerList.h"
#include "Common/Player.h"
#include "GameClient/ParticleSys.h"
#include "GameClient/Display.h"
#include "GameClient/GameClient.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "W3DDevice/GameClient/W3DWater.h"
#include "WW3D2/ww3d.h"

namespace cnc240 {
inline void verifyEffectClocks() {
    static const bool enabled = std::getenv("CNC240_VERIFY_SECONDS") && std::getenv("CNC240_VERIFY_EFFECTS");
    if (!enabled || !TheGameLogic || TheGameLogic->getFrame() < legacyFrames(60) || !TheParticleSystemManager) return;
    static ParticleSystemID id = INVALID_PARTICLE_SYSTEM_ID;
    static bool created = false, expired = false;
    static unsigned birth = 0, sample = 0, birthSync = 0;
    static Real birthWater = 0, birthBump = 0, startX = 0;
    if (!created) {
        Object *anchor = nullptr;
        for (Object *obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
            if (obj->getControllingPlayer() == ThePlayerList->getLocalPlayer() && obj->getAIUpdateInterface()) { anchor = obj; break; }
        if (!anchor) return;
        ParticleSystemTemplate *t = TheParticleSystemManager->newTemplate("Cnc240VisualClockProbe");
        t->m_shaderType = ParticleSystemInfo::ALPHA;
        t->m_particleType = ParticleSystemInfo::PARTICLE;
        // Use a texture actually present in this game's loaded particle data.
        bool textureFound = false;
        for (auto it = TheParticleSystemManager->beginParticleSystemTemplate(); it != TheParticleSystemManager->endParticleSystemTemplate(); ++it) {
            if (it->second != t && it->second->m_particleType == ParticleSystemInfo::PARTICLE && !it->second->m_particleTypeName.isEmpty()) {
                static_cast<ParticleSystemInfo&>(*t) = static_cast<const ParticleSystemInfo&>(*it->second); textureFound = true; break;
            }
        }
        if (!textureFound) { printf("CNC240_EFFECT_ERROR no stock particle texture\n"); created = true; return; }
        t->m_shaderType = ParticleSystemInfo::ALPHA;
        t->m_slaveSystemName.clear(); t->m_attachedSystemName.clear();
        t->m_driftVelocity.zero(); t->m_gravity = 0;
        t->m_windMotion = ParticleSystemInfo::WIND_MOTION_NOT_USED;
        t->m_priority = ALWAYS_RENDER;
        t->m_emissionVolumeType = ParticleSystemInfo::POINT;
        t->m_emissionVelocityType = ParticleSystemInfo::ORTHO;
        t->m_emissionVelocity.ortho.x.setRange(1,1);
        t->m_emissionVelocity.ortho.y.setRange(0,0);
        t->m_emissionVelocity.ortho.z.setRange(0,0);
        t->m_lifetime.setRange(91,91);
        t->m_systemLifetime = 1;
        t->m_burstCount.setRange(1,1);
        t->m_burstDelay.setRange(0,0);
        t->m_initialDelay.setRange(0,0);
        t->m_startSize.setRange(30,30);
        t->m_startSizeRate.setRange(0,0);
        t->m_sizeRate.setRange(0,0);
        t->m_sizeRateDamping.setRange(1,1);
        t->m_velDamping.setRange(1,1);
        t->m_angularDamping.setRange(1,1);
        t->m_angleZ.setRange(0,0);
        t->m_angularRateZ.setRange(0,0);
        t->m_colorScale.setRange(0,0);
        for (int key = 0; key < MAX_KEYFRAMES; ++key) {
            t->m_alphaKey[key].frame = 0;
            t->m_alphaKey[key].var.setRange(0,0);
            t->m_colorKey[key].frame = 0;
            t->m_colorKey[key].color.red = 1; t->m_colorKey[key].color.green = 0.5f; t->m_colorKey[key].color.blue = 0.2f;
        }
        t->m_alphaKey[1].frame = 30; t->m_alphaKey[1].var.setRange(1,1);
        t->m_alphaKey[2].frame = 90; t->m_alphaKey[2].var.setRange(0,0);
        t->m_colorKey[1].frame = 90;
        ParticleSystem *sys = TheParticleSystemManager->createParticleSystem(t, FALSE);
        if (!sys) { printf("CNC240_EFFECT_ERROR emitter creation failed\n"); created = true; return; }
        Coord3D position = *anchor->getPosition(); position.x += 40; position.z += 35;
        sys->setPosition(&position); sys->setSaveable(FALSE);
        id = sys->getSystemID(); created = true;
        printf("CNC240_EFFECT_START sim_hz=%d texture=%s\n", simulationHz(), t->m_particleTypeName.str());
    }
    ParticleSystem *sys = TheParticleSystemManager->findParticleSystem(id);
    Particle *particle = sys ? sys->getFirstParticle() : nullptr;
    static bool stateReported = false;
    if (!stateReported && TheGameLogic->getFrame() >= legacyFrames(90)) {
        stateReported = true;
        printf("CNC240_EFFECT_STATE client_frame=%u particle_frame=%u total=%u system_present=%d use_fx=%d dummy=%d\n", TheGameClient->getFrame(), TheParticleSystemManager->getParticleFrame(), TheParticleSystemManager->getParticleCount(), sys != nullptr, TheGlobalData->m_useFX, TheParticleSystemManager->isDummy());
        fflush(stdout);
    }
    if (particle && birth == 0) {
        birth = TheParticleSystemManager->getParticleFrame();
        birthSync = WW3D::Get_Sync_Time();
        birthWater = TheWaterRenderObj ? TheWaterRenderObj->getRiverPhaseForValidation() : -1;
        birthBump = TheWaterRenderObj ? TheWaterRenderObj->getBumpPhaseForValidation() : -1;
        startX = particle->getPosition()->x;
    }
    if (!birth) return;
    const unsigned age = TheParticleSystemManager->getParticleFrame() - birth;
    static bool stalled = false;
    if (particle && !stalled && age >= 15) {
        stalled = true;
        // Bounded diagnostic only: force the next display update to drain a
        // tick batch, rather than validating only the one-tick steady state.
        Sleep(250);
        printf("CNC240_EFFECT_CATCHUP forced_stall_ms=250\n");
        fflush(stdout);
    }
    // Exercise the real pause path for one wall-clock second. Let the current
    // display update finish before recording the frozen state.
    static DWORD pauseStart = 0;
    static unsigned pauseFrame = 0, pauseSync = 0;
    static Real pauseWater = 0, pauseAlpha = 0;
    static bool pauseCaptured = false, pauseDone = false;
    if (!pauseDone && particle && age >= 44 && !pauseStart) {
        pauseStart = GetTickCount();
        TheGameLogic->setGamePaused(TRUE, FALSE, FALSE);
    }
    if (pauseStart && !pauseDone) {
        if (!pauseCaptured && GetTickCount()-pauseStart >= 50) {
            pauseCaptured = true;
            pauseFrame = TheParticleSystemManager->getParticleFrame(); pauseSync = WW3D::Get_Sync_Time();
            pauseWater = TheWaterRenderObj ? TheWaterRenderObj->getRiverPhaseForValidation() : -1;
            pauseAlpha = particle ? particle->getAlpha() : -1;
        }
        if (GetTickCount()-pauseStart >= 1000) {
            const bool unchanged = pauseCaptured && particle && pauseFrame == TheParticleSystemManager->getParticleFrame()
                && pauseSync == WW3D::Get_Sync_Time() && fabs(particle->getAlpha()-pauseAlpha) < 0.00001f
                && (!TheWaterRenderObj || fabs(TheWaterRenderObj->getRiverPhaseForValidation()-pauseWater) < 0.00001f);
            printf("CNC240_EFFECT_PAUSE passed=%d wall_ms=%lu\n", unchanged, GetTickCount()-pauseStart);
            fflush(stdout);
            pauseDone = true; TheGameLogic->setGamePaused(FALSE, FALSE, FALSE);
        }
    }
    if (particle && sample < 3 && age >= 29 + 30 * sample) {
        printf("CNC240_EFFECT_SAMPLE index=%u legacy_age=%u particle_age=%u alpha=%.5f travel=%.5f ww_seconds=%.5f water_frames=%.5f bump_frames=%.5f count=%u\n",
            sample, age, particle->getElapsedFrames(), particle->getAlpha(), particle->getPosition()->x-startX,
            (WW3D::Get_Sync_Time()-birthSync)/1000.0,
            TheWaterRenderObj ? (TheWaterRenderObj->getRiverPhaseForValidation()-birthWater)/0.002f : -1,
            TheWaterRenderObj ? fmod(TheWaterRenderObj->getBumpPhaseForValidation()-birthBump+NUM_BUMP_FRAMES, static_cast<Real>(NUM_BUMP_FRAMES)) : -1,
            sys->getParticleCount());
        if (sample == 0 && std::getenv("CNC240_VERIFY_SCREENSHOT")) TheDisplay->takeScreenShot(SCREENSHOT_PNG);
        ++sample; fflush(stdout);
    }
    if (!particle && !expired) {
        expired = true;
        printf("CNC240_EFFECT_EXPIRED legacy_age=%u ww_seconds=%.5f samples=%u\n", age, (WW3D::Get_Sync_Time()-birthSync)/1000.0, sample);
        fflush(stdout);
    }
}
}
