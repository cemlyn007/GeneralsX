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
// FILE: EngineSockets.h
// GeneralsX @feature cemlyn007 16/10/2026 The engine's socket layer: the one place GeneralsX opens sockets, each
// recorded in the current engine's ResourceLedger (Common/EmbeddingContract.h, docs/EMBEDDING.md).
//
// Each socket this layer opens (or accepts) is recorded in the engine's ResourceLedger as kind "socket <what>", with
// the descriptor as its handle and close() as its release, and forgotten as this layer closes it. The host can then
// close whatever sockets a faulted engine still holds without running the engine's code. A socket opened here must be
// closed here (closeEngineSocket), never with close or closesocket. Calling any of these with no embedded engine
// current throws std::logic_error (rts::currentEmbedding).
//
// The layer also resolves host names for the engine's own threads. gethostbyname cannot be interrupted, so a thread
// blocked in it holds up its engine's stop until the system resolver gives up, which can take tens of seconds.
// resolveEngineHostName asks the name servers itself, over a socket of this layer, and returns within a poll interval
// of a stop request.
#pragma once

#if RTS_ENGINE_CONTEXT

// The engine's headers may define min and max as macros, which the standard headers cannot be parsed with.
#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#include <netinet/in.h>
#include <sys/socket.h>

#include <chrono>
#include <optional>
#include <stop_token>
#include <vector>
#pragma pop_macro("max")
#pragma pop_macro("min")

namespace rts
{

// socket() for the current engine: the new socket is recorded in its ResourceLedger as "socket <what>". On failure
// nothing is recorded and the result is -1, with socket()'s errno.
int openEngineSocket(int domain, int type, int protocol, const char* what);

// accept() for the current engine: the accepted connection is recorded as "socket <what>". The listening socket is
// left as it is. On failure (no connection waiting on a non-blocking listener, too) nothing is recorded and the result
// is -1, with accept()'s errno.
int acceptEngineSocket(int listener, sockaddr* address, socklen_t* addressLength, const char* what);

// close() for a socket this layer opened for the current engine: forgets it in the ledger, then closes it. A
// descriptor this layer did not open, or has already closed, is left alone: the result is -1 with errno EBADF, so a
// second close cannot close a descriptor the number has since been given to.
int closeEngineSocket(int socket);

// The IPv4 address of `name`, asked of the name servers the system resolver is configured with (resolv.conf), or
// none if it has none, or none answers, or `stop` is stopped first. A dotted-quad name is its own address. Unlike
// gethostbyname, it consults neither the hosts file nor the search domains.
std::optional<in_addr> resolveEngineHostName(const char* name, const std::stop_token& stop);

// As resolveEngineHostName, asking `nameServers` in turn, each up to `attempts` times, waiting up to `attemptTimeout`
// for each answer.
std::optional<in_addr> resolveEngineHostNameWith(const char* name, const std::vector<sockaddr_in>& nameServers,
	std::chrono::milliseconds attemptTimeout, int attempts, const std::stop_token& stop);

} // namespace rts

#endif
