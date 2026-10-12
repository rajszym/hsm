# Hierarchical State Machine

Header-only hierarchical state machine for C++17 and above.

## Usage

```cpp
#include <hsm.hpp>
#include <cstdio>

enum Event
{
	Power = hsm::Event::User, // user events start at hsm::Event::User
	Play,
};

hsm::State Off;
hsm::State On;
hsm::State OnIdle(On);    // substate of On
hsm::State OnPlaying(On); // substate of On

hsm::StateMachine player
{{
	{ Off,       hsm::Event::Entry, [](const hsm::Message&){ puts("standby"); } },
	{ Off,       Power,             On },
	{ On,        hsm::Event::Init,  OnIdle },
	{ On,        Power,             Off },          // handled in every substate of On
	{ OnIdle,    Play,              OnPlaying },
	{ OnPlaying, Play,              [](const hsm::Message& msg){ msg.hsm->transition(OnIdle); } },
}};

int main()
{
	player.start(Off);
	player.message({Power});
	player.message({Play});
	player.message({hsm::Event::Stop});
}
```

See [src/example.cpp](src/example.cpp) for a complete example.

## API

- `hsm::State` – a node of the state tree; `hsm::State child(parent)` creates a substate. States can be neither copied nor moved.
- `hsm::Action{ owner, event, target }` – an action of the `owner` state for the `event`; `target` is:
  - a `hsm::State` – transition to that state,
  - a `hsm::Handler` (`std::function<void(const hsm::Message&)>`) – an event handler; it may set the transition target with `msg.hsm->transition(state)`,
  - omitted – the event is handled without transition.
- `hsm::StateMachine` – a set of actions:
  - `add(table)`, `add(owner, event, target)`, `add(owner, event)` – adds actions; possible only when the machine is not running (returns `false` otherwise),
  - `start(state)` – starts the machine in the given state (entering all its ancestors),
  - `message(msg)` – handles the user event, or `hsm::Event::Stop` which exits all states and stops the machine; the machine can be started again.
- `hsm::Message` – defined in [hsm/hsmconfig.hpp](hsm/hsmconfig.hpp); add your own data and constructors there. `msg.hsm` points to the handling state machine, `msg.event` is the event value.

## Semantics

- **Events** below `hsm::Event::User` are reserved: `Exit`, `Entry` and `Init` are sent by the machine to the states during transitions, `Stop` stops the machine.
- **Dispatch**: a user event is offered to the current state, then to its ancestors; the first state with a matching action handles it.
- **Action lookup**: an action for the exact event takes precedence over `hsm::Event::ALL`, which applies to user events only. For duplicated events the first action in the table wins.
- **Transitions**: states are exited up to the common ancestor of the current and the target state, then entered down to the target, then `Init` of the target is handled. A transition to the handling state or its ancestor is local: the target is not exited and entered again, only its substates are.
- **Init** may transition only to a substate (child, grandchild, ...) of the initialized state; other targets are ignored.
- **Exit / Entry** handlers cannot make a transition.
- **Run-to-completion**: messages sent from a handler are queued and handled after the current message.
- **Sharing**: state objects can be used by many state machines; each machine has its own action table and current state.
- **Exceptions** thrown from a handler propagate to the caller of `start` / `message`; the queued messages are dropped and the machine stays in the state reached so far.
- The state machine is not thread-safe.
- Invalid use (e.g. `add` or `start` on a running machine, a reserved event in `message`) is reported with `assert` and ignored when `NDEBUG` is defined.

## Debug output

Optional macros `EDEBUG`, `WDEBUG`, `IDEBUG`, `TDEBUG` (error ... trace) are available in [hsm/hsmdebug.hpp](hsm/hsmdebug.hpp); the output level is selected with the `DEBUG` macro (1 ... 4).

## Building

```
make run          # build and run the example
make test         # build and run the tests
make DEFS=DEBUG   # debug build
make DEFS=CLANG   # build with clang
```

### [License](https://github.com/rajszym/hsm/blob/master/LICENSE)
The project is licensed under the MIT license.
