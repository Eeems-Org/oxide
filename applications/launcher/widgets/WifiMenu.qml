import QtQuick 2.10
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.0
import "../widgets"

Item {
    id: root
    property alias model: networks.model
    signal closed
    x: (parent.width / 2) - (wifi.width / 2)
    y: (parent.height / 2) - (wifi.height / 2)
    Popup {
        id: wifi
        visible: root.visible
        closePolicy: Popup.NoAutoClose
        onClosed: parent.closed()
        clip: true
        GridLayout {
            columns: 2
            rows: 10
            anchors.fill: parent
            BetterButton{
                text: "Turn wifi " + (controller.wifiOn() ? "off" : "on")
                Layout.columnSpan: parent.columns
                Layout.fillWidth: true
                onClicked: {
                    if(controller.wifiOn()){
                        controller.turnOffWifi();
                        text = "Turn wifi on";
                    }else{
                        controller.turnOnWifi();
                        text = "Turn wifi off";
                    }
                }
            }
            BetterButton{
                text: networks.model.scanning ? "Scanning..." : "Scan"
                enabled: controller.wifiOn() && !networks.model.scanning
                Layout.columnSpan: parent.columns
                Layout.fillWidth: true
                onClicked: networks.model.scan(true)
            }
            ListView {
                id: networks
                Layout.columnSpan: parent.columns
                Layout.fillWidth: true
                Layout.preferredHeight: 500
                Layout.preferredWidth: 500
                clip: true
                snapMode: ListView.SnapOneItem
                interactive: false
                boundsBehavior: Flickable.StopAtBounds
                delegate: Rectangle {
                    id: item
                    width: parent.width - scrollbar.width
                    height: networkRow.implicitHeight
                    enabled: !!model.display && model.display.available
                    RowLayout {
                        enabled: item.enabled
                        id: networkRow
                        anchors.fill: parent
                        Label {
                            text: (model.display.connected ? "* " : "") + model.display.ssid
                            Layout.alignment: Qt.AlignLeft
                            Layout.fillWidth: true
                        }
                    }
                    MouseArea {
                        enabled: item.enabled
                        anchors.fill: parent
                        onClicked: {
                            if(model.display.connected){
                                console.log("Disconnecting from " + model.display.ssid)
                                model.display.disconnect()
                            }else{
                                console.log("Attempting to connect to " + model.display.ssid)
                                model.display.connect()
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
                    property int currentIndex: networks.currentIndex
                    property int pageSize: 0
                    onSwipe: {
                        if(!pageSize && !networks.itemAt(0, 0)){
                            return;
                        }else if(!pageSize){
                            pageSize = (networks.height / networks.itemAt(0, 0).height).toFixed(0);
                        }
                        if(direction == "down"){
                            console.log("Scroll up");
                            currentIndex = currentIndex - pageSize;
                            if(currentIndex < 0){
                                currentIndex = 0;
                            }
                        }else if(direction == "up"){
                            console.log("Scroll down");
                            currentIndex = currentIndex + pageSize;
                            if(currentIndex > networks.count){
                                currentIndex = networks.count;
                            }
                        }else{
                            return;
                        }
                        networks.positionViewAtIndex(currentIndex, ListView.Beginning);
                    }
                }
            }
            BetterButton{
                text: "Close"
                Layout.columnSpan: parent.columns
                Layout.fillWidth: true
                onClicked: wifi.close()
            }
        }
    }
}
