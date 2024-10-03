#include <QCommandLineParser>
#include <QFile>
#include <QList>
#include <QSocketNotifier>
#include <QDebug>

#include <stdio.h>
#include <iostream>
#include <linux/input.h>

#include <liboxide.h>
#include <liboxide/event_device.h>

using namespace Oxide;
using namespace Oxide::Sentry;

event_device* device = nullptr;

#define check(eventType, name, eventCode) \
    if(code == name){ \
        device->write(eventType, eventCode, value); \
        return true; \
    }
#define syn(eventCode) check(EV_SYN, #eventCode, eventCode)
#define abs(eventCode) check(EV_ABS, #eventCode, eventCode)
#define rel(eventCode) check(EV_REL, #eventCode, eventCode)
#define key(eventCode) check(EV_KEY, #eventCode, eventCode)
#define msc(eventCode) check(EV_MSC, #eventCode, eventCode)
#define sw(eventCode)  check(EV_SW,  #eventCode, eventCode)
#define led(eventCode) check(EV_LED, #eventCode, eventCode)
#define snd(eventCode) check(EV_SND, #eventCode, eventCode)
#define rep(eventCode) check(EV_REP, #eventCode, eventCode)

// I pulled most of the codes from https://github.com/torvalds/linux/blob/master/include/uapi/linux/input-event-codes.h
// I've left EV_FF, EV_PWR, and EV_FF_STATUS out since they don't seem to really apply to the reMarkable

bool process_EV_SYN(const QStringList& args){
    auto code = args.at(1).toStdString();
    int value = 0;
    syn(SYN_REPORT);
    syn(SYN_MT_REPORT);
    syn(SYN_DROPPED);
    syn(SYN_CONFIG);
    qDebug() << "Unknown EV_SYN event code:" << code.c_str();
    return false;
}

bool process_EV_ABS(const QStringList& args){
    auto code = args.at(1).toStdString();
    int value = 0;
    if(args.count() > 2){
        bool ok;
        QString arg = args.at(2);
        value = arg.toInt(&ok);
        if(!ok){
            qDebug() << "Third argument must be a valid integer:" << args;
            return false;
        }
    }
    abs(ABS_X);
    abs(ABS_Y);
    abs(ABS_Z);
    abs(ABS_RX);
    abs(ABS_RY);
    abs(ABS_RZ);
    abs(ABS_THROTTLE);
    abs(ABS_RUDDER);
    abs(ABS_WHEEL);
    abs(ABS_GAS);
    abs(ABS_BRAKE);
    abs(ABS_HAT0X);
    abs(ABS_HAT0Y);
    abs(ABS_HAT1X);
    abs(ABS_HAT1Y);
    abs(ABS_HAT2X);
    abs(ABS_HAT2Y);
    abs(ABS_HAT3X);
    abs(ABS_HAT3Y);
    abs(ABS_PRESSURE);
    abs(ABS_DISTANCE);
    abs(ABS_TILT_X);
    abs(ABS_TILT_Y);
    abs(ABS_TOOL_WIDTH);
    abs(ABS_VOLUME);
    // abs(ABS_PROFILE);
    abs(ABS_MISC);
    abs(ABS_RESERVED);
    abs(ABS_MT_SLOT);
    abs(ABS_MT_TOUCH_MAJOR);
    abs(ABS_MT_TOUCH_MINOR);
    abs(ABS_MT_WIDTH_MAJOR);
    abs(ABS_MT_WIDTH_MINOR);
    abs(ABS_MT_ORIENTATION);
    abs(ABS_MT_POSITION_X);
    abs(ABS_MT_POSITION_Y);
    abs(ABS_MT_TOOL_TYPE);
    abs(ABS_MT_BLOB_ID);
    abs(ABS_MT_TRACKING_ID);
    abs(ABS_MT_PRESSURE);
    abs(ABS_MT_DISTANCE);
    abs(ABS_MT_TOOL_X);
    abs(ABS_MT_TOOL_Y);
    abs(ABS_MAX);
    abs(ABS_CNT);
    qDebug() << "Unknown EV_ABS event code:" << code.c_str();
    return false;
}

bool process_EV_REL(const QStringList& args){
    auto code = args.at(1).toStdString();
    int value = 0;
    if(args.count() > 2){
        bool ok;
        QString arg = args.at(2);
        value = arg.toInt(&ok);
        if(!ok){
            qDebug() << "Third argument must be a valid integer:" << args;
            return false;
        }
    }
    rel(REL_X);
    rel(REL_Y);
    rel(REL_Z);
    rel(REL_RX);
    rel(REL_RY);
    rel(REL_RZ);
    rel(REL_HWHEEL);
    rel(REL_DIAL);
    rel(REL_WHEEL);
    rel(REL_MISC);
    rel(REL_RESERVED);
    rel(REL_WHEEL_HI_RES);
    rel(REL_HWHEEL_HI_RES);
    rel(REL_MAX);
    rel(REL_CNT);
    qDebug() << "Unknown EV_REL event code:" << code.c_str();
    return false;
}

