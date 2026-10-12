/******************************************************************************

    @file    hsm.hpp
    @author  Rajmund Szymanski
    @date    12.10.2026
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

#ifndef HSM_HPP_
#define HSM_HPP_

#if !defined __cplusplus || __cplusplus < 201703L
#warning required c++ 17 or above!
#endif

#include <algorithm>
#include <deque>
#include <functional>
#include <iterator>
#include <utility>
#include <variant>
#include <vector>
#include <cassert>
#include "hsmconfig.hpp"

namespace hsm {

/* -------------------------------------------------------------------------- */

// hsm event values; redefine all event values in the application and add others

namespace Event {

enum : unsigned
{
	ALL = 0,// the state action applies to all user events not handled explicitly by the state
	Stop,   // stop the state machine
	Exit,   // exit from the state during transition
	Entry,  // entry to the state during transition
	Init,   // init the state after transition
	User,   // values less than Event::User are reserved for the system
};

}     //  namespace Event

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
 *                     user event values must be greater than or equal to Event::User
 *                     values less than Event::User are reserved for the system
 *                     transition is possible, if:
 *                     - handling user events or
 *                     - handling Event::Init and transition to the substate
 *                     the transition target is set with the function 'transition'
 *                     messages sent from the handler are queued and handled after
 *                     the current message (run-to-completion)
 *
 ******************************************************************************/

using Handler = std::function<void ( const Message& )>;

/******************************************************************************
 *
 * Type              : Variant
 *
 * Description       : hsm action variant: Handler / direct transition State
 *                     (nullptr: the event is handled without transition)
 *
 ******************************************************************************/

using Variant = std::variant<State*, Handler>;

/******************************************************************************
 *
 * Class             : State
 *
 * Description       : hsm state object
 *                     state objects can be shared by many state machines
 *
 * Constructor parameters
 *            parent : state parent in the hsm tree
 *
 ******************************************************************************/

struct State
{
	constexpr State() {}
	constexpr explicit State( State& parent_ ): parent{&parent_} {}

	State( State&& ) = delete;
	State( const State& ) = delete;
	State& operator=( State&& ) = delete;
	State& operator=( const State& ) = delete;

/* -------------------------------------------------------------------------- */

	private:
	State *parent{}; // pointer to parent state in the hsm tree

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

/******************************************************************************
 * Name              : hsm::State::isSubstate
 * Description       : check if the hsm state is a substate of the other
 * Parameters        :
 *             state : pointer to hsm state
 *             other : pointer to the other hsm state
 * Return            : true if the state is a child, grandchild, etc. of the other
 * Note              : for internal use
 ******************************************************************************/

	static bool isSubstate( State *state_, State *other_ )
	{
		while (state_ != nullptr)
		{
			state_ = state_->parent;
			if (state_ == other_)
				return other_ != nullptr;
		}

		return false;
	}

	friend struct StateMachine;
};

/******************************************************************************
 *
 * Class             : Action
 *
 * Description       : hsm action object
 *
 * Constructor parameters
 *             owner : hsm action owner (State)
 *             event : hsm action event value
 *    handler, state : event handler / transition target state
 *                     (none: the event is handled without transition)
 *
 ******************************************************************************/

struct Action
{
	Action( State& owner_, unsigned event_ ):                   owner{owner_}, event{event_}, action{static_cast<State*>(nullptr)} {}
	Action( State& owner_, unsigned event_, State&  state_ ):   owner{owner_}, event{event_}, action{& state_} {}
	Action( State& owner_, unsigned event_, Handler handler_ ): owner{owner_}, event{event_}, action{std::move(handler_)} {}

	Action( Action&& ) = default;
	Action( const Action& ) = default;
	Action& operator=( Action&& ) = delete;
	Action& operator=( const Action& ) = delete;

/* -------------------------------------------------------------------------- */

	private:
	State&   owner; // action owner state
	unsigned event; // action event value
	Variant action; // transition target state or event handler

	friend struct StateMachine;
};

/******************************************************************************
 *
 * Class             : StateMachine
 *
 * Description       : hierarchical state machine object
 *                     the object is not thread-safe
 *
 * Constructor parameters
 *               tab : std::vector with set of hsm actions
 *
 ******************************************************************************/

struct StateMachine
{
	StateMachine():                                  tab{}     {}
	StateMachine( const std::vector<Action>& tab_ ): tab{tab_} {}

	StateMachine( StateMachine&& other_ ):
		state{std::exchange(other_.state, nullptr)},
		tab{std::move(other_.tab)},
		index{std::move(other_.index)}
	{
		assert(!other_.busy);
	}

