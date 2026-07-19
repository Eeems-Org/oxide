import QtQuick 2.10
import QtQuick.Window 2.3
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.0
import "qrc:/codes.eeems.oxide"
import "widgets"

OxideWindow {
    id: window
    objectName: "window"
    visible: true
    title: qsTr("Oxide")
    focus: true
    headerBackgroundColor: "white"
    color: "black"
    backgroundColor: "white"
    reserveSystemSpace: true
    FontLoader { id: iconFont; source: "/font/icomoon.ttf" }
    onAfterSynchronizing: {
        if (stateController.state == "loading") {
            stateController.state = "loaded"
            controller.startup();
            appsView.model = controller.getApps();
        }
    }
    Connections {
        target: controller
        function onReload() {
            appsView.model = controller.getApps();
        }
    }
    Shortcut{
        sequence: "Option+L"
        context: Qt.ApplicationShortcut
        onActivated: controller.lock()
    }
    Shortcut{
        sequence: "Option+S"
        context: Qt.ApplicationShortcut
        onActivated: controller.suspend()
    }
    Shortcut{
        sequence: StandardKey.Refresh
        context: Qt.ApplicationShortcut
        onActivated: controller.reload()
    }
    Shortcut{
        sequence: "Ctrl+I"
        context: Qt.ApplicationShortcut
        onActivated: controller.importDraftApps()
    }
    Shortcut{
        sequence: StandardKey.Quit
        context: Qt.ApplicationShortcut
        onActivated: Qt.quit()
    }
    leftMenu: [
        CustomMenu {
            OxideMenu {
                id: optionsMenu
                title: qsTr("");
                font.family: iconFont.name
                width: 310
                Action {
                    text: qsTr(" Reload")
                    onTriggered: {
                        controller.breadcrumb("menu.reload", "click", "ui");
                        controller.startup();
                        appsView.model = controller.getApps();
                    }
                }
                Action {
                    text: qsTr(" Import Apps")
                    onTriggered:{
                        controller.breadcrumb("menu.import", "click", "ui");
                        controller.importDraftApps();
                        appsView.model = controller.getApps();
                    }
                }
            }
        }
    ]
    background: Rectangle { color: window.backgroundColor }
    initialItem: Item{
        anchors.fill: parent
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
            cellHeight: cellWidth
            model: apps
            ScrollBar.vertical: ScrollBar {
                id: scrollbar
                snapMode: ScrollBar.SnapAlways
                policy: ScrollBar.AlwaysOn
                contentItem: Rectangle {
                    color: window.color
                    implicitWidth: 6
                    implicitHeight: 100
                    radius: width / 2
                }
                background: Rectangle {
                    color: window.backgroundColor
                    implicitWidth: 6
                    implicitHeight: 100
                    radius: width / 2
                }
            }
            delegate: AppItem {
                enabled: appsView.enabled
                imgFile: model.modelData.imgFile
                text: (model.modelData.running ? "* " : "") + model.modelData.displayName
                bold: model.modelData.running
                width: appsView.cellWidth
                height: appsView.cellHeight
                onLongPress: {
                    controller.breadcrumb("appItem", "longPress", "ui");
                    itemInfo.model = model.modelData;
                    stateController.state = "itemInfo"
                }
                onClicked: {
                    controller.breadcrumb("appItem", "click", "ui");
                    model.modelData.execute();
                }
            }
        }
        Popup {
            id: itemInfo
            visible: false
            x: (parent.width / 2) - (width / 2)
            y: (parent.height / 2) - (height / 2)
            closePolicy: Popup.CloseOnPressOutside
            font.pixelSize: 32
            background: Rectangle {
                color: "white"
                border.color: "black"
                border.width: 2
                radius: 10
            }
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
            height: itemContent.height + itemAutoStartButton.height + 10
            contentItem: Item {
                anchors.fill: parent
                anchors.margins: 5
                Item {
                    id: itemImage
                    width: itemContent.height
                    height: itemContent.height
                    Image {
                        fillMode: Image.PreserveAspectFit
                        source: itemInfo.model ? (itemInfo.model.imgFile || "qrc:/img/icon.png") : ""
                        anchors.centerIn: parent
                        width: parent.width - itemInfo.textPadding
                        height: parent.height - itemInfo.textPadding
                    }
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                }
                ColumnLayout {
                    id: itemContent
                    anchors.left: itemImage.right
                    anchors.right: parent.right
                    Label {
                        text: (itemInfo.model ? itemInfo.model.displayName : "")
                        topPadding: 0
                        leftPadding: itemInfo.textPadding
                        rightPadding: itemInfo.textPadding
                        bottomPadding: 0
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Label {
                        text: (itemInfo.model ? itemInfo.model.desc : "")
                        topPadding: 0
                        leftPadding: itemInfo.textPadding
                        rightPadding: itemInfo.textPadding
                        bottomPadding: 0
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
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
                            controller.breadcrumb("appItem.autoStart", "click", "ui");
                            if(!itemInfo.model){
                                return
                            }
                            var name = itemInfo.model.name
                            controller.autoStartApplication = controller.autoStartApplication !== name ? name : "";
                            controller.saveSettings();
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
                            controller.breadcrumb("appItem.kill", "click", "ui");
                            if(itemInfo.model){
                                itemInfo.model.stop();
                                appsView.model = controller.getApps();
                            }
                        }
                    }
                }
            }
        }
        CalendarMenu {
            id: calendar
            onClosed: stateController.state = "loaded"
            visible: false
        }
    }
    StateGroup {
        id: stateController
        property string previousState;
        objectName: "stateController"
        state: "loading"
        states: [
            State { name: "loading" },
            State { name: "loaded" },
            State { name: "itemInfo" },
            State { name: "calendar" }
        ]
        transitions: [
            Transition {
                from: "*"; to: "calendar"
                SequentialAnimation {
                    ScriptAction { script: {
                        controller.breadcrumb("navigation", "calendar", "navigation");
                        stateController.previousState = "calendar";
                    } }
                    ScriptAction { script: console.log("Opening calendar") }
                    PropertyAction { target: calendar; property: "visible"; value: true }
                }
            },
            Transition {
                from: "loaded"; to: "itemInfo"
                SequentialAnimation {
                    ScriptAction { script: {
                        controller.breadcrumb("navigation", "itemInfo", "navigation");
                        console.log("Viewing item info");
                    } }
                    PropertyAction { target: itemInfo; property: "visible"; value: true }
                }
            },
            Transition {
                from: "*"; to: "loaded"
                SequentialAnimation {
                    ScriptAction { script: {
                        controller.breadcrumb("navigation", "main", "navigation");
                        console.log("Main display");
                    } }
                    PropertyAction { target: calendar; property: "visible"; value: false }
                }
            }
        ]
    }
}
