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
    onAfterSynchronizing: {
        if (stateController.state == "loading") {
            stateController.state = "loaded";
        }
    }
    Timer {
        interval: 1000 * 5
        repeat: true
        triggeredOnStart: true
        running: true
        onTriggered: controller.reload();
    }

    menuBar: ColumnLayout {
        width: parent.width
        ToolBar {
            Layout.fillWidth: true
            background: Rectangle { color: "black" }
            RowLayout {
                width: parent.width
                BetterButton {
                    text: "⬅️"
                    onClicked: quitTimer.start()
                    Timer {
                        id: quitTimer
                        interval: 1000
                        onTriggered: Qt.quit()
                    }
                }
                Item { Layout.fillWidth: true }
            }
        }
        RowLayout {
            id: tasksViewHeaderContent
            Layout.fillWidth: true
            Label {
                text: "Process"
                color: "black"
                font.pointSize: 8
                Layout.alignment: Qt.AlignLeft
                rightPadding: 10
                leftPadding: 10
                Layout.fillWidth: true
                MouseArea { anchors.fill: parent; onClicked: controller.sortBy("name") }
            }
            Label {
                id: pid
                text: "PID"
                color: "black"
                font.pointSize: 8
                Layout.alignment: Qt.AlignLeft
                leftPadding: 20
                Layout.preferredWidth: 180
                MouseArea { anchors.fill: parent; onClicked: controller.sortBy("pid") }
            }
            Label {
                id: ppid
                text: "Parent PID"
                color: "black"
                font.pointSize: 8
                Layout.alignment: Qt.AlignLeft
                leftPadding: 20
                Layout.preferredWidth: 180
                MouseArea { anchors.fill: parent; onClicked: controller.sortBy("ppid") }
            }
            Label {
                id: cpu
                text: "CPU"
                color: "black"
                font.pointSize: 8
                Layout.alignment: Qt.AlignLeft
                leftPadding: 20
                Layout.preferredWidth: 100
                MouseArea { anchors.fill: parent; onClicked: controller.sortBy("cpu") }
            }
            Item { width: scrollbar.width }
        }
    }
    background: Rectangle { color: "white" }
    contentData: [
        ColumnLayout {
            anchors.fill: parent
            BetterButton {
                text: "▲"
                visible: !tasksView.atYBeginning
                Layout.fillWidth: true
                color: "black"
                backgroundColor: "white"
                borderColor: "white"
                onClicked: {
                    console.log("Scroll up");
                    tasksView.currentIndex = tasksView.currentIndex - tasksView.pageSize();
                    if(tasksView.currentIndex < 0){
                        tasksView.currentIndex = 0;
                        tasksView.positionViewAtBeginning();
                    }else{
                        tasksView.positionViewAtIndex(tasksView.currentIndex, ListView.Beginning);
                    }
                }
            }
            ListView {
                Layout.fillHeight: true
                Layout.fillWidth: true
                id: tasksView
                enabled: stateController.state === "loaded"
                objectName: "tasksView"
                clip: true
                snapMode: ListView.SnapOneItem
                interactive: false
                boundsBehavior: Flickable.StopAtBounds
                model: controller.tasks
                onMovementEnded: console.log("Moved")
                ScrollIndicator.vertical: ScrollIndicator {
                    id: scrollbar
                    width: 10
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
                    enabled: tasksView.enabled
                    width: tasksView.width - scrollbar.width
                    height: 100
                    color: "white"
                    state: "released"
                    states: [
                        State { name: "released" },
                        State { name: "pressed" }
                    ]
                    RowLayout {
                        id: tasksRow
                        anchors.fill: parent
                        Label {
                            text: model.display.name
                            Layout.alignment: Qt.AlignLeft
                            Layout.fillWidth: true
                            leftPadding: 10
                            rightPadding: 10
                            topPadding: 5
                            bottomPadding: 5
                        }
                        Label {
                            text: model.display.pid
                            Layout.alignment: Qt.AlignLeft
                            leftPadding: 10
                            Layout.preferredWidth: pid.width
                            topPadding: 5
                            bottomPadding: 5
                        }
                        Label {
                            text: model.display.ppid
                            Layout.alignment: Qt.AlignLeft
                            leftPadding: 10
                            Layout.preferredWidth: ppid.width
                            topPadding: 5
                            bottomPadding: 5
                        }
                        Label {
                            text: model.display.cpu + "%"
                            Layout.alignment: Qt.AlignLeft
                            leftPadding: 10
                            Layout.preferredWidth: cpu.width
                            topPadding: 5
                            bottomPadding: 5
                        }
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: model.display.killable && killPrompt.open()
                    }
                    Popup {
                        id: killPrompt
                        visible: false
                        parent: Overlay.overlay
                        x: Math.round((parent.width - width) / 2)
                        y: Math.round((parent.height - height) / 2)
                        focus: true
                        closePolicy: Popup.CloseOnPressOutsideParent
                        onClosed: console.log("Closed")
                        onOpened: console.log("Opened")
                        contentItem: ColumnLayout {
                            Label {
                                text: "Would you like to kill " + model.display.name + " (" + model.display.pid + ")"
                            }
                            RowLayout {
                                BetterButton {
                                    backgroundColor: "white"
                                    color: "black"
                                    text: "Force Quit"
                                    MouseArea {
                                        anchors.fill: parent
                                        onClicked: {
                                            model.display.signal(15);
                                            killPrompt.close()
                                        }
                                    }
                                }
                                Item { Layout.fillWidth: true }
                                BetterButton {
                                    text: "Yes"
                                    MouseArea {
                                        anchors.fill: parent
                                        onClicked: {
                                            model.display.signal(9);
                                            killPrompt.close()
                                        }
                                    }
                                }
                                BetterButton {
                                    text: "No"
                                    backgroundColor: "white"
                                    color: "black"
                                    MouseArea {
                                        anchors.fill: parent
                                        onClicked: killPrompt.close()
                                    }
                                }
                            }
                        }
                    }
                }
                function pageHeight(){
                    if(!contentItem.children.length){
                        return 1;
                    }
                    var item = contentItem.children[0];
                    if(!item){
                        return 1;
                    }
                    console.log(item,height);
                    return height / item.height;
                }
                function pageSize(){
                    return Math.floor(pageHeight());
                }
            }
            BetterButton{
                text: "▼"
                Layout.fillWidth: true
                visible: !tasksView.atYEnd
                color: "black"
                backgroundColor: "white"
                borderColor: "white"
                onClicked: {
                    console.log("Scroll down");
                    tasksView.currentIndex = tasksView.currentIndex + tasksView.pageSize();
                    if(tasksView.currentIndex > tasksView.count){
                        tasksView.currentIndex = tasksView.count;
                        tasksView.positionViewAtEnd();
                    }else{
                        tasksView.positionViewAtIndex(tasksView.currentIndex, ListView.Beginning);
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
}