	StateMachine( const StateMachine& ) = delete;
	StateMachine& operator=( StateMachine&& ) = delete;
	StateMachine& operator=( const StateMachine& ) = delete;

/******************************************************************************
 * Name              : hsm::StateMachine::add
 * Description       : add set of hsm actions to the hsm
 *               tab : std::vector with set of hsm actions
 * Return            : true if actions have been added
 *                     false if the hsm is running (actions can be added only before start)
 ******************************************************************************/

	bool add( const std::vector<Action>& tab_ )
	{
		assert(StateMachine::state == nullptr);

		if (StateMachine::state != nullptr)
			return false;

		std::copy(std::begin(tab_), std::end(tab_), std::back_inserter(StateMachine::tab));
		return true;
	}

/******************************************************************************
 * Name              : hsm::StateMachine::add
 * Description       : add hsm action handling the event without transition
 * Parameters        :
 *             owner : hsm action owner (State)
 *             event : hsm action event value
 * Return            : true if action has been added
 *                     false if the hsm is running (actions can be added only before start)
 ******************************************************************************/

	bool add( State& owner_, unsigned event_ )
	{
		assert(StateMachine::state == nullptr);

		if (StateMachine::state != nullptr)
			return false;

		StateMachine::tab.emplace_back(owner_, event_);
		return true;
	}

/******************************************************************************
 * Name              : hsm::StateMachine::add
 * Description       : add hsm action with given parameters to the hsm
 * Parameters        :
 *             owner : hsm action owner (State)
 *             event : hsm action event value
 *    handler, state : variant parameter for hsm action constructor
 * Return            : true if action has been added
 *                     false if the hsm is running (actions can be added only before start)
 * Note              : declared and defined in header file
 ******************************************************************************/

	template<class T>
	bool add( State& owner_, unsigned event_, T&& action_ )
	{
		assert(StateMachine::state == nullptr);

		if (StateMachine::state != nullptr)
			return false;

		StateMachine::tab.emplace_back(owner_, event_, std::forward<T>(action_));
		return true;
	}

/******************************************************************************
 * Name              : hsm::StateMachine::start
 * Description       : start hierarchical state machine
 * Parameters        :
 *              init : initial hsm state
 * Return            : true if the hsm has been started
 *                     false if the hsm is already running
 ******************************************************************************/

	bool start( State& init_ )
	{
		assert(StateMachine::state == nullptr && !StateMachine::busy);

		if (StateMachine::state != nullptr || StateMachine::busy)
			return false;

		// index of the actions sorted by the owner state, keeping the order of actions in the table
		StateMachine::index.clear();
		for (auto& action_: StateMachine::tab)
			StateMachine::index.push_back(&action_);
		std::stable_sort(std::begin(StateMachine::index), std::end(StateMachine::index), StateMachine::Less{});

		StateMachine::run({}, [this, &init_]( const Message& message_ ){ StateMachine::transition(&init_, message_); });
		return true;
	}

/******************************************************************************
 * Name              : hsm::StateMachine::message
 * Description       : handle given user message
 *                     message sent from the event handler is queued and handled
 *                     after the current message (run-to-completion)
 * Parameters        :
 *               msg : message
 * Return            : none
 ******************************************************************************/

	void message( const Message& message_ )
	{
		assert(message_.event == Event::Stop || message_.event >= Event::User);
		assert(StateMachine::state != nullptr || StateMachine::busy);

		StateMachine::run(message_, [this]( const Message& msg_ ){ StateMachine::dispatch(msg_); });
	}

/******************************************************************************
 * Name              : hsm::StateMachine::transition
 * Description       : set the transition target state from the event handler
 *                     transition to the handling state or its ancestor is local:
 *                     the target state is not exited and entered again,
 *                     only its substates are exited and Event::Init is handled
 *                     the function is ignored in Event::Exit and Event::Entry handlers
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
	bool     busy{}; // the hsm is handling a message
	std::vector<Action> tab;           // set of hsm actions
	std::vector<const Action*> index;  // actions sorted by the owner state
	std::deque<Message> queue;         // messages sent from the event handlers

	struct Less
	{
		bool operator()( const Action *action_, const Action *other_ ) const { return std::less<const State*>{}(&action_->owner, &other_->owner); }
		bool operator()( const Action *action_, const State  *state_ ) const { return std::less<const State*>{}(&action_->owner, state_); }
	};

/******************************************************************************
 * Name              : hsm::StateMachine::run
 * Description       : handle the message, then all the messages queued meanwhile
 *                     queue the message if the hsm is already handling a message
 * Parameters        :
 *           message : message
 *              proc : procedure handling the first message
 * Return            : none
 * Note              : for internal use
 ******************************************************************************/

