import QtQuick 2.10
import QtQuick.Window 2.3
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.11
import "qrc:/codes.eeems.oxide"
import "widgets"

OxideWindow {
    id: window
    visible: true
    onAfterSynchronizing: {
        if (stateController.state == "loading") {
            stateController.state = "loaded";
        }
    }
    Timer {
        interval: 1000 * 5
        repeat: true
        triggeredOnStart: false
        running: true
        onTriggered: controller.reload()
    }
    leftMenu: [
        BetterButton {
            text: "⬅️"
            onClicked: quitTimer.start()
            Timer {
                id: quitTimer
                interval: 1000
                onTriggered: {
                    controller.breadcrumb("back", "click", "ui");
                    Qt.quit();
                }
            }
        }
    ]
    centerMenu: [
        Label {
            text: "Process Manager"
            color: "white"
        }
    ]
    initialItem: Item{
        RowLayout {
            id: tasksViewHeaderContent
            width: parent.width
            Label {
                text: "Process"
                color: "black"
                font.pointSize: 8
                font.bold: controller.sortBy === "name"
                Layout.alignment: Qt.AlignLeft
                rightPadding: 10
                leftPadding: 10
                Layout.fillWidth: true
                MouseArea { anchors.fill: parent; onClicked: {
                    controller.breadcrumb("tasksView.name", "sortBy", "ui");
                    controller.sortBy = "name";
                } }
            }
            Label {
                id: pid
                text: "PID"
                color: "black"
                font.pointSize: 8
                font.bold: controller.sortBy === "pid"
                Layout.alignment: Qt.AlignLeft
                leftPadding: 20
                Layout.preferredWidth: 180
                MouseArea { anchors.fill: parent; onClicked: {
                    controller.breadcrumb("tasksView.pid", "sortBy", "ui");
                    controller.sortBy = "pid";
                } }
            }
            Label {
                id: ppid
                text: "Parent PID"
                color: "black"
                font.pointSize: 8
                font.bold: controller.sortBy === "ppid"
                Layout.alignment: Qt.AlignLeft
                leftPadding: 20
                Layout.preferredWidth: 180
                MouseArea { anchors.fill: parent; onClicked: {
                    controller.breadcrumb("tasksView.ppid", "sortBy", "ui");
                    controller.sortBy = "ppid";
                } }
            }
            Label {
                id: cpu
                text: "CPU"
                color: "black"
                font.pointSize: 8
                font.bold: controller.sortBy === "cpu"
                Layout.alignment: Qt.AlignLeft
                leftPadding: 20
                Layout.preferredWidth: 100
                MouseArea { anchors.fill: parent; onClicked: {
                    controller.breadcrumb("tasksView.cpu", "sortBy", "ui");
                    controller.sortBy = "cpu";
                }}
            }
            Label {
                id: mem
                text: "Mem"
                color: "black"
                font.pointSize: 8
                font.bold: controller.sortBy === "mem"
                Layout.alignment: Qt.AlignLeft
                leftPadding: 20
                Layout.preferredWidth: 200
                MouseArea { anchors.fill: parent; onClicked: {
                    controller.breadcrumb("tasksView.mem", "sortBy", "ui");
                    controller.sortBy = "mem";
                }}
            }
            Item { width: scrollbar.width }
        }
        ColumnLayout {
            anchors.fill: parent
            anchors.topMargin: tasksViewHeaderContent.height
            BetterButton {
                text: "▲"
                visible: !tasksView.atYBeginning
                Layout.fillWidth: true
                color: "black"
                backgroundColor: "white"
                borderColor: "white"
                onClicked: {
                    controller.breadcrumb("tasksView", "scroll.up", "ui");
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
                        Label {
                            text: model.display.mem
                            Layout.alignment: Qt.AlignLeft
                            leftPadding: 10
                            Layout.preferredWidth: mem.width
                            topPadding: 5
                            bottomPadding: 5
                        }
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            controller.breadcrumb("tasksView.tasksRow", "clicked", "ui");
                            model.display.killable && killPrompt.open();
                        }
                    }
                    Popup {
                        id: killPrompt
                        visible: false
                        parent: Overlay.overlay
                        x: Math.round((parent.width - width) / 2)
                        y: Math.round((parent.height - height) / 2)
                        focus: true
                        closePolicy: Popup.CloseOnPressOutsideParent
                        onClosed: {
                            controller.breadcrumb("navigation", "main", "navigation");
                            console.log("Closed");
                        }
                        onOpened: {
                            controller.breadcrumb("navigation", "killPrompt", "navigation");
                            console.log("Opened");
                        }
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
                                            controller.breadcrumb("killPrompt.forceQuit", "clicked", "ui");
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
                                            controller.breadcrumb("killPrompt.yes", "clicked", "ui");
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
                                        onClicked: {
                                            controller.breadcrumb("killPrompt.no", "clicked", "ui");
                                            killPrompt.close();
                                        }
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
                    controller.breadcrumb("tasksView", "scroll.down", "ui");
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
    }
    page.footer: ToolBar {
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
                    ScriptAction { script: {
                        controller.breadcrumb("navigation", "loading", "navigation");
                        console.log("loading...");
                    } }
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
                    ScriptAction { script: {
                        controller.breadcrumb("navigation", "main", "navigation");
                        console.log("loaded.");
                    } }
                }
            }
        ]
    }
}
