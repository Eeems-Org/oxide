import QtQuick 2.10
import QtQuick.Window 2.3
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.0
import "widgets"

ApplicationWindow {
    id: window
    objectName: "window"
    visible: true
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
            id: background
            anchors.fill: parent
            color: "black"
        },
        GridLayout {
            id: pinEntry
            anchors.centerIn: parent
            rowSpacing: children[3].width / 2
            columnSpacing: children[3].width / 2
            columns: 3
            rows: 6

            RowLayout {
                Layout.columnSpan: parent.columns
                spacing: parent.columnSpacing

                Label {
                    text: "PIN"
                    color: "white"
                }

                RowLayout {
                    spacing: parent.parent.columnSpacing
                    property int itemSize: 50

                    Rectangle {
                        width: parent.itemSize
                        height: width
                        color: controller.pin.length > 0 ? "white" : "black"
                        border.color: "white"
                        border.width: 1
                        radius: width / 2
                    }
                    Rectangle {
                        width: parent.itemSize
                        height: width
                        color: controller.pin.length > 1 ? "white" : "black"
                        border.color: "white"
                        border.width: 1
                        radius: width / 2
                    }
                    Rectangle {
                        width: parent.itemSize
                        height: width
                        color: controller.pin.length > 2 ? "white" : "black"
                        border.color: "white"
                        border.width: 1
                        radius: width / 2
                    }
                    Rectangle {
                        width: parent.itemSize
                        height: width
                        color: controller.pin.length > 3 ? "white" : "black"
                        border.color: "white"
                        border.width: 1
                        radius: width / 2
                    }
                }
            }

            Item { Layout.columnSpan: parent.columns; Layout.fillHeight: true }

            PinButton { text: "7"; onClicked: controller.pin += text; enabled: controller.pin.length < 4 }
            PinButton { text: "8"; onClicked: controller.pin += text; enabled: controller.pin.length < 4 }
            PinButton { text: "9"; onClicked: controller.pin += text; enabled: controller.pin.length < 4 }

            PinButton { text: "4"; onClicked: controller.pin += text; enabled: controller.pin.length < 4 }
            PinButton { text: "5"; onClicked: controller.pin += text; enabled: controller.pin.length < 4 }
            PinButton { text: "6"; onClicked: controller.pin += text; enabled: controller.pin.length < 4 }

            PinButton { text: "1"; onClicked: controller.pin += text; enabled: controller.pin.length < 4 }
            PinButton { text: "2"; onClicked: controller.pin += text; enabled: controller.pin.length < 4 }
            PinButton { text: "3"; onClicked: controller.pin += text; enabled: controller.pin.length < 4 }

            Item { Layout.fillWidth: true }
            PinButton { text: "0"; onClicked: controller.pin += text; enabled: controller.pin.length < 4 }
            PinButton {
                contentItem: Item {
                    Image {
                        anchors.centerIn: parent
                        width: parent.width / 2
                        height: width
                        source: "qrc:/img/backspace.png"
                        fillMode: Image.PreserveAspectFit
                    }
                }
                hideBorder: true
                onClicked: controller.pin = controller.pin.slice(0, -1)
                enabled: controller.pin.length
            }
        }
    ]
    StateGroup {
        id: stateController
        objectName: "stateController"
        state: "loading"
        states: [
            State { name: "loaded" },
            State { name: "loading" }
        ]
        transitions: [
            Transition {
                from: "*"; to: "loaded"
                SequentialAnimation {
                    ScriptAction { script: console.log("Main display") }
                    PropertyAction { target: background; property: 'visible'; value: true }
                    PropertyAction { target: pinEntry; property: 'visible'; value: true }
                }
            },
            Transition {
                from: "*"; to: "loading"
                SequentialAnimation {
                    ScriptAction { script: console.log("Loading display") }
                    PropertyAction { target: background; property: 'visible'; value: false }
                    PropertyAction { target: pinEntry; property: 'visible'; value: false }
                    ScriptAction { script: controller.startup() }
                }
            }
        ]
    }
}
