/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "PreRTS.h"

#include "Common/FramePacer.h"
#include "Common/Cnc240Config.h"

#include "GameClient/View.h"

#include "GameLogic/GameLogic.h"
#include "GameLogic/ScriptEngine.h"

#include "GameNetwork/NetworkDefs.h"
#include "GameNetwork/NetworkInterface.h"


FramePacer* TheFramePacer = nullptr;

FramePacer::FramePacer()
{
	// Set the time slice size to 1 ms.
	timeBeginPeriod(1);

	m_maxFPS = cnc240::DefaultRenderFps;
	m_logicTimeScaleFPS = BaseFps;
	m_updateTime = 1.0f / (Real)cnc240::DefaultRenderFps;
	m_logicFramePhase = 1.0f;
	m_clientVisualAnchor = 0;
	m_clientVisualStep = 0;
	m_clientVisualFrame = 0;
	m_clientVisualValid = FALSE;
	m_enableFpsLimit = FALSE;
	m_enableLogicTimeScale = TRUE;
	m_isTimeFrozen = FALSE;
	m_isGameHalted = FALSE;
}

FramePacer::~FramePacer()
{
	// Restore the previous time slice for Windows.
	timeEndPeriod(1);
}

void FramePacer::update()
{
	// Uses a high resolution counter to cap the frame rate more accurately to the desired limit than retail did.
	const UnsignedInt maxFps = getActualFramesPerSecondLimit();
	m_updateTime = m_frameRateLimit.wait(maxFps);

	if (TheGameLogic != nullptr)
	{
		// Set or advance the logic frame phase by the render step that the next update will draw.
		// It is capped at a whole logic frame, because the render steps in between can add up to more than one.
		// Consumers are expected to interpolate towards the next logic frame and not extrapolate past it.
		const Real timeScale = getActualLogicTimeScaleOverFpsRatio();

		if (TheGameLogic->hasUpdated())
		{
			m_logicFramePhase = min(1.0f, timeScale);
		}
		else
		{
			m_logicFramePhase = min(1.0f, m_logicFramePhase + timeScale);
		}
	}
}

void FramePacer::reset()
{
	m_clientVisualValid = FALSE;
	m_clientVisualStep = 0.0f;
	m_frameRateLimit.reset();
	m_updateTime = 1.0f / (Real)getActualFramesPerSecondLimit();
	m_logicFramePhase = 1.0f;
}

void FramePacer::setFramesPerSecondLimit( Int fps )
{
	DEBUG_LOG(("FramePacer::setFramesPerSecondLimit() - setting max fps to %d (TheGlobalData->m_useFpsLimit == %d)", fps, TheGlobalData->m_useFpsLimit));
	m_maxFPS = cnc240::clampRenderFps(fps);
}

Int FramePacer::getFramesPerSecondLimit()  const
{
	return m_maxFPS;
}

void FramePacer::enableFramesPerSecondLimit( Bool enable )
{
	m_enableFpsLimit = enable;
}

Bool FramePacer::isFramesPerSecondLimitEnabled() const
{
	return m_enableFpsLimit;
}

Bool FramePacer::isActualFramesPerSecondLimitEnabled() const
{
	Bool allowFpsLimit = true;

	if (TheTacticalView != nullptr)
	{
		allowFpsLimit &= TheTacticalView->getTimeMultiplier()<=1 && !TheScriptEngine->isTimeFast();
	}

	if (TheGameLogic != nullptr)
	{
#if defined(_ALLOW_DEBUG_CHEATS_IN_RELEASE)
		allowFpsLimit &= !(!TheGameLogic->isGamePaused() && TheGlobalData->m_TiVOFastMode);
#else	//always allow this cheat key if we're in a replay game.
		allowFpsLimit &= !(!TheGameLogic->isGamePaused() && TheGlobalData->m_TiVOFastMode && TheGameLogic->isInReplayGame());
#endif
	}

	allowFpsLimit &= TheGlobalData->m_useFpsLimit;
	allowFpsLimit &= isFramesPerSecondLimitEnabled();

	return allowFpsLimit;
}

Int FramePacer::getActualFramesPerSecondLimit() const
{
	// Keep the selected rendering cap even when retail options request uncapped rendering.
	return getFramesPerSecondLimit();
}

