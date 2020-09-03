import QtQuick 2.10
import QtQuick.Window 2.3
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.11
import "widgets"

ApplicationWindow {
    id: window
    visible: true
    width: screenGeometry.width
    height: screenGeometry.height
    title: qsTr("Erode")
    menuBar: ToolBar {
        background: Rectangle { color: "black" }
        RowLayout {
            width: parent.width
            BetterButton {
                text: "⬅️"
                onClicked: Qt.quit()
            }
            Item { Layout.fillWidth: true }
            BetterButton {
                text: "Reload"
                onClicked: tasksView.model = controller.getTasks()
            }
        }
    }
    background: Rectangle { color: "white" }
    contentData: [
        MouseArea { anchors.fill: parent },
        Item {
            id: tasksViewHeader
            anchors.top: parent.top
            width: parent.width
//            height: tasksViewHeaderContent.implicitHeight
            height: window.menuBar.height
            RowLayout {
                id: tasksViewHeaderContent
                anchors.fill: parent
                Text {
                    text: "PID"
                    color: "black"
                    font.pointSize: 8
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignLeft
                    leftPadding: 10
                    MouseArea { anchors.fill: parent; onClicked: controller.sortBy("pid") }
                }
                Text {
                    text: "Process"
                    color: "black"
                    font.pointSize: 8
                    Layout.alignment: Qt.AlignRight
                    rightPadding: 10 + scrollbar.width
                    MouseArea { anchors.fill: parent; onClicked: controller.sortBy("name") }
                }
            }
            Rectangle {
                color: "grey"
                width: parent.width
                y: parent.y + parent.height - 2
                height: 2
            }
        },
        Rectangle {
            anchors.top: tasksViewHeader.bottom
            height: parent.height - tasksViewHeader.height - 4
            width: parent.width
            color: "white"
            ListView {
                id: tasksView
                enabled: stateController.state === "loaded"
                objectName: "tasksView"
                anchors.fill: parent
                clip: true
                snapMode: ListView.SnapOneItem
                interactive: false
                boundsBehavior: Flickable.StopAtBounds
                model: tasks
                ScrollBar.vertical: ScrollBar {
                    id: scrollbar
                    snapMode: ScrollBar.SnapAlways
                    policy: ScrollBar.AsNeeded
                    width: 10
                    stepSize: 1
                    contentItem: Rectangle {
                        color: "black"
                        implicitWidth: 6
                        implicitHeight: 100
                    }
                    background: Rectangle {
                        color: "white"
                        border.color: "black"
                        border.width: 1
                        implicitWidth: 6
                        implicitHeight: 100
                    }
                }
                delegate: Rectangle {
                    id: root
                    enabled: parent.enabled
                    width: parent.width - scrollbar.width
                    height: tasksRow.implicitHeight
                    color: "white"
                    state: "released"
                    states: [
                        State { name: "released" },
                        State { name: "pressed" }
                    ]
                    RowLayout {
                        id: tasksRow
                        anchors.fill: parent
                        Text {
                            id: pid
                            text: model.modelData.pid
                            Layout.alignment: Qt.AlignLeft
                            Layout.fillWidth: true
                            leftPadding: 10
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            id: name
                            text: model.modelData.name
                            Layout.alignment: Qt.AlignRight
                            rightPadding: 10
                        }
                    }
                }
                SwipeArea {
                    anchors.fill: parent
                    property int currentIndex: tasksView.currentIndex
                    property int pageSize: (tasksView.height / tasksView.itemAt(0, 0).height).toFixed(0);
                    onSwipe: {
                        if(direction == "down"){
                            currentIndex = currentIndex - pageSize;
                            if(currentIndex < 0){
                                currentIndex = 0;
                            }
                        }else if(direction == "up"){
                            currentIndex = currentIndex + pageSize;
                            if(currentIndex > tasksView.count){
                                currentIndex = tasksView.count;
                            }
                        }else{
                            return;
                        }
                        console.log(currentIndex);
                        tasksView.positionViewAtIndex(currentIndex, ListView.Beginning);
                    }
                }
            }
        }
    ]
    footer: ToolBar {
        background: Rectangle { color: "black" }
    }
    StateGroup {
        id: stateController
        state: "loading"
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
            }
        ]
    }
    Component.onCompleted: stateController.state = "loaded"
}
