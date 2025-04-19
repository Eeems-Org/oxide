import QtQuick 2.10
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.0
import Qt.labs.calendar 1.0
import "qrc:/codes.eeems.oxide"
import "../widgets"
import "../js/moment.js" as Moment

Item {
    id: root
    signal closed
    property alias model: notifications.model
    x: (parent.width / 2) - (popup.width / 2)
    y: (parent.height / 2) - (popup.height / 2)
    width: 1000
    Popup {
        id: popup
        visible: root.visible
        closePolicy: Popup.NoAutoClose
        onClosed: {
            parent.closed()
        }
        width: parent.width
        clip: true
        ColumnLayout {
            width: parent.width
            ListView {
                id: notifications
                Layout.fillWidth: true
                Layout.preferredHeight: 1000
                Layout.preferredWidth: 1000
                clip: true
                snapMode: ListView.SnapOneItem
                interactive: false
                boundsBehavior: Flickable.StopAtBounds
                delegate: Rectangle {
                    id: item
                    width: parent.width - scrollbar.width
                    height: listRow.implicitHeight
                    visible: !!model.display
                    RowLayout {
                        id: listRow
                        anchors.fill: parent
                        Image {
                            source: {
                                if(!model.display || !model.display.icon){
                                    return "qrc:/img/notifications/black.png";
                                }
                                var icon = model.display.icon;
                                if(!icon.startsWith("qrc:") && !icon.startsWith("file:")){
                                    return "file:" + icon;
                                }
                                return icon;
                            }
                            Layout.preferredHeight: 64
                            Layout.preferredWidth: 64
                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    controller.breadcrumb("notifications.notification.icon", "click", "ui");
                                    model.display && model.display.click();
                                }
                            }
                        }
                        Label {
                            text: (model.display && model.display.text) || "Notification"
                            Layout.alignment: Qt.AlignLeft
                            Layout.fillWidth: true
                            Layout.preferredWidth: parent.width - 50 - 64
                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    controller.breadcrumb("notifications.notification.text", "click", "ui");
                                    model.display && model.display.click();
                                }
                            }
                        }
                        Label {
                            text: ""
                            Timer {
                                interval: 100
                                running: true
                                repeat: true
                                property var moment: null
                                onTriggered: {
                                    if(!model.display || model.display.created === -1){
                                        return;
                                    }
                                    if(model.display.created === 0){
                                        this.stop();
                                        return;
                                    }
                                    if(this.interval === 100){
                                        this.interval = 1000;
                                    }
                                    if(this.moment === null){
                                        this.moment = Moment.moment.unix(model.display.created);
                                    }
                                    if(this.interval === 1000 && Moment.moment().isAfter(Moment.moment(this.moment).add(1, 'minutes'))){
                                        this.interval = 30000;
                                    }
                                    parent.text = this.moment.fromNow();
                                }
                            }
                        }
                        OxideButton {
                            id: closeButton
                            text: "X"
                            Layout.fillWidth: true
                            Layout.preferredWidth: 50
                            MouseArea {
                                anchors.fill: parent
                                enabled: parent.enabled
                                onClicked: {
                                    controller.breadcrumb("notifications.notification.remove", "click", "ui");
                                    if(!model.display){
                                        return
                                    }
                                    notifications.model.remove(model.display.identifier)
                                    if(!notifications.count){
                                        popup.close();
                                    }
                                }
                            }
                        }
                    }
                }
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
                SwipeArea {
                    anchors.fill: parent
                    propagateComposedEvents: true
                    property int currentIndex: notifications.currentIndex
                    property int pageSize: 0
                    onSwipe: {
                        if(!pageSize && !notifications.itemAt(0, 0)){
                            return;
                        }else if(!pageSize){
                            pageSize = (notifications.height / notifications.itemAt(0, 0).height).toFixed(0);
                        }
                        if(direction == "down"){
                            controller.breadcrumb("notifications", "scroll.up", "ui");
                            console.log("Scroll up");
                            currentIndex = currentIndex - pageSize;
                            if(currentIndex < 0){
                                currentIndex = 0;
                            }
                        }else if(direction == "up"){
                            controller.breadcrumb("notifications", "scroll.down", "ui");
                            console.log("Scroll down");
                            currentIndex = currentIndex + pageSize;
                            if(currentIndex > notifications.count){
                                currentIndex = notifications.count;
                            }
                        }else{
                            return;
                        }
                        notifications.positionViewAtIndex(currentIndex, ListView.Beginning);
                    }
                }
            }
            RowLayout {
                OxideButton{
                    text: "Clear"
                    Layout.fillWidth: true
                    onClicked: {
                        controller.breadcrumb("notifications.clear", "click", "ui");
                        notifications.model.clear();
                        popup.close();
                    }
                }
                OxideButton{
                    text: "Close"
                    Layout.fillWidth: true
                    onClicked: {
                        controller.breadcrumb("notifications.close", "click", "ui");
                        popup.close();
                    }
                }
            }
        }
    }
}
