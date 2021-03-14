import QtQuick 2.10
import QtQuick.Window 2.3
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.0
import "./widgets"

ApplicationWindow {
    id: window
    objectName: "window"
    visible: stateController.state !== "loading"
    width: screenGeometry.width
    height: screenGeometry.height
    title: Qt.application.displayName
    FontLoader { id: iconFont; source: "/font/icomoon.ttf" }
    Component.onCompleted: {
        controller.startup();
    }
    header: Rectangle {
        color: "black"
        height: menu.height
        RowLayout {
            id: menu
            anchors.left: parent.left
            anchors.right: parent.right
            Label {
                text: "⬅️"
                color: "white"
                topPadding: 5
                bottomPadding: 5
                leftPadding: 10
                rightPadding: 10
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        if(stateController.state !== "viewing"){
                            Qt.quit();
                            return;
                        }
                        viewer.model = undefined;
                        stateController.state = "loaded";
                    }
                }
            }
            Item { Layout.fillWidth: true }
            Label {
                color: "white"
                text: window.title
            }
            Item { Layout.fillWidth: true }
            Label {
                text: "🗑"
                color: "white"
                visible: stateController.state === "viewing"
                topPadding: 5
                bottomPadding: 5
                leftPadding: 10
                rightPadding: 10
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        viewer.model.remove();
                        viewer.model = undefined;
                        stateController.state = "loading";
                    }
                }
            }
            CustomMenu {
                visible: stateController.state === "loaded"
                BetterMenu {
                    title: ""
                    font: iconFont.name
                    width: 310
                    Action {
                        text: controller.columns == 4 ? "* Small" : "Small"
                        onTriggered: controller.columns = 4
                    }
                    Action {
                        text: controller.columns == 3 ? "* Medium" : "Medium"
                        onTriggered: controller.columns = 3
                    }
                    Action {
                        text: controller.columns == 2 ? "* Large" : "Large"
                        onTriggered: controller.columns = 2
                    }
                }
            }
        }
    }
    contentData: [
        Rectangle {
            anchors.fill: parent
            color: "white"
        },
        GridView {
            id: screenshots
            enabled: stateController.state == "loaded"
            visible: enabled
            anchors.fill: parent
            model: controller.screenshots
            cellWidth: parent.width / controller.columns
            cellHeight: cellWidth
            delegate: AppItem {
                enabled: screenshots.enabled
                text: model.display.name
                source: 'file:' + model.display.path
                width: screenshots.cellWidth
                height: screenshots.cellHeight
                onClicked: {
                    viewer.model = model;
                    stateController.state = "viewing";
                }
            }
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
            function pageWidth(){
                return controller.columns;
            }
            function pageHeight(){
                var item = itemAt(0, 0),
                    itemHeight = item ? item.height : height;
                return height / itemHeight;
            }
            function pageSize(){
                return Math.floor(pageHeight() * pageWidth());
            }
        },
        SwipeArea {
            enabled: stateController.state === "viewing"
            visible: enabled
            anchors.fill: parent
            propagateComposedEvents: true
            property int currentIndex: 0
            property int pageSize: 0

            onSwipe: {
                if(direction == "down"){
                    console.log("Scroll up");
                    currentIndex = currentIndex - screenshots.pageSize();
                    if(currentIndex < 0){
                        currentIndex = 0;
                        screenshots.positionViewAtBeginning();
                    }else{
                        screenshots.positionViewAtIndex(currentIndex, ListView.Beginning);
                    }
                }else if(direction == "up"){
                    console.log("Scroll down");
                    currentIndex = currentIndex + screenshots.pageSize();
                    if(currentIndex > screenshots.count){
                        currentIndex = screenshots.count;
                        screenshots.positionViewAtEnd();
                    }else{
                        screenshots.positionViewAtIndex(currentIndex, ListView.Beginning);
                    }
                }else{
                    return;
                }
            }
        },
        Image {
            id: viewer
            anchors.fill: parent
            visible: stateController.state == "viewing"
            property var model
            fillMode: Image.PreserveAspectFit
            source: model ? 'file:' + model.display.path : ''
            sourceSize.width: width
            sourceSize.height: height
        }
    ]
    StateGroup {
        id: stateController
        objectName: "stateController"
        state: "loading"
        states: [
            State { name: "loaded" },
            State { name: "loading" },
            State { name: "viewing" }
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
            },
            Transition {
                from: "*"; to: "viewing"
                SequentialAnimation {
                    ScriptAction { script: {
                        console.log("Viewing Image");
                    } }
                }
            }
        ]
    }
}
