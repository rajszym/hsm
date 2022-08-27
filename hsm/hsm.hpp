/******************************************************************************

    @file    hsm.hpp
    @author  Rajmund Szymanski
    @date    09.08.2022
    @brief   This file contains definitions for hierachical state machine.

 ******************************************************************************

   Copyright (c) 2018-2022 Rajmund Szymanski. All rights reserved.

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to
   deal in the Software without restriction, including without limitation the
   rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
   sell copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
   THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
   IN THE SOFTWARE.

 ******************************************************************************/

#ifndef __HSM_HPP
#define __HSM_HPP

#if !defined __cplusplus || __cplusplus < 201703L
#warning required c++ 17 or above!
#endif

#include <functional>
#include <iterator>
#include <variant>
#include <vector>
#include <cassert>
#include "hsm.hpp"
#include "hsmconfig.hpp"

namespace hsm {

/* -------------------------------------------------------------------------- */

// hsm event values; redefine all event values in the application and add others

enum Event : unsigned
{
	ALL = 0,// the state action applies to all events
	Stop,   // stop the state machine
	Exit,   // exit from the state during transition
	Entry,  // entry to the state during transition
	Init,   // init the state after transition
	User,   // values less than hsmUser are reserved for the system
};

/* -------------------------------------------------------------------------- */

struct State;        // forward declarations
struct Action;       // *
struct StateMachine; // *

/******************************************************************************
 *
 * Type              : Handler
 *
 * Description       : hsm event handler
 *                     each state can handle the following system events:
 *                     - Event::Exit (exit from the state during transition)
 *                     - Event::Entry (entry to the state during transition)
 *                     - Event::Init (init the state after transition)
 *                     each state should also handle the necessary user events
 *                     user event values must be greater than or equal to hsmUser
 *                     values less than hsmUser are reserved for the system
 *                     transition is possible, if:
 *                     - handling user events or
 *                     - handling Event::Init and transition to child state
 *
 ******************************************************************************/

using Handler = std::function<void ( const Message& )>;

/******************************************************************************
 *
 * Type              : Variant
 *
 * Description       : hsm action variant: Handler / direct transition State
 *
 ******************************************************************************/

using Variant = std::variant<State*, Handler>;

/******************************************************************************
 *
 * Class             : State
 *
 * Description       : hsm state object
 *
 * Constructor parameters
 *            parent : state parent in the hsm tree
 *
 ******************************************************************************/

struct State
{
	constexpr State() {}
	constexpr State( State& parent_ ): parent{&parent_} {}

	State( State&& ) = default;
	State( const State& ) = delete;
	State& operator=( State&& ) = delete;
	State& operator=( const State& ) = delete;

/* -------------------------------------------------------------------------- */

	private:
	State *parent{}; // pointer to parent state in the hsm tree
	Action *queue{}; // state action queue

/******************************************************************************
 * Name              : hsm::State::getLevel
 * Description       : calculate state level in the hsm state tree
 * Parameters        :
 *             state : pointer to hsm state
 * Return            : state level for given hsm state
 * Note              : for internal use
 ******************************************************************************/

	static int getLevel( State *state_ )
	{
		int level_ = 0;

		while (state_ != nullptr)
		{
			level_++;
			state_ = state_->parent;
		}

		return level_;
	}

/******************************************************************************
 * Name              : hsm::State::getRoot
 * Description       : get pointer to root state for hsm state and the other
 * Parameters        :
 *             state : pointer to hsm state
 *             other : pointer to the other hsm state
 * Return            : pointer to root state
 * Note              : for internal use
 ******************************************************************************/

	static State* getRoot( State *state_, State *other_ )
	{
		int diff_ = State::getLevel(state_) - State::getLevel(other_);

		while (diff_-- > 0) state_ = state_->parent;
		while (++diff_ < 0) other_ = other_->parent;
		while (state_ != other_)
		{
			state_ = state_->parent;
			other_ = other_->parent;
		}

		return state_;
	}

/******************************************************************************
 * Name              : hsm::State::getQueue
 * Description       : get pointer to the first action in the state action queue
 * Parameters        : none
 * Return            : pointer to the first action in the state action queue
 * Note              : for internal use
 ******************************************************************************/

