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
    property bool reloaded: true
    property int itemPadding: 10
    FontLoader { id: iconFont; source: "/font/icomoon.ttf" }

    header: Rectangle {
        color: "black"
        height: menu.height
        RowLayout {
            id: menu
            width: parent.width
            CustomMenu {
                Menu {
                    title: "";
                    font: iconFont.name
                    width: 250
                    Action { text: qsTr(" Reload"); onTriggered: appsView.model = controller.getApps() }
                    Action { text: qsTr(" Options"); onTriggered: stateController.state = "settings" }
                }
            }
            Item { Layout.fillWidth: true }
            CustomMenu {
                Menu {
                    title: "";
                    font: iconFont.name
                    width: 250
                    Action { text: qsTr(" Suspend"); onTriggered: stateController.state = "suspended" }
                    Action { text: qsTr(" Shutdown"); onTriggered: controller.powerOff() }
                }
            }

        }
    }
    background: Rectangle {
        color: "white"
    }
    contentData: [
        MouseArea { anchors.fill: parent },
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
                    text: model.modelData.name
                    font.family: "Noto Serif"
                    font.italic: true
                    font.pixelSize: controller.fontSize
                    width: parent.width - window.itemPadding * 2
                    anchors.top: image.bottom
                    anchors.horizontalCenter: image.horizontalCenter
                    horizontalAlignment: Text.AlignHCenter
                    clip: true
                }
