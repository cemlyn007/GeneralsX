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
// FILE: EngineContext.cpp
// GeneralsX @feature cemlyn007 28/09/2026 The engine context (PLAN-023 Phase 1; see Common/EngineContext.h)

#include "PreRTS.h"

#if RTS_ENGINE_CONTEXT

#include "Common/EngineContext.h"

#include "Common/EmbeddingContract.h"

#include <algorithm>
#include <atomic>
#include <cfenv>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifndef _WIN32
#include <dlfcn.h>
#include <locale.h>
#include <pthread.h>
#if defined(__APPLE__)
#include <xlocale.h>
#endif
#endif

// GameLogic.cpp (GameLogic/FPUControl.h): the engine's floating-point mode.
void setFPMode();

namespace rts
{

EngineContext g_noEngine;

constinit thread_local EngineContext* t_engine RTS_TLS_IE = &g_noEngine;

// GeneralsX @feature cemlyn007 14/10/2026 One context's per-engine statics (Common/EngineStatic.h): an open-addressed
// table from each static's key to its storage, which threads of the engine read without a lock, and the order the
// statics were made in, which they are destroyed in reverse.
struct EngineStatics
{
	struct Entry
	{
		std::atomic<const EngineStaticKey*> key{nullptr};
		std::atomic<void*> storage{nullptr};
	};

	struct Table
	{
		explicit Table(unsigned int bits) : shift(64 - bits), mask((std::size_t(1) << bits) - 1), entries(new Entry[mask + 1]) {}

		std::size_t first(const EngineStaticKey& key) const noexcept
		{
			const std::uint64_t address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(&key));
			return static_cast<std::size_t>((address * 0x9E3779B97F4A7C15ull) >> shift);
		}

		Entry* find(const EngineStaticKey& key) const noexcept
		{
			for (std::size_t index = first(key);; index = (index + 1) & mask)
			{
				const EngineStaticKey* const found = entries[index].key.load(std::memory_order_acquire);
				if (found == &key)
					return &entries[index];
				if (found == nullptr)
					return nullptr;
			}
		}

		unsigned int shift;
		std::size_t mask;
		std::unique_ptr<Entry[]> entries;
		std::size_t used = 0;
	};

	struct Made
	{
		const EngineStaticKey* key;
		void* storage;
		const EngineStaticType* type;
	};

	static constexpr unsigned int initialBits = 10;

	void* findLocked(const EngineStaticKey& key) const noexcept
	{
		const Table* const current = table.load(std::memory_order_relaxed);
		if (current == nullptr)
			return nullptr;
		const Entry* const entry = current->find(key);
		return entry != nullptr ? entry->storage.load(std::memory_order_relaxed) : nullptr;
	}

	void insertLocked(const EngineStaticKey& key, void* storage)
	{
		Table* current = table.load(std::memory_order_relaxed);
		if (current != nullptr)
		{
			Entry* const existing = current->find(key);
			if (existing != nullptr)
			{
				existing->storage.store(storage, std::memory_order_release);
				return;
			}
		}
		if (current == nullptr || (current->used + 1) * 2 > current->mask + 1)
			current = grownLocked(current);
		std::size_t index = current->first(key);
		while (current->entries[index].key.load(std::memory_order_relaxed) != nullptr)
			index = (index + 1) & current->mask;
		current->entries[index].storage.store(storage, std::memory_order_relaxed);
		current->entries[index].key.store(&key, std::memory_order_release);
		++current->used;
	}

	// A table twice the size of `current` (or the first table), holding its entries, published for readers. The old
	// table is kept, since a reader may still be probing it; one that misses a static there finds it under the lock.
	Table* grownLocked(const Table* current)
	{
		const unsigned int bits = current == nullptr ? initialBits : 64 - current->shift + 1;
		std::unique_ptr<Table> grown = std::make_unique<Table>(bits);
		if (current != nullptr)
		{
			for (std::size_t index = 0; index <= current->mask; ++index)
			{
				const EngineStaticKey* const key = current->entries[index].key.load(std::memory_order_relaxed);
				if (key == nullptr)
					continue;
				std::size_t target = grown->first(*key);
				while (grown->entries[target].key.load(std::memory_order_relaxed) != nullptr)
					target = (target + 1) & grown->mask;
				grown->entries[target].storage.store(current->entries[index].storage.load(std::memory_order_relaxed), std::memory_order_relaxed);
				grown->entries[target].key.store(key, std::memory_order_relaxed);
				++grown->used;
			}
		}
		Table* const published = grown.get();
		tables.push_back(std::move(grown));
		table.store(published, std::memory_order_release);
		return published;
	}

