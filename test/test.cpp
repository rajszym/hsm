#include <hsm.hpp>
#include <cstdio>
#include <string>

static int failures = 0;

#define CHECK( cond ) do { if (!(cond)) { failures++; printf("FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

enum Event : unsigned
{
	Go = hsm::Event::User,
	Back,
	Other,
};

static std::string trace;

static hsm::Handler put( const char *text_ )
{
	return [text_]( const hsm::Message& ){ trace += text_; };
}

/* -------------------------------------------------------------------------- */

// restart after stop must not create a cycle in the state action queue
static void testRestart()
{
	hsm::State A, B;
	hsm::StateMachine sm{{
		{ A, hsm::Event::Entry, put("a") },
		{ A, Go,                B        },
		{ B, hsm::Event::Entry, put("b") },
		{ B, Back,              A        },
	}};

	for (int i = 0; i < 3; i++)
	{
		trace.clear();
		sm.start(A);
		sm.message({Other}); // unhandled event: must return
		sm.message({Go});
		sm.message({Other});
		sm.message({Back});
		sm.message({hsm::Event::Stop});
		CHECK(trace == "aba");
	}
}

/* -------------------------------------------------------------------------- */

// handlers called from start() and on Stop must receive the pointer to the hsm
static void testMessageHsm()
{
	hsm::StateMachine *seen[4]{};
	hsm::State P, C1(P), C2(P);
	hsm::StateMachine sm{{
		{ P,  hsm::Event::Entry, [&]( const hsm::Message& m ){ seen[0] = m.hsm; } },
		{ P,  hsm::Event::Init,  [&]( const hsm::Message& m ){ seen[1] = m.hsm; if (m.hsm) m.hsm->transition(C2); } },
		{ C2, hsm::Event::Entry, [&]( const hsm::Message& m ){ seen[2] = m.hsm; } },
		{ C2, hsm::Event::Exit,  [&]( const hsm::Message& m ){ seen[3] = m.hsm; } },
	}};

	sm.start(P);
	sm.message({hsm::Event::Stop});

	for (auto hsm_: seen)
		CHECK(hsm_ == &sm);
}

/* -------------------------------------------------------------------------- */

// actions can be added only when the hsm is not running
static void testAdd()
{
	hsm::State A, B;
	hsm::StateMachine sm;

	CHECK(sm.add(A, Go, B));
	sm.start(A);
#ifdef NDEBUG // with assertions enabled adding to the running hsm aborts
	CHECK(!sm.add(B, Back, A));
	CHECK(!sm.add({{ B, Back, A }}));
#endif
	sm.message({Go});
	sm.message({Back}); // B has no actions: ignored
	sm.message({hsm::Event::Stop});

	// adding many actions while stopped reallocates the table; start relinks it
	trace.clear();
	for (int i = 0; i < 100; i++)
		CHECK(sm.add(B, Other, put("")));
	CHECK(sm.add(B, Back, A));
	CHECK(sm.add({{ A, hsm::Event::Entry, put("a") }, { B, hsm::Event::Entry, put("b") }}));
	sm.start(A);
	sm.message({Go});
	sm.message({Back});
	sm.message({hsm::Event::Stop});
	CHECK(trace == "aba");
}

/* -------------------------------------------------------------------------- */

// exact match takes precedence over Event::ALL regardless of the table order;
// Event::ALL does not apply to system events
static void testAll()
{
	hsm::State A, B;
	hsm::StateMachine sm{{
		{ A, hsm::Event::ALL, put("*") },
		{ A, Go,              put("g") },
		{ B, Go,              put("g") },
		{ B, hsm::Event::ALL, put("*") },
	}};

	for (auto state_: { &A, &B })
	{
		trace.clear();
		sm.start(*state_);
		sm.message({Go});
		sm.message({Other});
		sm.message({hsm::Event::Stop});
		CHECK(trace == "g*");
	}
}

/* -------------------------------------------------------------------------- */

// the first action in the table wins for duplicated events
static void testOrder()
{
	hsm::State A;
	hsm::StateMachine sm{{
		{ A, Go, put("1") },
		{ A, Go, put("2") },
	}};

	trace.clear();
	sm.start(A);
	sm.message({Go});
	sm.message({hsm::Event::Stop});
	CHECK(trace == "1");
}

/* -------------------------------------------------------------------------- */

// events are dispatched to parent states; entry and exit order across the hierarchy
static void testHierarchy()
{
	hsm::State P, C1(P), C2(P), Q;
	hsm::StateMachine sm{{
		{ P,  hsm::Event::Entry, put("+P") },
		{ P,  hsm::Event::Exit,  put("-P") },
		{ P,  hsm::Event::Init,  C1        },
		{ P,  Back,              Q         },
		{ C1, hsm::Event::Entry, put("+1") },
		{ C1, hsm::Event::Exit,  put("-1") },
		{ C1, Go,                C2        },
		{ C2, hsm::Event::Entry, put("+2") },
		{ C2, hsm::Event::Exit,  put("-2") },
		{ Q,  hsm::Event::Entry, put("+Q") },
		{ Q,  hsm::Event::Exit,  put("-Q") },
	}};

	trace.clear();
	sm.start(P);
	sm.message({Go});
	sm.message({Back});
	sm.message({hsm::Event::Stop});
	CHECK(trace == "+P+1-1+2-2-P+Q-Q");
}

/* -------------------------------------------------------------------------- */

int main()
{
	testRestart();
	testMessageHsm();
	testAdd();
	testAll();
	testOrder();
	testHierarchy();

	if (failures == 0)
		puts("All tests passed");

	return failures == 0 ? 0 : 1;
}
