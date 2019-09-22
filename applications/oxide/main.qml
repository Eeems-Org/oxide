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
    property bool reloaded: false
    background: Rectangle {
        color: "white"
    }

    header: ToolBar {
        contentHeight: title.implicitHeight
        width: window.width
        background: Rectangle { color: "black" }

        Label {
            id: title
            text: window.title
            color: "white"
            anchors.centerIn: parent
        }
    }
    MouseArea { anchors.fill: parent }
    Column {
        id: appsView
        objectName: "appsView"
        anchors.fill: parent
        spacing: -1
    }
    StateGroup {
        state: "loaded"
        states: [
            State { name: "loaded"; when: !window.reloaded },
            State { name: "reloading"; when: window.reloaded }
        ]
        transitions: [
            Transition {
                from: "loaded"; to: "reloading"
                SequentialAnimation {
                    PropertyAction { target: appsView; property: "visible"; value: true }
                    PropertyAction { target: window.header; property: "visible"; value: true }
                    PauseAnimation { duration: 10 }
                    PropertyAction { target: appsView; property: "visible"; value: false }
                    PropertyAction { target: window.header; property: "visible"; value: false }
                    PropertyAction { target: window; property: "reloaded"; value: false }
                }
            },
            Transition {
                from: "reloading"; to: "loaded"
                SequentialAnimation {
                    PropertyAction { target: appsView; property: "visible"; value: false }
                    PropertyAction { target: window.header; property: "visible"; value: false }
                    PauseAnimation { duration: 10 }
                    PropertyAction { target: appsView; property: "visible"; value: true }
                    PropertyAction { target: window.header; property: "visible"; value: true }
                }
            }
        ]
    }
}