	bool inConstructionLocked(const EngineStaticKey& key, std::thread::id* builder) const
	{
		for (const auto& building : inConstruction)
		{
			if (building.first == &key)
			{
				*builder = building.second;
				return true;
			}
		}
		return false;
	}

	void endConstructionLocked(const EngineStaticKey& key)
	{
		inConstruction.erase(std::find_if(inConstruction.begin(), inConstruction.end(),
			[&key](const auto& building) { return building.first == &key; }));
		madeOrFailed.notify_all();
	}

	mutable std::mutex mutex;
	std::condition_variable madeOrFailed;
	std::atomic<Table*> table{nullptr};
	std::vector<std::unique_ptr<Table>> tables;
	std::vector<Made> inCreationOrder;
	std::vector<std::pair<const EngineStaticKey*, std::thread::id>> inConstruction;
};

#ifdef DEBUG_CRASHING
namespace
{
// Appends each live field's name to a bounded, comma-separated list (for the ~EngineContext assert).
struct LiveFieldNames
{
	char text[512] = "";
	std::size_t length = 0;
};

void appendLiveFieldName(const char* name, void* user)
{
	LiveFieldNames& names = *static_cast<LiveFieldNames*>(user);
	const std::size_t room = sizeof(names.text) - names.length;
	if (room <= 1)
		return;
	const int written = std::snprintf(names.text + names.length, room, "%s%s", names.length == 0 ? "" : ", ", name);
	if (written > 0)
		names.length += std::min<std::size_t>((std::size_t)written, room - 1);
}
}
#endif

EngineContext::~EngineContext()
{
#ifdef DEBUG_CRASHING
	if (this != &g_noEngine && (countLiveSingletons() != 0 || originalGlobalData != nullptr || wwMathInitialized
			|| drawableModelLockCount != 0))
	{
		LiveFieldNames names;
		const std::size_t live = forEachLiveSingleton(&appendLiveFieldName, &names);
		DEBUG_CRASH(("EngineContext destroyed before its engine was shut down, or its shutdown left state: "
			"%u live singleton/pointer fields (%s), originalGlobalData %s, wwMathInitialized %s, "
			"drawableModelLockCount %d",
			(unsigned)live, live != 0 ? names.text : "none", originalGlobalData != nullptr ? "set" : "null",
			wwMathInitialized ? "true" : "false", drawableModelLockCount));
	}
#endif
	DEBUG_ASSERTCRASH(this == &g_noEngine || noEngineIsPristine(), ("g_noEngine was written: engine state leaked outside every Scope"));

	destroyStatics();
	delete m_statics.load(std::memory_order_acquire);
	delete m_embedding;
}

EngineContext::EngineContext(EmbeddingConfig embedding) : m_embedding(new EmbeddingConfig(std::move(embedding)))
{
}

EngineStatics& EngineContext::statics()
{
	EngineStatics* existing = m_statics.load(std::memory_order_acquire);
	if (existing != nullptr)
		return *existing;
	EngineStatics* const made = new EngineStatics;
	if (m_statics.compare_exchange_strong(existing, made, std::memory_order_acq_rel, std::memory_order_acquire))
		return *made;
	delete made;
	return *existing;
}

void* EngineContext::findStatic(const EngineStaticKey& key) const noexcept
{
	const EngineStatics* const statics = m_statics.load(std::memory_order_acquire);
	if (statics == nullptr)
		return nullptr;
	const EngineStatics::Table* const table = statics->table.load(std::memory_order_acquire);
	if (table == nullptr)
		return nullptr;
	const EngineStatics::Entry* const entry = table->find(key);
	return entry != nullptr ? entry->storage.load(std::memory_order_acquire) : nullptr;
}

