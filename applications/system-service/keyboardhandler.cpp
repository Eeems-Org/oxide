#include "keyboardhandler.h"

#include <QFileInfo>
#include <QKeyEvent>
#include <liboxide/devicesettings.h>

KeyboardHandler* KeyboardHandler::init(){
    static KeyboardHandler* instance;
    if(instance != nullptr){
        return instance;
    }
    instance = new KeyboardHandler();
    instance->moveToThread(instance);
    instance->start();
    return instance;
}

KeyboardHandler::KeyboardHandler(){
    setObjectName("OxideVirtKeyboard");
    fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if(fd == -1){
        O_WARNING("Failed to open uinput!");
    }else{
        ioctl(fd, UI_SET_EVBIT, EV_REP);
        ioctl(fd, UI_SET_EVBIT, EV_LED);
        ioctl(fd, UI_SET_EVBIT, EV_KEY);
        ioctl(fd, UI_SET_EVBIT, EV_SYN);

        ioctl(fd, UI_SET_KEYBIT, KEY_RESERVED);
        ioctl(fd, UI_SET_KEYBIT, KEY_ESC);
        ioctl(fd, UI_SET_KEYBIT, KEY_1);
        ioctl(fd, UI_SET_KEYBIT, KEY_2);
        ioctl(fd, UI_SET_KEYBIT, KEY_3);
        ioctl(fd, UI_SET_KEYBIT, KEY_4);
        ioctl(fd, UI_SET_KEYBIT, KEY_5);
        ioctl(fd, UI_SET_KEYBIT, KEY_6);
        ioctl(fd, UI_SET_KEYBIT, KEY_7);
        ioctl(fd, UI_SET_KEYBIT, KEY_8);
        ioctl(fd, UI_SET_KEYBIT, KEY_9);
        ioctl(fd, UI_SET_KEYBIT, KEY_0);
        ioctl(fd, UI_SET_KEYBIT, KEY_MINUS);
        ioctl(fd, UI_SET_KEYBIT, KEY_EQUAL);
        ioctl(fd, UI_SET_KEYBIT, KEY_BACKSPACE);
        ioctl(fd, UI_SET_KEYBIT, KEY_TAB);
        ioctl(fd, UI_SET_KEYBIT, KEY_Q);
        ioctl(fd, UI_SET_KEYBIT, KEY_W);
        ioctl(fd, UI_SET_KEYBIT, KEY_E);
        ioctl(fd, UI_SET_KEYBIT, KEY_R);
        ioctl(fd, UI_SET_KEYBIT, KEY_T);
        ioctl(fd, UI_SET_KEYBIT, KEY_Y);
        ioctl(fd, UI_SET_KEYBIT, KEY_U);
        ioctl(fd, UI_SET_KEYBIT, KEY_I);
        ioctl(fd, UI_SET_KEYBIT, KEY_O);
        ioctl(fd, UI_SET_KEYBIT, KEY_P);
        ioctl(fd, UI_SET_KEYBIT, KEY_LEFTBRACE);
        ioctl(fd, UI_SET_KEYBIT, KEY_RIGHTBRACE);
        ioctl(fd, UI_SET_KEYBIT, KEY_ENTER);
        ioctl(fd, UI_SET_KEYBIT, KEY_LEFTCTRL);
        ioctl(fd, UI_SET_KEYBIT, KEY_A);
        ioctl(fd, UI_SET_KEYBIT, KEY_S);
        ioctl(fd, UI_SET_KEYBIT, KEY_D);
        ioctl(fd, UI_SET_KEYBIT, KEY_F);
        ioctl(fd, UI_SET_KEYBIT, KEY_G);
        ioctl(fd, UI_SET_KEYBIT, KEY_H);
        ioctl(fd, UI_SET_KEYBIT, KEY_J);
        ioctl(fd, UI_SET_KEYBIT, KEY_K);
        ioctl(fd, UI_SET_KEYBIT, KEY_L);
        ioctl(fd, UI_SET_KEYBIT, KEY_SEMICOLON);
        ioctl(fd, UI_SET_KEYBIT, KEY_APOSTROPHE);
        ioctl(fd, UI_SET_KEYBIT, KEY_GRAVE);
        ioctl(fd, UI_SET_KEYBIT, KEY_LEFTSHIFT);
        ioctl(fd, UI_SET_KEYBIT, KEY_BACKSLASH);
        ioctl(fd, UI_SET_KEYBIT, KEY_Z);
        ioctl(fd, UI_SET_KEYBIT, KEY_X);
        ioctl(fd, UI_SET_KEYBIT, KEY_C);
        ioctl(fd, UI_SET_KEYBIT, KEY_V);
        ioctl(fd, UI_SET_KEYBIT, KEY_B);
        ioctl(fd, UI_SET_KEYBIT, KEY_N);
        ioctl(fd, UI_SET_KEYBIT, KEY_M);
        ioctl(fd, UI_SET_KEYBIT, KEY_COMMA);
        ioctl(fd, UI_SET_KEYBIT, KEY_DOT);
        ioctl(fd, UI_SET_KEYBIT, KEY_SLASH);
        ioctl(fd, UI_SET_KEYBIT, KEY_RIGHTSHIFT);
        ioctl(fd, UI_SET_KEYBIT, KEY_KPASTERISK);
        ioctl(fd, UI_SET_KEYBIT, KEY_LEFTALT);
        ioctl(fd, UI_SET_KEYBIT, KEY_SPACE);
        ioctl(fd, UI_SET_KEYBIT, KEY_CAPSLOCK);
        ioctl(fd, UI_SET_KEYBIT, KEY_F1);
        ioctl(fd, UI_SET_KEYBIT, KEY_F2);
        ioctl(fd, UI_SET_KEYBIT, KEY_F3);
        ioctl(fd, UI_SET_KEYBIT, KEY_F4);
        ioctl(fd, UI_SET_KEYBIT, KEY_F5);
        ioctl(fd, UI_SET_KEYBIT, KEY_F6);
        ioctl(fd, UI_SET_KEYBIT, KEY_F7);
        ioctl(fd, UI_SET_KEYBIT, KEY_F8);
        ioctl(fd, UI_SET_KEYBIT, KEY_F9);
        ioctl(fd, UI_SET_KEYBIT, KEY_F10);
        ioctl(fd, UI_SET_KEYBIT, KEY_NUMLOCK);
        ioctl(fd, UI_SET_KEYBIT, KEY_SCROLLLOCK);
        ioctl(fd, UI_SET_KEYBIT, KEY_KP7);
        ioctl(fd, UI_SET_KEYBIT, KEY_KP8);
        ioctl(fd, UI_SET_KEYBIT, KEY_KP9);
        ioctl(fd, UI_SET_KEYBIT, KEY_KPMINUS);
        ioctl(fd, UI_SET_KEYBIT, KEY_KP4);
        ioctl(fd, UI_SET_KEYBIT, KEY_KP5);
        ioctl(fd, UI_SET_KEYBIT, KEY_KP6);
        ioctl(fd, UI_SET_KEYBIT, KEY_KPPLUS);
        ioctl(fd, UI_SET_KEYBIT, KEY_KP1);
        ioctl(fd, UI_SET_KEYBIT, KEY_KP2);
        ioctl(fd, UI_SET_KEYBIT, KEY_KP3);
        ioctl(fd, UI_SET_KEYBIT, KEY_KP0);
        ioctl(fd, UI_SET_KEYBIT, KEY_KPDOT);
        ioctl(fd, UI_SET_KEYBIT, KEY_ZENKAKUHANKAKU);
        ioctl(fd, UI_SET_KEYBIT, KEY_102ND);
        ioctl(fd, UI_SET_KEYBIT, KEY_F11);
        ioctl(fd, UI_SET_KEYBIT, KEY_F12);
        ioctl(fd, UI_SET_KEYBIT, KEY_RO);
        ioctl(fd, UI_SET_KEYBIT, KEY_KATAKANA);
        ioctl(fd, UI_SET_KEYBIT, KEY_HIRAGANA);
        ioctl(fd, UI_SET_KEYBIT, KEY_HENKAN);
        ioctl(fd, UI_SET_KEYBIT, KEY_KATAKANAHIRAGANA);
        ioctl(fd, UI_SET_KEYBIT, KEY_MUHENKAN);
        ioctl(fd, UI_SET_KEYBIT, KEY_KPJPCOMMA);
        ioctl(fd, UI_SET_KEYBIT, KEY_KPENTER);
        ioctl(fd, UI_SET_KEYBIT, KEY_RIGHTCTRL);
        ioctl(fd, UI_SET_KEYBIT, KEY_KPSLASH);
        ioctl(fd, UI_SET_KEYBIT, KEY_SYSRQ);
        ioctl(fd, UI_SET_KEYBIT, KEY_RIGHTALT);
        ioctl(fd, UI_SET_KEYBIT, KEY_LINEFEED);
        ioctl(fd, UI_SET_KEYBIT, KEY_HOME);
        ioctl(fd, UI_SET_KEYBIT, KEY_UP);
        ioctl(fd, UI_SET_KEYBIT, KEY_PAGEUP);
        ioctl(fd, UI_SET_KEYBIT, KEY_LEFT);
        ioctl(fd, UI_SET_KEYBIT, KEY_RIGHT);
        ioctl(fd, UI_SET_KEYBIT, KEY_END);
        ioctl(fd, UI_SET_KEYBIT, KEY_DOWN);
        ioctl(fd, UI_SET_KEYBIT, KEY_PAGEDOWN);
        ioctl(fd, UI_SET_KEYBIT, KEY_INSERT);
        ioctl(fd, UI_SET_KEYBIT, KEY_DELETE);
        ioctl(fd, UI_SET_KEYBIT, KEY_MACRO);
        ioctl(fd, UI_SET_KEYBIT, KEY_MUTE);
        ioctl(fd, UI_SET_KEYBIT, KEY_VOLUMEDOWN);
        ioctl(fd, UI_SET_KEYBIT, KEY_VOLUMEUP);
        ioctl(fd, UI_SET_KEYBIT, KEY_POWER);
        ioctl(fd, UI_SET_KEYBIT, KEY_KPEQUAL);
        ioctl(fd, UI_SET_KEYBIT, KEY_KPPLUSMINUS);
        ioctl(fd, UI_SET_KEYBIT, KEY_PAUSE);
        ioctl(fd, UI_SET_KEYBIT, KEY_SCALE);
        ioctl(fd, UI_SET_KEYBIT, KEY_KPCOMMA);
        ioctl(fd, UI_SET_KEYBIT, KEY_HANGEUL);
        ioctl(fd, UI_SET_KEYBIT, KEY_HANGUEL);
        ioctl(fd, UI_SET_KEYBIT, KEY_HANJA);
        ioctl(fd, UI_SET_KEYBIT, KEY_YEN);
        ioctl(fd, UI_SET_KEYBIT, KEY_LEFTMETA);
        ioctl(fd, UI_SET_KEYBIT, KEY_RIGHTMETA);
        ioctl(fd, UI_SET_KEYBIT, KEY_COMPOSE);
        ioctl(fd, UI_SET_KEYBIT, KEY_STOP);
        ioctl(fd, UI_SET_KEYBIT, KEY_AGAIN);
        ioctl(fd, UI_SET_KEYBIT, KEY_PROPS);
        ioctl(fd, UI_SET_KEYBIT, KEY_UNDO);
        ioctl(fd, UI_SET_KEYBIT, KEY_FRONT);
        ioctl(fd, UI_SET_KEYBIT, KEY_COPY);
        ioctl(fd, UI_SET_KEYBIT, KEY_OPEN);
        ioctl(fd, UI_SET_KEYBIT, KEY_PASTE);
        ioctl(fd, UI_SET_KEYBIT, KEY_FIND);
        ioctl(fd, UI_SET_KEYBIT, KEY_CUT);
        ioctl(fd, UI_SET_KEYBIT, KEY_HELP);
        ioctl(fd, UI_SET_KEYBIT, KEY_MENU);
        ioctl(fd, UI_SET_KEYBIT, KEY_CALC);
        ioctl(fd, UI_SET_KEYBIT, KEY_SETUP);
        ioctl(fd, UI_SET_KEYBIT, KEY_SLEEP);
        ioctl(fd, UI_SET_KEYBIT, KEY_WAKEUP);
        ioctl(fd, UI_SET_KEYBIT, KEY_FILE);
        ioctl(fd, UI_SET_KEYBIT, KEY_SENDFILE);
        ioctl(fd, UI_SET_KEYBIT, KEY_DELETEFILE);
        ioctl(fd, UI_SET_KEYBIT, KEY_XFER);
        ioctl(fd, UI_SET_KEYBIT, KEY_PROG1);
        ioctl(fd, UI_SET_KEYBIT, KEY_PROG2);
        ioctl(fd, UI_SET_KEYBIT, KEY_WWW);
        ioctl(fd, UI_SET_KEYBIT, KEY_MSDOS);
        ioctl(fd, UI_SET_KEYBIT, KEY_COFFEE);
        ioctl(fd, UI_SET_KEYBIT, KEY_SCREENLOCK);
        ioctl(fd, UI_SET_KEYBIT, KEY_ROTATE_DISPLAY);
        ioctl(fd, UI_SET_KEYBIT, KEY_DIRECTION);
        ioctl(fd, UI_SET_KEYBIT, KEY_CYCLEWINDOWS);
        ioctl(fd, UI_SET_KEYBIT, KEY_MAIL);
        ioctl(fd, UI_SET_KEYBIT, KEY_BOOKMARKS);
        ioctl(fd, UI_SET_KEYBIT, KEY_COMPUTER);
        ioctl(fd, UI_SET_KEYBIT, KEY_BACK);
        ioctl(fd, UI_SET_KEYBIT, KEY_FORWARD);
        ioctl(fd, UI_SET_KEYBIT, KEY_CLOSECD);
        ioctl(fd, UI_SET_KEYBIT, KEY_EJECTCD);
        ioctl(fd, UI_SET_KEYBIT, KEY_EJECTCLOSECD);
        ioctl(fd, UI_SET_KEYBIT, KEY_NEXTSONG);
        ioctl(fd, UI_SET_KEYBIT, KEY_PLAYPAUSE);
        ioctl(fd, UI_SET_KEYBIT, KEY_PREVIOUSSONG);
        ioctl(fd, UI_SET_KEYBIT, KEY_STOPCD);
        ioctl(fd, UI_SET_KEYBIT, KEY_RECORD);
        ioctl(fd, UI_SET_KEYBIT, KEY_REWIND);
        ioctl(fd, UI_SET_KEYBIT, KEY_PHONE);
        ioctl(fd, UI_SET_KEYBIT, KEY_ISO);
        ioctl(fd, UI_SET_KEYBIT, KEY_CONFIG);
        ioctl(fd, UI_SET_KEYBIT, KEY_HOMEPAGE);
        ioctl(fd, UI_SET_KEYBIT, KEY_REFRESH);
        ioctl(fd, UI_SET_KEYBIT, KEY_EXIT);
        ioctl(fd, UI_SET_KEYBIT, KEY_MOVE);
        ioctl(fd, UI_SET_KEYBIT, KEY_EDIT);
        ioctl(fd, UI_SET_KEYBIT, KEY_SCROLLUP);
        ioctl(fd, UI_SET_KEYBIT, KEY_SCROLLDOWN);
        ioctl(fd, UI_SET_KEYBIT, KEY_KPLEFTPAREN);
        ioctl(fd, UI_SET_KEYBIT, KEY_KPRIGHTPAREN);
        ioctl(fd, UI_SET_KEYBIT, KEY_NEW);
        ioctl(fd, UI_SET_KEYBIT, KEY_REDO);
        ioctl(fd, UI_SET_KEYBIT, KEY_F13);
        ioctl(fd, UI_SET_KEYBIT, KEY_F14);
        ioctl(fd, UI_SET_KEYBIT, KEY_F15);
        ioctl(fd, UI_SET_KEYBIT, KEY_F16);
        ioctl(fd, UI_SET_KEYBIT, KEY_F17);
        ioctl(fd, UI_SET_KEYBIT, KEY_F18);
        ioctl(fd, UI_SET_KEYBIT, KEY_F19);
        ioctl(fd, UI_SET_KEYBIT, KEY_F20);
        ioctl(fd, UI_SET_KEYBIT, KEY_F21);
        ioctl(fd, UI_SET_KEYBIT, KEY_F22);
        ioctl(fd, UI_SET_KEYBIT, KEY_F23);
        ioctl(fd, UI_SET_KEYBIT, KEY_F24);
        ioctl(fd, UI_SET_KEYBIT, KEY_PLAYCD);
        ioctl(fd, UI_SET_KEYBIT, KEY_PAUSECD);
        ioctl(fd, UI_SET_KEYBIT, KEY_PROG3);
        ioctl(fd, UI_SET_KEYBIT, KEY_PROG4);
        ioctl(fd, UI_SET_KEYBIT, KEY_DASHBOARD);
        ioctl(fd, UI_SET_KEYBIT, KEY_SUSPEND);
        ioctl(fd, UI_SET_KEYBIT, KEY_CLOSE);
        ioctl(fd, UI_SET_KEYBIT, KEY_PLAY);
        ioctl(fd, UI_SET_KEYBIT, KEY_FASTFORWARD);
        ioctl(fd, UI_SET_KEYBIT, KEY_BASSBOOST);
        ioctl(fd, UI_SET_KEYBIT, KEY_PRINT);
        ioctl(fd, UI_SET_KEYBIT, KEY_HP);
        ioctl(fd, UI_SET_KEYBIT, KEY_CAMERA);
        ioctl(fd, UI_SET_KEYBIT, KEY_SOUND);
        ioctl(fd, UI_SET_KEYBIT, KEY_QUESTION);
        ioctl(fd, UI_SET_KEYBIT, KEY_EMAIL);
        ioctl(fd, UI_SET_KEYBIT, KEY_CHAT);
        ioctl(fd, UI_SET_KEYBIT, KEY_SEARCH);
        ioctl(fd, UI_SET_KEYBIT, KEY_CONNECT);
        ioctl(fd, UI_SET_KEYBIT, KEY_FINANCE);
        ioctl(fd, UI_SET_KEYBIT, KEY_SPORT);
        ioctl(fd, UI_SET_KEYBIT, KEY_SHOP);
        ioctl(fd, UI_SET_KEYBIT, KEY_ALTERASE);
        ioctl(fd, UI_SET_KEYBIT, KEY_CANCEL);
        ioctl(fd, UI_SET_KEYBIT, KEY_BRIGHTNESSDOWN);
        ioctl(fd, UI_SET_KEYBIT, KEY_BRIGHTNESSUP);
        ioctl(fd, UI_SET_KEYBIT, KEY_MEDIA);
        ioctl(fd, UI_SET_KEYBIT, KEY_SWITCHVIDEOMODE);
        ioctl(fd, UI_SET_KEYBIT, KEY_KBDILLUMTOGGLE);
        ioctl(fd, UI_SET_KEYBIT, KEY_KBDILLUMDOWN);
        ioctl(fd, UI_SET_KEYBIT, KEY_KBDILLUMUP);
        ioctl(fd, UI_SET_KEYBIT, KEY_SEND);
        ioctl(fd, UI_SET_KEYBIT, KEY_REPLY);
        ioctl(fd, UI_SET_KEYBIT, KEY_FORWARDMAIL);
        ioctl(fd, UI_SET_KEYBIT, KEY_SAVE);
        ioctl(fd, UI_SET_KEYBIT, KEY_DOCUMENTS);
        ioctl(fd, UI_SET_KEYBIT, KEY_BATTERY);
        ioctl(fd, UI_SET_KEYBIT, KEY_BLUETOOTH);
        ioctl(fd, UI_SET_KEYBIT, KEY_WLAN);
        ioctl(fd, UI_SET_KEYBIT, KEY_UWB);
        ioctl(fd, UI_SET_KEYBIT, KEY_UNKNOWN);
        ioctl(fd, UI_SET_KEYBIT, KEY_VIDEO_NEXT);
        ioctl(fd, UI_SET_KEYBIT, KEY_VIDEO_PREV);
        ioctl(fd, UI_SET_KEYBIT, KEY_BRIGHTNESS_CYCLE);
        ioctl(fd, UI_SET_KEYBIT, KEY_BRIGHTNESS_AUTO);
        ioctl(fd, UI_SET_KEYBIT, KEY_BRIGHTNESS_ZERO);
        ioctl(fd, UI_SET_KEYBIT, KEY_DISPLAY_OFF);
        ioctl(fd, UI_SET_KEYBIT, KEY_WWAN);
        ioctl(fd, UI_SET_KEYBIT, KEY_WIMAX);
        ioctl(fd, UI_SET_KEYBIT, KEY_RFKILL);
        ioctl(fd, UI_SET_KEYBIT, KEY_MICMUTE);
        ioctl(fd, UI_SET_KEYBIT, BTN_MISC);
        ioctl(fd, UI_SET_KEYBIT, BTN_0);
        ioctl(fd, UI_SET_KEYBIT, BTN_1);
        ioctl(fd, UI_SET_KEYBIT, BTN_2);
        ioctl(fd, UI_SET_KEYBIT, BTN_3);
        ioctl(fd, UI_SET_KEYBIT, BTN_4);
        ioctl(fd, UI_SET_KEYBIT, BTN_5);
        ioctl(fd, UI_SET_KEYBIT, BTN_6);
        ioctl(fd, UI_SET_KEYBIT, BTN_7);
        ioctl(fd, UI_SET_KEYBIT, BTN_8);
        ioctl(fd, UI_SET_KEYBIT, BTN_9);
        ioctl(fd, UI_SET_KEYBIT, BTN_MOUSE);
        ioctl(fd, UI_SET_KEYBIT, BTN_LEFT);
        ioctl(fd, UI_SET_KEYBIT, BTN_RIGHT);
        ioctl(fd, UI_SET_KEYBIT, BTN_MIDDLE);
        ioctl(fd, UI_SET_KEYBIT, BTN_SIDE);
        ioctl(fd, UI_SET_KEYBIT, BTN_EXTRA);
        ioctl(fd, UI_SET_KEYBIT, BTN_FORWARD);
        ioctl(fd, UI_SET_KEYBIT, BTN_BACK);
        ioctl(fd, UI_SET_KEYBIT, BTN_TASK);
        ioctl(fd, UI_SET_KEYBIT, BTN_JOYSTICK);
        ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER);
        ioctl(fd, UI_SET_KEYBIT, BTN_THUMB);
        ioctl(fd, UI_SET_KEYBIT, BTN_THUMB2);
        ioctl(fd, UI_SET_KEYBIT, BTN_TOP);
        ioctl(fd, UI_SET_KEYBIT, BTN_TOP2);
        ioctl(fd, UI_SET_KEYBIT, BTN_PINKIE);
        ioctl(fd, UI_SET_KEYBIT, BTN_BASE);
        ioctl(fd, UI_SET_KEYBIT, BTN_BASE2);
        ioctl(fd, UI_SET_KEYBIT, BTN_BASE3);
        ioctl(fd, UI_SET_KEYBIT, BTN_BASE4);
        ioctl(fd, UI_SET_KEYBIT, BTN_BASE5);
        ioctl(fd, UI_SET_KEYBIT, BTN_BASE6);
        ioctl(fd, UI_SET_KEYBIT, BTN_DEAD);
        ioctl(fd, UI_SET_KEYBIT, BTN_GAMEPAD);
        ioctl(fd, UI_SET_KEYBIT, BTN_SOUTH);
        ioctl(fd, UI_SET_KEYBIT, BTN_A);
        ioctl(fd, UI_SET_KEYBIT, BTN_EAST);
        ioctl(fd, UI_SET_KEYBIT, BTN_B);
        ioctl(fd, UI_SET_KEYBIT, BTN_C);
        ioctl(fd, UI_SET_KEYBIT, BTN_NORTH);
        ioctl(fd, UI_SET_KEYBIT, BTN_X);
        ioctl(fd, UI_SET_KEYBIT, BTN_WEST);
        ioctl(fd, UI_SET_KEYBIT, BTN_Y);
        ioctl(fd, UI_SET_KEYBIT, BTN_Z);
        ioctl(fd, UI_SET_KEYBIT, BTN_TL);
        ioctl(fd, UI_SET_KEYBIT, BTN_TR);
        ioctl(fd, UI_SET_KEYBIT, BTN_TL2);
        ioctl(fd, UI_SET_KEYBIT, BTN_TR2);
        ioctl(fd, UI_SET_KEYBIT, BTN_SELECT);
        ioctl(fd, UI_SET_KEYBIT, BTN_START);
        ioctl(fd, UI_SET_KEYBIT, BTN_MODE);
        ioctl(fd, UI_SET_KEYBIT, BTN_THUMBL);
        ioctl(fd, UI_SET_KEYBIT, BTN_THUMBR);
        ioctl(fd, UI_SET_KEYBIT, BTN_DIGI);
        ioctl(fd, UI_SET_KEYBIT, BTN_TOOL_PEN);
        ioctl(fd, UI_SET_KEYBIT, BTN_TOOL_RUBBER);
        ioctl(fd, UI_SET_KEYBIT, BTN_TOOL_BRUSH);
        ioctl(fd, UI_SET_KEYBIT, BTN_TOOL_PENCIL);
        ioctl(fd, UI_SET_KEYBIT, BTN_TOOL_AIRBRUSH);
        ioctl(fd, UI_SET_KEYBIT, BTN_TOOL_FINGER);
        ioctl(fd, UI_SET_KEYBIT, BTN_TOOL_MOUSE);
        ioctl(fd, UI_SET_KEYBIT, BTN_TOOL_LENS);
        ioctl(fd, UI_SET_KEYBIT, BTN_TOOL_QUINTTAP);
        ioctl(fd, UI_SET_KEYBIT, BTN_STYLUS3);
        ioctl(fd, UI_SET_KEYBIT, BTN_TOUCH);
        ioctl(fd, UI_SET_KEYBIT, BTN_STYLUS);
        ioctl(fd, UI_SET_KEYBIT, BTN_STYLUS2);
        ioctl(fd, UI_SET_KEYBIT, BTN_TOOL_DOUBLETAP);
        ioctl(fd, UI_SET_KEYBIT, BTN_TOOL_TRIPLETAP);
        ioctl(fd, UI_SET_KEYBIT, BTN_TOOL_QUADTAP);
        ioctl(fd, UI_SET_KEYBIT, BTN_WHEEL);
        ioctl(fd, UI_SET_KEYBIT, BTN_GEAR_DOWN);
        ioctl(fd, UI_SET_KEYBIT, BTN_GEAR_UP);
        ioctl(fd, UI_SET_KEYBIT, KEY_OK);
        ioctl(fd, UI_SET_KEYBIT, KEY_SELECT);
        ioctl(fd, UI_SET_KEYBIT, KEY_GOTO);
        ioctl(fd, UI_SET_KEYBIT, KEY_CLEAR);
        ioctl(fd, UI_SET_KEYBIT, KEY_POWER2);
        ioctl(fd, UI_SET_KEYBIT, KEY_OPTION);
        ioctl(fd, UI_SET_KEYBIT, KEY_INFO);
        ioctl(fd, UI_SET_KEYBIT, KEY_TIME);
        ioctl(fd, UI_SET_KEYBIT, KEY_VENDOR);
        ioctl(fd, UI_SET_KEYBIT, KEY_ARCHIVE);
        ioctl(fd, UI_SET_KEYBIT, KEY_PROGRAM);
        ioctl(fd, UI_SET_KEYBIT, KEY_CHANNEL);
        ioctl(fd, UI_SET_KEYBIT, KEY_FAVORITES);
        ioctl(fd, UI_SET_KEYBIT, KEY_EPG);
        ioctl(fd, UI_SET_KEYBIT, KEY_PVR);
        ioctl(fd, UI_SET_KEYBIT, KEY_MHP);
        ioctl(fd, UI_SET_KEYBIT, KEY_LANGUAGE);
        ioctl(fd, UI_SET_KEYBIT, KEY_TITLE);
        ioctl(fd, UI_SET_KEYBIT, KEY_SUBTITLE);
        ioctl(fd, UI_SET_KEYBIT, KEY_ANGLE);
        ioctl(fd, UI_SET_KEYBIT, KEY_FULL_SCREEN);
        ioctl(fd, UI_SET_KEYBIT, KEY_ZOOM);
        ioctl(fd, UI_SET_KEYBIT, KEY_MODE);
        ioctl(fd, UI_SET_KEYBIT, KEY_KEYBOARD);
        ioctl(fd, UI_SET_KEYBIT, KEY_ASPECT_RATIO);
        ioctl(fd, UI_SET_KEYBIT, KEY_SCREEN);
        ioctl(fd, UI_SET_KEYBIT, KEY_PC);
        ioctl(fd, UI_SET_KEYBIT, KEY_TV);
        ioctl(fd, UI_SET_KEYBIT, KEY_TV2);
        ioctl(fd, UI_SET_KEYBIT, KEY_VCR);
        ioctl(fd, UI_SET_KEYBIT, KEY_VCR2);
        ioctl(fd, UI_SET_KEYBIT, KEY_SAT);
        ioctl(fd, UI_SET_KEYBIT, KEY_SAT2);
        ioctl(fd, UI_SET_KEYBIT, KEY_CD);
        ioctl(fd, UI_SET_KEYBIT, KEY_TAPE);
        ioctl(fd, UI_SET_KEYBIT, KEY_RADIO);
        ioctl(fd, UI_SET_KEYBIT, KEY_TUNER);
        ioctl(fd, UI_SET_KEYBIT, KEY_PLAYER);
        ioctl(fd, UI_SET_KEYBIT, KEY_TEXT);
        ioctl(fd, UI_SET_KEYBIT, KEY_DVD);
        ioctl(fd, UI_SET_KEYBIT, KEY_AUX);
        ioctl(fd, UI_SET_KEYBIT, KEY_MP3);
        ioctl(fd, UI_SET_KEYBIT, KEY_AUDIO);
        ioctl(fd, UI_SET_KEYBIT, KEY_VIDEO);
        ioctl(fd, UI_SET_KEYBIT, KEY_DIRECTORY);
        ioctl(fd, UI_SET_KEYBIT, KEY_LIST);
        ioctl(fd, UI_SET_KEYBIT, KEY_MEMO);
        ioctl(fd, UI_SET_KEYBIT, KEY_CALENDAR);
        ioctl(fd, UI_SET_KEYBIT, KEY_RED);
        ioctl(fd, UI_SET_KEYBIT, KEY_GREEN);
        ioctl(fd, UI_SET_KEYBIT, KEY_YELLOW);
        ioctl(fd, UI_SET_KEYBIT, KEY_BLUE);
        ioctl(fd, UI_SET_KEYBIT, KEY_CHANNELUP);
        ioctl(fd, UI_SET_KEYBIT, KEY_CHANNELDOWN);
        ioctl(fd, UI_SET_KEYBIT, KEY_FIRST);
        ioctl(fd, UI_SET_KEYBIT, KEY_LAST);
        ioctl(fd, UI_SET_KEYBIT, KEY_AB);
        ioctl(fd, UI_SET_KEYBIT, KEY_NEXT);
        ioctl(fd, UI_SET_KEYBIT, KEY_RESTART);
        ioctl(fd, UI_SET_KEYBIT, KEY_SLOW);
        ioctl(fd, UI_SET_KEYBIT, KEY_SHUFFLE);
        ioctl(fd, UI_SET_KEYBIT, KEY_BREAK);
        ioctl(fd, UI_SET_KEYBIT, KEY_PREVIOUS);
        ioctl(fd, UI_SET_KEYBIT, KEY_DIGITS);
        ioctl(fd, UI_SET_KEYBIT, KEY_TEEN);
        ioctl(fd, UI_SET_KEYBIT, KEY_TWEN);
        ioctl(fd, UI_SET_KEYBIT, KEY_VIDEOPHONE);
        ioctl(fd, UI_SET_KEYBIT, KEY_GAMES);
        ioctl(fd, UI_SET_KEYBIT, KEY_ZOOMIN);
        ioctl(fd, UI_SET_KEYBIT, KEY_ZOOMOUT);
        ioctl(fd, UI_SET_KEYBIT, KEY_ZOOMRESET);
        ioctl(fd, UI_SET_KEYBIT, KEY_WORDPROCESSOR);
        ioctl(fd, UI_SET_KEYBIT, KEY_EDITOR);
        ioctl(fd, UI_SET_KEYBIT, KEY_SPREADSHEET);
        ioctl(fd, UI_SET_KEYBIT, KEY_GRAPHICSEDITOR);
        ioctl(fd, UI_SET_KEYBIT, KEY_PRESENTATION);
        ioctl(fd, UI_SET_KEYBIT, KEY_DATABASE);
        ioctl(fd, UI_SET_KEYBIT, KEY_NEWS);
        ioctl(fd, UI_SET_KEYBIT, KEY_VOICEMAIL);
        ioctl(fd, UI_SET_KEYBIT, KEY_ADDRESSBOOK);
        ioctl(fd, UI_SET_KEYBIT, KEY_MESSENGER);
        ioctl(fd, UI_SET_KEYBIT, KEY_DISPLAYTOGGLE);
        ioctl(fd, UI_SET_KEYBIT, KEY_BRIGHTNESS_TOGGLE);
        ioctl(fd, UI_SET_KEYBIT, KEY_SPELLCHECK);
        ioctl(fd, UI_SET_KEYBIT, KEY_LOGOFF);
        ioctl(fd, UI_SET_KEYBIT, KEY_DOLLAR);
        ioctl(fd, UI_SET_KEYBIT, KEY_EURO);
        ioctl(fd, UI_SET_KEYBIT, KEY_FRAMEBACK);
        ioctl(fd, UI_SET_KEYBIT, KEY_FRAMEFORWARD);
        ioctl(fd, UI_SET_KEYBIT, KEY_CONTEXT_MENU);
        ioctl(fd, UI_SET_KEYBIT, KEY_MEDIA_REPEAT);
        ioctl(fd, UI_SET_KEYBIT, KEY_10CHANNELSUP);
        ioctl(fd, UI_SET_KEYBIT, KEY_10CHANNELSDOWN);
        ioctl(fd, UI_SET_KEYBIT, KEY_IMAGES);
        ioctl(fd, UI_SET_KEYBIT, KEY_DEL_EOL);
        ioctl(fd, UI_SET_KEYBIT, KEY_DEL_EOS);
        ioctl(fd, UI_SET_KEYBIT, KEY_INS_LINE);
        ioctl(fd, UI_SET_KEYBIT, KEY_DEL_LINE);
        ioctl(fd, UI_SET_KEYBIT, KEY_FN);
        ioctl(fd, UI_SET_KEYBIT, KEY_FN_ESC);
        ioctl(fd, UI_SET_KEYBIT, KEY_FN_F1);
        ioctl(fd, UI_SET_KEYBIT, KEY_FN_F2);
        ioctl(fd, UI_SET_KEYBIT, KEY_FN_F3);
        ioctl(fd, UI_SET_KEYBIT, KEY_FN_F4);
        ioctl(fd, UI_SET_KEYBIT, KEY_FN_F5);
        ioctl(fd, UI_SET_KEYBIT, KEY_FN_F6);
        ioctl(fd, UI_SET_KEYBIT, KEY_FN_F7);
        ioctl(fd, UI_SET_KEYBIT, KEY_FN_F8);
        ioctl(fd, UI_SET_KEYBIT, KEY_FN_F9);
        ioctl(fd, UI_SET_KEYBIT, KEY_FN_F10);
        ioctl(fd, UI_SET_KEYBIT, KEY_FN_F11);
        ioctl(fd, UI_SET_KEYBIT, KEY_FN_F12);
        ioctl(fd, UI_SET_KEYBIT, KEY_FN_1);
        ioctl(fd, UI_SET_KEYBIT, KEY_FN_2);
        ioctl(fd, UI_SET_KEYBIT, KEY_FN_D);
        ioctl(fd, UI_SET_KEYBIT, KEY_FN_E);
        ioctl(fd, UI_SET_KEYBIT, KEY_FN_F);
        ioctl(fd, UI_SET_KEYBIT, KEY_FN_S);
        ioctl(fd, UI_SET_KEYBIT, KEY_FN_B);
        ioctl(fd, UI_SET_KEYBIT, KEY_BRL_DOT1);
        ioctl(fd, UI_SET_KEYBIT, KEY_BRL_DOT2);
        ioctl(fd, UI_SET_KEYBIT, KEY_BRL_DOT3);
        ioctl(fd, UI_SET_KEYBIT, KEY_BRL_DOT4);
        ioctl(fd, UI_SET_KEYBIT, KEY_BRL_DOT5);
        ioctl(fd, UI_SET_KEYBIT, KEY_BRL_DOT6);
        ioctl(fd, UI_SET_KEYBIT, KEY_BRL_DOT7);
        ioctl(fd, UI_SET_KEYBIT, KEY_BRL_DOT8);
        ioctl(fd, UI_SET_KEYBIT, KEY_BRL_DOT9);
        ioctl(fd, UI_SET_KEYBIT, KEY_BRL_DOT10);
        ioctl(fd, UI_SET_KEYBIT, KEY_NUMERIC_0);
        ioctl(fd, UI_SET_KEYBIT, KEY_NUMERIC_1);
        ioctl(fd, UI_SET_KEYBIT, KEY_NUMERIC_2);
        ioctl(fd, UI_SET_KEYBIT, KEY_NUMERIC_3);
        ioctl(fd, UI_SET_KEYBIT, KEY_NUMERIC_4);
        ioctl(fd, UI_SET_KEYBIT, KEY_NUMERIC_5);
        ioctl(fd, UI_SET_KEYBIT, KEY_NUMERIC_6);
        ioctl(fd, UI_SET_KEYBIT, KEY_NUMERIC_7);
        ioctl(fd, UI_SET_KEYBIT, KEY_NUMERIC_8);
        ioctl(fd, UI_SET_KEYBIT, KEY_NUMERIC_9);
        ioctl(fd, UI_SET_KEYBIT, KEY_NUMERIC_STAR);
        ioctl(fd, UI_SET_KEYBIT, KEY_NUMERIC_POUND);
        ioctl(fd, UI_SET_KEYBIT, KEY_NUMERIC_A);
        ioctl(fd, UI_SET_KEYBIT, KEY_NUMERIC_B);
        ioctl(fd, UI_SET_KEYBIT, KEY_NUMERIC_C);
        ioctl(fd, UI_SET_KEYBIT, KEY_NUMERIC_D);
        ioctl(fd, UI_SET_KEYBIT, KEY_CAMERA_FOCUS);
        ioctl(fd, UI_SET_KEYBIT, KEY_WPS_BUTTON);
        ioctl(fd, UI_SET_KEYBIT, KEY_TOUCHPAD_TOGGLE);
        ioctl(fd, UI_SET_KEYBIT, KEY_TOUCHPAD_ON);
        ioctl(fd, UI_SET_KEYBIT, KEY_TOUCHPAD_OFF);
        ioctl(fd, UI_SET_KEYBIT, KEY_CAMERA_ZOOMIN);
        ioctl(fd, UI_SET_KEYBIT, KEY_CAMERA_ZOOMOUT);
        ioctl(fd, UI_SET_KEYBIT, KEY_CAMERA_UP);
        ioctl(fd, UI_SET_KEYBIT, KEY_CAMERA_DOWN);
        ioctl(fd, UI_SET_KEYBIT, KEY_CAMERA_LEFT);
        ioctl(fd, UI_SET_KEYBIT, KEY_CAMERA_RIGHT);
        ioctl(fd, UI_SET_KEYBIT, KEY_ATTENDANT_ON);
        ioctl(fd, UI_SET_KEYBIT, KEY_ATTENDANT_OFF);
        ioctl(fd, UI_SET_KEYBIT, KEY_ATTENDANT_TOGGLE);
        ioctl(fd, UI_SET_KEYBIT, KEY_LIGHTS_TOGGLE);
        ioctl(fd, UI_SET_KEYBIT, BTN_DPAD_UP);
        ioctl(fd, UI_SET_KEYBIT, BTN_DPAD_DOWN);
        ioctl(fd, UI_SET_KEYBIT, BTN_DPAD_LEFT);
        ioctl(fd, UI_SET_KEYBIT, BTN_DPAD_RIGHT);
        ioctl(fd, UI_SET_KEYBIT, KEY_ALS_TOGGLE);
        ioctl(fd, UI_SET_KEYBIT, KEY_ROTATE_LOCK_TOGGLE);
        ioctl(fd, UI_SET_KEYBIT, KEY_BUTTONCONFIG);
        ioctl(fd, UI_SET_KEYBIT, KEY_TASKMANAGER);
        ioctl(fd, UI_SET_KEYBIT, KEY_JOURNAL);
        ioctl(fd, UI_SET_KEYBIT, KEY_CONTROLPANEL);
        ioctl(fd, UI_SET_KEYBIT, KEY_APPSELECT);
        ioctl(fd, UI_SET_KEYBIT, KEY_SCREENSAVER);
        ioctl(fd, UI_SET_KEYBIT, KEY_VOICECOMMAND);
        ioctl(fd, UI_SET_KEYBIT, KEY_ASSISTANT);
        ioctl(fd, UI_SET_KEYBIT, KEY_KBD_LAYOUT_NEXT);
        ioctl(fd, UI_SET_KEYBIT, KEY_BRIGHTNESS_MIN);
        ioctl(fd, UI_SET_KEYBIT, KEY_BRIGHTNESS_MAX);
        ioctl(fd, UI_SET_KEYBIT, KEY_KBDINPUTASSIST_PREV);
        ioctl(fd, UI_SET_KEYBIT, KEY_KBDINPUTASSIST_NEXT);
        ioctl(fd, UI_SET_KEYBIT, KEY_KBDINPUTASSIST_PREVGROUP);
        ioctl(fd, UI_SET_KEYBIT, KEY_KBDINPUTASSIST_NEXTGROUP);
        ioctl(fd, UI_SET_KEYBIT, KEY_KBDINPUTASSIST_ACCEPT);
        ioctl(fd, UI_SET_KEYBIT, KEY_KBDINPUTASSIST_CANCEL);
        ioctl(fd, UI_SET_KEYBIT, KEY_RIGHT_UP);
        ioctl(fd, UI_SET_KEYBIT, KEY_RIGHT_DOWN);
        ioctl(fd, UI_SET_KEYBIT, KEY_LEFT_UP);
        ioctl(fd, UI_SET_KEYBIT, KEY_LEFT_DOWN);
        ioctl(fd, UI_SET_KEYBIT, KEY_ROOT_MENU);
        ioctl(fd, UI_SET_KEYBIT, KEY_MEDIA_TOP_MENU);
        ioctl(fd, UI_SET_KEYBIT, KEY_NUMERIC_11);
        ioctl(fd, UI_SET_KEYBIT, KEY_NUMERIC_12);
        ioctl(fd, UI_SET_KEYBIT, KEY_AUDIO_DESC);
        ioctl(fd, UI_SET_KEYBIT, KEY_3D_MODE);
        ioctl(fd, UI_SET_KEYBIT, KEY_NEXT_FAVORITE);
        ioctl(fd, UI_SET_KEYBIT, KEY_STOP_RECORD);
        ioctl(fd, UI_SET_KEYBIT, KEY_PAUSE_RECORD);
        ioctl(fd, UI_SET_KEYBIT, KEY_VOD);
        ioctl(fd, UI_SET_KEYBIT, KEY_UNMUTE);
        ioctl(fd, UI_SET_KEYBIT, KEY_FASTREVERSE);
        ioctl(fd, UI_SET_KEYBIT, KEY_SLOWREVERSE);
        ioctl(fd, UI_SET_KEYBIT, KEY_DATA);
        ioctl(fd, UI_SET_KEYBIT, KEY_ONSCREEN_KEYBOARD);
        ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY);
        ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY1);
        ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY2);
        ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY3);
        ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY4);
        ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY5);
        ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY6);
        ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY7);
        ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY8);
        ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY9);
        ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY10);
        ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY11);
        ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY12);
        ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY13);
        ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY14);
        ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY15);
        ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY16);
        ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY17);
        ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY18);
        ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY19);
        ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY20);
        ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY21);
        ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY22);
        ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY23);
        ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY24);
        ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY25);
        ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY26);
        ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY27);
        ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY28);
        ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY29);
        ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY30);
        ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY31);
        ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY32);
        ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY33);
        ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY34);
        ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY35);
        ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY36);
        ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY37);
        ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY38);
        ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY39);
        ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY40);
        for(int i = LED_NUML; i <= LED_MISC; i++){
            if(ioctl(fd, UI_SET_LEDBIT, i)){
                perror("ioctl set_ledbit");
                exit(-1);
            }
        }
        ioctl(fd, UI_SET_KEYBIT, KEY_ZOOM);
        memset(&usetup, 0, sizeof(usetup));
        usetup.id.bustype = BUS_USB;
        usetup.id.vendor = 0x0000;
        usetup.id.product = 0xFFFF;
        strcpy(usetup.name, "Oxide Virtual Keyboard");

        ioctl(fd, UI_DEV_SETUP, &usetup);
        ioctl(fd, UI_DEV_CREATE);
    }
    reloadDevices();
    deviceSettings.onKeyboardAttachedChanged([this]{ reloadDevices(); });
}

