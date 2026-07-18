import QtQuick 2.10
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.0
import "qrc:/codes.eeems.oxide"
import "../widgets"

Page {
    id: root
    objectName: "display"
    title: "Display"
    background: Rectangle { color: "white" }
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: "Show Battery Percent"
                font.pixelSize: 32
                Layout.fillWidth: true
            }
            BetterCheckBox {
                tristate: false
                checkState: displayController.showBatteryPercent ? Qt.Checked : Qt.Unchecked
                onClicked: displayController.showBatteryPercent = this.checkState === Qt.Checked
                Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: "Show Battery Temperature"
                font.pixelSize: 32
                Layout.fillWidth: true
            }
            BetterCheckBox {
                tristate: false
                checkState: displayController.showBatteryTemperature ? Qt.Checked : Qt.Unchecked
                onClicked: displayController.showBatteryTemperature = this.checkState === Qt.Checked
                Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: "Show Wifi dB"
                font.pixelSize: 32
                Layout.fillWidth: true
            }
            BetterCheckBox {
                tristate: false
                checkState: displayController.showWifiDb ? Qt.Checked : Qt.Unchecked
                onClicked: displayController.showWifiDb = this.checkState === Qt.Checked
                Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: "Show Date"
                font.pixelSize: 32
                Layout.fillWidth: true
            }
            BetterCheckBox {
                tristate: false
                checkState: displayController.showDate ? Qt.Checked : Qt.Unchecked
                onClicked: displayController.showDate = this.checkState === Qt.Checked
                Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
            }
        }

        RowLayout {
            Layout.fillWidth: true
            visible: displayController.hasFrontlight
            Label {
                text: "Extra Brightness"
                font.pixelSize: 32
                Layout.fillWidth: true
            }
            BetterCheckBox {
                tristate: false
                checkState: displayController.extraBrightness ? Qt.Checked : Qt.Unchecked
                onClicked: displayController.extraBrightness = this.checkState === Qt.Checked
                Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: "Home Screen Columns"
                font.pixelSize: 32
                Layout.fillWidth: true
            }
            BetterSpinBox {
                from: 2
                to: 10
                stepSize: 2
                value: displayController.columns
                onValueChanged: displayController.columns = this.value
                Layout.preferredWidth: 300
            }
        }

        Item {
            Layout.fillHeight: true
            Layout.fillWidth: true
        }
    }
}
