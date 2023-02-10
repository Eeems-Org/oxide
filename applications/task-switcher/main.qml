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
        onReload: {
            appsView.model = controller.getApps();
            if(appsView.currentIndex > appsView.count){
                appsView.currentIndex = appsView.count
            }
        }
    }
    Component.onCompleted: {
        controller.startup();
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
                controller.breadcrumb("background", "reload");
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
            onClicked: {
                controller.breadcrumb("background", "click", "ui");
                controller.previousApplication();
            }
        }
    ]
    footer: Rectangle {
        id: footer
        color: "white"
        border.color: "black"
        border.width: 1
        height: 150
        anchors.left: parent.left
        anchors.right: parent.right
        visible: stateController.state === "loaded"
        clip: true
        RowLayout {
            anchors.fill: parent
            AppItem {
                text: "Home"
                source: "qrc:/img/home.svg"
                height: parent.height
                onClicked: {
                    controller.breadcrumb("appsView.home", "click", "ui");
                    controller.launchStartupApp();
                }
            }
            AppItemSeperator {}
            AppItem {
                text: ""
                source: "qrc:/img/left.svg"
                enabled: appsView.currentIndex > 0
                opacity: enabled ? 1 : 0.5
                height: parent.height
                width: height / 2
                onClicked: {
                    controller.breadcrumb("appsView", "scroll.left", "ui");
                    console.log("Scroll left");
                    appsView.currentIndex = appsView.currentIndex - appsView.pageSize();
                    if(appsView.currentIndex < 0){
                        appsView.currentIndex = 0;
                    }else if(appsView.currentIndex > appsView.count){
                        appsView.currentIndex = appsView.count;
                    }
                    appsView.positionViewAtIndex(appsView.currentIndex, ListView.Beginning);
                    console.log("Index " + appsView.currentIndex + "/" + appsView.count);
                }
            }
            Item {
                Layout.fillHeight: true
                Layout.fillWidth: true
                clip: true
                ListView {
                    id: appsView
                    clip: true
                    orientation: Qt.Horizontal
                    snapMode: ListView.SnapOneItem
                    maximumFlickVelocity: 0
                    interactive: false
                    boundsBehavior: Flickable.StopAtBounds
                    enabled: stateController.state === "loaded"
                    model: apps
                    objectName: "appsView"
                    anchors.fill: parent
                    delegate: AppItem {
                        visible: model.modelData.running
                        enabled: visible && appsView.enabled
                        height: visible ? appsView.height : 0
                        source: model.modelData.imgFile
                        text: model.modelData.displayName
                        onClicked: {
                            controller.breadcrumb("appsView.app", "click", "ui");
                            model.modelData.execute();
                        }
                        onLongPress: {
                            controller.breadcrumb("appsView.app", "longPress", "ui");
                            model.modelData.stop();
                            if(index === 0){
                                background.source = "";
                            }
                        }
                    }
                    function pageSize(){
                        var item = itemAt(0, 0),
                            itemWidth = item ? item.width : appsView.height;
                        return (width / itemWidth).toFixed(0);
                    }
                    function rightBound(){
                        return currentIndex == count || currentIndex + pageSize() >= count;
                    }
                }
            }
            AppItem {
                text: ""
                source: "qrc:/img/right.svg"
                enabled: !appsView.rightBound()
                opacity: enabled ? 1 : 0.5
                height: parent.height
                width: height / 2
                onClicked: {
                    controller.breadcrumb("appsView", "scroll.right", "ui");
                    console.log("Scroll right");
                    appsView.currentIndex = appsView.currentIndex + appsView.pageSize();
                    if(appsView.currentIndex > appsView.count){
                        appsView.currentIndex = appsView.count;
                    }else if(appsView.currentIndex < 0){
                        appsView.currentIndex = 0;
                    }

                    appsView.positionViewAtIndex(appsView.currentIndex, ListView.Beginning);
                    console.log("Index " + appsView.currentIndex + "/" + appsView.count);
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
                        controller.breadcrumb("navigation", "main", "navigation");
                        console.log("Display loaded");
                    } }
                }
            },
            Transition {
                from: "*"; to: "loading"
                SequentialAnimation {
                    ScriptAction { script: {
                        controller.breadcrumb("navigation", "loading", "navigation");
                        console.log("Loading display");
                        controller.startup();
                    } }
                }
            }
        ]
    }
}