KeyboardHandler::~KeyboardHandler(){
    if(fd != -1){
        ioctl(fd, UI_DEV_DESTROY);
        close(fd);
    }
}

void KeyboardHandler::flood(){
    O_DEBUG("Flooding");
    for(int i = 0; i < 512 * 8; i+=4){
        writeEvent(EV_KEY, KEY_ROTATE_LOCK_TOGGLE, 1);
        writeEvent(EV_SYN, SYN_REPORT, 0);
        writeEvent(EV_KEY, KEY_ROTATE_LOCK_TOGGLE, 0);
        writeEvent(EV_SYN, SYN_REPORT, 0);
    }
}

void KeyboardHandler::writeEvent(int type, int code, int val){
    input_event ie;
    ie.type = type;
    ie.code = code;
    ie.value = val;
    // timestamp values below are ignored
    ie.time.tv_sec = 0;
    ie.time.tv_usec = 0;
    ::write(fd, &ie, sizeof(input_event));
}

void KeyboardHandler::keyEvent(int code, int value){
    writeEvent(EV_KEY, code, value);
    writeEvent(EV_SYN, SYN_REPORT, 0);
}

bool KeyboardHandler::hasDevice(event_device device){
    for(auto keyboard : devices){
        if(device.device.c_str() == keyboard->path()){
            return true;
        }
    }
    return false;
}

void KeyboardHandler::reloadDevices(){
    O_DEBUG("Reloading keyboards");
    for(auto device : deviceSettings.keyboards()){
        if(device.device == deviceSettings.getButtonsDevicePath()){
            continue;
        }
        if(!hasDevice(device) && device.fd != -1){
            auto keyboard = new KeyboardDevice(this, device);
            O_DEBUG(keyboard->name() << "added");
            devices.append(keyboard);
            keyboard->readEvents();
        }
    }
    QMutableListIterator<KeyboardDevice*> i(devices);
    while(i.hasNext()){
        KeyboardDevice* device = i.next();
        if(device->exists()){
            continue;
        }
        O_DEBUG(device->name() << "removed");
        i.remove();
        delete device;
    }
}