void* EngineContext::makeStatic(const EngineStaticKey& key, const EngineStaticType& type, void* closure)
{
	if (this == &g_noEngine)
	{
		DEBUG_CRASH(("the per-engine static %s is used with no engine current", key.name));
		throw std::logic_error(std::string("the per-engine static ") + key.name + " is used with no engine current");
	}
	EngineStatics& statics = this->statics();
	std::unique_lock<std::mutex> lock(statics.mutex);
	for (;;)
	{
		if (void* const made = statics.findLocked(key))
			return made;
		std::thread::id builder;
		if (!statics.inConstructionLocked(key, &builder))
			break;
		if (builder == std::this_thread::get_id())
		{
			DEBUG_CRASH(("the per-engine static %s is used by its own initialiser", key.name));
			throw std::logic_error(std::string("the per-engine static ") + key.name + " is used by its own initialiser");
		}
		statics.madeOrFailed.wait(lock);
	}
	statics.inConstruction.emplace_back(&key, std::this_thread::get_id());
	lock.unlock();

	void* storage = nullptr;
	try
	{
		storage = ::operator new(type.size, std::align_val_t(type.alignment));
		std::memset(storage, 0, type.size);
		type.construct(storage, closure);
	}
	catch (...)
	{
		if (storage != nullptr)
			::operator delete(storage, std::align_val_t(type.alignment));
		lock.lock();
		statics.endConstructionLocked(key);
		throw;
	}

	lock.lock();
	statics.insertLocked(key, storage);
	statics.inCreationOrder.push_back(EngineStatics::Made{&key, storage, &type});
	statics.endConstructionLocked(key);
	return storage;
}

void EngineContext::destroyStatics()
{
	EngineStatics* const statics = m_statics.load(std::memory_order_acquire);
	if (statics == nullptr)
		return;

	// A static's destructor may read engine state, which must be this engine's.
	Scope scope(this);

	// A static's destructor may use another static (or make one), so they are destroyed newest first, and those
	// destroyed last (allocators whose objects other statics hold) after every other. A static stays set while its own
	// destructor runs, so a destructor that reaches its own static finds the object being destroyed, not a fresh one;
	// it is cleared once the destructor returns.
	for (;;)
	{
		EngineStatics::Made made;
		{
			std::lock_guard<std::mutex> lock(statics->mutex);
			if (statics->inCreationOrder.empty())
				break;
			auto next = std::find_if(statics->inCreationOrder.rbegin(), statics->inCreationOrder.rend(),
				[](const EngineStatics::Made& candidate) { return !candidate.type->destroyLast; });
			if (next == statics->inCreationOrder.rend())
				next = statics->inCreationOrder.rbegin();
			made = *next;
			statics->inCreationOrder.erase(std::next(next).base());
		}
		made.type->destroy(made.storage);
		{
			std::lock_guard<std::mutex> lock(statics->mutex);
			statics->insertLocked(*made.key, nullptr);
		}
		::operator delete(made.storage, std::align_val_t(made.type->alignment));
	}
}

std::size_t EngineContext::countStatics() const
{
	const EngineStatics* const statics = m_statics.load(std::memory_order_acquire);
	if (statics == nullptr)
		return 0;
	std::lock_guard<std::mutex> lock(statics->mutex);
	return statics->inCreationOrder.size();
}

void EngineContext::forEachStatic(void (*visit)(const char* name, void* user), void* user) const
{
	const EngineStatics* const statics = m_statics.load(std::memory_order_acquire);
	if (statics == nullptr)
		return;
	std::vector<const char*> names;
	{
		std::lock_guard<std::mutex> lock(statics->mutex);
		names.reserve(statics->inCreationOrder.size());
		for (const EngineStatics::Made& made : statics->inCreationOrder)
			names.push_back(made.key->name);
	}
	for (const char* name : names)
		visit(name, user);
}

std::size_t EngineContext::countLiveSingletons() const
{
	return forEachLiveSingleton([](const char*, void*) {}, nullptr);
}