bool process_EV_KEY(const QStringList& args){
    auto code = args.at(1).toStdString();
    int value = 0;
    if(args.count() > 2){
        bool ok;
        QString arg = args.at(2);
        value = arg.toInt(&ok);
        if(!ok){
            qDebug() << "Third argument must be a valid integer:" << args;
            return false;
        }
    }
    key(KEY_RESERVED);
    key(KEY_ESC);
    key(KEY_1);
    key(KEY_2);
    key(KEY_3);
    key(KEY_4);
    key(KEY_5);
    key(KEY_6);
    key(KEY_7);
    key(KEY_8);
    key(KEY_9);
    key(KEY_0);
    key(KEY_MINUS);
    key(KEY_EQUAL);
    key(KEY_BACKSPACE);
    key(KEY_TAB);
    key(KEY_Q);
    key(KEY_W);
    key(KEY_E);
    key(KEY_R);
    key(KEY_T);
    key(KEY_Y);
    key(KEY_U);
    key(KEY_I);
    key(KEY_O);
    key(KEY_P);
    key(KEY_LEFTBRACE);
    key(KEY_RIGHTBRACE);
    key(KEY_ENTER);
    key(KEY_LEFTCTRL);
    key(KEY_A);
    key(KEY_S);
    key(KEY_D);
    key(KEY_F);
    key(KEY_G);
    key(KEY_H);
    key(KEY_J);
    key(KEY_K);
    key(KEY_L);
    key(KEY_SEMICOLON);
    key(KEY_APOSTROPHE);
    key(KEY_GRAVE);
    key(KEY_LEFTSHIFT);
    key(KEY_BACKSLASH);
    key(KEY_Z);
    key(KEY_X);
    key(KEY_C);
    key(KEY_V);
    key(KEY_B);
    key(KEY_N);
    key(KEY_M);
    key(KEY_COMMA);
    key(KEY_DOT);
    key(KEY_SLASH);
    key(KEY_RIGHTSHIFT);
    key(KEY_KPASTERISK);
    key(KEY_LEFTALT);
    key(KEY_SPACE);
    key(KEY_CAPSLOCK);
    key(KEY_F1);
    key(KEY_F2);
    key(KEY_F3);
    key(KEY_F4);
    key(KEY_F5);
    key(KEY_F6);
    key(KEY_F7);
    key(KEY_F8);
    key(KEY_F9);
    key(KEY_F10);
    key(KEY_NUMLOCK);
    key(KEY_SCROLLLOCK);
    key(KEY_KP7);
    key(KEY_KP8);
    key(KEY_KP9);
    key(KEY_KPMINUS);
    key(KEY_KP4);
    key(KEY_KP5);
    key(KEY_KP6);
    key(KEY_KPPLUS);
    key(KEY_KP1);
    key(KEY_KP2);
    key(KEY_KP3);
    key(KEY_KP0);
    key(KEY_KPDOT);
    key(KEY_ZENKAKUHANKAKU);
    key(KEY_102ND);
    key(KEY_F11);
    key(KEY_F12);
    key(KEY_RO);
    key(KEY_KATAKANA);
    key(KEY_HIRAGANA);
    key(KEY_HENKAN);
    key(KEY_KATAKANAHIRAGANA);
    key(KEY_MUHENKAN);
    key(KEY_KPJPCOMMA);
    key(KEY_KPENTER);
    key(KEY_RIGHTCTRL);
    key(KEY_KPSLASH);
    key(KEY_SYSRQ);
    key(KEY_RIGHTALT);
    key(KEY_LINEFEED);
    key(KEY_HOME);
    key(KEY_UP);
    key(KEY_PAGEUP);
    key(KEY_LEFT);
    key(KEY_RIGHT);
    key(KEY_END);
    key(KEY_DOWN);
    key(KEY_PAGEDOWN);
    key(KEY_INSERT);
    key(KEY_DELETE);
    key(KEY_MACRO);
    key(KEY_MUTE);
    key(KEY_VOLUMEDOWN);
    key(KEY_VOLUMEUP);
    key(KEY_POWER);
    key(KEY_KPEQUAL);
    key(KEY_KPPLUSMINUS);
    key(KEY_PAUSE);
    key(KEY_SCALE);
    key(KEY_KPCOMMA);
    key(KEY_HANGEUL);
    key(KEY_HANGUEL);
    key(KEY_HANJA);
    key(KEY_YEN);
    key(KEY_LEFTMETA);
    key(KEY_RIGHTMETA);
    key(KEY_COMPOSE);
    key(KEY_STOP);
    key(KEY_AGAIN);
    key(KEY_PROPS);
    key(KEY_UNDO);
    key(KEY_FRONT);
    key(KEY_COPY);
    key(KEY_OPEN);
    key(KEY_PASTE);
    key(KEY_FIND);
    key(KEY_CUT);
    key(KEY_HELP);
    key(KEY_MENU);
    key(KEY_CALC);
    key(KEY_SETUP);
    key(KEY_SLEEP);
    key(KEY_WAKEUP);
    key(KEY_FILE);
    key(KEY_SENDFILE);
    key(KEY_DELETEFILE);
    key(KEY_XFER);
    key(KEY_PROG1);
    key(KEY_PROG2);
    key(KEY_WWW);
    key(KEY_MSDOS);
    key(KEY_COFFEE);
    key(KEY_SCREENLOCK);
    key(KEY_ROTATE_DISPLAY);
    key(KEY_DIRECTION);
    key(KEY_CYCLEWINDOWS);
    key(KEY_MAIL);
    key(KEY_BOOKMARKS);
    key(KEY_COMPUTER);
    key(KEY_BACK);
    key(KEY_FORWARD);
    key(KEY_CLOSECD);
    key(KEY_EJECTCD);
    key(KEY_EJECTCLOSECD);
    key(KEY_NEXTSONG);
    key(KEY_PLAYPAUSE);
    key(KEY_PREVIOUSSONG);
    key(KEY_STOPCD);
    key(KEY_RECORD);
    key(KEY_REWIND);
    key(KEY_PHONE);
    key(KEY_ISO);
    key(KEY_CONFIG);
    key(KEY_HOMEPAGE);
    key(KEY_REFRESH);
    key(KEY_EXIT);
    key(KEY_MOVE);
    key(KEY_EDIT);
    key(KEY_SCROLLUP);
    key(KEY_SCROLLDOWN);
    key(KEY_KPLEFTPAREN);
    key(KEY_KPRIGHTPAREN);
    key(KEY_NEW);
    key(KEY_REDO);
    key(KEY_F13);
    key(KEY_F14);
    key(KEY_F15);
    key(KEY_F16);
    key(KEY_F17);
    key(KEY_F18);
    key(KEY_F19);
    key(KEY_F20);
    key(KEY_F21);
    key(KEY_F22);
    key(KEY_F23);
    key(KEY_F24);
    key(KEY_PLAYCD);
    key(KEY_PAUSECD);
    key(KEY_PROG3);
    key(KEY_PROG4);
    // key(KEY_ALL_APPLICATIONS);
    key(KEY_DASHBOARD);
    key(KEY_SUSPEND);
    key(KEY_CLOSE);
    key(KEY_PLAY);
    key(KEY_FASTFORWARD);
    key(KEY_BASSBOOST);
    key(KEY_PRINT);
    key(KEY_HP);
    key(KEY_CAMERA);
    key(KEY_SOUND);
    key(KEY_QUESTION);
    key(KEY_EMAIL);
    key(KEY_CHAT);
    key(KEY_SEARCH);
    key(KEY_CONNECT);
    key(KEY_FINANCE);
    key(KEY_SPORT);
    key(KEY_SHOP);
    key(KEY_ALTERASE);
    key(KEY_CANCEL);
    key(KEY_BRIGHTNESSDOWN);
    key(KEY_BRIGHTNESSUP);
    key(KEY_MEDIA);
    key(KEY_SWITCHVIDEOMODE);
    key(KEY_KBDILLUMTOGGLE);
    key(KEY_KBDILLUMDOWN);
    key(KEY_KBDILLUMUP);
    key(KEY_SEND);
    key(KEY_REPLY);
    key(KEY_FORWARDMAIL);
    key(KEY_SAVE);
    key(KEY_DOCUMENTS);
    key(KEY_BATTERY);
    key(KEY_BLUETOOTH);
    key(KEY_WLAN);
    key(KEY_UWB);
    key(KEY_UNKNOWN);
    key(KEY_VIDEO_NEXT);
    key(KEY_VIDEO_PREV);
    key(KEY_BRIGHTNESS_CYCLE);
    key(KEY_BRIGHTNESS_AUTO);
    key(KEY_BRIGHTNESS_ZERO);
    key(KEY_DISPLAY_OFF);
    key(KEY_WWAN);
    key(KEY_WIMAX);
    key(KEY_RFKILL);
    key(KEY_MICMUTE);
    key(KEY_OK);
    key(KEY_SELECT);
    key(KEY_GOTO);
    key(KEY_CLEAR);
    key(KEY_POWER2);
    key(KEY_OPTION);
    key(KEY_INFO);
    key(KEY_TIME);
    key(KEY_VENDOR);
    key(KEY_ARCHIVE);
    key(KEY_PROGRAM);
    key(KEY_CHANNEL);
    key(KEY_FAVORITES);
    key(KEY_EPG);
    key(KEY_PVR);
    key(KEY_MHP);
    key(KEY_LANGUAGE);
    key(KEY_TITLE);
    key(KEY_SUBTITLE);
    key(KEY_ANGLE);
    key(KEY_FULL_SCREEN);
    key(KEY_ZOOM);
    key(KEY_MODE);
    key(KEY_KEYBOARD);
    key(KEY_ASPECT_RATIO);
    key(KEY_SCREEN);
    key(KEY_PC);
    key(KEY_TV);
    key(KEY_TV2);
    key(KEY_VCR);
    key(KEY_VCR2);
    key(KEY_SAT);
    key(KEY_SAT2);
    key(KEY_CD);
    key(KEY_TAPE);
    key(KEY_RADIO);
    key(KEY_TUNER);
    key(KEY_PLAYER);
    key(KEY_TEXT);
    key(KEY_DVD);
    key(KEY_AUX);
    key(KEY_MP3);
    key(KEY_AUDIO);
    key(KEY_VIDEO);
    key(KEY_DIRECTORY);
    key(KEY_LIST);
    key(KEY_MEMO);
    key(KEY_CALENDAR);
    key(KEY_RED);
    key(KEY_GREEN);
    key(KEY_YELLOW);
    key(KEY_BLUE);
    key(KEY_CHANNELUP);
    key(KEY_CHANNELDOWN);
    key(KEY_FIRST);
    key(KEY_LAST);
    key(KEY_AB);
    key(KEY_NEXT);
    key(KEY_RESTART);
    key(KEY_SLOW);
    key(KEY_SHUFFLE);
    key(KEY_BREAK);
    key(KEY_PREVIOUS);
    key(KEY_DIGITS);
    key(KEY_TEEN);
    key(KEY_TWEN);
    key(KEY_VIDEOPHONE);
    key(KEY_GAMES);
    key(KEY_ZOOMIN);
    key(KEY_ZOOMOUT);
    key(KEY_ZOOMRESET);
    key(KEY_WORDPROCESSOR);
    key(KEY_EDITOR);
    key(KEY_SPREADSHEET);
    key(KEY_GRAPHICSEDITOR);
    key(KEY_PRESENTATION);
    key(KEY_DATABASE);
    key(KEY_NEWS);
    key(KEY_VOICEMAIL);
    key(KEY_ADDRESSBOOK);
    key(KEY_MESSENGER);
    key(KEY_DISPLAYTOGGLE);
    key(KEY_BRIGHTNESS_TOGGLE);
    key(KEY_SPELLCHECK);
    key(KEY_LOGOFF);
    key(KEY_DOLLAR);
    key(KEY_EURO);
    key(KEY_FRAMEBACK);
    key(KEY_FRAMEFORWARD);
    key(KEY_CONTEXT_MENU);
    key(KEY_MEDIA_REPEAT);
    key(KEY_10CHANNELSUP);
    key(KEY_10CHANNELSDOWN);
    key(KEY_IMAGES);
    // key(KEY_NOTIFICATION_CENTER);
    // key(KEY_PICKUP_PHONE);
    // key(KEY_HANGUP_PHONE);
    key(KEY_DEL_EOL);
    key(KEY_DEL_EOS);
    key(KEY_INS_LINE);
    key(KEY_DEL_LINE);
    key(KEY_FN);
    key(KEY_FN_ESC);
    key(KEY_FN_F1);
    key(KEY_FN_F2);
    key(KEY_FN_F3);
    key(KEY_FN_F4);
    key(KEY_FN_F5);
    key(KEY_FN_F6);
    key(KEY_FN_F7);
    key(KEY_FN_F8);
    key(KEY_FN_F9);
    key(KEY_FN_F10);
    key(KEY_FN_F11);
    key(KEY_FN_F12);
    key(KEY_FN_1);
    key(KEY_FN_2);
    key(KEY_FN_D);
    key(KEY_FN_E);
    key(KEY_FN_F);
    key(KEY_FN_S);
    key(KEY_FN_B);
    // key(KEY_FN_RIGHT_SHIFT);
    key(KEY_BRL_DOT1);
    key(KEY_BRL_DOT2);
    key(KEY_BRL_DOT3);
    key(KEY_BRL_DOT4);
    key(KEY_BRL_DOT5);
    key(KEY_BRL_DOT6);
    key(KEY_BRL_DOT7);
    key(KEY_BRL_DOT8);
    key(KEY_BRL_DOT9);
    key(KEY_BRL_DOT10);
    key(KEY_NUMERIC_0);
    key(KEY_NUMERIC_1);
    key(KEY_NUMERIC_2);
    key(KEY_NUMERIC_3);
    key(KEY_NUMERIC_4);
    key(KEY_NUMERIC_5);
    key(KEY_NUMERIC_6);
    key(KEY_NUMERIC_7);
    key(KEY_NUMERIC_8);
    key(KEY_NUMERIC_9);
    key(KEY_NUMERIC_STAR);
    key(KEY_NUMERIC_POUND);
    key(KEY_NUMERIC_A);
    key(KEY_NUMERIC_B);
    key(KEY_NUMERIC_C);
    key(KEY_NUMERIC_D);
    key(KEY_CAMERA_FOCUS);
    key(KEY_WPS_BUTTON);
    key(KEY_TOUCHPAD_TOGGLE);
    key(KEY_TOUCHPAD_ON);
    key(KEY_TOUCHPAD_OFF);
    key(KEY_CAMERA_ZOOMIN);
    key(KEY_CAMERA_ZOOMOUT);
    key(KEY_CAMERA_UP);
    key(KEY_CAMERA_DOWN);
    key(KEY_CAMERA_LEFT);
    key(KEY_CAMERA_RIGHT);
    key(KEY_ATTENDANT_ON);
    key(KEY_ATTENDANT_OFF);
    key(KEY_ATTENDANT_TOGGLE);
    key(KEY_LIGHTS_TOGGLE);
    key(KEY_ALS_TOGGLE);
    key(KEY_ROTATE_LOCK_TOGGLE);
    key(KEY_BUTTONCONFIG);
    key(KEY_TASKMANAGER);
    key(KEY_JOURNAL);
    key(KEY_CONTROLPANEL);
    key(KEY_APPSELECT);
    key(KEY_SCREENSAVER);
    key(KEY_VOICECOMMAND);
    key(KEY_ASSISTANT);
    key(KEY_KBD_LAYOUT_NEXT);
    // key(KEY_EMOJI_PICKER);
    // key(KEY_DICTATE);
    // key(KEY_CAMERA_ACCESS_ENABLE);
    // key(KEY_CAMERA_ACCESS_DISABLE);
    // key(KEY_CAMERA_ACCESS_TOGGLE);
    key(KEY_BRIGHTNESS_MIN);
    key(KEY_BRIGHTNESS_MAX);
    key(KEY_KBDINPUTASSIST_PREV);
    key(KEY_KBDINPUTASSIST_NEXT);
    key(KEY_KBDINPUTASSIST_PREVGROUP);
    key(KEY_KBDINPUTASSIST_NEXTGROUP);
    key(KEY_KBDINPUTASSIST_ACCEPT);
    key(KEY_KBDINPUTASSIST_CANCEL);
    key(KEY_RIGHT_UP);
    key(KEY_RIGHT_DOWN);
    key(KEY_LEFT_UP);
    key(KEY_LEFT_DOWN);
    key(KEY_ROOT_MENU);
    key(KEY_MEDIA_TOP_MENU);
    key(KEY_NUMERIC_11);
    key(KEY_NUMERIC_12);
    key(KEY_AUDIO_DESC);
    key(KEY_3D_MODE);
    key(KEY_NEXT_FAVORITE);
    key(KEY_STOP_RECORD);
    key(KEY_PAUSE_RECORD);
    key(KEY_VOD);
    key(KEY_UNMUTE);
    key(KEY_FASTREVERSE);
    key(KEY_SLOWREVERSE);
    key(KEY_DATA);
    key(KEY_ONSCREEN_KEYBOARD);
    // key(KEY_PRIVACY_SCREEN_TOGGLE);
    // key(KEY_SELECTIVE_SCREENSHOT);
    // key(KEY_NEXT_ELEMENT);
    // key(KEY_PREVIOUS_ELEMENT);
    // key(KEY_AUTOPILOT_ENGAGE_TOGGLE);
    // key(KEY_MARK_WAYPOINT);
    // key(KEY_SOS);
    // key(KEY_NAV_CHART);
    // key(KEY_FISHING_CHART);
    // key(KEY_SINGLE_RANGE_RADAR);
    // key(KEY_DUAL_RANGE_RADAR);
    // key(KEY_RADAR_OVERLAY);
    // key(KEY_TRADITIONAL_SONAR);
    // key(KEY_CLEARVU_SONAR);
    // key(KEY_SIDEVU_SONAR);
    // key(KEY_NAV_INFO);
    // key(KEY_BRIGHTNESS_MENU);
    // key(KEY_MACRO1);
    // key(KEY_MACRO2);
    // key(KEY_MACRO3);
    // key(KEY_MACRO4);
    // key(KEY_MACRO5);
    // key(KEY_MACRO6);
    // key(KEY_MACRO7);
    // key(KEY_MACRO8);
    // key(KEY_MACRO9);
    // key(KEY_MACRO10);
    // key(KEY_MACRO11);
    // key(KEY_MACRO12);
    // key(KEY_MACRO13);
    // key(KEY_MACRO14);
    // key(KEY_MACRO15);
    // key(KEY_MACRO16);
    // key(KEY_MACRO17);
    // key(KEY_MACRO18);
    // key(KEY_MACRO19);
    // key(KEY_MACRO20);
    // key(KEY_MACRO21);
    // key(KEY_MACRO22);
    // key(KEY_MACRO23);
    // key(KEY_MACRO24);
    // key(KEY_MACRO25);
    // key(KEY_MACRO26);
    // key(KEY_MACRO27);
    // key(KEY_MACRO28);
    // key(KEY_MACRO29);
    // key(KEY_MACRO30);
    // key(KEY_MACRO_RECORD_START);
    // key(KEY_MACRO_RECORD_STOP);
    // key(KEY_MACRO_PRESET_CYCLE);
    // key(KEY_MACRO_PRESET1);
    // key(KEY_MACRO_PRESET2);
    // key(KEY_MACRO_PRESET3);
    // key(KEY_KBD_LCD_MENU1);
    // key(KEY_KBD_LCD_MENU2);
    // key(KEY_KBD_LCD_MENU3);
    // key(KEY_KBD_LCD_MENU4);
    // key(KEY_KBD_LCD_MENU5);
    key(KEY_MIN_INTERESTING);
    key(KEY_MAX);
    key(KEY_CNT);

    key(BTN_MISC);
    key(BTN_0);
    key(BTN_1);
    key(BTN_2);
    key(BTN_3);
    key(BTN_4);
    key(BTN_5);
    key(BTN_6);
    key(BTN_7);
    key(BTN_8);
    key(BTN_9);
    key(BTN_MOUSE);
    key(BTN_LEFT);
    key(BTN_RIGHT);
    key(BTN_MIDDLE);
    key(BTN_SIDE);
    key(BTN_EXTRA);
    key(BTN_FORWARD);
    key(BTN_BACK);
    key(BTN_TASK);
    key(BTN_JOYSTICK);
    key(BTN_TRIGGER);
    key(BTN_THUMB);
    key(BTN_THUMB2);
    key(BTN_TOP);
    key(BTN_TOP2);
    key(BTN_PINKIE);
    key(BTN_BASE);
    key(BTN_BASE2);
    key(BTN_BASE3);
    key(BTN_BASE4);
    key(BTN_BASE5);
    key(BTN_BASE6);
    key(BTN_DEAD);
    key(BTN_GAMEPAD);
    key(BTN_SOUTH);
    key(BTN_A);
    key(BTN_EAST);
    key(BTN_B);
    key(BTN_C);
    key(BTN_NORTH);
    key(BTN_X);
    key(BTN_WEST);
    key(BTN_Y);
    key(BTN_Z);
    key(BTN_TL);
    key(BTN_TR);
    key(BTN_TL2);
    key(BTN_TR2);
    key(BTN_SELECT);
    key(BTN_START);
    key(BTN_MODE);
    key(BTN_THUMBL);
    key(BTN_THUMBR);
    key(BTN_DIGI);
    key(BTN_TOOL_PEN);
    key(BTN_TOOL_RUBBER);
    key(BTN_TOOL_BRUSH);
    key(BTN_TOOL_PENCIL);
    key(BTN_TOOL_AIRBRUSH);
    key(BTN_TOOL_FINGER);
    key(BTN_TOOL_MOUSE);
    key(BTN_TOOL_LENS);
    key(BTN_TOOL_QUINTTAP);
    key(BTN_STYLUS3);
    key(BTN_TOUCH);
    key(BTN_STYLUS);
    key(BTN_STYLUS2);
    key(BTN_TOOL_DOUBLETAP);
    key(BTN_TOOL_TRIPLETAP);
    key(BTN_TOOL_QUADTAP);
    key(BTN_WHEEL);
    key(BTN_GEAR_DOWN);
    key(BTN_GEAR_UP);
    key(BTN_TRIGGER_HAPPY);
    key(BTN_TRIGGER_HAPPY1);
    key(BTN_TRIGGER_HAPPY2);
    key(BTN_TRIGGER_HAPPY3);
    key(BTN_TRIGGER_HAPPY4);
    key(BTN_TRIGGER_HAPPY5);
    key(BTN_TRIGGER_HAPPY6);
    key(BTN_TRIGGER_HAPPY7);
    key(BTN_TRIGGER_HAPPY8);
    key(BTN_TRIGGER_HAPPY9);
    key(BTN_TRIGGER_HAPPY10);
    key(BTN_TRIGGER_HAPPY11);
    key(BTN_TRIGGER_HAPPY12);
    key(BTN_TRIGGER_HAPPY13);
    key(BTN_TRIGGER_HAPPY14);
    key(BTN_TRIGGER_HAPPY15);
    key(BTN_TRIGGER_HAPPY16);
    key(BTN_TRIGGER_HAPPY17);
    key(BTN_TRIGGER_HAPPY18);
    key(BTN_TRIGGER_HAPPY19);
    key(BTN_TRIGGER_HAPPY20);
    key(BTN_TRIGGER_HAPPY21);
    key(BTN_TRIGGER_HAPPY22);
    key(BTN_TRIGGER_HAPPY23);
    key(BTN_TRIGGER_HAPPY24);
    key(BTN_TRIGGER_HAPPY25);
    key(BTN_TRIGGER_HAPPY26);
    key(BTN_TRIGGER_HAPPY27);
    key(BTN_TRIGGER_HAPPY28);
    key(BTN_TRIGGER_HAPPY29);
    key(BTN_TRIGGER_HAPPY30);
    key(BTN_TRIGGER_HAPPY31);
    key(BTN_TRIGGER_HAPPY32);
    key(BTN_TRIGGER_HAPPY33);
    key(BTN_TRIGGER_HAPPY34);
    key(BTN_TRIGGER_HAPPY35);
    key(BTN_TRIGGER_HAPPY36);
    key(BTN_TRIGGER_HAPPY37);
    key(BTN_TRIGGER_HAPPY38);
    key(BTN_TRIGGER_HAPPY39);
    key(BTN_TRIGGER_HAPPY40);
    qDebug() << "Unknown EV_KEY event code:" << code.c_str();
    return false;
}

