import QtQuick 2.10
import QtQuick.Window 2.3
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.0
import "widgets"

ApplicationWindow {
    id: window
    objectName: "window"
    width: screenGeometry.width
    height: screenGeometry.height
    title: qsTr("Oxide")
    visible: true
    property int itemPadding: 10
    FontLoader { id: iconFont; source: "/font/icomoon.ttf" }
    Component.onCompleted: controller.startup()
    header: Rectangle {
        color: "black"
        height: menu.height
        RowLayout {
            id: menu
            width: parent.width
            Label { Layout.fillWidth: true }
            RowLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignRight
                StatusIcon {
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
                }
                StatusIcon {
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
                }
                CustomMenu {
                    BetterMenu {
                        id: powerMenu
                        title: qsTr("");
                        font: iconFont.name
                        width: 260
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
            }
        }
        Label {
            objectName: "clock"
            anchors.centerIn: parent
            color: "white"
        }
    }
    background: Rectangle { color: "black" }
    contentData: [
        Rectangle {
            anchors.fill: parent
            color: "black"
        },
        PinPad {
            id: pinEntry
            objectName: "pinEntry"
            visible: false
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
                if(stateController.state === "waiting"){
                    message = "Correct!";
                    controller.previousApplication();
                    return;
                }
                message = "";
            }
        },
        Popup {
            id: importPrompt
            x: (parent.width / 2) - (width / 2)
            y: (parent.height / 2) - (height / 2)
            width: 1000
            clip: true
            closePolicy: Popup.NoAutoClose
            ColumnLayout {
                anchors.fill: parent
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
                    BetterButton {
                        text: "Cancel"
                        width: height * 2
                        Layout.fillWidth: true
                        onClicked: {
                            controller.breadcrumb("cancel", "clicked", "ui");
                            stateController.state = "pinPrompt";
                        }
                    }
                    BetterButton {
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
        },
        Popup {
            id: pinPrompt
            x: (parent.width / 2) - (width / 2)
            y: (parent.height / 2) - (height / 2)
            width: 1000
            clip: true
            closePolicy: Popup.NoAutoClose
            ColumnLayout {
                anchors.fill: parent
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
                    BetterButton {
                        text: "No PIN"
                        width: height * 2
                        Layout.fillWidth: true
                        onClicked: {
                            controller.breadcrumb("nopin", "clicked", "ui");
                            controller.clearPin();
                            controller.startup();
                        }
                    }
                    BetterButton {
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
        },
        Popup {
            id: telemetry
            x: (parent.width / 2) - (width / 2)
            y: (parent.height / 2) - (height / 2)
            width: 1000
            clip: true
            closePolicy: Popup.NoAutoClose
            ColumnLayout {
                anchors.fill: parent
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
                    BetterButton {
                        text: "Full Telemetry"
                        width: height * 2
                        Layout.fillWidth: true
                        onClicked: {
                            controller.breadcrumb("full-telemetry", "clicked", "ui");
                            controller.telemetry = true;
                            controller.applicationUsage = true;
                            controller.crashReport = true;
                            controller.firstLaunch = false;
                            stateController.state = "waiting";
                        }
                    }
                    BetterButton {
                        text: "Basic Telemetry"
                        width: height * 2
                        Layout.fillWidth: true
                        onClicked: {
                            controller.breadcrumb("basic-telemetry", "clicked", "ui");
                            controller.telemetry = true;
                            controller.applicationUsage = false;
                            controller.crashReport = true;
                            controller.firstLaunch = false;
                            stateController.state = "waiting";
                        }
                    }
                    BetterButton {
                        text: "Crash Reports Only"
                        width: height * 2
                        Layout.fillWidth: true
                        onClicked: {
                            controller.breadcrumb("crash-report-only", "clicked", "ui");
                            controller.telemetry = false;
                            controller.applicationUsage = false;
                            controller.crashReport = true;
                            controller.firstLaunch = false;
                            stateController.state = "waiting";
                        }
                    }
                    BetterButton {
                        text: "No Telemetry"
                        width: height * 2
                        Layout.fillWidth: true
                        onClicked: {
                            controller.breadcrumb("no-telemetry", "clicked", "ui");
                            controller.telemetry = false;
                            controller.applicationUsage = false;
                            controller.crashReport = false;
                            controller.firstLaunch = false;
                            stateController.state = "waiting";
                        }
                    }
                }
            }
        }
    ]
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
            State { name: "loading" },
            State { name: "waiting" }
        ]
        transitions: [
            Transition {
                from: "*"; to: "loaded"
                SequentialAnimation {
                    ScriptAction { script: {
                        controller.breadcrumb("navigation", "main", "navigation");
                        console.log("PIN Entry");
                        pinEntry.value = "";
                    } }
                    ParallelAnimation {
                        PropertyAction { target: pinEntry; property: "visible"; value: true }
                        PropertyAction { target: telemetry; property: "visible"; value: false }
                        PropertyAction { target: pinPrompt; property: "visible"; value: false }
                        PropertyAction { target: importPrompt; property: "visible"; value: false }
                    }
                }
            },
            Transition {
                from: "*"; to: "pinPrompt"
                SequentialAnimation {
                    ParallelAnimation {
                        PropertyAction { target: pinEntry; property: "visible"; value: false }
                        PropertyAction { target: telemetry; property: "visible"; value: false }
                        PropertyAction { target: pinPrompt; property: "visible"; value: true }
                        PropertyAction { target: importPrompt; property: "visible"; value: false }
                    }
                    ScriptAction { script: {
                        controller.breadcrumb("navigation", "pinPrompt", "navigation");
                        console.log("Prompt for PIN creation");
                    } }
                }
            },
            Transition {
                from: "*"; to: "telemetry"
                SequentialAnimation {
                    ParallelAnimation {
                        PropertyAction { target: pinEntry; property: "visible"; value: false }
                        PropertyAction { target: telemetry; property: "visible"; value: true }
                        PropertyAction { target: pinPrompt; property: "visible"; value: false }
                        PropertyAction { target: importPrompt; property: "visible"; value: false }
                    }
                    ScriptAction { script: {
                        controller.breadcrumb("navigation", "telemetry", "navigation");
                        console.log("Telemetry opt-in screen");
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
                    } }
                    ParallelAnimation {
                        PropertyAction { target: pinEntry; property: "visible"; value: true }
                        PropertyAction { target: telemetry; property: "visible"; value: false }
                        PropertyAction { target: pinPrompt; property: "visible"; value: false }
                        PropertyAction { target: importPrompt; property: "visible"; value: false }
                    }
                }
            },
            Transition {
                from: "*"; to: "import"
                SequentialAnimation {
                    ScriptAction { script: {
                        controller.breadcrumb("navigation", "import", "navigation");
                        console.log("Import PIN");
                    } }
                    ParallelAnimation {
                        PropertyAction { target: pinEntry; property: "visible"; value: false }
                        PropertyAction { target: telemetry; property: "visible"; value: false }
                        PropertyAction { target: pinPrompt; property: "visible"; value: false }
                        PropertyAction { target: importPrompt; property: "visible"; value: true }
                    }
                }
            },
            Transition {
                from: "*"; to: "prompt"
                SequentialAnimation {
                    ScriptAction { script: {
                        controller.breadcrumb("navigation", "prompt", "navigation");
                        console.log("PIN Setup");
                        pinEntry.value = "";
                    } }
                    ParallelAnimation {
                        PropertyAction { target: pinEntry; property: "visible"; value: true }
                        PropertyAction { target: telemetry; property: "visible"; value: false }
                        PropertyAction { target: pinPrompt; property: "visible"; value: false }
                        PropertyAction { target: importPrompt; property: "visible"; value: false }
                    }
                }
            },
            Transition {
                from: "*"; to: "noPin"
                SequentialAnimation {
                    ScriptAction { script: controller.breadcrumb("navigation", "noPin", "navigation"); }
                    ParallelAnimation {
                        PropertyAction { target: pinEntry; property: "visible"; value: false }
                        PropertyAction { target: telemetry; property: "visible"; value: false }
                        PropertyAction { target: pinPrompt; property: "visible"; value: false }
                        PropertyAction { target: importPrompt; property: "visible"; value: false }
                    }
                }
            },
            Transition {
                from: "*"; to: "waiting"
                ParallelAnimation {
                    PropertyAction { target: pinEntry; property: "visible"; value: true }
                    PropertyAction { target: telemetry; property: "visible"; value: false }
                    PropertyAction { target: pinPrompt; property: "visible"; value: false }
                    PropertyAction { target: importPrompt; property: "visible"; value: false }
                }
            }
        ]
    }
}
