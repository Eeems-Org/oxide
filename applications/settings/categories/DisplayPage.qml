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
                checkState: controller.showBatteryPercent ? Qt.Checked : Qt.Unchecked
                onClicked: controller.showBatteryPercent = this.checkState === Qt.Checked
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
                checkState: controller.showBatteryTemperature ? Qt.Checked : Qt.Unchecked
                onClicked: controller.showBatteryTemperature = this.checkState === Qt.Checked
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
                checkState: controller.showWifiDb ? Qt.Checked : Qt.Unchecked
                onClicked: controller.showWifiDb = this.checkState === Qt.Checked
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
                checkState: controller.showDate ? Qt.Checked : Qt.Unchecked
                onClicked: controller.showDate = this.checkState === Qt.Checked
                Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
            }
        }

        RowLayout {
            Layout.fillWidth: true
            visible: controller.hasFrontlight
            Label {
                text: "Extra Brightness"
                font.pixelSize: 32
                Layout.fillWidth: true
            }
            BetterCheckBox {
                tristate: false
                checkState: controller.extraBrightness ? Qt.Checked : Qt.Unchecked
                onClicked: controller.extraBrightness = this.checkState === Qt.Checked
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
                value: controller.columns
                onValueChanged: controller.columns = this.value
                Layout.preferredWidth: 300
            }
        }

        Item {
            Layout.fillHeight: true
            Layout.fillWidth: true
        }
    }
}
