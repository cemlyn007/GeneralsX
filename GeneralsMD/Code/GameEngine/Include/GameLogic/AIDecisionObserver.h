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
class SpecialPowerModuleInterface;
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
	AI_DECISION_SCRIPT,     // a script (executeScript, a sequential script, friend_executeScriptActions)
	AI_DECISION_MESSAGE,    // a GameMessage (GameLogic::logicMessageDispatcher)

	AI_DECISION_ORIGIN_COUNT // GeneralsX @feature cemlyn007 16/10/2026 The count, for the name table.
};

// GeneralsX @feature cemlyn007 16/10/2026 The origins' names, valued as AIDecisionOrigin, for the host's
// vocabulary generator (design decision 20).
#ifdef DEFINE_AIDECISION_NAMES
static const char *const TheAIDecisionOriginNames[] =
{
	"REFLEX",
	"AI_PLAYER",
	"SCRIPT",
	"MESSAGE",

	nullptr
};
static_assert(ARRAY_SIZE(TheAIDecisionOriginNames) == AI_DECISION_ORIGIN_COUNT + 1, "Incorrect array size");
#endif

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
	AI_DECISION_SPECIAL_POWER,     // Object::doSpecialPower*
	AI_DECISION_SWITCH_WEAPON,     // a SWITCH_WEAPON command button (Object::doCommandButton) or a player's
	                               // MSG_SWITCH_WEAPONS (AIGroup::setWeaponLockForGroup); see m_intValue
	// GeneralsX @feature cemlyn007 16/10/2026 The orders the hooks above missed: a construction cancelled by its
	// player (MSG_DOZER_CANCEL_CONSTRUCT), a rally point set and an overcharge toggled.
	AI_DECISION_CANCEL_BUILD,      // GameLogic::onDozerCancelConstruct; m_actor is the structure
	AI_DECISION_SET_RALLY_POINT,   // doSetRallyPoint (MSG_SET_RALLY_POINT), once the point is accepted; see m_pos
	AI_DECISION_TOGGLE_OVERCHARGE, // AIGroup::groupToggleOvercharge, per structure that has an overcharge

	AI_DECISION_KIND_COUNT
};

// GeneralsX @feature cemlyn007 16/10/2026 The kinds' names, valued as AIDecisionKind, for the host's
// vocabulary generator (design decision 20).
#ifdef DEFINE_AIDECISION_NAMES
static const char *const TheAIDecisionKindNames[] =
{
	"UNIT_COMMAND",
	"QUEUE_UNIT",
	"QUEUE_UPGRADE",
	"CANCEL_UNIT",
	"CANCEL_UPGRADE",
	"BUILD_STRUCTURE",
	"SELL",
	"PURCHASE_SCIENCE",
	"SPECIAL_POWER",
	"SWITCH_WEAPON",
	"CANCEL_BUILD",
	"SET_RALLY_POINT",
	"TOGGLE_OVERCHARGE",

	nullptr
};
static_assert(ARRAY_SIZE(TheAIDecisionKindNames) == AI_DECISION_KIND_COUNT + 1, "Incorrect array size");
#endif

// One decision. Pointers are only valid for the duration of the observer call.
struct AIDecision
{
	UnsignedInt m_frame;
	UnsignedInt m_seq;            // running count of decisions reported, for ordering
	AIDecisionOrigin m_origin;
	AIDecisionKind m_kind;
	UnsignedByte m_depth;         // > 0: issued while carrying out another decision (a unit command,
	                              // build, sale or special power), e.g. units moved off a building site
	Int m_player;                 // index of the player the actor belongs to, -1 if none
	const Object *m_actor;        // the unit ordered, producer, builder, seller or power source
	AICommandType m_aiCommand;    // AICMD_NO_COMMAND unless m_kind == AI_DECISION_UNIT_COMMAND
	CommandSourceType m_commandSource; // UNIT_COMMAND: the order's own source; (CommandSourceType)-1 otherwise
	const Coord3D *m_pos;         // target position, or null; UNIT_COMMAND: always the order's m_pos,
	                              // meaningful only for positional commands
	const Object *m_target;       // target object, or null
	const Object *m_other;        // AICommandParms::m_otherObj, or null
	const Team *m_team;           // AICommandParms::m_team, or null
	const Waypoint *m_waypoint;
	const PolygonTrigger *m_polygon;
	const Coord3D *m_coords;      // AICommandParms::m_coords
	Int m_numCoords;
	Int m_intValue;               // AICommandParms::m_intValue; SWITCH_WEAPON: the WeaponSlotType locked
	const CommandButton *m_commandButton; // AICommandParms::m_commandButton; SWITCH_WEAPON: the button
	const ThingTemplate *m_thing; // unit queued / structure built
	const UpgradeTemplate *m_upgrade;
	ScienceType m_science;
	const SpecialPowerTemplate *m_specialPower;
	UnsignedInt m_commandOptions; // special power command options
	const char *m_script;         // innermost script name, or null
};

#if RTS_ENGINE_CONTEXT
// GeneralsX @feature cemlyn007 16/10/2026 With RTS_ENGINE_CONTEXT there is no observer to install: every decision
// goes to the current engine's decision sink (EmbeddingConfig::decisionSink, as an rts::Decision of
// GameLogic/Decision.h), and an engine whose host gave it none records nothing. Each hook is then a null check.
#else
typedef void (*AIDecisionObserverFn)(const AIDecision &decision, void *userData);

// Install (or, with null, remove) the observer. One at a time.
void setAIDecisionObserver(AIDecisionObserverFn fn, void *userData);
#endif

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
	void purchaseScience(const Player *player, ScienceType science);
	void switchWeapon(const Object *actor, const CommandButton *button, Int weaponSlot);
	// GeneralsX @feature cemlyn007 16/10/2026 A construction cancelled by its player, a rally point set and an
	// overcharge toggled.
	void cancelBuild(const Object *structure);
	void setRallyPoint(const Object *structure, const Coord3D *pos);
	void toggleOvercharge(const Object *structure);

	// Brackets one BuildAssistant::buildObjectNow: what it does on the way (units moved off
	// the site, the dozer stopped) is reported one level deeper, then report() reports the
	// build itself, at the outer level, once the structure exists.
	class Build
	{
	public:
		Build();
		~Build();
		void report(const Object *builder, const Object *structure, const Player *owner);
	private:
		Bool m_counted;
		UnsignedByte m_outerDepth;
	};

	// Brackets one BuildAssistant::sellObject: reports the sale, and what it does on the way
	// (the building stopped, its occupants evacuated) one level deeper.
	class Sale
	{
	public:
		Sale(const Object *structure);
		~Sale();
	private:
		Bool m_counted;
	};

	// Brackets one Object::doSpecialPower* call: reports the power if the module will act on it
	// (the source is not disabled, under construction or paused, nor a rejected launcher), and
	// what the power does on the way one level deeper.
	class SpecialPower
	{
	public:
		SpecialPower(const Object *source, const SpecialPowerModuleInterface *module,
			const SpecialPowerTemplate *power, const Coord3D *pos, const Object *target,
			const Waypoint *waypoint, UnsignedInt commandOptions);
		~SpecialPower();
	private:
		Bool m_counted;
	};
}