	Action* getQueue()
	{
		return State::queue;
	}

/******************************************************************************
 * Name              : hsm::State::setQueue
 * Description       : set the state action queue
 * Parameters        :
 *            action : pointer to the first action in the state action queue
 * Return            : none
 * Note              : for internal use
 ******************************************************************************/

	void setQueue( Action* action_ )
	{
		State::queue = action_;
	}

/******************************************************************************
 * Name              : hsm::State::getPrev
 * Description       : get the parent of the hsm state
 * Parameters        :
 *             state : pointer to given hsm state
 * Return            : pointer to the parent state
 * Note              : for internal use
 ******************************************************************************/

	static State* getPrev( State *state_ )
	{
		if (state_ != nullptr)
			state_ = state_->parent;

		return state_;
	}

/******************************************************************************
 * Name              : hsm::State::getNext
 * Description       : get child state in the child branch 
 * Parameters        :
 *             state : pointer to given hsm state
 *              sign : pointer to the hsm state in the child branch
 * Return            : pointer to the child state
 * Note              : for internal use
 ******************************************************************************/

	static State* getNext( State *state_, State *sign_ )
	{
		while (sign_ != nullptr)
		{
			if (state_ == sign_->parent)
				break;
			sign_ = sign_->parent;
		}

		return sign_;
	}

	friend struct Action;
	friend struct StateMachine;
};

/******************************************************************************
 *
 * Class             : Action
 *
 * Description       : hsm action object
 *
 * Constructor parameters
 *                   : none
 *
 ******************************************************************************/

struct Action
{
	Action( State& owner_, unsigned event_ ):                   owner{owner_}, event{event_}, action{& owner_} {}
	Action( State& owner_, unsigned event_, State&  state_ ):   owner{owner_}, event{event_}, action{& state_} {}
	Action( State& owner_, unsigned event_, Handler handler_ ): owner{owner_}, event{event_}, action{handler_} {}

	Action( Action&& ) = default;
	Action( const Action& ) = default;
	Action& operator=( Action&& ) = delete;
	Action& operator=( const Action& ) = delete;

/* -------------------------------------------------------------------------- */

	private:
	State&   owner; // action owner state
	unsigned event; // action event value
	Variant action; // transition target state or event handler
	Action *next{}; // next element in the hsm state action queue

	struct Visitor
	{
		Visitor(const Message& message_): message{message_} {}

		State* operator()(State*  state_)   const { return state_; }
		State* operator()(Handler handler_) const { handler_(Action::Visitor::message); return nullptr; }

		private:
		const Message& message;
	};

/******************************************************************************
 * Name              : hsm::Action::getAction
 * Description       : get action assigned to the given state and event value
 * Parameters        :
 *             state : state receiving the message
 *             event : event value in the message
 * Return            : pointer to the assigned action
 * Note              : for internal use
 ******************************************************************************/

	static Action* getAction( State *state_, unsigned event_ )
	{
		if (state_ == nullptr)
			return nullptr;

		Action *action_ = state_->getQueue();

		while (action_ != nullptr && action_->event != event_ && action_->event != Event::ALL)
			action_ = action_->next;

		return action_;
	}

/******************************************************************************
 * Name              : hsm::Action::callHandler
 * Description       : invoke event handler assigned to the given action, if such a variant exists
 * Parameters        :
 *             state : state receiving the message
 *           message : received message
 * Return            : pointer to the assigned action
 * Note              : for internal use
 ******************************************************************************/

	static State* callHandler( State *state_, const Message& message_ )
	{
		Action *action_ = Action::getAction(state_, message_.event);

		if (action_ == nullptr)
			return nullptr;

 		State* target_ = std::visit(Visitor{message_}, action_->action);

		if (target_ == nullptr)
			target_ = state_;

 		return target_;
	}

/******************************************************************************
 * Name              : hsm::Action::link
 * Description       : link hsm state action to the owner state action queue
 * Parameters        : none
 * Return            : none
 * Note              : for internal use
 ******************************************************************************/

