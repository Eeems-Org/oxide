import QtQuick 2.10
import QtQuick.Window 2.3
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.0
import "./widgets"

ApplicationWindow {
    id: window
    objectName: "window"
    visible: stateController.state === "loaded"
    width: screenGeometry.width
    height: screenGeometry.height
    title: qsTr("Corrupt")
    property int itemPadding: 10
    Connections{
        target: screenProvider
        onImageChanged: background.reload()
    }
    Connections {
        target: controller
        onReload: appsView.model = controller.getApps()
    }
    Component.onCompleted: {
        controller.startup();
        appsView.model = controller.getApps();
    }
    background: Rectangle {
        Image {
            id: background
            objectName: "background"
            anchors.fill: parent
            cache: false
            source: "image://screen/image"
            visible: stateController.state === "loaded"
            property bool counter: false
            function reload(){
                console.log("Reloading background");
                counter = !counter
                source = "image://screen/image?id=" + counter
            }
        }
    }
    contentData: [
        MouseArea {
            anchors.fill: parent
            enabled: stateController.state === "loaded"
            onClicked: controller.previousApplication()
        }
    ]
    footer: Rectangle {
        id: footer
        color: "white"
        border.color: "black"
        border.width: 1
        height: 200
        width: parent.width
        visible: stateController.state === "loaded"
        RowLayout {
            anchors.fill: parent
            AppItem {
                text: "Home"
                source: "qrc:/img/home.svg"
                height: parent.height
                onClicked: controller.launchStartupApp()
            }
            AppItemSeperator {}
            ListView {
                id: appsView
                orientation: Qt.Horizontal
                snapMode: ListView.SnapOneItem
                maximumFlickVelocity: 0
                boundsBehavior: Flickable.StopAtBounds
                enabled: stateController.state === "loaded"
                model: apps
                objectName: "appsView"
                Layout.fillHeight: true
                Layout.fillWidth: true
                delegate: AppItem {
                    visible: model.modelData.running
                    enabled: visible && appsView.enabled
                    height: visible ? appsView.height : 0
                    source: model.modelData.imgFile
                    text: model.modelData.displayName
                    onClicked: model.modelData.execute()
                }
            }
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
