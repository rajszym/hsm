#include <hsm.hpp>
#include <cstdio>

enum Event
{
	ALL   = hsm::Event::ALL,
	End   = hsm::Event::Stop,
	Exit  = hsm::Event::Exit,
	Entry = hsm::Event::Entry,
	Init  = hsm::Event::Init,
	Power = hsm::Event::User,
	Stop,
	Play,
	Pause,
	Rec,
	Rew,
	FF,
};

auto StateOff             = hsm::State();
auto StateIdle            = hsm::State();
auto StateIdleStop        = hsm::State(StateIdle);
auto StateIdleFF          = hsm::State(StateIdle);
auto StateIdleRew         = hsm::State(StateIdle);
auto StatePlaying         = hsm::State();
auto StatePlayingPlay     = hsm::State(StatePlaying);
auto StatePlayingPause    = hsm::State(StatePlaying);
auto StateRecording       = hsm::State();
auto StateRecordingRecord = hsm::State(StateRecording);
auto StateRecordingPause  = hsm::State(StateRecording);
auto vcr = hsm::StateMachine
{{
	{ StateOff,             Event::Entry,   [](const hsm::Message&){ puts("Enter standby mode"); } },
	{ StateOff,             Event::Exit,    [](const hsm::Message&){ puts("Exit standby mode"); } },
	{ StateOff,             Event::Power,   StateIdle },
	{ StateIdle,            Event::Entry,   [](const hsm::Message&){ puts("Enter idle"); } },
	{ StateIdle,            Event::Exit,    [](const hsm::Message&){ puts("Exit idle"); } },
	{ StateIdle,            Event::Init,    StateIdleStop },
	{ StateIdle,            Event::Power,   StateOff },
	{ StateIdle,            Event::Play,    StatePlaying },
	{ StateIdle,            Event::Rec,     StateRecording },
	{ StateIdleStop,        Event::Entry,   [](const hsm::Message&){ puts("Get ready"); } },
	{ StateIdleStop,        Event::Rew,     StateIdleRew },
	{ StateIdleStop,        Event::FF,      StateIdleFF },
	{ StateIdleRew,         Event::Entry,   [](const hsm::Message&){ puts("Rewind"); } },
	{ StateIdleRew,         Event::Stop,    StateIdle },
	{ StateIdleFF,          Event::Entry,   [](const hsm::Message&){ puts("Fast forward"); } },
	{ StateIdleFF,          Event::Stop,    StateIdle },
	{ StatePlaying,         Event::Entry,   [](const hsm::Message&){ puts("Enter playing"); } },
	{ StatePlaying,         Event::Exit,    [](const hsm::Message&){ puts("Exit playing"); } },
	{ StatePlaying,         Event::Init,    StatePlayingPlay },
	{ StatePlaying,         Event::Power,   StateOff },
	{ StatePlaying,         Event::Stop,    StateIdle },
	{ StatePlayingPlay,     Event::Entry,   [](const hsm::Message&){ puts("Playing"); } },
	{ StatePlayingPlay,     Event::Pause,   StatePlayingPause },
	{ StatePlayingPause,    Event::Entry,   [](const hsm::Message&){ puts("Playing pause"); } },
	{ StatePlayingPause,    Event::Play,    StatePlayingPlay },
	{ StateRecording,       Event::Entry,   [](const hsm::Message&){ puts("Enter recording"); } },
	{ StateRecording,       Event::Exit,    [](const hsm::Message&){ puts("Exit recording"); } },
	{ StateRecording,       Event::Init,    StateRecordingRecord },
	{ StateRecording,       Event::Power,   StateOff },
	{ StateRecording,       Event::Stop,    StateIdle },
	{ StateRecordingRecord, Event::Entry,   [](const hsm::Message&){ puts("Recording"); } },
	{ StateRecordingRecord, Event::Pause,   StateRecordingPause },
	{ StateRecordingPause,  Event::Entry,   [](const hsm::Message&){ puts("Recording pause"); } },
	{ StateRecordingPause,  Event::Rec,     StateRecordingRecord },
}};

int main()
{
	vcr.start(StateOff);
	vcr.message({Event::Power}); // Turn on the power
	vcr.message({Event::Rew});   // Rewind to the beginning
	vcr.message({Event::Stop});  // Beginning of tape, end of rewinding
	vcr.message({Event::Play});  // Watching movie
	vcr.message({Event::Pause}); // A little break
	vcr.message({Event::Play});  // Resume watching a movie
	vcr.message({Event::Stop});  // End of the movie
	vcr.message({Event::Rew});   // Rewind to the beginning
	vcr.message({Event::Stop});  // Beginning of tape, end of rewinding
	vcr.message({Event::Rec});   // Now we're gonna record something
	vcr.message({Event::Stop});  // End of recording
	vcr.message({Event::Power}); // Turn off the power
	vcr.message({Event::End});   // Stop state machine
}
