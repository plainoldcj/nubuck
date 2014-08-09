#include <Nubuck\animation\animation.h>
#include <Nubuck\animation\animator.h>

#include <UI\window_events.h>
#include <world\world_events.h>

namespace A {

Animation::Animation()
    : _isDone(true) // animation starts paused
{
    g_animator.LinkAnimation(this);
}

Animation::~Animation() {
    g_animator.UnlinkAnimation(this);
}

bool Animation::IsDone() const { return _isDone; }

void Animation::Move(float secsPassed) {
    DoMove(secsPassed);

    _time += secsPassed;

    if(AnimMode::PLAY_FOR_DURATION == _mode && _duration <= _time) {
        _isDone = true;
    }
}

void Animation::FilterEvent(const EV::Event& event) {
    if(AnimMode::PLAY_UNTIL_EVENT == _mode) {
        assert(_eventFilter);
        if(_eventFilter(event)) {
            _isDone = true;
        }
    }
}

void Animation::PlayFor(float duration) {
    _time = 0.0f;
    _duration = duration;
    _mode = AnimMode::PLAY_FOR_DURATION;
    _isDone = false;
}

void Animation::PlayUntil(eventFilter_t eventFilter) {
    _time = 0.0f;
    _eventFilter = eventFilter;
    _mode = AnimMode::PLAY_UNTIL_EVENT;
    _isDone = false;
}

// stock event filters

bool IsWidgetEvent(const EV::Event& event) {
    if(EV::def_Key.GetEventID()                 == event.id) return false;
    if(EV::def_Mouse.GetEventID()   			== event.id) return false;
    if(EV::def_Resize.GetEventID()  			== event.id) return false;
    if(EV::def_SelectionChanged.GetEventID()    == event.id) return false;
    // TODO ... event tags would be nice

    return true;
}

} // namespace A