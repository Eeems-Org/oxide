import QtQuick 2.10
import QtQuick.Window 2.3
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.0
import "widgets"

ApplicationWindow {
    id: window
    objectName: "window"
    visible: stateController.state != "loading"
    width: screenGeometry.width
    height: screenGeometry.height
    title: qsTr("Oxide")
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
                    property int link: 0
                    property int level: 0
                    property bool connected: false
                    source: {
                        var icon;
                        if(state === "unknown"){
                            icon = "unknown";
                        }else if(state === "down"){
                            icon = "down";
                        }else if(!connected){
                            icon = "disconnected";
                        }else if(link < 20){
                            icon = "0_bar";
                        }else if(link < 40){
                            icon = "1_bar";
                        }else if(link < 60){
                            icon = "2_bar";
                        }else if(link < 80){
                            icon = "3_bar";
                        }else{
                            icon = "4_bar";
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
                        title: "";
                        font: iconFont.name
                        width: 260
                        Action {
                            text: qsTr(" Suspend")
                            enabled: !controller.sleepInhibited
                            onTriggered: controller.suspend();
                        }
                        Action {
                            text: qsTr(" Shutdown")
                            enabled: !controller.powerOffInhibited
                            onTriggered: controller.powerOff()
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
            visible: stateController.state != "import"
            anchors.centerIn: parent
            label: {
                switch(stateController.state){
                    case "firstLaunch":
                        return "Set PIN";
                    case "confirmPin":
                        return "Confirm PIN";
                    default:
                        return "PIN";
                }
            }
            onSubmit: {
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
        },
        Popup {
            visible: stateController.state == "import"
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
                        onClicked: stateController.state = "firstLaunch"
                    }
                    BetterButton {
                        text: "Import"
                        width: height * 2
                        Layout.fillWidth: true
                        onClicked: controller.importPin()
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
            State { name: "firstLaunch" },
            State { name: "confirmPin" },
            State { name: "import" },
            State { name: "loading" }
        ]
        transitions: [
            Transition {
                from: "*"; to: "loaded"
                SequentialAnimation {
                    ScriptAction { script: {
                        console.log("PIN Entry");
                        pinEntry.value = "";
                    } }
                }
            },
            Transition {
                from: "*"; to: "firstLaunch"
                SequentialAnimation {
                    ScriptAction { script: {
                        console.log("PIN Setup");
                        pinEntry.value = "";
                    } }
                }
            },
            Transition {
                from: "*"; to: "confirmPin"
                SequentialAnimation {
                    ScriptAction { script: {
                        console.log("PIN Confirmation");
                        pinEntry.value = "";
                    } }
                }
            },
            Transition {
                from: "*"; to: "import"
                SequentialAnimation {
                    ScriptAction { script: {
                        console.log("Import PIN");
                    } }
                }
            },
            Transition {
                from: "*"; to: "loading"
                SequentialAnimation {
                    ScriptAction { script: {
                        console.log("Loading display");
                        controller.startup();
                    } }
                }
            }
        ]
    }
}
