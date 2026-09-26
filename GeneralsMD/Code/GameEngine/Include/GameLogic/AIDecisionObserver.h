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

// AIDecisionObserver.h
// A read-only hook that reports every order issued in the sim -- unit commands,
// production, construction, sales, science purchases and special powers -- to
// an optional observer, together with what issued it: an AI player, a script,
// a player's message, or none of those (a unit's own behaviour).
//
// The engine's AI players act on objects directly rather than through the
// message stream, so their decisions are otherwise invisible outside the sim.
// With no observer installed (the default) every hook is a null check; with
// one installed the hook only reads, so the sim, its CRC and replays are
// unaffected either way.

#pragma once

#include "Common/GameType.h"

// Forward declarations only: GameLogic/AI.h includes this header.
class AICommandInterface;
struct AICommandParms;
class CommandButton;
class Object;
class Player;
class PolygonTrigger;
class SpecialPowerTemplate;
class Team;
class ThingTemplate;
class UpgradeTemplate;
class Waypoint;

enum AICommandType CPP_11(: Int);
enum CommandSourceType CPP_11(: Int);
enum ScienceType CPP_11(: Int);

// What issued a decision: the outermost decision scope active when it was
// made (see AIDecisionScope). REFLEX is no scope at all, i.e. an object's own
// per-frame behaviour (auto-targeting, supply-truck loops, dozer repairs, ...).
enum AIDecisionOrigin CPP_11(: Byte)
{
	AI_DECISION_REFLEX = 0,
	AI_DECISION_AI_PLAYER,  // an AIPlayer, via Player's calls into it
	AI_DECISION_SCRIPT,     // a script action (ScriptEngine::executeActions)
	AI_DECISION_MESSAGE,    // a GameMessage (GameLogic::logicMessageDispatcher)
};

enum AIDecisionKind CPP_11(: Byte)
{
	AI_DECISION_UNIT_COMMAND = 0,  // an AICommandInterface order; see m_aiCommand
	AI_DECISION_QUEUE_UNIT,
	AI_DECISION_QUEUE_UPGRADE,
	AI_DECISION_CANCEL_UNIT,
	AI_DECISION_CANCEL_UPGRADE,
	AI_DECISION_BUILD_STRUCTURE,   // BuildAssistant::buildObjectNow
	AI_DECISION_SELL,
	AI_DECISION_PURCHASE_SCIENCE,
	AI_DECISION_SPECIAL_POWER,     // a special power module fired by a group or a script
};

// One decision. Pointers are only valid for the duration of the observer call.
struct AIDecision
{
	UnsignedInt m_frame;
	UnsignedInt m_seq;            // running count of decisions reported, for ordering
	AIDecisionOrigin m_origin;
	AIDecisionKind m_kind;
	UnsignedByte m_depth;         // unit commands issued while carrying out another one
	Int m_player;                 // index of the player the actor belongs to, -1 if none
	const Object *m_actor;        // the unit ordered, producer, builder, seller or power source
	AICommandType m_aiCommand;    // AICMD_NO_COMMAND unless m_kind == AI_DECISION_UNIT_COMMAND
	CommandSourceType m_commandSource; // UNIT_COMMAND only (the order's own source); undefined otherwise
	const Coord3D *m_pos;         // target position, or null
	const Object *m_target;       // target object, or null
	const Object *m_other;        // AICommandParms::m_otherObj, or null
	const Team *m_team;           // AICommandParms::m_team, or null
	const Waypoint *m_waypoint;
	const PolygonTrigger *m_polygon;
	const Coord3D *m_coords;      // AICommandParms::m_coords
	Int m_numCoords;
	Int m_intValue;               // AICommandParms::m_intValue
	const CommandButton *m_commandButton;
	const ThingTemplate *m_thing; // unit queued / structure built
	const UpgradeTemplate *m_upgrade;
	ScienceType m_science;
	const SpecialPowerTemplate *m_specialPower;
	UnsignedInt m_commandOptions; // special power command options
	const char *m_script;         // innermost script name, or null
};

typedef void (*AIDecisionObserverFn)(const AIDecision &decision, void *userData);

// Install (or, with null, remove) the observer. One at a time.
void setAIDecisionObserver(AIDecisionObserverFn fn, void *userData);

// RAII: attributes every decision made while alive to origin. Scopes nest; the
// outermost origin wins (a script calling into an AIPlayer is the script's
// decision), and the innermost non-null script name is reported.
class AIDecisionScope
{
public:
	AIDecisionScope(AIDecisionOrigin origin, const char *script = nullptr);
	~AIDecisionScope();
private:
	AIDecisionOrigin m_savedOrigin;
	const char *m_savedScript;
	Bool m_savedActive;
};

// RAII: reports nothing while alive, for engine bookkeeping that goes through a call
// which is otherwise a decision (e.g. cancelling a duplicate upgrade after a player
// gains it). Nests.
class AIDecisionMute
{
public:
	AIDecisionMute();
	~AIDecisionMute();
};

// The emit points. Each is a no-op without an observer or while muted.
namespace AIDecisionHook
{
	// Brackets one AICommandInterface order (see AICommandInterface::aiDoCommand's
	// non-virtual overload); commands issued inside it are reported one level deeper.
	class UnitCommand
	{
	public:
		UnitCommand(const AICommandInterface *ai, const AICommandParms *parms);
		~UnitCommand();
	private:
		Bool m_counted;
	};

	void queueUnit(const Object *producer, const ThingTemplate *thing);
	void queueUpgrade(const Object *producer, const UpgradeTemplate *upgrade);
	void cancelUnit(const Object *producer, const ThingTemplate *thing);
	void cancelUpgrade(const Object *producer, const UpgradeTemplate *upgrade);
	void buildStructure(const Object *builder, const Object *structure, const Player *owner);
	void sell(const Object *structure);
	void purchaseScience(const Player *player, ScienceType science);
	void specialPower(const Object *source, const SpecialPowerTemplate *power,
		const Coord3D *pos, const Object *target, const Waypoint *waypoint, UnsignedInt commandOptions);
}
