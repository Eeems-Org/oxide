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
        onClosed: {
            controller.disconnectWifiSignals();
            parent.closed()
        }
        onOpened: {
            controller.connectWifiSignals();
            if(controller.wifiOn){
                networks.model.scan(false)
            }
        }
        width: 1000
        clip: true
        ColumnLayout {
            anchors.fill: parent
            RowLayout {
                Layout.fillWidth: true
                BetterButton{
                    text: "Turn wifi " + (controller.wifiOn ? "off" : "on")
                    Layout.fillWidth: true
                    Layout.preferredWidth: wifi.width / 2
                    onClicked: {
                        if(controller.wifiOn){
                            controller.turnOffWifi();
                            text = "Turn wifi on";
                        }else{
                            controller.turnOnWifi();
                            text = "Turn wifi off";
                        }
                        networks.model.sort();
                    }
                }
                BetterButton{
                    text: (!!networks.model && networks.model.scanning) ? "Scanning..." : "Scan"
                    enabled: controller.wifiOn && !!networks.model && !networks.model.scanning
                    Layout.fillWidth: true
                    Layout.preferredWidth: wifi.width / 2
                    onClicked: {
                        networks.model.scan(true);
                        networks.model.sort();
                    }
                }
            }
            ListView {
                id: networks
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
                    height: networkRow.implicitHeight
                    visible: !!model.display
                    RowLayout {
                        id: networkRow
                        anchors.fill: parent
                        Label {
                            enabled: controller.wifiOn && !!model.display && model.display.available
                            text: model.display ? (model.display.connected ? "* " : "") + model.display.ssid : ""
                            font.bold: !!model.display && model.display.connected
                            Layout.alignment: Qt.AlignLeft
                            Layout.fillWidth: true
                            Layout.preferredWidth: parent.width - 400
                        }
                        RowLayout {
                            id: buttons
                            Layout.preferredWidth: 400
                            BetterButton {
                                enabled: controller.wifiOn
                                text: "Forget"
                                visible: !!model.display && model.display.known
                                Layout.fillWidth: true
                                MouseArea {
                                    anchors.fill: parent
                                    enabled: parent.enabled
                                    onClicked: {
                                        if(!model.display){
                                            return
                                        }
                                        console.log("Forgetting from " + model.display.ssid)
                                        model.display.remove()
                                        networks.model.remove(model.display.ssid)
                                    }
                                }
                            }
                            BetterButton {
                                enabled: controller.wifiOn
                                text: model.display && model.display.connected ? "Disconnect" : "Connect"
                                Layout.fillWidth: true
                                MouseArea {
                                    anchors.fill: parent
                                    enabled: parent.enabled
                                    onClicked: {
                                        if(!model.display){
                                            return
                                        }
                                        if(model.display.connected){
                                            console.log("Disconnecting from " + model.display.ssid)
                                            model.display.disconnect()
                                        }else{
                                            console.log("Attempting to connect to " + model.display.ssid)
                                            model.display.connect()
                                        }
                                        networks.model.sort();
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
                Layout.fillWidth: true
                onClicked: wifi.close()
            }
        }
    }
}
