import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.0
import "qrc:/codes.eeems.oxide"

Window{
    id: window
    x: 0
    y: 0
    width: Screen.width
    height: Screen.height
    visible: true
    color: powerMenu.opened ? "white" : "transparent"
    property bool enableHeldKeys: controller.deviceName === "reMarkable 1"
    Connections{
        target: controller
        function onLandscapeChanged(landscape){
            console.log("landscape changed", landscape)
        }
    }
    function getState(){
        return {
            "powerMenu": powerMenu.opened,
            "resumeApplication": powerMenu.resumeApplication
        };
    }
    function setState(state){
        window.raise();
        window.show();
        powerMenu.resumeApplication = state["resumeApplication"];
        if(state['powerMenu']){
            showPowerMenu();
        }
    }
    function shouldResume(){
        return controller.lidOpen;
    }
    function showPowerMenu(){
        window.raise();
        powerMenu.open();
    }
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
        function onLidClosed(){
            controller.suspend()
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
        onTriggered: showPowerMenu()
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
                    && end > start + (controller.landscape ? landscapeLength : portraitLength);
            }
            function isSwipeHigh(start, end, portraitLength, landscapeLength, size){
                return start > size - offset
                    && end < start - (controller.landscape ? landscapeLength : portraitLength);
            }
            function isEnabled(swiped, portraitEnabled, landscapeEnabled){
                return swiped && (controller.landscape ? landscapeEnabled : portraitEnabled);
            }
            function swipeAction(portraitFn, landscapeFn){
                if(controller.landscape){
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
        sequences: ["Meta+Backspace", "End+Backspace"]
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
    Popup {
        id: powerMenu
        modal: true
        anchors.centerIn: parent
        closePolicy: Popup.NoAutoClose
        font.pixelSize: 48
        implicitWidth: 500
        implicitHeight: 500
        Overlay.modal: Rectangle { color: "white" }
        background: Rectangle { color: "white" }
        property string resumeApplication: ""
        onOpened: {
            resumeApplication = controller.currentApplication();
            controller.pauseApplication(resumeApplication, false);
        }
        onClosed: {
            if(resumeApplication != ""){
                controller.resumeApplication(resumeApplication);
            }else{
                controller.back();
            }
        }
        contentItem: ColumnLayout {
            rotation: controller.landscape ? 90 : 0
            spacing: 10
            Slider {
                id: brightnessSlider
                visible: controller.hasFrontlight
                Layout.fillWidth: true
                Layout.preferredHeight: 60
                from: 0
                to: 100
                value: controller.frontlightBrightness
                onMoved: controller.frontlightBrightness = value
                background: Rectangle {
                    x: brightnessSlider.leftPadding
                    y: brightnessSlider.topPadding + brightnessSlider.availableHeight / 2 - height / 2
                    width: brightnessSlider.availableWidth
                    height: 8
                    color: "black"
                }
                handle: Rectangle {
                    x: brightnessSlider.leftPadding + brightnessSlider.visualPosition * (brightnessSlider.availableWidth - width)
                    y: brightnessSlider.topPadding + brightnessSlider.availableHeight / 2 - height / 2
                    width: 50
                    height: 50
                    radius: 25
                    color: "white"
                    border.color: "black"
                    border.width: 2
                }
            }
            Label {
                text: "Brightness"
                visible: controller.hasFrontlight
                Layout.fillWidth: true
                font.pixelSize: 36
                horizontalAlignment: Text.AlignRight
            }
            Item {
                visible: controller.hasFrontlight
                Layout.fillWidth: true
                Layout.preferredHeight: 20
            }
            OxideButton {
                text: "Suspend"
                Layout.fillWidth: true
                onClicked: {
                    controller.suspend();
                    powerMenu.close();
                }
            }
            OxideButton {
                text: "Reboot"
                Layout.fillWidth: true
                onClicked: {
                    controller.reboot();
                    powerMenu.close();
                }
            }
            OxideButton {
                text: "Shutdown"
                Layout.fillWidth: true
                onClicked: {
                    controller.powerOff();
                    powerMenu.close();
                }
            }
            OxideButton {
                text: "Lock"
                Layout.fillWidth: true
                visible: powerMenu.resumeApplication != controller.lockscreenApplication()
                enabled: visible
                onClicked: {
                    powerMenu.resumeApplication = "";
                    controller.lock();
                    powerMenu.close();
                }
            }
            OxideButton {
                text: "Cancel"
                Layout.fillWidth: true
                onClicked: powerMenu.close()
            }
        }
    }
}
