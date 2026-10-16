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
// FILE: EngineSockets.cpp
// GeneralsX @feature cemlyn007 16/10/2026 The engine's socket layer (see Common/EngineSockets.h)

#include "PreRTS.h"

#if RTS_ENGINE_CONTEXT

#include "Common/EngineSockets.h"

#include "Common/EmbeddingContract.h"

#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#include <arpa/inet.h>
#include <poll.h>
#include <resolv.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#pragma pop_macro("max")
#pragma pop_macro("min")

namespace rts
{

namespace
{

// The ledger ids of the sockets this engine has open, by descriptor. Destroyed after the engine's other per-engine
// statics, so that a socket one of their destructors closes is still found.
struct OpenEngineSockets
{
	std::mutex mutex;
	std::unordered_map<int, ResourceLedger::Id> ids;
};

OpenEngineSockets& openEngineSockets()
{
	RTS_ENGINE_STATIC_LAST(OpenEngineSockets, sockets);
	return sockets;
}

ResourceLedger& engineLedger()
{
	return *currentEmbedding("the socket layer's resource ledger").resourceLedger;
}

int recordSocket(ResourceLedger& ledger, int socket, const char* what)
{
	try
	{
		const ResourceLedger::Id id = ledger.record(std::string("socket ") + (what != nullptr ? what : ""),
			static_cast<std::uintptr_t>(socket), [](std::uintptr_t handle) { ::close(static_cast<int>(handle)); });
		OpenEngineSockets& sockets = openEngineSockets();
		const std::scoped_lock lock(sockets.mutex);
		sockets.ids[socket] = id;
	}
	catch (...)
	{
		::close(socket);
		throw;
	}
	return socket;
}

constexpr std::uint16_t dnsTypeA = 1;
constexpr std::uint16_t dnsClassInternet = 1;
constexpr std::size_t dnsHeaderSize = 12;
constexpr std::size_t dnsMaximumUdpMessage = 512;
constexpr auto stopPollInterval = std::chrono::milliseconds(50);

void appendUint16(std::vector<unsigned char>& message, std::uint16_t value)
{
	message.push_back(static_cast<unsigned char>(value >> 8));
	message.push_back(static_cast<unsigned char>(value & 0xff));
}

std::uint16_t readUint16(const unsigned char* bytes)
{
	return static_cast<std::uint16_t>((bytes[0] << 8) | bytes[1]);
}

// A recursive query for the A records of `name`, or none if `name` is not a valid host name.
std::optional<std::vector<unsigned char>> addressQuery(std::uint16_t id, const char* name)
{
	std::vector<unsigned char> query;
	appendUint16(query, id);
	appendUint16(query, 0x0100);
	appendUint16(query, 1);
	appendUint16(query, 0);
	appendUint16(query, 0);
	appendUint16(query, 0);

	const std::string text(name);
	if (text.empty() || text.size() > 253)
		return std::nullopt;
	std::size_t start = 0;
	while (start < text.size())
	{
		std::size_t end = text.find('.', start);
		if (end == std::string::npos)
			end = text.size();
		const std::size_t length = end - start;
		if (length == 0 || length > 63)
			return std::nullopt;
		query.push_back(static_cast<unsigned char>(length));
		query.insert(query.end(), text.begin() + static_cast<std::ptrdiff_t>(start),
			text.begin() + static_cast<std::ptrdiff_t>(end));
		start = end + 1;
	}
	query.push_back(0);
	appendUint16(query, dnsTypeA);
	appendUint16(query, dnsClassInternet);
	return query;
}

// The offset just past the (possibly compressed) name at `offset`, or none if it runs off the message.
std::optional<std::size_t> skipName(const unsigned char* message, std::size_t size, std::size_t offset)
{
	while (offset < size)
	{
		const unsigned char length = message[offset];
		if (length == 0)
			return offset + 1;
		if ((length & 0xc0) == 0xc0)
			return offset + 2 <= size ? std::optional<std::size_t>(offset + 2) : std::nullopt;
		if ((length & 0xc0) != 0)
			return std::nullopt;
		offset += 1 + length;
	}
	return std::nullopt;
}

enum class AnswerKind
{
	notOurs,
	failed,
	address,
};

// What `message` says about query `id`: not an answer to it, a failure (an error, or no A record), or an address.
AnswerKind readAnswer(const unsigned char* message, std::size_t size, std::uint16_t id, in_addr& address)
{
	if (size < dnsHeaderSize || readUint16(message) != id || (message[2] & 0x80) == 0)
		return AnswerKind::notOurs;
	if ((message[3] & 0x0f) != 0)
		return AnswerKind::failed;
	const std::uint16_t questions = readUint16(message + 4);
	const std::uint16_t answers = readUint16(message + 6);
	std::size_t offset = dnsHeaderSize;
	for (std::uint16_t i = 0; i < questions; ++i)
	{
		const std::optional<std::size_t> next = skipName(message, size, offset);
		if (!next || *next + 4 > size)
			return AnswerKind::failed;
		offset = *next + 4;
	}
	for (std::uint16_t i = 0; i < answers; ++i)
	{
		const std::optional<std::size_t> next = skipName(message, size, offset);
		if (!next || *next + 10 > size)
			return AnswerKind::failed;
		const std::uint16_t type = readUint16(message + *next);
		const std::uint16_t dataClass = readUint16(message + *next + 2);
		const std::uint16_t dataLength = readUint16(message + *next + 8);
		const std::size_t data = *next + 10;
		if (data + dataLength > size)
			return AnswerKind::failed;
		if (type == dnsTypeA && dataClass == dnsClassInternet && dataLength == 4)
		{
			std::memcpy(&address, message + data, 4);
			return AnswerKind::address;
		}
		offset = data + dataLength;
	}
	return AnswerKind::failed;
}

enum class AttemptResult
{
	answered,
	unanswered,
	stopped,
};

// Sends `query` to `server` over a socket of this layer and waits up to `timeout` for its answer, waking every
// stopPollInterval to check `stop`.
AttemptResult askNameServer(const sockaddr_in& server, const std::vector<unsigned char>& query, std::uint16_t id,
	std::chrono::milliseconds timeout, const std::stop_token& stop, std::optional<in_addr>& result)
{
	const int socket = openEngineSocket(AF_INET, SOCK_DGRAM, 0, "host name lookup");
	if (socket < 0)
		return AttemptResult::unanswered;

	AttemptResult outcome = AttemptResult::unanswered;
	if (::connect(socket, reinterpret_cast<const sockaddr*>(&server), sizeof(server)) == 0
		&& ::send(socket, query.data(), query.size(), 0) == static_cast<ssize_t>(query.size()))
	{
		const auto deadline = std::chrono::steady_clock::now() + timeout;
		while (true)
		{
			if (stop.stop_requested())
			{
				outcome = AttemptResult::stopped;
				break;
			}
			const auto now = std::chrono::steady_clock::now();
			if (now >= deadline)
				break;
			const auto wait = std::min(stopPollInterval,
				std::chrono::ceil<std::chrono::milliseconds>(deadline - now));
			pollfd readable{socket, POLLIN, 0};
			const int ready = ::poll(&readable, 1, static_cast<int>(wait.count()));
			if (ready < 0 && errno != EINTR)
				break;
			if (ready <= 0)
				continue;
			unsigned char answer[dnsMaximumUdpMessage];
			const ssize_t received = ::recv(socket, answer, sizeof(answer), 0);
			if (received < 0)
				break;
			in_addr address{};
			const AnswerKind kind = readAnswer(answer, static_cast<std::size_t>(received), id, address);
			if (kind == AnswerKind::notOurs)
				continue;
			if (kind == AnswerKind::address)
				result = address;
			outcome = AttemptResult::answered;
			break;
		}
	}
	closeEngineSocket(socket);
	return outcome;
}

std::uint16_t queryId()
{
	const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
	return static_cast<std::uint16_t>((ticks >> 16) ^ ticks);
}

} // namespace

int openEngineSocket(int domain, int type, int protocol, const char* what)
{
	ResourceLedger& ledger = engineLedger();
	const int socket = ::socket(domain, type, protocol);
	if (socket < 0)
		return -1;
	return recordSocket(ledger, socket, what);
}

int acceptEngineSocket(int listener, sockaddr* address, socklen_t* addressLength, const char* what)
{
	ResourceLedger& ledger = engineLedger();
	const int socket = ::accept(listener, address, addressLength);
	if (socket < 0)
		return -1;
	return recordSocket(ledger, socket, what);
}

int closeEngineSocket(int socket)
{
	ResourceLedger& ledger = engineLedger();
	OpenEngineSockets& sockets = openEngineSockets();
	ResourceLedger::Id id = 0;
	{
		const std::scoped_lock lock(sockets.mutex);
		const auto found = sockets.ids.find(socket);
		if (found == sockets.ids.end())
		{
			errno = EBADF;
			return -1;
		}
		id = found->second;
		sockets.ids.erase(found);
	}
	ledger.forget(id);
	return ::close(socket);
}

std::optional<in_addr> resolveEngineHostName(const char* name, const std::stop_token& stop)
{
	struct __res_state resolver;
	std::memset(&resolver, 0, sizeof(resolver));
	if (res_ninit(&resolver) != 0)
		return std::nullopt;
	std::vector<sockaddr_in> nameServers;
	for (int i = 0; i < resolver.nscount && i < MAXNS; ++i)
	{
		if (resolver.nsaddr_list[i].sin_family == AF_INET)
			nameServers.push_back(resolver.nsaddr_list[i]);
	}
	const auto attemptTimeout = std::chrono::seconds(std::max(1, resolver.retrans));
	const int attempts = std::max(1, resolver.retry);
	res_nclose(&resolver);
	return resolveEngineHostNameWith(name, nameServers, attemptTimeout, attempts, stop);
}

std::optional<in_addr> resolveEngineHostNameWith(const char* name, const std::vector<sockaddr_in>& nameServers,
	std::chrono::milliseconds attemptTimeout, int attempts, const std::stop_token& stop)
{
	if (name == nullptr)
		return std::nullopt;
	in_addr numeric{};
	if (::inet_pton(AF_INET, name, &numeric) == 1)
		return numeric;

	const std::uint16_t id = queryId();
	const std::optional<std::vector<unsigned char>> query = addressQuery(id, name);
	if (!query)
		return std::nullopt;

	for (int attempt = 0; attempt < attempts; ++attempt)
	{
		for (const sockaddr_in& server : nameServers)
		{
			std::optional<in_addr> result;
			const AttemptResult outcome = askNameServer(server, *query, id, attemptTimeout, stop, result);
			if (outcome == AttemptResult::stopped)
				return std::nullopt;
			if (outcome == AttemptResult::answered)
				return result;
		}
	}
	return std::nullopt;
}

} // namespace rts

#endif