	void link()
	{
		if (Action::next == nullptr)
		{
			Action::next = Action::owner.getQueue();
			Action::owner.setQueue(this);
		}
	}

	friend struct StateMachine;
};

/******************************************************************************
 *
 * Class             : StateMachine
 *
 * Description       : hierarchical state machine object
 *
 * Constructor parameters
 *                   : none
 *
 ******************************************************************************/

struct StateMachine
{
	StateMachine():                                  tab{}     {}
	StateMachine( const std::vector<Action>& tab_ ): tab{tab_} {}

	StateMachine( StateMachine&& ) = default;
	StateMachine( const StateMachine& ) = delete;
	StateMachine& operator=( StateMachine&& ) = delete;
	StateMachine& operator=( const StateMachine& ) = delete;

/******************************************************************************
 * Name              : hsm::StateMachine::add
 * Description       : add set of hsm actions to the hsm
 *               tab : std::vector with set of hsm actions
 * Return            : none
 ******************************************************************************/

	void add( const std::vector<Action>& tab_ )
	{
		std::copy(std::begin(tab_), std::end(tab_), std::back_inserter(StateMachine::tab));
	}

/******************************************************************************
 * Name              : hsm::StateMachine::add
 * Description       : add hsm action with given parameters to the hsm
 * Parameters        :
 *             owner : hsm action owner (State)
 *             event : hsm action event value
 *    handler, state : variant parameter for hsm action constructor
 * Return            : none
 * Note              : declared and defined in header file
 ******************************************************************************/

	template<class T>
	void add( State& owner_, unsigned event_, T&& action_ )
	{
		StateMachine::tab.emplace_back(owner_, event_, action_);
	}

/******************************************************************************
 * Name              : hsm::StateMachine::start
 * Description       : start hierarchical state machine
 * Parameters        :
 *              init : initial hsm state
 * Return            : none
 ******************************************************************************/

	void start( State& init_ )
	{
		assert(StateMachine::state == nullptr);
		assert(StateMachine::state == init_.parent);

		if (StateMachine::state == nullptr)
		{
			for (auto& action_: StateMachine::tab)
				action_.link();

			StateMachine::transition(&init_, {});
		}
	}

/******************************************************************************
 * Name              : hsm::StateMachine::message
 * Description       : handle given user message
 * Parameters        :
 *               msg : message
 * Return            : none
 ******************************************************************************/

	void message( const Message& message_ )
	{
		assert(StateMachine::state != nullptr);
		assert(message_.event == Event::Stop || message_.event >= Event::User);

		if      (message_.event >= Event::User) StateMachine::eventHandler({message_, this});
		else if (message_.event == Event::Stop) StateMachine::transition(nullptr, {});
	}

/******************************************************************************
 * Name              : hsm::StateMachine::transition
 * Description       : set the transition target state from the event handler
 * Parameters        :
 *            target : transition target state
 * Return            : none
 ******************************************************************************/

	void transition( State& target_ )
	{
		StateMachine::target = &target_;
	}

/* -------------------------------------------------------------------------- */

	private:
	State  *state{}; // current hsm state
	State *target{}; // the transition target set by the user in event handler
	                 // procedure with the function 'transition'
	std::vector<Action> tab; // set of hsm actions

/******************************************************************************
 * Name              : hsm::StateMachine::callAction
 * Description       : try to handle the message for the given hsm state
 * Parameters        :
 *             state : state handling the event
 *           message : received message
 * Return            : assigned action exist; message has been handled
 * Note              : for internal use
 ******************************************************************************/

