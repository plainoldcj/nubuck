#pragma once

#include <string>
#include <ostream>

#include <Nubuck\nubuck_api.h>
#include <Nubuck\events\events.h>

namespace W { class Entity; }
namespace NB { typedef W::Entity* Entity; }

namespace EV {

struct ResizeEvent : Event {
    EVENT_TYPE(ResizeEvent)

    int width;
    int height;
};

struct MouseEvent : Event {
    EVENT_TYPE(MouseEvent)

    enum Type { MOUSE_DOWN, MOUSE_UP, MOUSE_WHEEL, MOUSE_MOVE };
    enum Button { 
        BUTTON_NONE     = 0,    // == Qt::NoButton
        BUTTON_LEFT     = 1,    // == Qt::LeftButton
        BUTTON_RIGHT    = 2,    // == Qt::RightButton
        BUTTON_MIDDLE   = 4     // == Qt::MiddleButton
    };
    enum Modifier {
        MODIFIER_SHIFT  = 0x02000000, // == Qt::ShiftModifier
        MODIFIER_CTRL   = 0x04000000  // == Qt::ControlModifier
    };
    int type, button, mods, delta, x, y;
    int* ret;
};

struct KeyEvent : Event {
    EVENT_TYPE(KeyEvent)

    enum Type { KEY_DOWN = 0, KEY_UP };
    int type;
    int keyCode;
    int nativeScanCode;
    bool autoRepeat;
    enum Modifier {
        // MODIFER_* == Qt::*Modifier
        MODIFIER_SHIFT = 0x02000000,
        MODIFIER_CTRL = 0x04000000,
        MODIFIER_ALT = 0x08000000
    };
    int mods;
};

std::ostream& operator<<(std::ostream& stream, const MouseEvent& mouseEvent);
std::ostream& operator<<(std::ostream& stream, const KeyEvent& keyEvent);

struct Usr_SelectEntity : Event {
    EVENT_TYPE(Usr_SelectEntity)

    NB::Entity  entity;
    bool        shiftModifier;
};

struct ShowQuestionBox : Event {
    EVENT_TYPE(ShowQuestionBox);

    std::string caption;
    std::string message;
};

} // namespace EV

NUBUCK_API extern EV::ConcreteEventDef<EV::ResizeEvent> ev_resize;
NUBUCK_API extern EV::ConcreteEventDef<EV::MouseEvent>  ev_mouse;
NUBUCK_API extern EV::ConcreteEventDef<EV::KeyEvent>    ev_key;
NUBUCK_API extern EV::ConcreteEventDef<EV::Event>       ev_buttonClicked;
NUBUCK_API extern EV::ConcreteEventDef<EV::Arg<bool> >  ev_checkBoxToggled;

NUBUCK_API extern EV::ConcreteEventDef<EV::Event>       ev_op_requestFinish; // operator is asked to terminate

NUBUCK_API extern EV::ConcreteEventDef<EV::ShowQuestionBox> ev_ui_showQuestionBox;

NUBUCK_API extern EV::ConcreteEventDef<EV::Usr_SelectEntity>    ev_usr_selectEntity;
NUBUCK_API extern EV::ConcreteEventDef<EV::Arg<int> >           ev_usr_changeEditMode;