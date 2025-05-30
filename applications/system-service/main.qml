import QtQuick 2.15
import QtQuick.Window 2.15

Window{
    id: window
    x: 0
    y: 0
    width: Screen.width
    height: Screen.height
    visible: true
    color: "transparent"
    property bool enableHeldKeys: controller.deviceName === "reMarkable 1"

    Connections{
        target: controller
        property alias enableHeldKeys: window.enableHeldKeys
        function onKeyPressed(key, modifiers, text){
            switch(key){
                case Qt.Key_Left:
                    if(enableHeldKeys){
                        leftHeldTimer.start();
                    }
                    break;
                case Qt.Key_Right:
                    if(enableHeldKeys){
                        rightHeldTimer.start();
                    }
                    break;
                case Qt.Key_Home:
                    if(enableHeldKeys){
                        homeHeldTimer.start();
                    }
                    break;
                case Qt.Key_PowerOff:
                case Qt.Key_Suspend:
                    powerHeldTimer.start();
                    break;
            }
        }
        function onKeyReleased(key, modifiers, text){
            switch(key){
                case Qt.Key_Left:
                    if(enableHeldKeys){
                        leftHeldTimer.stop();
                    }
                    break;
                case Qt.Key_Right:
                    if(enableHeldKeys){
                        rightHeldTimer.stop();
                    }
                    break;
                case Qt.Key_Home:
                    if(enableHeldKeys){
                        homeHeldTimer.stop();
                    }
                    break;
                case Qt.Key_PowerOff:
                case Qt.Key_Suspend:
                    if(powerHeldTimer.running){
                        powerHeldTimer.stop();
                        controller.suspend();
                    }
                    break;
            }
        }
    }
    Timer{
        id: leftHeldTimer
        running: false
        repeat: false
        interval: 700
        onTriggered: controller.back()
    }
    Timer{
        id: rightHeldTimer
        running: false
        repeat: false
        interval: 700
        onTriggered: controller.screenshot()
    }
    Timer{
        id: homeHeldTimer
        running: false
        repeat: false
        interval: 700
        onTriggered: controller.processManager()
    }
    Timer{
        id: powerHeldTimer
        running: false
        repeat: false
        interval: 700
        onTriggered:{
            // TODO - power menu app
        }
    }

    Item{
        anchors.fill: parent
        MultiPointTouchArea{
            anchors.fill: parent
            minimumTouchPoints: 1
            maximumTouchPoints: 5
            mouseEnabled: false
            property int offset: 100

            function isSwipeLow(start, end, portraitLength, landscapeLength){
                return start < offset
                    && end > start + (Oxide.landscape ? landscapeLength : portraitLength);
            }
            function isSwipeHigh(start, end, portraitLength, landscapeLength, size){
                return start > size - offset
                    && end < start - (Oxide.landscape ? landscapeLength : portraitLength);
            }
            function isEnabled(swiped, portraitEnabled, landscapeEnabled){
                return swiped && (Oxide.landscape ? landscapeEnabled : portraitEnabled);
            }
            function swipeAction(portraitFn, landscapeFn){
                if(Oxide.landscape){
                    landscapeFn();
                }else{
                    portraitFn();
                }
            }

            onReleased: function(touchPoints){
                if(touchPoints.length !== 1){
                    return;
                }
                var point = touchPoints[0];
                if(isEnabled(
                    isSwipeHigh(point.startX, point.x, controller.leftSwipeLength, controller.downSwipeLength, window.width),
                    controller.leftSwipeEnabled,
                    controller.downSwipeEnabled
                )){
                    swipeAction(controller.screenshot, controller.toggleSwipes);
                }else if(isEnabled(
                    isSwipeLow(point.startX, point.x, controller.rightSwipeLength, controller.upSwipeLength),
                    controller.rightSwipeEnabled,
                    controller.upSwipeEnabled
                )){
                    swipeAction(controller.back, controller.taskSwitcher);
                }else if(isEnabled(
                    isSwipeHigh(point.startY, point.y, controller.upSwipeLength, controller.leftSwipeLength, window.height),
                    controller.upSwipeEnabled,
                    controller.leftSwipeEnabled
                )){
                    swipeAction(controller.taskSwitcher, controller.screenshot);
                }else if(isEnabled(
                    isSwipeLow(point.startY, point.y, controller.downSwipeLength, controller.rightSwipeLength),
                    controller.downSwipeEnabled,
                    controller.rightSwipeEnabled
                )){
                    swipeAction(controller.toggleSwipes, controller.back);
                }
            }
        }
    }
    Shortcut{
        sequences: ["Alt+Tab"]
        context: Qt.ApplicationShortcut
        onActivated: controller.taskSwitcher()
    }
    Shortcut{
        sequences: ["Meta+Shift+S", "End+Shift+S", StandardKey.Print]
        context: Qt.ApplicationShortcut
        onActivated: controller.screenshot()
    }
    Shortcut{
        sequences: ["Ctrl+Shift+Esc", "Ctrl+Shift+1"]
        context: Qt.ApplicationShortcut
        onActivated: controller.processManager()
    }
    Shortcut{
        sequences: ["Meta+Backspace", "End+Backspace", StandardKey.Back]
        context: Qt.ApplicationShortcut
        onActivated: controller.back()
    }
    Shortcut{
        sequences: ["LogOff", "ScreenSaver", "End+L", "Meta+L"]
        context: Qt.ApplicationShortcut
        onActivated: controller.lock()
    }
    Shortcut{
        sequences: ["Terminal", "Ctrl+Alt+T"]
        context: Qt.ApplicationShortcut
        onActivated: controller.terminal()
    }
    Shortcut{
        sequences: ["End+Alt+4", "Alt+F4", "Meta+Alt+4"]
        context: Qt.ApplicationShortcut
        onActivated: controller.close()
    }
}
