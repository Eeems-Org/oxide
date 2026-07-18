import QtQuick 2.10
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.0
import "qrc:/codes.eeems.oxide"
import "../widgets"

Page {
    id: root
    objectName: "power"
    title: "Power"
    background: Rectangle { color: "white" }
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: "Lock after sleep"
                font.pixelSize: 32
                Layout.fillWidth: true
            }
            BetterCheckBox {
                tristate: false
                checkState: controller.lockOnSuspend ? Qt.Checked : Qt.Unchecked
                onClicked: controller.lockOnSuspend = this.checkState === Qt.Checked
                Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: "Automatic sleep"
                font.pixelSize: 32
                Layout.fillWidth: true
            }
            BetterCheckBox {
                tristate: false
                checkState: controller.automaticSleep ? Qt.Checked : Qt.Unchecked
                onClicked: controller.automaticSleep = this.checkState === Qt.Checked
                Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
            }
        }

        RowLayout {
            Layout.fillWidth: true
            enabled: controller.automaticSleep
            Label {
                text: "Sleep After (minutes)"
                font.pixelSize: 32
                Layout.fillWidth: true
            }
            BetterSpinBox {
                from: 1
                to: 360
                stepSize: 1
                value: controller.sleepAfter
                onValueChanged: controller.sleepAfter = this.value
                Layout.preferredWidth: 300
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: "Automatic lock"
                font.pixelSize: 32
                Layout.fillWidth: true
            }
            BetterCheckBox {
                tristate: false
                checkState: controller.automaticLock ? Qt.Checked : Qt.Unchecked
                onClicked: controller.automaticLock = this.checkState === Qt.Checked
                Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
            }
        }

        RowLayout {
            Layout.fillWidth: true
            enabled: controller.automaticLock
            Label {
                text: "Lock After (minutes)"
                font.pixelSize: 32
                Layout.fillWidth: true
            }
            BetterSpinBox {
                from: 1
                to: 360
                stepSize: 1
                value: controller.lockAfter
                onValueChanged: controller.lockAfter = this.value
                Layout.preferredWidth: 300
            }
        }

        Item {
            Layout.fillHeight: true
            Layout.fillWidth: true
        }
    }
}