std::size_t EngineContext::forEachLiveSingleton(void (*visit)(const char* name, void* user), void* user) const
{
	std::size_t live = 0;
	// Three separate definitions: forwarding one macro to another would macro-expand the name.
#define RTS_ENGINE_SINGLETON(T, n) if (n##_ != nullptr) { ++live; visit(#n, user); }
#define RTS_ENGINE_SINGLETON_STRUCT(T, n) if (n##_ != nullptr) { ++live; visit(#n, user); }
#define RTS_ENGINE_SINGLETON_ZH(T, n) if (n##_ != nullptr) { ++live; visit(#n, user); }
#include "Common/EngineSingletons.inl"
#undef RTS_ENGINE_SINGLETON
#undef RTS_ENGINE_SINGLETON_STRUCT
#undef RTS_ENGINE_SINGLETON_ZH
	// The direct pointer fields, which the engine's teardown frees and nulls just as it does the singletons.
#define RTS_ENGINE_CONTEXT_POINTER(T, n) if (n != nullptr) { ++live; visit(#n, user); }
#define RTS_ENGINE_CONTEXT_VALUE(T, n, init)
#include "Common/EngineContextFields.inl"
#undef RTS_ENGINE_CONTEXT_POINTER
#undef RTS_ENGINE_CONTEXT_VALUE
	return live;
}

#if RTS_ENGINE_SCOPE_ENVIRONMENT
// GeneralsX @feature cemlyn007 28/09/2026 The per-thread invariants a Scope sets when it enters an engine (PLAN-023
// Phase 5b; see Scope).
static_assert(sizeof(std::fenv_t) <= sizeof(ThreadInvariants::floatingPointEnvironment), "fenv_t does not fit ThreadInvariants");
static_assert(alignof(std::fenv_t) <= 8, "fenv_t is over-aligned for ThreadInvariants");
#ifndef _WIN32
static_assert(sizeof(locale_t) <= sizeof(void*), "locale_t does not fit ThreadInvariants");
#endif

namespace
{
#if defined(__x86_64__) && !defined(_WIN32)
#define RTS_SCOPE_CONTROL_WORDS 1
// GeneralsX @bugfix cemlyn007 28/09/2026 On x86-64 the engine's floating-point mode is two control words, the x87 one
// and MXCSR, as setFPMode() leaves them. The same for every thread, so the first Scope learns them (after its
// setFPMode()) and later ones load them directly: fnstcw/stmxcsr to save, fldcw/ldmxcsr to set and restore, in
// place of fegetenv, setFPMode()'s fesetenv(FE_DFL_ENV) and friends, and fesetenv. A thread already in the mode
// loads nothing at entry (its exit reloads only if the Scope changed it). Neither exception flags (masked, so they change no result) nor the empty x87 stack (empty at
// every call) are part of the mode.
constexpr unsigned int kMxcsrExceptionFlags = 0x3F;
// (x87 control word << 32) | MXCSR less its exception flags, with bit 63 set once learnt.
constexpr unsigned long long kFloatingPointModeKnown = 1ull << 63;
std::atomic<unsigned long long> theEngineFloatingPointMode(0);

unsigned long long packFloatingPointMode(unsigned short controlWord, unsigned int mxcsr) noexcept
{
	return kFloatingPointModeKnown | (static_cast<unsigned long long>(controlWord) << 32) | (mxcsr & ~kMxcsrExceptionFlags);
}
#else
#define RTS_SCOPE_CONTROL_WORDS 0
#endif

#ifndef _WIN32
// GeneralsX @bugfix cemlyn007 29/09/2026 The engine's locale for this thread: the thread's own locale with LC_NUMERIC
// "C", made from the locale the thread had when it first entered an engine (`source`, not owned) and kept, so
// that every Scope switches to it with one uselocale, whatever the thread's LC_NUMERIC. Always a locale of the
// thread's own, never the process's global one: with the GIL released, another thread's setlocale() cannot change
// the engine's number parsing in the middle of a step. A thread on the global locale (LC_GLOBAL_LOCALE, as
// Python's are) keeps the snapshot of it taken at its first entry; one that uselocale()s another locale of its
// own gets a new engine locale made from that one at each outermost entry (`source` is only trusted while it is
// LC_GLOBAL_LOCALE: the host may free its own locale and a new one may take its address). `users` counts the Scopes on this
// thread that switched to it and have not yet left. Freed at thread exit (a pthread key's destructor; the main
// thread's goes with the process).
struct ThreadEngineLocale
{
	locale_t locale;
	locale_t source;
	unsigned int users;
};
constinit thread_local ThreadEngineLocale t_engineLocale = {(locale_t)0, (locale_t)0, 0};

void freeThreadEngineLocale(void* locale) noexcept
{
	freelocale(static_cast<locale_t>(locale));
	if (t_engineLocale.locale == static_cast<locale_t>(locale))
		t_engineLocale = {(locale_t)0, (locale_t)0, 0};
}

struct ThreadEngineLocaleKey
{
	pthread_key_t key;
	bool made;
};

const ThreadEngineLocaleKey& threadEngineLocaleKey() noexcept
{
	static const ThreadEngineLocaleKey key = []() noexcept {
		ThreadEngineLocaleKey k{};
		// The destructor is code in this library, so the library is pinned (never unloaded) before the key exists:
		// a thread that exits after a dlclose would otherwise call unmapped code.
		Dl_info info{};
		if (dladdr(reinterpret_cast<const void*>(&freeThreadEngineLocale), &info) != 0 && info.dli_fname != nullptr
			&& dlopen(info.dli_fname, RTLD_LAZY | RTLD_NOLOAD | RTLD_NODELETE) != nullptr)
			k.made = pthread_key_create(&k.key, freeThreadEngineLocale) == 0;
		return k;
	}();
	return key;
}

// `from` with LC_NUMERIC "C" (a copy; `from` is left alone), or null if it cannot be made.
locale_t makeEngineLocale(locale_t from) noexcept
{
	const locale_t base = duplocale(from);
	if (base == (locale_t)0)
		return (locale_t)0;
	const locale_t engine = newlocale(LC_NUMERIC_MASK, "C", base);
	if (engine == (locale_t)0)
		freelocale(base);
	return engine;
}
#endif
}

