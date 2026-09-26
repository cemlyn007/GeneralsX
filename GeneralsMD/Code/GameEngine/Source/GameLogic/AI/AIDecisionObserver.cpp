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

// AIDecisionObserver.cpp
// See GameLogic/AIDecisionObserver.h.

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/Player.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIDecisionObserver.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/Module/AIUpdate.h"

namespace
{
	AIDecisionObserverFn s_observer = nullptr;
	void *s_userData = nullptr;

	// The logic runs on one thread, so plain statics suffice.
	Bool s_scopeActive = FALSE;
	AIDecisionOrigin s_origin = AI_DECISION_REFLEX;
	const char *s_script = nullptr;
	UnsignedByte s_depth = 0;
	UnsignedInt s_seq = 0;

	AIDecision makeDecision(AIDecisionKind kind, const Object *actor)
	{
		AIDecision d = {};
		d.m_frame = TheGameLogic ? TheGameLogic->getFrame() : 0;
		d.m_seq = s_seq++;
		d.m_origin = s_scopeActive ? s_origin : AI_DECISION_REFLEX;
		d.m_kind = kind;
		d.m_depth = s_depth;
		d.m_player = -1;
		if (actor)
		{
			const Player *p = actor->getControllingPlayer();
			if (p)
				d.m_player = p->getPlayerIndex();
		}
		d.m_actor = actor;
		d.m_aiCommand = AICMD_NO_COMMAND;
		d.m_commandSource = CMD_FROM_AI;
		d.m_science = SCIENCE_INVALID;
		d.m_script = s_script;
		return d;
	}

	void emit(const AIDecision &d)
	{
		s_observer(d, s_userData);
	}
}

//-------------------------------------------------------------------------------------------------
void setAIDecisionObserver(AIDecisionObserverFn fn, void *userData)
{
	s_observer = fn;
	s_userData = fn ? userData : nullptr;
	s_seq = 0;
}

//-------------------------------------------------------------------------------------------------
AIDecisionScope::AIDecisionScope(AIDecisionOrigin origin, const char *script)
	: m_savedOrigin(s_origin), m_savedScript(s_script), m_savedActive(s_scopeActive)
{
	if (!s_scopeActive)
		s_origin = origin;
	if (script)
		s_script = script;
	s_scopeActive = TRUE;
}

AIDecisionScope::~AIDecisionScope()
{
	s_origin = m_savedOrigin;
	s_script = m_savedScript;
	s_scopeActive = m_savedActive;
}

//-------------------------------------------------------------------------------------------------
AIDecisionHook::UnitCommand::UnitCommand(const AICommandInterface *ai, const AICommandParms *parms)
	: m_counted(FALSE)
{
	if (!s_observer)
		return;

	// AIUpdateInterface is the only AICommandInterface.
	const Object *actor = static_cast<const AIUpdateInterface *>(ai)->friend_getCommandedObject();
	AIDecision d = makeDecision(AI_DECISION_UNIT_COMMAND, actor);
	d.m_aiCommand = parms->m_cmd;
	d.m_commandSource = parms->m_cmdSource;
	d.m_pos = &parms->m_pos;
	d.m_target = parms->m_obj;
	d.m_other = parms->m_otherObj;
	d.m_team = parms->m_team;
	d.m_waypoint = parms->m_waypoint;
	d.m_polygon = parms->m_polygon;
	d.m_coords = parms->m_coords.empty() ? nullptr : &parms->m_coords[0];
	d.m_numCoords = (Int)parms->m_coords.size();
	d.m_intValue = parms->m_intValue;
	d.m_commandButton = parms->m_commandButton;
	emit(d);

	++s_depth;
	m_counted = TRUE;
}

AIDecisionHook::UnitCommand::~UnitCommand()
{
	if (m_counted)
		--s_depth;
}

//-------------------------------------------------------------------------------------------------
void AIDecisionHook::queueUnit(const Object *producer, const ThingTemplate *thing)
{
	if (!s_observer)
		return;
	AIDecision d = makeDecision(AI_DECISION_QUEUE_UNIT, producer);
	d.m_thing = thing;
	emit(d);
}

void AIDecisionHook::queueUpgrade(const Object *producer, const UpgradeTemplate *upgrade)
{
	if (!s_observer)
		return;
	AIDecision d = makeDecision(AI_DECISION_QUEUE_UPGRADE, producer);
	d.m_upgrade = upgrade;
	emit(d);
}

void AIDecisionHook::cancelUnit(const Object *producer, const ThingTemplate *thing)
{
	if (!s_observer)
		return;
	AIDecision d = makeDecision(AI_DECISION_CANCEL_UNIT, producer);
	d.m_thing = thing;
	emit(d);
}

void AIDecisionHook::cancelUpgrade(const Object *producer, const UpgradeTemplate *upgrade)
{
	if (!s_observer)
		return;
	AIDecision d = makeDecision(AI_DECISION_CANCEL_UPGRADE, producer);
	d.m_upgrade = upgrade;
	emit(d);
}

void AIDecisionHook::buildStructure(const Object *builder, const Object *structure, const Player *owner)
{
	if (!s_observer)
		return;
	AIDecision d = makeDecision(AI_DECISION_BUILD_STRUCTURE, builder);
	if (owner)
		d.m_player = owner->getPlayerIndex();
	d.m_target = structure;
	d.m_thing = structure->getTemplate();
	d.m_pos = structure->getPosition();
	emit(d);
}

void AIDecisionHook::sell(const Object *structure)
{
	if (!s_observer)
		return;
	AIDecision d = makeDecision(AI_DECISION_SELL, structure);
	d.m_pos = structure->getPosition();
	emit(d);
}

void AIDecisionHook::purchaseScience(const Player *player, ScienceType science)
{
	if (!s_observer)
		return;
	AIDecision d = makeDecision(AI_DECISION_PURCHASE_SCIENCE, nullptr);
	d.m_player = player->getPlayerIndex();
	d.m_science = science;
	emit(d);
}

void AIDecisionHook::specialPower(const Object *source, const SpecialPowerTemplate *power,
	const Coord3D *pos, const Object *target, const Waypoint *waypoint, UnsignedInt commandOptions)
{
	if (!s_observer)
		return;
	AIDecision d = makeDecision(AI_DECISION_SPECIAL_POWER, source);
	d.m_specialPower = power;
	d.m_pos = pos ? pos : (target ? target->getPosition() : nullptr);
	d.m_target = target;
	d.m_waypoint = waypoint;
	d.m_commandOptions = commandOptions;
	emit(d);
}
