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
            text: controller.clockText
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
            source: {
                var icon;
                if(controller.wifiStateStr === "unknown"){
                    icon = "unknown";
                }else if(controller.wifiStateStr === "down"){
                    icon = "down";
                }else if(!controller.wifiConnected){
                    icon = "disconnected";
                }else if(controller.wifiRssi > -50) {
                    icon = "4_bar";
                }else if(controller.wifiRssi > -60){
                    icon = "3_bar";
                }else if(controller.wifiRssi > -70){
                    icon = "2_bar";
                }else if(controller.wifiRssi > -80){
                    icon = "1_bar";
                }else{
                    icon = "0_bar";
                }
                return "qrc:/codes.eeems.oxide/img/wifi/" + icon + ".png";
            }
            text: controller.showWifiDb ? controller.wifiRssi + "dBm" : ""
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
            source: {
                var icon = "";
                if(controller.batteryAlert || !controller.batteryPresent){
                    icon = "alert";
                }else if(controller.batteryWarning){
                    icon = "unknown";
                }else{
                    if(controller.batteryCharging || controller.chargerConnected){
                        icon = "charging_";
                    }
                    if(controller.batteryLevel < 25){
                        icon += "20";
                    }else if(controller.batteryLevel < 35){
                        icon += "30";
                    }else if(controller.batteryLevel < 55){
                        icon += "50";
                    }else if(controller.batteryLevel < 65){
                        icon += "60";
                    }else if(controller.batteryLevel < 85){
                        icon += "80";
                    }else if(controller.batteryLevel < 95){
                        icon += "90";
                    }else{
                        icon += 100;
                    }
                }
                return "qrc:/codes.eeems.oxide/img/battery/" + icon + ".png";
            }
            text: (controller.showBatteryPercent ? controller.batteryLevel + "% " : "") +
                (controller.showBatteryTemperature ? controller.batteryTemperature + "C" : "")
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
