import QtQuick 2.10
import QtQuick.Window 2.3
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.0

ApplicationWindow {
    id: window
    objectName: "window"
    visible: stateController.state != "loading"
    width: screenGeometry.width
    height: screenGeometry.height
    title: qsTr("Corrupt")
    Connections{
        target: screenProvider
        onImageChanged: background.reload()
    }
    Component.onCompleted: controller.startup()
    background: Rectangle {
        Image {
            id: background
            objectName: "background"
            anchors.fill: parent
            cache: false
            source: "image://screen/image"
            property bool counter: false
            function reload(){
                console.log("Reloading background");
                counter = !counter
                source = "image://screen/image?id=" + counter
            }
        }
    }
    footer: Rectangle {
        id: footer
        color: "white"
        border.color: "black"
        border.width: 1
        height: 100
        RowLayout {
            width: parent.width
            Label { text: "Test" }
        }
    }
    StateGroup {
        id: stateController
        objectName: "stateController"
        state: "loading"
        states: [
            State { name: "loaded" },
            State { name: "hidden" },
            State { name: "loading" }
        ]
        transitions: [
            Transition {
                from: "*"; to: "loaded"
                SequentialAnimation {
                    ScriptAction { script: {
                        console.log("Display loaded");
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