void enterEngineThreadInvariants(ThreadInvariants& saved) noexcept
{
	saved.floatingPointSaved = true;
#if RTS_SCOPE_CONTROL_WORDS
	__asm__ __volatile__("fnstcw %0" : "=m" (saved.x87ControlWord));
	__asm__ __volatile__("stmxcsr %0" : "=m" (saved.mxcsr));
	const unsigned long long mode = theEngineFloatingPointMode.load(std::memory_order_relaxed);
	if (mode == packFloatingPointMode(saved.x87ControlWord, saved.mxcsr))
	{
		saved.floatingPointSaved = false;
	}
	else if (mode != 0)
	{
		unsigned short controlWord = static_cast<unsigned short>(mode >> 32);
		unsigned int mxcsr = static_cast<unsigned int>(mode);
		__asm__ __volatile__("fldcw %0" : : "m" (controlWord));
		__asm__ __volatile__("ldmxcsr %0" : : "m" (mxcsr));
	}
	else
	{
		setFPMode();
		unsigned short controlWord = 0;
		unsigned int mxcsr = 0;
		__asm__ __volatile__("fnstcw %0" : "=m" (controlWord));
		__asm__ __volatile__("stmxcsr %0" : "=m" (mxcsr));
		theEngineFloatingPointMode.store(packFloatingPointMode(controlWord, mxcsr), std::memory_order_relaxed);
	}
#else
	std::fegetenv(reinterpret_cast<std::fenv_t*>(saved.floatingPointEnvironment));
#if defined(__i386__) && !defined(_WIN32)
	// GeneralsX @bugfix cemlyn007 29/09/2026 glibc's i386 fesetenv does not restore the x87 precision bits that
	// setFPMode() clears, so the control word is saved (and restored with fldcw) too.
	__asm__ __volatile__("fnstcw %0" : "=m" (saved.x87ControlWord));
#else
	saved.x87ControlWord = 0;
#endif
	saved.mxcsr = 0;
	setFPMode();
#endif
	saved.locale = nullptr;
	saved.engineLocale = nullptr;
#ifndef _WIN32
	ThreadEngineLocale& cache = t_engineLocale;
	const locale_t current = uselocale((locale_t)0);
	// Already the engine's (a Scope for another engine inside one): nothing to switch.
	if (cache.locale != (locale_t)0 && current == cache.locale)
		return;
	if (cache.locale == (locale_t)0 || current != cache.source || current != LC_GLOBAL_LOCALE)
	{
		const locale_t engine = makeEngineLocale(current);
		if (engine == (locale_t)0)
			return;
		const ThreadEngineLocaleKey& key = threadEngineLocaleKey();
		if (cache.users != 0 || !key.made)
		{
			// The cached one is still in use further out on this thread (which has since switched to a locale of
			// its own), or cannot be freed at thread exit: one for this Scope only, freed when it ends.
			saved.engineLocale = static_cast<void*>(engine);
			saved.locale = static_cast<void*>(uselocale(engine));
			return;
		}
		if (cache.locale != (locale_t)0)
			freelocale(cache.locale);
		cache.locale = engine;
		cache.source = current;
		pthread_setspecific(key.key, static_cast<void*>(engine));
	}
	++cache.users;
	saved.locale = static_cast<void*>(uselocale(cache.locale));
#else
	// Windows has no per-thread uselocale; the engine keeps the thread's locale there.
#endif
}

