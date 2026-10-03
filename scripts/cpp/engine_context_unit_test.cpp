// GeneralsX @feature cemlyn007 03/10/2026 Unit test of rts::PerEngineStatic's get() and get(initialize) (PLAN-023)
//
// Built and run by engine_context_unit_test.py against Common/EngineContext.cpp alone, with a stub PreRTS.h
// whose DEBUG_ASSERTCRASH is assert(), under AddressSanitizer so that a leaked slot object fails the run.

#include "Common/EngineContext.h"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <vector>

static int theFailures = 0;

#define CHECK(condition) \
	do \
	{ \
		if (!(condition)) \
		{ \
			std::printf("FAILED %s:%d: %s\n", __FILE__, __LINE__, #condition); \
			++theFailures; \
		} \
	} while (false)

namespace
{
// Counts the objects alive, so a slot object that is dropped without being destroyed shows up.
int theLiveCounters = 0;
std::vector<int> theDestroyed;

struct Counter
{
	int value = 0;
	Counter() { ++theLiveCounters; }
	~Counter()
	{
		--theLiveCounters;
		theDestroyed.push_back(value);
	}
	Counter(const Counter&) = delete;
	Counter& operator=(const Counter&) = delete;
};

rts::PerEngineStatic<Counter> theCounter;
rts::PerEngineStatic<Counter> theOther;
rts::PerEngineStatic<Counter> theThird;
rts::PerEngineStatic<Counter> theThrowing;
int theConstructorRuns = 0;
rts::PerEngineStatic<Counter> theWithConstructorInitializer([](Counter& counter) {
	++theConstructorRuns;
	counter.value = 40 + theConstructorRuns;
});
rts::PerEngineStatic<Counter> theThrowingConstructorInitializer([](Counter&) { throw std::runtime_error("init"); });
}

static void testGetInitializeRunsOncePerContext()
{
	rts::EngineContext first;
	rts::EngineContext second;
	int runs = 0;
	auto initialize = [&](Counter& counter) {
		++runs;
		counter.value = 10 + runs;
	};

	{
		rts::Scope scope(&first);
		CHECK(theCounter.get(initialize).value == 11);
		CHECK(theCounter.get(initialize).value == 11);
		CHECK(&theCounter.get() == &theCounter.get(initialize));
		CHECK(runs == 1);
	}
	{
		rts::Scope scope(&second);
		CHECK(theCounter.get(initialize).value == 12);
		CHECK(theCounter.get(initialize).value == 12);
		CHECK(runs == 2);
	}
	{
		rts::Scope scope(&first);
		CHECK(theCounter.get().value == 11);
	}
	CHECK(theLiveCounters == 2);
}

static void testGetInitializeCleansUpWhenItThrows()
{
	rts::EngineContext context;
	rts::Scope scope(&context);
	const int before = theLiveCounters;

	bool threw = false;
	try
	{
		theThrowing.get([](Counter&) { throw std::runtime_error("initialiser"); });
	}
	catch (const std::runtime_error&)
	{
		threw = true;
	}
	CHECK(threw);
	CHECK(theLiveCounters == before);
	CHECK(!context.hasSlotObjects());

	// The slot is still unset, so the next call initialises it.
	CHECK(theThrowing.get([](Counter& counter) { counter.value = 7; }).value == 7);
	CHECK(context.hasSlotObjects());
}

static void testConstructorInitializerRunsOncePerContext()
{
	rts::EngineContext first;
	rts::EngineContext second;
	{
		rts::Scope scope(&first);
		CHECK(theWithConstructorInitializer.get().value == 41);
		CHECK(theWithConstructorInitializer.get().value == 41);
		CHECK(theConstructorRuns == 1);
	}
	{
		rts::Scope scope(&second);
		CHECK(theWithConstructorInitializer.get().value == 42);
		CHECK(theConstructorRuns == 2);
	}
}

static void testConstructorInitializerCleansUpWhenItThrows()
{
	rts::EngineContext context;
	rts::Scope scope(&context);
	const int before = theLiveCounters;

	bool threw = false;
	try
	{
		theThrowingConstructorInitializer.get();
	}
	catch (const std::runtime_error&)
	{
		threw = true;
	}
	CHECK(threw);
	CHECK(theLiveCounters == before);
	CHECK(!context.hasSlotObjects());
}

static void testSlotsAreDestroyedNewestFirstWithTheContext()
{
	const int before = theLiveCounters;
	theDestroyed.clear();
	{
		rts::EngineContext context;
		rts::Scope scope(&context);
		// theCounter, theOther and theThird have ascending slot indices. Creating theOther, theThird, then
		// theCounter matches neither ascending nor descending index order, so only newest-first passes.
		theOther.get().value = 1;
		theThird.get().value = 2;
		theCounter.get().value = 3;
		CHECK(theLiveCounters == before + 3);
	}
	CHECK(theLiveCounters == before);
	CHECK(theDestroyed.size() == 3);
	if (theDestroyed.size() == 3)
		CHECK(theDestroyed[0] == 3 && theDestroyed[1] == 2 && theDestroyed[2] == 1);
}

int main()
{
	testGetInitializeRunsOncePerContext();
	testGetInitializeCleansUpWhenItThrows();
	testConstructorInitializerRunsOncePerContext();
	testConstructorInitializerCleansUpWhenItThrows();
	testSlotsAreDestroyedNewestFirstWithTheContext();
	if (theFailures == 0)
		std::printf("engine_context_unit_test: ok\n");
	return theFailures == 0 ? 0 : 1;
}