//                Text {
//                    id: description
//                    anchors.top: name.bottom
//                    anchors.left: name.left
//                    font.family: "Noto Serif"
//                    font.pixelSize: 40
//                    font.italic: true
//                    text: model.modelData.desc
//                }
                MouseArea {
                    width: root.width
                    height: appsView.cellHeight
                    enabled: root.enabled
                    onPressed: root.state = "pressed"
                    onReleased: root.state = "released"
                    onCanceled: root.state = "released"
                    onClicked: {
                        model.modelData.execute();
                        stateController.state = "loading"
                        root.state = "released"
                    }
                }
                transitions: [
                    Transition {
                        from: "pressed"; to: "released"
                        ParallelAnimation {
                            PropertyAction { target: image; property: "width"; value: root.width - window.itemPadding * 2 }
                            PropertyAction { target: image; property: "height"; value: root.width - window.itemPadding * 2 - controller.fontSize }
                            SequentialAnimation {
                                PauseAnimation { duration: 1500 }
                                ScriptAction { script: controller.killXochitl() }
                            }
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
            id: settings
            visible: false
            width: 1000
            height: 1000
            x: (parent.width / 2) - (width / 2)
            y: (parent.height / 2) - (height / 2)
            closePolicy: Popup.NoAutoClose
            focus: true
            onClosed: stateController.state = "loaded"
            GridLayout {
                columns: 3
                rows: 10
                anchors.fill: parent
                RowLayout {
                    Layout.columnSpan: parent.columns
                    Label {
                        text: "Automatic sleep"
                        Layout.columnSpan: parent.columns - 1
                        Layout.fillWidth: true
                    }
                    BetterCheckBox {
                        tristate: false
                        checkState: controller.automaticSleep ? Qt.Checked : Qt.Unchecked
                        onClicked: controller.automaticSleep = this.checkState === Qt.Checked
                        Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                        Layout.fillWidth: false
                    }
                }
                RowLayout {
                    Layout.columnSpan: 2
                    Label {
                        text: "Home Screen Columns"
                        Layout.fillWidth: true
                        Layout.preferredWidth: parent.width / 2
                    }
                    BetterSpinBox {
                        id: columnsSpinBox
                        objectName: "columnsSpinBox"
                        from: 2
                        to: 10
                        stepSize: 2
                        onValueChanged: controller.columns = this.value
                        Layout.fillWidth: true
                        Layout.preferredWidth: parent.width / 2
                    }
                }
                RowLayout {
                    Layout.columnSpan: 2
                    Label {
                        text: "Font Size"
                        Layout.fillWidth: true
                        Layout.preferredWidth: parent.width / 2
                    }
                    BetterSpinBox {
                        id: fontSizeSpinBox
                        objectName: "fontSizeSpinBox"
                        from: 20
                        to: 35
                        stepSize: 3
                        onValueChanged: controller.fontSize = this.value
                        Layout.fillWidth: true
                        Layout.preferredWidth: parent.width / 2
                    }
                }
                Item {
                    Layout.rowSpan: 6
                    Layout.columnSpan: parent.columns
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                }
                BetterButton {
                    text: "Reset"
                    Layout.columnSpan: parent.columns
                    Layout.fillWidth: true
                    onClicked: controller.loadSettings()
                }
                BetterButton {
                    text: "Close"
                    Layout.columnSpan: parent.columns
                    Layout.fillWidth: true
                    onClicked: {
                        controller.saveSettings();
                        settings.close();
                    }
                }
            }
        },
        Popup {
            id: suspendMessage
            visible: false
            width: 250
            height: 250
            x: (parent.width / 2) - (width / 2)
            y: (parent.height / 2) - (height / 2)
            modal: true
            background: Rectangle {
                color: "black"
                anchors.fill: parent
            }
            Text {
                anchors.centerIn: parent
                color: "white"
                text: "Suspended..."
            }
        }

    ]
    footer: ToolBar {
        background: Rectangle { color: "black" }
        contentHeight: title.implicitHeight
        Label {
            id: title
            text: window.title
            color: "white"
            anchors.centerIn: parent
        }
        Label {
            id: batteryLevel
            objectName: "batteryLevel"
            property string batterylevel: controller.getBatteryLevel()
            font: iconFont.name
            text: "" + batterylevel + "%"
            color: "white"
            anchors.right: parent.right
            rightPadding: 10
        }
    }
    Component.onCompleted: stateController.state = "loaded"
    StateGroup {
        id: stateController
        objectName: "stateController"
        state: "loading"
        states: [
            State { name: "loaded" },
            State { name: "settings" },
            State { name: "loading" },
            State { name: "suspended" }
        ]
        transitions: [
            Transition {
                from: "loaded"; to: "loading"
                SequentialAnimation {
                    ParallelAnimation {
                        PropertyAction { target: window; property: "visible"; value: true }
                        PropertyAction { target: window.contentItem; property: "visible"; value: true }
                    }
                    PauseAnimation { duration: 10 }
                    ParallelAnimation {
                        PropertyAction { target: window; property: "visible"; value: false }
                        PropertyAction { target: window.contentItem; property: "visible"; value: false }
                        PropertyAction { target: stateController; property: "state"; value: "loaded" }
                    }
                    ScriptAction { script: console.log("loading...") }
                }
            },
            Transition {
                from: "loading"; to: "loaded"
                SequentialAnimation {
                    ParallelAnimation {
                        PropertyAction { target: window; property: "visible"; value: false }
                        PropertyAction { target: window.contentItem; property: "visible"; value: false }
                    }
                    PauseAnimation { duration: 10 }
                    ParallelAnimation {
                        PropertyAction { target: window; property: "visible"; value: true }
                        PropertyAction { target: window.contentItem; property: "visible"; value: true }
                    }
                    ScriptAction { script: console.log("loaded.") }
                }
            },
            Transition {
                from: "settings"; to: "suspended"
                SequentialAnimation {
                    ParallelAnimation {
                        PropertyAction { target: window; property: "visible"; value: true }
                        PropertyAction { target: window.contentItem; property: "visible"; value: true }
                        PropertyAction { target: suspendMessage; property: "visible"; value: true }
                    }
                    PauseAnimation { duration: 1000 }
                    ScriptAction { script: console.log("suspending...") }
                    ScriptAction { script: controller.suspend() }
                    PauseAnimation { duration: 1000 }
                    ScriptAction { script: controller.resetInactiveTimer() && console.log("waking up...") }
                    ParallelAnimation {
                        PropertyAction { target: suspendMessage; property: "visible"; value: false }
                        PropertyAction { target: stateController; property: "state"; value: "settings" }
                    }
                }
            },
            Transition {
                from: "loaded"; to: "suspended"
                SequentialAnimation {
                    ParallelAnimation {
                        PropertyAction { target: window; property: "visible"; value: true }
                        PropertyAction { target: window.contentItem; property: "visible"; value: true }
                        PropertyAction { target: suspendMessage; property: "visible"; value: true }
                    }
                    PauseAnimation { duration: 1000 }
                    ScriptAction { script: console.log("suspending...") }
                    ScriptAction { script: controller.suspend() }
                    PauseAnimation { duration: 1000 }
                    ScriptAction { script: controller.resetInactiveTimer() && console.log("waking up...") }
                    ParallelAnimation {
                        PropertyAction { target: suspendMessage; property: "visible"; value: false }
                        PropertyAction { target: stateController; property: "state"; value: "loaded" }
                    }
                }
            },
            Transition {
                from: "loaded"; to: "settings"
                SequentialAnimation {
                    ParallelAnimation {
                        PropertyAction { target: window; property: "visible"; value: true }
                        PropertyAction { target: window.contentItem; property: "visible"; value: true }
                        PropertyAction { target: settings; property: "visible"; value: true }
                    }
                    PropertyAction { target: menu; property: "focus"; value: false }
                }
            },
            Transition {
                from: "settings"; to: "loaded"
                SequentialAnimation {
                    ParallelAnimation {
                        PropertyAction { target: window; property: "visible"; value: true }
                        PropertyAction { target: window.contentItem; property: "visible"; value: true }
                        PropertyAction { target: settings; property: "visible"; value: false }
                    }
                    PropertyAction { target: appsView; property: "focus"; value: true }
                }
            }
        ]
    }
}