void leaveEngineThreadInvariants(const ThreadInvariants& saved) noexcept
{
#ifndef _WIN32
	if (saved.locale != nullptr)
	{
		uselocale(static_cast<locale_t>(saved.locale));
		if (saved.engineLocale != nullptr)
			freelocale(static_cast<locale_t>(saved.engineLocale));
		else
			--t_engineLocale.users;
	}
#endif
#if RTS_SCOPE_CONTROL_WORDS
	if (!saved.floatingPointSaved)
	{
		// The thread entered in the engine's mode: reload it only if something inside the Scope changed it.
		unsigned short controlWord = 0;
		unsigned int mxcsr = 0;
		__asm__ __volatile__("fnstcw %0" : "=m" (controlWord));
		__asm__ __volatile__("stmxcsr %0" : "=m" (mxcsr));
		if (controlWord == saved.x87ControlWord && mxcsr == saved.mxcsr)
			return;
	}
	__asm__ __volatile__("fldcw %0" : : "m" (saved.x87ControlWord));
	__asm__ __volatile__("ldmxcsr %0" : : "m" (saved.mxcsr));
#else
	std::fesetenv(reinterpret_cast<const std::fenv_t*>(saved.floatingPointEnvironment));
#if defined(__i386__) && !defined(_WIN32)
	__asm__ __volatile__("fldcw %0" : : "m" (saved.x87ControlWord));
#endif
#endif
}
#endif

static_assert(isStandInGuarded<ContextField<::W3DRenderState*, &EngineContext::w3dRender>>,
	"a ContextField must not be copyable or have unary operator& (ContextFieldOps)");

bool noEngineIsPristine()
{
	if (g_noEngine.countLiveSingletons() != 0 || g_noEngine.originalGlobalData != nullptr || g_noEngine.countStatics() != 0)
		return false;
	// Every value field still has its initial value, the seeds included: a write outside every Scope (a
	// host callback, a parked engine's leftover call) is engine state leaking into the no-engine context.
#define RTS_ENGINE_CONTEXT_POINTER(T, n)
#define RTS_ENGINE_CONTEXT_VALUE(T, n, init) if (!(g_noEngine.n == (init))) return false;
#include "Common/EngineContextFields.inl"
#undef RTS_ENGINE_CONTEXT_POINTER
#undef RTS_ENGINE_CONTEXT_VALUE
	static const std::uint32_t initialSeed[6] = RTS_RANDOM_SEED_INITIAL_VALUES;
	return std::memcmp(g_noEngine.gameAudioSeed, initialSeed, sizeof(initialSeed)) == 0
		&& std::memcmp(g_noEngine.gameClientSeed, initialSeed, sizeof(initialSeed)) == 0
		&& std::memcmp(g_noEngine.gameLogicSeed, initialSeed, sizeof(initialSeed)) == 0
		&& g_noEngine.gameLogicBaseSeed == 0;
}

} // namespace rts

#endif // RTS_ENGINE_CONTEXT
