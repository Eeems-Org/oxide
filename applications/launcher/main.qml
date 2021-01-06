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
    Component.onCompleted:{
        stateController.state = "loaded"
        controller.startup();
        appsView.model = controller.getApps();
    }
    Connections {
        target: controller
        onReload: appsView.model = controller.getApps()
    }
    header: Rectangle {
        enabled: stateController.state === "loaded"
        color: "black"
        height: menu.height
        RowLayout {
            id: menu
            width: parent.width
            RowLayout {
                Layout.fillWidth: true
                CustomMenu {
                    BetterMenu {
                        id: optionsMenu
                        title: "";
                        font: iconFont.name
                        width: 310
                        Action { text: qsTr(" Reload"); onTriggered: appsView.model = controller.getApps() }
                        Action {
                            text: qsTr(" Import Apps");
                            onTriggered:{
                                controller.importDraftApps();
                                appsView.model = controller.getApps();
                            }
                        }
                        Action { text: qsTr(" Options"); onTriggered: stateController.state = "settings" }
                    }
                }
                StatusIcon {
                    source: "qrc:/img/notifications/white.png"
                    text: controller.notificationText
                    visible: controller.hasNotification
                    clip: true
                    Layout.maximumWidth: 300
                    MouseArea {
                        anchors.fill: parent
                        enabled: parent.visible
                        onClicked: stateController.state = "notifications"
                    }
                }
            }
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
                    text: controller.showWifiDb ? level + "dBm" : ""
                    MouseArea {
                        anchors.fill: parent
                        onClicked: stateController.state = "wifi"
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
                        return "qrc:/img/battery/" + icon + ".png";
                    }
                    text: (controller.showBatteryPercent ? level + "% " : "") + (controller.showBatteryTemperature ? temperature + "C" : "")
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
                            text: qsTr(" Restart")
                            enabled: !controller.powerOffInhibited
                            onTriggered: controller.restart()
                        }
                        Action {
                            text: qsTr(" Shutdown")
                            enabled: !controller.powerOffInhibited
                            onTriggered: controller.powerOff()
                        }
                        Action {
                            text: qsTr(" Lock")
                            onTriggered: controller.lock()
                        }
                    }
                }
            }
        }
        Label {
            objectName: "clock"
            anchors.centerIn: parent
            color: "white"
            MouseArea {
                anchors.fill: parent
                onClicked: stateController.state = "calendar"
            }
        }
    }
    background: Rectangle { color: "white" }
    contentData: [
        Rectangle {
            anchors.fill: parent
            color: "white"
        },
        GridView {
            id: appsView
            enabled: stateController.state === "loaded"
            objectName: "appsView"
            anchors.fill: parent
            clip: true
            snapMode: ListView.SnapOneItem
            maximumFlickVelocity: 0
            boundsBehavior: Flickable.StopAtBounds
            cellWidth: parent.width / controller.columns
            cellHeight: cellWidth + controller.fontSize
            model: apps
            ScrollBar.vertical: ScrollBar {
                id: scrollbar
                snapMode: ScrollBar.SnapAlways
                policy: ScrollBar.AlwaysOn
                contentItem: Rectangle {
                    color: "white"
                    implicitWidth: 6
                    implicitHeight: 100
                    radius: width / 2
                }
                background: Rectangle {
                    color: "black"
                    implicitWidth: 6
                    implicitHeight: 100
                    radius: width / 2
                }
            }
            delegate: Item {
                id: root
                enabled: appsView.enabled
                width: appsView.cellWidth
                height: appsView.cellHeight
                state: "released"
                states: [
                    State { name: "released" },
                    State { name: "pressed" }
                ]
                Image {
                    id: image
                    fillMode: Image.PreserveAspectFit
                    y: window.itemPadding
                    width: parent.width - window.itemPadding * 2
                    height: parent.width - window.itemPadding * 2 - controller.fontSize
                    source: model.modelData.imgFile
                    anchors.horizontalCenter: parent.horizontalCenter
                }
                Text {
                    id: name
                    text: (model.modelData.running ? "* " : "") + model.modelData.displayName
                    font.family: "Noto Serif"
                    font.italic: true
                    font.bold: model.modelData.running
                    font.pixelSize: controller.fontSize
                    width: parent.width - window.itemPadding * 2
                    anchors.top: image.bottom
                    anchors.horizontalCenter: image.horizontalCenter
                    horizontalAlignment: Text.AlignHCenter
                    clip: true
                }
                MouseArea {
                    anchors.fill: root
                    enabled: root.enabled
                    onPressed: root.state = "pressed"
                    onReleased: root.state = "released"
                    onCanceled: root.state = "released"
                    onClicked: {
                        root.state = "released"
                        model.modelData.execute();
                    }
                    onPressAndHold: {
                        itemInfo.model = model.modelData;
                        stateController.state = "itemInfo"
                    }
                }
                transitions: [
                    Transition {
                        from: "pressed"; to: "released"
                        ParallelAnimation {
                            PropertyAction { target: image; property: "width"; value: root.width - window.itemPadding * 2 }
                            PropertyAction { target: image; property: "height"; value: root.width - window.itemPadding * 2 - controller.fontSize }
                        }
                    },
                    Transition {
                        from: "released"; to: "pressed"
                        ParallelAnimation {
                            PropertyAction { target: image; property: "width"; value: image.width - 10}
                            PropertyAction { target: image; property: "height"; value: image.height - 10 }
                        }
                    }

                ]
            }
        },
        Popup {
            id: itemInfo
            visible: false
            x: (parent.width / 2) - (width / 2)
            y: (parent.height / 2) - (height / 2)
            closePolicy: Popup.CloseOnPressOutside
            onClosed: stateController.state = "loaded"
            property var model;
            property int textPadding: 10
            function getModel(){
                return itemInfo.model || {
                    imgFile: "",
                    name: "(Unknown)",
                    desc: "",
                    running: false
                }
            }
            width: {
                var calculatedWidth = itemImage.width + itemContent.width + (itemInfo.textPadding * 2);
                if(window.width <= calculatedWidth){
                    return window.width - 10;
                }
                var minWidth = itemAutoStartButton.width + itemImage.width;
                if(calculatedWidth < minWidth){
                    return minWidth;
                }

                return calculatedWidth;
            }
            height: itemContent.height + itemAutoStartButton.height
            contentItem: Item {
                anchors.fill: parent
                Item {
                    id: itemImage
                    width: itemContent.height
                    height: itemContent.height
                    Image {
                        fillMode: Image.PreserveAspectFit
                        source: itemInfo.model ? itemInfo.model.imgFile : ""
                        anchors.centerIn: parent
                        width: parent.with - itemInfo.textPadding
                        height: parent.height - itemInfo.textPadding
                    }
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                }
                ColumnLayout {
                    id: itemContent
                    anchors.left: itemImage.right
                    Label {
                        text: (itemInfo.model ? itemInfo.model.displayName : "")
                        topPadding: 0
                        leftPadding: itemInfo.textPadding
                        rightPadding: itemInfo.textPadding
                        bottomPadding: 0
                    }
                    Label {
                        text: (itemInfo.model ? itemInfo.model.desc : "")
                        topPadding: 0
                        leftPadding: itemInfo.textPadding
                        rightPadding: itemInfo.textPadding
                        bottomPadding: 0
                    }
                }
                Label {
                    id: itemAutoStartButton
                    visible: !!itemInfo.model
                    text: !!itemInfo.model && controller.autoStartApplication !== itemInfo.model.name
                          ? "Start this application on boot"
                          : "Don't start this application on boot"
                    anchors.bottom: parent.bottom
                    anchors.right: parent.right
                    padding: 5
                    background: Rectangle {
                        border.color: "black"
                        border.width: 1
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            if(!itemInfo.model){
                                return
                            }
                            var name = itemInfo.model.name
                            controller.autoStartApplication = controller.autoStartApplication !== name ? name : "";

                        }
                    }
                }
                Label {
                    id: itemCloseButton
                    visible: !!(itemInfo.model && itemInfo.model.running)
                    text: "Kill"
                    padding: 5
                    anchors.top: parent.top
                    anchors.right: parent.right
                    background: Rectangle {
                        border.color: "black"
                        border.width: 1
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            if(itemInfo.model){
                                itemInfo.model.stop();
                                appsView.model = controller.getApps();
                            }
                        }
                    }
                }
            }
        },
        SettingsPopup {
            id: settings
            onClosed: stateController.state = "loaded"
            visible: false
        },
        WifiMenu {
            id: wifi
            onClosed: stateController.state = "loaded"
            visible: false
            model: controller.networks
        },
        CalendarMenu {
            id: calendar
            onClosed: stateController.state = "loaded"
            visible: false
        },
        NotificationsPopup {
            id: notifications
            onClosed: stateController.state = "loaded"
            visible: false
            model: controller.notifications
        }

    ]
    StateGroup {
        id: stateController
        property string previousState;
        objectName: "stateController"
        state: "loading"
        states: [
            State { name: "loading" },
            State { name: "loaded" },
            State { name: "settings" },
            State { name: "itemInfo" },
            State { name: "wifi" },
            State { name: "calendar" },
            State { name: "notifications" }
        ]
        transitions: [
            Transition {
                from: "*"; to: "settings"
                SequentialAnimation {
                    ScriptAction { script: stateController.previousState = "settings" }
                    ScriptAction { script: console.log("Opening settings") }
                    PropertyAction { target: settings; property: "visible"; value: true }
                    PropertyAction { target: wifi; property: "visible"; value: false }
                    PropertyAction { target: calendar; property: "visible"; value: false }
                    PropertyAction { target: notifications; property: "visible"; value: false }
                    PropertyAction { target: menu; property: "focus"; value: false }
                }
            },
            Transition {
                from: "*"; to: "wifi"
                ParallelAnimation {
                    ScriptAction { script: stateController.previousState = "wifi" }
                    ScriptAction { script: console.log("Opening wifi menu") }
                    PropertyAction { target: wifi; property: "visible"; value: true }
                    PropertyAction { target: calendar; property: "visible"; value: false }
                    PropertyAction { target: settings; property: "visible"; value: false }
                    PropertyAction { target: notifications; property: "visible"; value: false }
                    PropertyAction { target: menu; property: "focus"; value: false }
                }
            },
            Transition {
                from: "*"; to: "calendar"
                SequentialAnimation {
                    ScriptAction { script: stateController.previousState = "calendar" }
                    ScriptAction { script: console.log("Opening calendar") }
                    PropertyAction { target: calendar; property: "visible"; value: true }
                    PropertyAction { target: settings; property: "visible"; value: false }
                    PropertyAction { target: wifi; property: "visible"; value: false }
                    PropertyAction { target: notifications; property: "visible"; value: false }
                    PropertyAction { target: menu; property: "focus"; value: false }
                }
            },
            Transition {
                from: "*"; to: "notifications"
                SequentialAnimation {
                    ScriptAction { script: stateController.previousState = "notifications" }
                    ScriptAction { script: console.log("Opening notifications") }
                    PropertyAction { target: notifications; property: "visible"; value: true }
                    PropertyAction { target: calendar; property: "visible"; value: false }
                    PropertyAction { target: settings; property: "visible"; value: false }
                    PropertyAction { target: wifi; property: "visible"; value: false }
                    PropertyAction { target: menu; property: "focus"; value: false }
                }
            },
            Transition {
                from: "loaded"; to: "itemInfo"
                SequentialAnimation {
                    ScriptAction { script: console.log("Viewing item info") }
                    PropertyAction { target: itemInfo; property: "visible"; value: true }
                    PropertyAction { target: menu; property: "focus"; value: false }
                }
            },
            Transition {
                from: "*"; to: "loaded"
                SequentialAnimation {
                    ScriptAction { script: console.log("Main display") }
                    PropertyAction { target: calendar; property: "visible"; value: false }
                    PropertyAction { target: settings; property: "visible"; value: false }
                    PropertyAction { target: wifi; property: "visible"; value: false }
                    PropertyAction { target: notifications; property: "visible"; value: false }
                    PropertyAction { target: menu; property: "focus"; value: false }
                    PropertyAction { target: appsView; property: "focus"; value: true }
                }
            }
        ]
    }
}