bool process_EV_MSC(const QStringList& args){
    auto code = args.at(1).toStdString();
    int value = 0;
    if(args.count() > 2){
        bool ok;
        QString arg = args.at(2);
        value = arg.toInt(&ok);
        if(!ok){
            qDebug() << "Third argument must be a valid integer:" << args;
            return false;
        }
    }
    msc(MSC_SERIAL);
    msc(MSC_PULSELED);
    msc(MSC_GESTURE);
    msc(MSC_RAW);
    msc(MSC_SCAN);
    msc(MSC_TIMESTAMP);
    msc(MSC_MAX);
    msc(MSC_CNT);
    qDebug() << "Unknown EV_MSC event code:" << code.c_str();
    return false;
}

bool process_EV_SW(const QStringList& args){
    auto code = args.at(1).toStdString();
    int value = 0;
    if(args.count() > 2){
        bool ok;
        QString arg = args.at(2);
        value = arg.toInt(&ok);
        if(!ok){
            qDebug() << "Third argument must be a valid integer:" << args;
            return false;
        }
    }
    sw(SW_LID);
    sw(SW_TABLET_MODE);
    sw(SW_HEADPHONE_INSERT);
    sw(SW_RFKILL_ALL);
    sw(SW_RADIO);
    sw(SW_MICROPHONE_INSERT);
    sw(SW_DOCK);
    sw(SW_LINEOUT_INSERT);
    sw(SW_JACK_PHYSICAL_INSERT);
    sw(SW_VIDEOOUT_INSERT);
    sw(SW_CAMERA_LENS_COVER);
    sw(SW_KEYPAD_SLIDE);
    sw(SW_FRONT_PROXIMITY);
    sw(SW_ROTATE_LOCK);
    sw(SW_LINEIN_INSERT);
    sw(SW_MUTE_DEVICE);
    sw(SW_PEN_INSERTED);
    //sw(SW_MACHINE_COVER);
    sw(SW_MAX);
    sw(SW_CNT);
    qDebug() << "Unknown EV_SW event code:" << code.c_str();
    return false;
}