Real FramePacer::getUpdateTime()  const
{
	return m_updateTime;
}

Real FramePacer::getUpdateFps()  const
{
	return 1.0f / m_updateTime;
}

Real FramePacer::getBaseOverUpdateFpsRatio(Real minUpdateFps)
{
	// Update fps is floored to default 5 fps, 200 ms.
	// Useful to prevent insane ratios on frame spikes/stalls.
	return (Real)BaseFps / std::max(getUpdateFps(), minUpdateFps);
}

void FramePacer::setTimeFrozen(Bool frozen)
{
	m_isTimeFrozen = frozen;
}

void FramePacer::setGameHalted(Bool halted)
{
	m_isGameHalted = halted;
}

Bool FramePacer::isTimeFrozen() const
{
	return m_isTimeFrozen;
}

Bool FramePacer::isGameHalted() const
{
	return m_isGameHalted;
}

void FramePacer::setLogicTimeScaleFps( Int fps )
{
	m_logicTimeScaleFPS = fps;
}

Int FramePacer::getLogicTimeScaleFps() const
{
	return m_logicTimeScaleFPS;
}

void FramePacer::enableLogicTimeScale( Bool enable )
{
	m_enableLogicTimeScale = enable;
}

Bool FramePacer::isLogicTimeScaleEnabled() const
{
	return m_enableLogicTimeScale;
}

Int FramePacer::getActualLogicTimeScaleFps(LogicTimeQueryFlags flags) const
{
	if (m_isTimeFrozen && (flags & IgnoreFrozenTime) == 0)
	{
		return 0;
	}

	if (m_isGameHalted && (flags & IgnoreHaltedGame) == 0)
	{
		return 0;
	}

	if (TheNetwork != nullptr)
	{
		return TheNetwork->getFrameRate();
	}

	if (isLogicTimeScaleEnabled())
	{
		return static_cast<Int>(std::lround(getLogicTimeScaleFps() * (cnc240::simulationHz() / 30.0)));
	}

	// Returns uncapped value to align with the render update as per the original game behavior.
	return RenderFpsPreset::UncappedFpsValue;
}

Real FramePacer::getActualLogicTimeScaleRatio(LogicTimeQueryFlags flags) const
{
	return (Real)getActualLogicTimeScaleFps(flags) / LOGICFRAMES_PER_SECONDS_REAL;
}

Real FramePacer::getActualLogicTimeScaleOverFpsRatio(LogicTimeQueryFlags flags) const
{
	// Display consumers can span multiple simulation steps at lower render rates.
	return (Real)getActualLogicTimeScaleFps(flags) * min(getUpdateTime(), 0.2f);
}

void FramePacer::updateClientVisualTime(UnsignedInt frame)
{
    const double clock = (frame + static_cast<double>(getLogicFramePhase())) / cnc240::simulationHz();
    m_clientVisualStep = 0.0f;
    if (!m_clientVisualValid || frame < m_clientVisualFrame) {
        m_clientVisualAnchor = clock;
        m_clientVisualValid = TRUE;
    } else if (!isTimeFrozen() && !isGameHalted() && clock > m_clientVisualAnchor) {
        m_clientVisualStep = static_cast<Real>(clock - m_clientVisualAnchor);
        m_clientVisualAnchor = clock;
    }
    m_clientVisualFrame = frame;
}

Real FramePacer::getClientVisualTimeStepTicks() const
{
    return m_clientVisualStep * cnc240::simulationHz();
}

Real FramePacer::getVisualTimeStepFrames(LogicTimeQueryFlags flags) const
{
    return flags ? 30.0f * getLogicTimeStepSeconds(flags) : 30.0f * m_clientVisualStep;
}

Real FramePacer::getLogicTimeStepSeconds(LogicTimeQueryFlags flags) const
{
	return getActualLogicTimeScaleRatio(flags) * min(getUpdateTime(), 0.2f);
}

Real FramePacer::getLogicTimeStepMilliseconds(LogicTimeQueryFlags flags) const
{
	return 1000.0f * getLogicTimeStepSeconds(flags);
}

Real FramePacer::getLogicFramePhase() const
{
	return m_logicFramePhase;
}
