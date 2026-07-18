import QtQuick 2.10
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.0
import "qrc:/codes.eeems.oxide"
import "../widgets"

Page {
    id: root
    objectName: "wifi"
    title: "WiFi"
    background: Rectangle { color: "white" }
    Component.onCompleted: {
        wifiController.connectWifiSignals();
        if (wifiController.wifiOn) {
            wifiController.networks.scan(false);
        }
        sortTimer.start();
    }
    Component.onDestruction: {
        sortTimer.stop();
        wifiController.disconnectWifiSignals();
    }

    Timer {
        id: sortTimer
        interval: 3000
        repeat: true
        onTriggered: wifiController.networks.sort()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            OxideButton {
                text: "Turn wifi " + (wifiController.wifiOn ? "off" : "on")
                color: "black"
                Layout.fillWidth: true
                Layout.preferredWidth: root.width / 2
                onClicked: {
                    controller.breadcrumb("wifi.toggle", "click", "ui");
                    if (wifiController.wifiOn) {
                        wifiController.turnOffWifi();
                    } else {
                        wifiController.turnOnWifi();
                    }
                    wifiController.networks.sort();
                }
            }
            OxideButton {
                text: (!!wifiController.networks && wifiController.networks.scanning) ? "Scanning..." : "Scan"
                color: "black"
                enabled: wifiController.wifiOn && !!wifiController.networks && !wifiController.networks.scanning
                Layout.fillWidth: true
                Layout.preferredWidth: root.width / 2
                onClicked: {
                    controller.breadcrumb("wifi.scan", "click", "ui");
                    wifiController.networks.scan(true);
                    wifiController.networks.sort();
                }
            }
        }

        ListView {
            id: networks
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: wifiController.networks
            delegate: Rectangle {
                id: item
                width: networks.width
                height: networkRow.implicitHeight + 16
                visible: !!model.display
                color: "white"
                RowLayout {
                    id: networkRow
                    anchors.fill: parent
                    anchors.margins: 8
                    Label {
                        enabled: wifiController.wifiOn && !!model.display && model.display.available
                        text: model.display ? (model.display.connected ? "* " : "") + model.display.ssid : ""
                        font.bold: !!model.display && model.display.connected
                        font.pixelSize: 32
                        Layout.alignment: Qt.AlignLeft
                        Layout.fillWidth: true
                    }
                    RowLayout {
                        spacing: 8
                        OxideButton {
                            enabled: wifiController.wifiOn
                            text: "Forget"
                            color: "black"
                            visible: !!model.display && model.display.known
                            onClicked: {
                                controller.breadcrumb("wifi.network.forget", "click", "ui");
                                if (!model.display) return;
                                model.display.remove();
                                wifiController.networks.remove(model.display.ssid);
                            }
                        }
                        OxideButton {
                            enabled: wifiController.wifiOn
                            text: model.display && model.display.connected ? "Disconnect" : "Connect"
                            color: "black"
                            onClicked: {
                                controller.breadcrumb("wifi.network.toggle", "click", "ui");
                                if (!model.display) return;
                                if (model.display.connected) {
                                    model.display.disconnect();
                                } else {
                                    model.display.connect();
                                }
                                wifiController.networks.sort();
                            }
                        }
                    }
                }
            }
            ScrollIndicator.vertical: ScrollIndicator {
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
        }
    }
}