bool process_EV_LED(const QStringList& args){
    auto code = args.at(1).toStdString();
    int value = 0;
    if(args.count() > 2){
        bool ok;
        QString arg = args.at(2);
        value = arg.toInt(&ok);
        if(!ok){
            qDebug() << "Third argument must be a valid integer:" << args;
            return false;
        }
    }
    led(LED_NUML);
    led(LED_CAPSL);
    led(LED_SCROLLL);
    led(LED_COMPOSE);
    led(LED_KANA);
    led(LED_SLEEP);
    led(LED_SUSPEND);
    led(LED_MUTE);
    led(LED_MISC);
    led(LED_MAIL);
    led(LED_CHARGING);
    led(LED_MAX);
    led(LED_CNT);
    qDebug() << "Unknown EV_LED event code:" << code.c_str();
    return false;
}

bool process_EV_SND(const QStringList& args){
    auto code = args.at(1).toStdString();
    int value = 0;
    if(args.count() > 2){
        bool ok;
        QString arg = args.at(2);
        value = arg.toInt(&ok);
        if(!ok){
            qDebug() << "Third argument must be a valid integer:" << args;
            return false;
        }
    }
    snd(SND_CLICK);
    snd(SND_BELL);
    snd(SND_TONE);
    snd(SND_MAX);
    snd(SND_CNT);
    qDebug() << "Unknown EV_SND event code:" << code.c_str();
    return false;
}