	bool callAction( State *state_, const Message& message_ )
	{
		StateMachine::target = state_;

		State* target_ = Action::callHandler(state_, message_);

		if (target_ == nullptr)
			return false;

		if (target_ == state_)
			target_ = StateMachine::target;

		if (target_ == state_)
			return true;

		assert(message_.event >= Event::User || target_->parent == state_);
		StateMachine::transition(target_, message_);

		return true;
	}

/******************************************************************************
 * Name              : hsm::StateMachine::transition
 * Description       : do the transition to the given target state
 * Parameters        :
 *              next : transition target state
 *           message : handled message
 * Return            : none
 * Note              : for internal use
 ******************************************************************************/

	void transition( State *next_, const Message& message_ )
	{
		State *root_ = State::getRoot(StateMachine::state, next_);

		while (StateMachine::state != root_)
		{
			Action::callHandler(StateMachine::state, {message_, Event::Exit});
			StateMachine::state = State::getPrev(StateMachine::state);
		}

		while (StateMachine::state != next_)
		{
			StateMachine::state = State::getNext(StateMachine::state, next_);
			Action::callHandler(StateMachine::state, {message_, Event::Entry});
		}

		StateMachine::callAction(StateMachine::state, {message_, Event::Init});
	}

/******************************************************************************
 * Name              : hsm::StateMachine::eventHandler
 * Description       : internal event handler procedure
 * Parameters        :
 *           message : received message
 * Return            : none
 * Note              : for internal use
 ******************************************************************************/

	void eventHandler( const Message& message_ )
	{
		State *state_ = StateMachine::state;

		while (state_ != nullptr && !StateMachine::callAction(state_, message_))
			state_ = state_->parent;
	}
};

/* -------------------------------------------------------------------------- */

}     //  namespace hsm

#endif//__HSM_HPP

/* -------------------------------------------------------------------------- */

#ifndef __HSM_DEBUGOUT_HPP
#define __HSM_DEBUGOUT_HPP

/* -------------------------------------------------------------------------- */

#if defined(DEBUG) && DEBUG > 0

	#ifdef WIN32
		#include <windows.h>
		__attribute__((constructor))
		static void WIN32_ANSI_INIT() { SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE),
		                 ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_PROCESSED_OUTPUT); }
	#endif

	#ifndef PRINTF
		#include <cstdio>
		#define PRINTF printf
	#endif

	#ifndef NOCOLORED
	#define DEBUGOUT( c, h, s, ... ) PRINTF("\x1b[%dm" h s "\x1b[0m" "\n", c, ##__VA_ARGS__) 
	#else
	#define DEBUGOUT( c, h, s, ... ) PRINTF(""         h s ""        "\n",    ##__VA_ARGS__)
	#endif

#endif

/* -------------------------------------------------------------------------- */

// 30 Black,   90 Bright Black
// 31 Red,     91 Bright Red
// 32 Green,   92 Bright Green
// 33 Yellow,  93 Bright Yellow
// 34 Blue,    94 Bright Blue
// 35 Magenta, 95 Bright Magenta
// 36 Cyan,    96 Bright Cyan
// 37 White,   97 Bright White

#if defined(DEBUG) && DEBUG >= 1 // ERROR
	#define EDEBUG( s, ... ) DEBUGOUT( 91, "[error] ",   s, ##__VA_ARGS__ )
#else
	#define EDEBUG( ... ) do {} while (0)
#endif

#if defined(DEBUG) && DEBUG >= 2 // WARNING
	#define WDEBUG( s, ... ) DEBUGOUT( 93, "[warning] ", s, ##__VA_ARGS__ )
#else
	#define WDEBUG( ... ) do {} while (0)
#endif

#if defined(DEBUG) && DEBUG >= 3 // INFO
	#define IDEBUG( s, ... ) DEBUGOUT( 32, "[info] ",    s, ##__VA_ARGS__ )
#else
	#define IDEBUG( ... ) do {} while (0)
#endif

#if defined(DEBUG) && DEBUG >= 4 // TRACE
	#define TDEBUG( s, ... ) DEBUGOUT( 36, "[trace] ",   s, ##__VA_ARGS__ )
#else
	#define TDEBUG( ... ) do {} while (0)
#endif

/* -------------------------------------------------------------------------- */

#endif//__HSM_DEBUGOUT_HPP
