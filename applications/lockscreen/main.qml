import QtQuick 2.10
import QtQuick.Window 2.3
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.0
import "qrc:/codes.eeems.oxide"
import "widgets"

OxideWindow {
    headerBackgroundColor: "white"
    backgroundColor: "white"
    color: "black"
    id: window
    objectName: "window"
    visible: stateController.state != "loading"
    title: qsTr("Oxide")
    property int itemPadding: 10
    FontLoader { id: iconFont; source: "/font/icomoon.ttf" }
    Component.onCompleted: {
        controller.startup();
        pinEntry.forceActiveFocus();
    }
    centerMenu: [
        Label {
            objectName: "clock"
            Layout.alignment: Qt.AlignCenter
            color: window.color
        }
    ]
    rightMenu: [
        OxideStatusIcon {
            id: wifiState
            objectName: "wifiState"
            property string state: "unknown"
            property int rssi: 0
            property bool connected: false
            source: {
                var icon;
                if(state === "unknown"){
                    icon = "unknown";
                }else if(state === "down"){
                    icon = "down";
                }else if(!connected){
                    icon = "disconnected";
                }else if(rssi > -50) {
                    icon = "4_bar";
                }else if(rssi > -60){
                    icon = "3_bar";
                }else if(rssi > -70){
                    icon = "2_bar";
                }else if(rssi > -80){
                    icon = "1_bar";
                }else{
                    icon = "0_bar";
                }
                return "qrc:/img/wifi/" + icon + ".png";
            }
        },
        OxideStatusIcon {
            id: batteryLevel
            objectName: "batteryLevel"
            property bool alert: false
            property bool warning: false
            property bool charging: false
            property bool connected: false
            property bool present: true
            property int level: 0
            source: {
                var icon = "";
                if(alert || !present){
                    icon = "alert";
                }else if(warning){
                    icon = "unknown";
                }else{
                    if(charging || connected){
                        icon = "charging_";
                    }
                    if(level < 25){
                        icon += "20";
                    }else if(level < 35){
                        icon += "30";
                    }else if(level < 55){
                        icon += "50";
                    }else if(level < 65){
                        icon += "60";
                    }else if(level < 85){
                        icon += "80";
                    }else if(level < 95){
                        icon += "90";
                    }else{
                        icon += 100;
                    }
                }
                return "qrc:/img/battery/" + icon + ".png";
            }
        },
        CustomMenu {
            OxideMenu {
                id: powerMenu
                title: qsTr("");
                font: iconFont.name
                width: 260
                color: window.color
                backgroundColor: window.headerBackgroundColor
                activeColor: window.color
                activeBackgroundColor: window.backgroundColor
                borderColor: window.color
                Action {
                    text: qsTr(" Suspend")
                    enabled: !controller.sleepInhibited
                    onTriggered: {
                        controller.breadcrumb("menu.suspend", "clicked", "ui");
                        controller.suspend();
                    }
                }
                Action {
                    text: qsTr(" Reboot")
                    enabled: !controller.powerOffInhibited
                    onTriggered: {
                        controller.breadcrumb("menu.reboot", "clicked", "ui");
                        controller.reboot();
                    }
                }
                Action {
                    text: qsTr(" Shutdown")
                    enabled: !controller.powerOffInhibited
                    onTriggered: {
                        controller.breadcrumb("menu.shutdown", "clicked", "ui");
                        controller.powerOff();
                    }
                }
            }
        }
    ]
    initialItem: Item {
        anchors.fill: parent
        PinPad {
            id: pinEntry
            focus: true
            objectName: "pinEntry"
            buttonsVisible: !window.landscape
            showPress: false
            anchors.centerIn: parent
            label: {
                switch(stateController.state){
                    case "prompt":
                        return "Set PIN";
                    case "confirmPin":
                        return "Confirm PIN";
                    default:
                        return "PIN";
                }
            }
            onSubmit: {
                controller.breadcrumb("pinEntry", "submit", "ui");
                if(!controller.submitPin(pin)){
                    message = "Incorrect PIN";
                    value = "";
                    return;
                }
                var state = stateController.state
                if(state === "loaded" || state === "confirmPin"){
                    message = "Correct!";
                    return;
                }
                message = "";
            }
        }
    }
    Component {
        id: importDialog
        Rectangle{
            color: window.color
            anchors.centerIn: parent
            width: 1000
            height: contentItem.implicitHeight
            clip: true
            ColumnLayout {
                anchors.centerIn: parent
                RowLayout {
                    Item { Layout.fillWidth: true }
                    Label {
                        text: "Import PIN from Xochitl?\nThis will remove your pin from Xochitl."
                        Layout.fillHeight: true
                    }
                    Item { Layout.fillWidth: true }
                }
                Item {
                    Layout.rowSpan: 2
                    Layout.fillHeight: true
                }
                RowLayout {
                    OxideButton {
                        text: "Cancel"
                        width: height * 2
                        Layout.fillWidth: true
                        onClicked: {
                            controller.breadcrumb("cancel", "clicked", "ui");
                            stateController.state = "pinPrompt";
                        }
                    }
                    OxideButton {
                        text: "Import"
                        width: height * 2
                        Layout.fillWidth: true
                        onClicked: {
                            controller.breadcrumb("import", "clicked", "ui");
                            controller.importPin();
                        }
                    }
                }
            }
        }
    }
    Component {
        id: pinPrompt
        Rectangle{
            color: window.color
            anchors.centerIn: parent
            width: 1000
            height: contentItem.implicitHeight
            clip: true
            ColumnLayout {
                anchors.centerIn: parent
                RowLayout {
                    Item { Layout.fillWidth: true }
                    Label {
                        text: "Create a PIN?"
                        Layout.fillHeight: true
                    }
                    Item { Layout.fillWidth: true }
                }
                Item {
                    Layout.rowSpan: 2
                    Layout.fillHeight: true
                }
                RowLayout {
                    OxideButton {
                        text: "No PIN"
                        width: height * 2
                        Layout.fillWidth: true
                        onClicked: {
                            controller.breadcrumb("nopin", "clicked", "ui");
                            controller.clearPin();
                        }
                    }
                    OxideButton {
                        text: "Create"
                        width: height * 2
                        Layout.fillWidth: true
                        onClicked: {
                            controller.breadcrumb("create", "clicked", "ui");
                            stateController.state = "prompt";
                        }
                    }
                }
            }
        }
    }
    Component {
        id: telemetryDialog
        Rectangle{
            color: window.color
            anchors.centerIn: parent
            height: contentItem.implicitHeight
            width: 1000
            clip: true
            ColumnLayout {
                anchors.centerIn: parent
                RowLayout {
                    Item { Layout.fillWidth: true }
                    Label {
                        text: "Opt-in to telemetry?"
                        Layout.fillHeight: true
                    }
                    Item { Layout.fillWidth: true }
                }
                Label {
                    text: "Oxide has basic telemetry and crash reporting.\nWould you like to enable it?\nSee https://oxide.eeems.codes/faq.html for more\ninformation."
                    Layout.fillWidth: true
                }
                Item {
                    Layout.rowSpan: 2
                    Layout.fillHeight: true
                }
                ColumnLayout {
                    OxideButton {
                        text: "Full Telemetry"
                        width: height * 2
                        Layout.fillWidth: true
                        onClicked: {
                            controller.breadcrumb("full-telemetry", "clicked", "ui");
                            controller.telemetry = true;
                            controller.applicationUsage = true;
                            controller.crashReport = true;
                            controller.firstLaunch = false;
                            stateController.state = "loading";
                        }
                    }
                    OxideButton {
                        text: "Basic Telemetry"
                        width: height * 2
                        Layout.fillWidth: true
                        onClicked: {
                            controller.breadcrumb("basic-telemetry", "clicked", "ui");
                            controller.telemetry = true;
                            controller.applicationUsage = false;
                            controller.crashReport = true;
                            controller.firstLaunch = false;
                            stateController.state = "loading";
                        }
                    }
                    OxideButton {
                        text: "Crash Reports Only"
                        width: height * 2
                        Layout.fillWidth: true
                        onClicked: {
                            controller.breadcrumb("crash-report-only", "clicked", "ui");
                            controller.telemetry = false;
                            controller.applicationUsage = false;
                            controller.crashReport = true;
                            controller.firstLaunch = false;
                            stateController.state = "loading";
                        }
                    }
                    OxideButton {
                        text: "No Telemetry"
                        width: height * 2
                        Layout.fillWidth: true
                        onClicked: {
                            controller.breadcrumb("no-telemetry", "clicked", "ui");
                            controller.telemetry = false;
                            controller.applicationUsage = false;
                            controller.crashReport = false;
                            controller.firstLaunch = false;
                            stateController.state = "loading";
                        }
                    }
                }
        }
        }
    }
    StateGroup {
        id: stateController
        objectName: "stateController"
        state: "loading"
        states: [
            State { name: "loaded" },
            State { name: "pinPrompt" },
            State { name: "telemetry" },
            State { name: "prompt" },
            State { name: "confirmPin" },
            State { name: "import" },
            State { name: "noPin" },
            State { name: "loading" }
        ]
        transitions: [
            Transition {
                from: "*"; to: "loaded"
                SequentialAnimation {
                    ScriptAction { script: {
                        controller.breadcrumb("navigation", "main", "navigation");
                        console.log("PIN Entry");
                        pinEntry.value = "";
                        pinEntry.forceActiveFocus();
                        window.stack.pop(window.stack.initialItem);
                    } }
                }
            },
            Transition {
                from: "*"; to: "pinPrompt"
                SequentialAnimation {
                    ScriptAction { script: {
                        controller.breadcrumb("navigation", "pinPrompt", "navigation");
                        console.log("Prompt for PIN creation");
                        window.stack.push(pinPrompt);
                    } }
                }
            },
            Transition {
                from: "*"; to: "telemetry"
                SequentialAnimation {
                    ScriptAction { script: {
                        controller.breadcrumb("navigation", "telemetry", "navigation");
                        console.log("Telemetry opt-in screen");
                        window.stack.push(telemetryDialog);
                    } }
                }
            },
            Transition {
                from: "*"; to: "confirmPin"
                SequentialAnimation {
                    ScriptAction { script: {
                        controller.breadcrumb("navigation", "confirmPin", "navigation");
                        console.log("PIN Confirmation");
                        pinEntry.value = "";
                        pinEntry.forceActiveFocus();
                        window.stack.pop(window.stack.initialItem);
                    } }
                }
            },
            Transition {
                from: "*"; to: "import"
                SequentialAnimation {
                    ScriptAction { script: {
                        controller.breadcrumb("navigation", "import", "navigation");
                        console.log("Import PIN");
                        window.stack.push(importDialog);
                    } }
                }
            },
            Transition {
                from: "*"; to: "prompt"
                SequentialAnimation {
                    ScriptAction { script: {
                        controller.breadcrumb("navigation", "prompt", "navigation");
                        console.log("PIN Setup");
                        pinEntry.value = "";
                        pinEntry.forceActiveFocus();
                        window.stack.pop(window.stack.initialItem);
                    } }
                }
            },
            Transition {
                from: "*"; to: "noPin"
                SequentialAnimation {
                    ScriptAction { script: controller.breadcrumb("navigation", "noPin", "navigation"); }
                }
            },
            Transition {
                from: "*"; to: "loading"
                SequentialAnimation {
                    ScriptAction { script: {
                        controller.breadcrumb("navigation", "loading", "navigation");
                        console.log("Loading display");
                        controller.startup();
                        window.stack.pop(window.stack.initialItem);
                    } }
                }
            }
        ]
    }
    onKeyPressed: (event) => pinEntry.keyPress(event)
    onKeyReleased: (event) => pinEntry.keyRelease(event)
}
