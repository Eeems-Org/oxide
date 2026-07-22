import QtQuick 2.10
import QtQuick.Window 2.3
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.0
import "qrc:/codes.eeems.oxide"

OxideWindow {
    id: window
    objectName: "window"
    visible: controller.visible
    title: qsTr("Stain")
    focus: true
    color: "transparent"
    headerBackgroundColor: "white"
    FontLoader { id: iconFont; source: "/font/icomoon.ttf" }
    Binding {
        target: Oxide
        property: "topSystemSpace"
        value: page.header.height
        when: page.header.height > 0
    }
    // function onVisibleChanged(visible){
    //     if(visible){
    //         window.raise()
    //     }else{
    //         window.lower()
    //     }
    // }
    // Component.onCompleted: window.onVisibleChanged(controller.visible)
    // Connections {
    //     target: controller
    //     function onVisibleChanged(visible){ window.onVisibleChanged(visible); }
    // }
    leftMenu: [
        AbstractButton {
            leftPadding: 4
            topPadding: 4
            bottomPadding: 4
            Layout.maximumWidth: 300
            enabled: controller.enabled && contentItem.visible
            contentItem: OxideStatusIcon {
                source: "qrc:/img/notifications/black.png"
                text: controller.notificationText
                visible: controller.hasNotification && controller.enabled
                clip: true
                Layout.maximumWidth: 300
                Layout.alignment: Qt.AlignCenter
            }
            background: Rectangle{
                implicitWidth: 40
                implicitHeight: 40
                color: "transparent"
            }
            onClicked: {
                console.log("notifications display clicked")
                controller.openSettings("notifications")
            }
        }
    ]
    centerMenu: [
        Label {
            objectName: "clock"
            Layout.alignment: Qt.AlignCenter
            MouseArea {
                anchors.fill: parent
                enabled: controller.enabled
                onClicked: {
                    console.log("clocked clicked")
                    // TODO open calendar app
                }
            }
        }
    ]
    rightMenu: [
        OxideStatusIcon {
            id: wifiState
            objectName: "wifiState"
            Layout.alignment: Qt.AlignCenter
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
                return "qrc:/codes.eeems.oxide/img/wifi/" + icon + ".png";
            }
            text: controller.showWifiDb ? rssi + "dBm" : ""
            MouseArea {
                anchors.fill: parent
                enabled: controller.enabled
                onClicked: {
                    console.log("wifi icon clicked")
                    controller.openSettings("wifi")
                }
            }
        },
        OxideStatusIcon {
            id: batteryLevel
            objectName: "batteryLevel"
            Layout.alignment: Qt.AlignCenter
            property bool alert: false
            property bool warning: false
            property bool charging: false
            property bool connected: false
            property bool present: true
            property int level: 0
            property int temperature: 0
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
                return "qrc:/codes.eeems.oxide/img/battery/" + icon + ".png";
            }
            text: (controller.showBatteryPercent ? level + "% " : "") + (controller.showBatteryTemperature ? temperature + "C" : "")
            MouseArea {
                anchors.fill: parent
                enabled: controller.enabled
                onClicked: {
                    console.log("battery icon clicked")
                    // TODO open battery app
                }
            }
        },
        AbstractButton {
            topPadding: 4
            bottomPadding: 4
            contentItem: Text{
                text: qsTr("")
                font.family: iconFont.name
                font.pixelSize: 32
                horizontalAlignment: Text.AlignLeft
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
            background: Rectangle{
                implicitWidth: 40
                implicitHeight: 40
                color: "transparent"
            }
            onClicked: {
                console.log("power icon clicked")
                controller.showPowerMenu()
            }
        }
    ]
}
