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
// FILE: Decision.h
// GeneralsX @feature cemlyn007 16/10/2026 The decision an engine hands its host's decision sink
// (rts::DecisionSink of Common/EmbeddingContract.h, docs/EMBEDDING.md): one order an AI player, an AI script, the
// message stream or a unit's own reaction issued, as AIDecisionObserver's hooks see it.
//
// A Decision owns everything it holds: game objects, templates, special powers, waypoints and polygon triggers by
// their ids, upgrades, sciences, command buttons, teams and scripts by name. The sink may keep it after the call.
// A field that does not apply to the decision's kind is empty.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "GameLogic/AIDecisionObserver.h"

namespace rts
{

struct DecisionPosition
{
	float x;
	float y;
	float z;

	friend bool operator==(const DecisionPosition&, const DecisionPosition&) = default;
};

struct Decision
{
	// The engine's count of decisions before this one: 0 for its first.
	std::uint64_t sequence = 0;
	// The logic frame the decision was made on.
	std::uint32_t frame = 0;
	// What issued it: the outermost AIDecisionScope active, or AI_DECISION_REFLEX outside any.
	AIDecisionOrigin origin = AI_DECISION_REFLEX;
	AIDecisionKind kind = AI_DECISION_UNIT_COMMAND;
	// Above 0: issued while another decision was carried out (a unit command, build, sale or special power), so it
	// followed from that order.
	std::uint8_t depth = 0;
	// The index of the acting player in the player list.
	std::optional<std::int32_t> player;
	// The unit ordered, the producer, builder, seller or power source, or the structure whose construction is
	// cancelled, whose rally point is set or whose overcharge is toggled.
	std::optional<std::uint32_t> actor;
	// A unit command's command (AICommandType) and the order's own source (CommandSourceType).
	std::optional<AICommandType> command;
	std::optional<CommandSourceType> commandSource;
	// Where: a unit command's position (always given for one, meaningful for a positional command), a built or
	// sold structure's, a special power's target position or a rally point.
	std::optional<DecisionPosition> position;
	// A unit command's path (AICommandParms::m_coords).
	std::vector<DecisionPosition> path;
	// What it acted on: a target object (a unit command's or a special power's, or a built structure), a unit
	// command's second object (AICommandParms::m_otherObj) and its team, by name.
	std::optional<std::uint32_t> target;
	std::optional<std::uint32_t> otherObject;
	std::optional<std::string> team;
	std::optional<std::uint32_t> waypoint;
	std::optional<std::int32_t> polygonTrigger;
	// A unit command's integer argument (AICommandParms::m_intValue).
	std::optional<std::int32_t> intValue;
	// The weapon slot a weapon switch locks (WeaponSlotType).
	std::optional<std::int32_t> weaponSlot;
	// The command button, by name: a unit command's, or a weapon switch's when a button made it.
	std::optional<std::string> commandButton;
	// The thing template queued or built, by template id.
	std::optional<std::uint32_t> thingTemplate;
	std::optional<std::string> upgrade;
	std::optional<std::string> science;
	// The special power, by its id, and its command options.
	std::optional<std::uint32_t> specialPower;
	std::optional<std::uint32_t> commandOptions;
	// The innermost script that issued it, by name.
	std::optional<std::string> script;

	friend bool operator==(const Decision&, const Decision&) = default;
};

} // namespace rts
