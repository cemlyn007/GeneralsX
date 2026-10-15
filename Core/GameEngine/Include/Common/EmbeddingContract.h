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
// FILE: EmbeddingContract.h
// GeneralsX @feature cemlyn007 14/10/2026 The embedding contract: what a host that embeds engines in its process
// gives each engine, and how the engine is configured (docs/EMBEDDING.md).
//
// GeneralsX declares these interfaces and the host implements them. An engine reaches them through its
// EngineContext's EmbeddingConfig, never through anything process-wide, so engines in one process share nothing.
#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <stop_token>
#include <string>

namespace rts
{

// The engine's crash paths end here instead of ending the process.
class FaultSink
{
public:
	virtual ~FaultSink() = default;

	// Latches the engine's fault, with `reason` kept as raw bytes, and does not return: it throws the host's
	// unwind exception, which leaves the engine's code. The engine never runs again.
	[[noreturn]] virtual void fault(std::string reason) = 0;
};

// One decision an AI player, an AI script, the message stream or a unit's own reaction made.
struct Decision;

// Receives the engine's decisions while the host records them.
class DecisionSink
{
public:
	virtual ~DecisionSink() = default;

	virtual void record(const Decision& decision) = 0;
};

// Told when the game asks to quit, in place of the game ending the process.
class QuitListener
{
public:
	virtual ~QuitListener() = default;

	virtual void quitRequested() = 0;
};

// Starts every thread the engine runs.
class ThreadStarter
{
public:
	using ThreadBody = std::function<void(std::stop_token)>;

	virtual ~ThreadStarter() = default;

	// Starts a thread named `name` that runs `body` with this engine's context current. `body` returns when its
	// stop token is stopped; stopping and joining it needs nothing from the game objects it works on.
	virtual void startThread(std::string name, ThreadBody body) = 0;
};

// Records every operating-system resource the engine acquires, so that the host can release them by handle
// without running the engine's code.
class ResourceLedger
{
public:
	using Id = std::uint64_t;
	using Release = std::function<void(std::uintptr_t handle)>;

	virtual ~ResourceLedger() = default;

	// Records a resource just acquired: its kind (for reports), its raw handle and how to release it.
	virtual Id record(std::string kind, std::uintptr_t handle, Release release) = 0;
	// Releases a recorded resource now, as the layer that acquired it closes it. False if it is not recorded.
	virtual bool releaseOne(Id id) = 0;
	// Drops a recorded resource that its layer has already released. False if it is not recorded.
	virtual bool forget(Id id) = 0;
};

// Whether an engine renders, and to what.
enum class RenderMode
{
	none,
	headless,
	window,
};

// Whether an engine opens an audio device.
enum class AudioMode
{
	off,
	on,
};

// How one engine is embedded: its roots, its modes and the host's services. Every field is required; only
// decisionSink may be null, for an engine that does not record its decisions.
struct EmbeddingConfig
{
	std::filesystem::path assetRoot;
	std::filesystem::path userDataRoot;
	RenderMode render;
	AudioMode audio;
	FaultSink* faultSink;
	QuitListener* quitListener;
	ThreadStarter* threadStarter;
	ResourceLedger* resourceLedger;
	DecisionSink* decisionSink;
};

} // namespace rts