	template<class F>
	void run( const Message& message_, F proc_ )
	{
		if (StateMachine::busy)
		{
			StateMachine::queue.push_back(message_);
			return;
		}

		struct Lock
		{
			Lock( StateMachine& hsm_ ): hsm{hsm_} { hsm.busy = true; }
		   ~Lock() { hsm.busy = false; hsm.queue.clear(); }
			Lock( const Lock& ) = delete;
			Lock& operator=( const Lock& ) = delete;
			private:
			StateMachine& hsm;
		} const lock_{*this};

		proc_({message_, this});

		while (!StateMachine::queue.empty())
		{
			const Message next_ = StateMachine::queue.front();
			StateMachine::queue.pop_front();
			StateMachine::dispatch({next_, this});
		}
	}

/******************************************************************************
 * Name              : hsm::StateMachine::getAction
 * Description       : get action assigned to the given state and event value
 *                     action with the given event value takes precedence over
 *                     action with Event::ALL, which applies to user events only
 * Parameters        :
 *             state : state receiving the message
 *             event : event value in the message
 * Return            : pointer to the assigned action
 * Note              : for internal use
 ******************************************************************************/

	const Action* getAction( State *state_, unsigned event_ ) const
	{
		if (state_ == nullptr)
			return nullptr;

		const Action *any_ = nullptr;

		auto action_ = std::lower_bound(std::begin(StateMachine::index), std::end(StateMachine::index), state_, StateMachine::Less{});

		for (; action_ != std::end(StateMachine::index) && &(*action_)->owner == state_; ++action_)
		{
			if ((*action_)->event == event_)
				return *action_;

			if (any_ == nullptr && (*action_)->event == Event::ALL && event_ >= Event::User)
				any_ = *action_;
		}

		return any_;
	}

/******************************************************************************
 * Name              : hsm::StateMachine::callAction
 * Description       : invoke the action
 * Parameters        :
 *            action : the action
 *           message : received message
 * Return            : transition target state or nullptr if no transition
 * Note              : for internal use
 ******************************************************************************/

	State* callAction( const Action *action_, const Message& message_ )
	{
		StateMachine::target = nullptr;

		if (auto state_ = std::get_if<State*>(&action_->action))
			StateMachine::target = *state_;
		else
			std::get<Handler>(action_->action)(message_);

		return std::exchange(StateMachine::target, nullptr);
	}

/******************************************************************************
 * Name              : hsm::StateMachine::signal
 * Description       : handle the system event (Exit, Entry) for the given hsm state
 *                     the transition target is ignored
 * Parameters        :
 *             state : state handling the event
 *           message : handled message
 *             event : system event value
 * Return            : none
 * Note              : for internal use
 ******************************************************************************/

	void signal( State *state_, const Message& message_, unsigned event_ )
	{
		const Action *action_ = StateMachine::getAction(state_, event_);

		if (action_ != nullptr)
			StateMachine::callAction(action_, {message_, event_});
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
		while (next_ != nullptr || StateMachine::state != nullptr)
		{
			State *root_ = State::getRoot(StateMachine::state, next_);

			while (StateMachine::state != root_)
			{
				StateMachine::signal(StateMachine::state, message_, Event::Exit);
				StateMachine::state = State::getPrev(StateMachine::state);
			}

			while (StateMachine::state != next_)
			{
				StateMachine::state = State::getNext(StateMachine::state, next_);
				StateMachine::signal(StateMachine::state, message_, Event::Entry);
			}

			if (StateMachine::state == nullptr)
				break;

			const Action *action_ = StateMachine::getAction(StateMachine::state, Event::Init);

			if (action_ == nullptr)
				break;

			next_ = StateMachine::callAction(action_, {message_, Event::Init});

			// the init transition is possible only to the substate
			assert(next_ == nullptr || State::isSubstate(next_, StateMachine::state));

			if (!State::isSubstate(next_, StateMachine::state))
				break;
		}
	}

/******************************************************************************
 * Name              : hsm::StateMachine::dispatch
 * Description       : internal event handler procedure
 * Parameters        :
 *           message : received message
 * Return            : none
 * Note              : for internal use
 ******************************************************************************/

	void dispatch( const Message& message_ )
	{
		if (message_.event == Event::Stop)
		{
			StateMachine::transition(nullptr, message_);
			return;
		}

		if (message_.event < Event::User)
			return;

		for (State *state_ = StateMachine::state; state_ != nullptr; state_ = state_->parent)
		{
			const Action *action_ = StateMachine::getAction(state_, message_.event);

			if (action_ == nullptr)
				continue;

			State *next_ = StateMachine::callAction(action_, message_);

			if (next_ != nullptr)
				StateMachine::transition(next_, message_);

			break;
		}
	}
};

/* -------------------------------------------------------------------------- */

}     //  namespace hsm

#endif//HSM_HPP_
