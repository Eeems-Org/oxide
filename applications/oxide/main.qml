import QtQuick 2.10
import QtQuick.Window 2.3
import QtQuick.Controls 2.4

ApplicationWindow {
    id: window
    objectName: "window"
    visible: true
    width: screenGeometry.width
    height: screenGeometry.height
    title: qsTr("Oxide")
    property bool reloaded: true
    menuBar: MenuBar {
//        background: Rectangle { color: "black" }
        Menu {
            title: qsTr("File")
            Action { text: qsTr("Shutdown") }
            Action { text: qsTr("Reload"); onTriggered: appsView.model = controller.getApps() }
            MenuSeparator {}
            Action { text: qsTr("Quit"); onTriggered: Qt.quit() }
        }
    }
    header: TabBar {
//        background: Rectangle { color: "black" }
        TabButton { text: qsTr("Applications") }
        TabButton { text: qsTr("Settings") }
    }
    background: Rectangle {
        color: "white"
    }
    contentData: [
        MouseArea { anchors.fill: parent },
        ListView {
            id: appsView
            objectName: "appsView"
            anchors.fill: parent
            clip: true
            snapMode: ListView.SnapOneItem
            maximumFlickVelocity: 0
            boundsBehavior: Flickable.StopAtBounds
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
            model: apps
            delegate: Rectangle {
                id: root
                width: parent.width - scrollbar.width
                height: 180
                color: "white"
                border.color: "#cccccc"
                border.width: 3
                state: "released"
                states: [
                    State { name: "released" },
                    State { name: "pressed" }
                ]

                Text {
                    id: name
                    anchors.top: parent.top
                    anchors.left: parent.left
                    font.family: "Noto Serif"
                    font.pixelSize: 80
                    font.italic: true
                    text: model.modelData.name
                }
                Text {
                    id: description
                    anchors.top: name.bottom
                    anchors.left: name.left
                    font.family: "Noto Serif"
                    font.pixelSize: 40
                    font.italic: true
                    text: model.modelData.desc
                }
                Image {
                    fillMode: Image.PreserveAspectFit
                    width: height
                    anchors.top: parent.top
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: 15
                    source: model.modelData.imgFile
                }
                MouseArea {
                    anchors.fill: parent
                    onPressed: parent.state = "pressed"
                    onReleased: parent.state = "released"
                    onCanceled: parent.state = "released"
                    onClicked: {
                        model.modelData.execute();
                        stateController.state = "loading"
                        parent.state = "released"
                    }
                }
                transitions: [
                    Transition {
                        from: "pressed"; to: "released"
                        SequentialAnimation {
                            PropertyAction { target: root; property: "color"; value: "white" }
                            PropertyAction { target: name; property: "color"; value: "black" }
                            PropertyAction { target: description; property: "color"; value: "black" }
                        }
                    },
                    Transition {
                        from: "released"; to: "pressed"
                        SequentialAnimation {
                            PropertyAction { target: root; property: "color"; value: "black" }
                            PropertyAction { target: name; property: "color"; value: "white" }
                            PropertyAction { target: description; property: "color"; value: "white" }
                        }
                    }
                ]
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
    }
    StateGroup {
        id: stateController
        state: "loaded"
        states: [
            State { name: "loaded" },
            State { name: "loading" }
        ]
        transitions: [
            Transition {
                from: "loaded"; to: "loading"
                SequentialAnimation {
                    PropertyAction { target: window; property: "visible"; value: true }
                    PropertyAction { target: window.contentItem; property: "visible"; value: true }
                    PauseAnimation { duration: 10 }
                    PropertyAction { target: window; property: "visible"; value: false }
                    PropertyAction { target: window.contentItem; property: "visible"; value: false }
                    PropertyAction { target: stateController; property: "state"; value: "loaded" }
                    ScriptAction { script: console.log("loading...") }
                }
            },
            Transition {
                from: "loading"; to: "loaded"
                SequentialAnimation {
                    PropertyAction { target: window; property: "visible"; value: false }
                    PropertyAction { target: window.contentItem; property: "visible"; value: false }
                    PauseAnimation { duration: 10 }
                    PropertyAction { target: window; property: "visible"; value: true }
                    PropertyAction { target: window.contentItem; property: "visible"; value: true }
                    ScriptAction { script: console.log("loaded.") }
                }
            }
        ]
    }
}