bool process_EV_REP(const QStringList& args){
    auto code = args.at(1).toStdString();
    int value = 0;
    if(args.count() > 2){
        bool ok;
        QString arg = args.at(2);
        value = arg.toInt(&ok);
        if(!ok){
            qDebug() << "Third argument must be a valid integer:" << args;
            return false;
        }
    }
    rep(REP_DELAY);
    rep(REP_PERIOD);
    rep(REP_MAX);
    rep(REP_CNT);
    qDebug() << "Unknown EV_REP event code:" << code.c_str();
    return false;
}

#define process_type(eventType) if(type == #eventType){ return process_##eventType(args); }

bool process(const QStringList& args){
    if(args.count() < 2 || args.count() > 3){
        qDebug() << "Incorrect number of arguments.";
        if(debugEnabled()){
            qDebug() << "Arguments: " << args.count();
        }
        return false;
    }
    auto type = args.first().toStdString();
    process_type(EV_SYN);
    process_type(EV_ABS);
    process_type(EV_REL);
    process_type(EV_KEY);
    process_type(EV_MSC);
    process_type(EV_SW);
    process_type(EV_LED);
    process_type(EV_SND);
    process_type(EV_REP);
    qDebug() << "Unknown event type:" << type.c_str();
    return false;
}

int main(int argc, char *argv[]){
    QCoreApplication app(argc, argv);
    sentry_init("inject_evdev", argv);
    app.setOrganizationName("Eeems");
    app.setOrganizationDomain(OXIDE_SERVICE);
    app.setApplicationName("inject_evdev");
    app.setApplicationVersion(APP_VERSION);
    QCommandLineParser parser;
    parser.setApplicationDescription("Inject evdev events.");
    parser.applicationDescription();
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("device", "Device to emit events to. See /dev/input for possible values.");
    parser.process(app);
    QStringList args = parser.positionalArguments();
    if (args.isEmpty() || args.count() > 1) {
        parser.showHelp(EXIT_FAILURE);
    }
    auto name = args.first();
    QString path = QString("/dev/input/%1").arg(name);
    if(!QFile::exists(path)){
        qDebug() << "Device does not exist:" << name.toStdString().c_str();
        return EXIT_FAILURE;
    }
    device = new event_device(path.toStdString(), O_RDWR);
    if(device->error > 0){
        qDebug() << "Failed to open device:" << name.toStdString().c_str();
        qDebug() << strerror(device->error);
        return EXIT_FAILURE;
    }
    int fd = fileno(stdin);
    bool isStream = !isatty(fd);
    QSocketNotifier notifier(fd, QSocketNotifier::Read);
    QObject::connect(&notifier, &QSocketNotifier::activated, [&app, &isStream](){
        std::string input;
        std::getline(std::cin, input);
        auto args = QString(input.c_str()).simplified().split(QRegExp("\\s+"), Qt::SkipEmptyParts);
        auto count = args.count();
        if(count && debugEnabled()){
            qDebug() << args;
        }
        if(!isStream && count == 1 && args.first().toLower() == "exit"){
            app.exit(EXIT_SUCCESS);
            return;
        }
        if(count && !process(args) && isStream){
            app.exit(EXIT_FAILURE);
            return;
        }
        if(!isStream){
            std::cout << "> " << std::flush;
        }else if(std::cin.eof()){
            app.exit(EXIT_SUCCESS);
            return;
        }
    });
    if(!isStream){
        qDebug() << "Input is expected in the following format:";
        qDebug() << "  TYPE CODE [VALUE]";
        qDebug() << "";
        qDebug() << "For example:";
        qDebug() << "  ABS_MT_TRACKING_ID -1";
        qDebug() << "  EV_SYN SYN_REPORT";
        qDebug() << "";
        std::cout << "> " << std::flush;
    }
    return app.exec();
}
