import QtQuick 2.10
import QtQuick.Controls
import QtQuick.Layouts 1.0
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
            font.pixelSize: 32
            visible: root.visible
            background: Rectangle {
                color: "white"
                border.color: "black"
                border.width: 2
                radius: 10
            }
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
                delegate: Item {
                    id: item
                    width: parent.width - scrollbar.width
                    height: contentColumn.implicitHeight
                    visible: !!model.display
                    ColumnLayout {
                        id: contentColumn
                        anchors.fill: parent
                        spacing: 0
                        RowLayout {
                            id: listRow
                            Layout.fillWidth: true
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
                        Flow {
                            id: actionRow
                            Layout.fillWidth: true
                            Layout.margins: 5
                            spacing: 5
                            visible: model.display && Object.keys(model.display.actions).length > 0
                            property var actions: model.display ? model.display.actions : ({})
                            property var notificationItem: model.display
                            property var buttonWidths: ({})
                            function recalculate(){
                                var keys = Object.keys(actions)
                                var available = actionRow.width
                                var ideal = []
                                for(var i = 0; i < keys.length; i++){
                                    textMetrics.text = actions[keys[i]]
                                    ideal.push(textMetrics.width + 20)
                                }
                                var rows = []
                                var row = []
                                var rowWidth = 0
                                for(var j = 0; j < ideal.length; j++){
                                    var need = row.length === 0 ? ideal[j] : rowWidth + actionRow.spacing + ideal[j]
                                    if(need <= available){
                                        row.push(j)
                                        rowWidth = need
                                        continue;
                                    }
                                    if(row.length > 0){
                                        rows.push(row)
                                    }
                                    row = [j]
                                    rowWidth = ideal[j]
                                }
                                if(row.length > 0){
                                    rows.push(row)
                                }
                                var widths = {}
                                for(var r = 0; r < rows.length; r++){
                                    var items = rows[r]
                                    var totalIdeal = 0
                                    for(var k = 0; k < items.length; k++){
                                        totalIdeal += ideal[items[k]]
                                    }
                                    var totalSpacing = (items.length - 1) * actionRow.spacing
                                    var excess = available - totalIdeal - totalSpacing
                                    var perItem = excess / items.length
                                    for(var k2 = 0; k2 < items.length; k2++){
                                        widths[items[k2]] = ideal[items[k2]] + perItem
                                    }
                                }
                                buttonWidths = widths
                            }
                            onWidthChanged: recalculate()
                            onActionsChanged: recalculate()
                            Component.onCompleted: recalculate()
                            TextMetrics{
                                id: textMetrics
                                font.pixelSize: 24
                            }
                            Repeater{
                                model: Object.keys(actionRow.actions)
                                delegate: Rectangle{
                                    width: actionRow.buttonWidths[index] || actionRow.width
                                    height: 40
                                    radius: 2
                                    border.color: "black"
                                    border.width: 1
                                    color: "white"
                                    Text{
                                        anchors.centerIn: parent
                                        text: actionRow.actions[modelData]
                                        font.pixelSize: 24
                                    }
                                    MouseArea{
                                        anchors.fill: parent
                                        onClicked: {
                                            controller.breadcrumb("notifications.notification.action", "click", "ui");
                                            actionRow.notificationItem && actionRow.notificationItem.click(modelData);
                                        }
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
