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

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// W3DDynamicLight.cpp
// Class to handle dynamic lights.
// Author: John Ahlquist, April 2001
#include <stdlib.h>
#include <cstdio>
#include <cstring>
#include "WW3D2/ww3d.h"

#include "W3DDevice/GameClient/W3DDynamicLight.h"

W3DDynamicLight::W3DDynamicLight():
LightClass(LightClass::POINT)
{

	m_priorEnable = false;
	setEnabled(true);

}

W3DDynamicLight::~W3DDynamicLight()
{
}

void W3DDynamicLight::On_Frame_Update()
{
	if (!m_enabled) {
		return;
	}
    const unsigned now=WW3D::Get_Logic_Time_Milliseconds();
    const Real factor=m_fadeClock.factor(now);
    if(m_verifyFade) {
        printf("CNC240_LIGHT_FADE age_ms=%.0f factor=%.6f expired=%d\n",m_fadeClock.age(now),factor,m_fadeClock.expired(now));
    }
    if(m_fadeClock.expired(now)) {m_enabled=false;return;}
	if (m_decayRange) {
		this->FarAttenEnd = factor*m_targetRange;
		if (FarAttenEnd < FarAttenStart) {
			FarAttenEnd = FarAttenStart;
		}
	}
	if (m_decayColor) {
		this->Ambient = m_targetAmbient*factor;
		this->Diffuse = m_targetDiffuse*factor;
	}
}

void W3DDynamicLight::setFrameFade(UnsignedInt frameIncreaseTime, UnsignedInt decayFrameTime)
{
    m_fadeClock.reset(frameIncreaseTime,decayFrameTime,WW3D::Get_Logic_Time_Milliseconds());
    const char *mode=std::getenv("CNC240_VERIFY_GAMEPLAY");
    m_verifyFade=mode && !strcmp(mode,"audit") && FarAttenEnd==137;
    if(m_verifyFade)printf("CNC240_LIGHT_START hz=%d grow_frames=%u decay_frames=%u\n",cnc240::simulationHz(),frameIncreaseTime,decayFrameTime);
	m_targetAmbient = Ambient;
	m_targetDiffuse = Diffuse;
	m_targetRange = FarAttenEnd;
}
