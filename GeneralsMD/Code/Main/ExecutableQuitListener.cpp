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
// FILE: ExecutableQuitListener.cpp
// GeneralsX @feature cemlyn007 15/10/2026 The game executable's QuitListener (see ExecutableEmbedding.h)
//
// The game asks to quit through GameEngine::setQuitting, which sets the engine's quitting flag and then tells this
// listener. The flag ends GameEngine::execute()'s main loop, so GameMain returns and the executable exits as upstream
// does; the executable runs its engine's frames itself, so it has nothing more to do. The listener holds no state:
// it is constant initialised, so it lives in read-only storage.

#if RTS_ENGINE_CONTEXT

#include "ExecutableEmbedding.h"

namespace rts
{

namespace
{

class ExecutableQuitListener final : public QuitListener
{
public:
	constexpr ExecutableQuitListener() = default;

	void quitRequested() override
	{
	}
};

constinit ExecutableQuitListener TheExecutableQuitListener;

} // namespace

QuitListener* executableQuitListener()
{
	return &TheExecutableQuitListener;
}

} // namespace rts

#endif
