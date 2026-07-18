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
    Component.onCompleted: powerController.init()
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
                checkState: powerController.lockOnSuspend ? Qt.Checked : Qt.Unchecked
                onClicked: powerController.lockOnSuspend = this.checkState === Qt.Checked
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
                checkState: powerController.automaticSleep ? Qt.Checked : Qt.Unchecked
                onClicked: powerController.automaticSleep = this.checkState === Qt.Checked
                Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
            }
        }

        RowLayout {
            Layout.fillWidth: true
            enabled: powerController.automaticSleep
            Label {
                text: "Sleep After (minutes)"
                font.pixelSize: 32
                Layout.fillWidth: true
            }
            BetterSpinBox {
                from: 1
                to: 360
                stepSize: 1
                value: powerController.sleepAfter
                onValueChanged: powerController.sleepAfter = this.value
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
                checkState: powerController.automaticLock ? Qt.Checked : Qt.Unchecked
                onClicked: powerController.automaticLock = this.checkState === Qt.Checked
                Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
            }
        }

        RowLayout {
            Layout.fillWidth: true
            enabled: powerController.automaticLock
            Label {
                text: "Lock After (minutes)"
                font.pixelSize: 32
                Layout.fillWidth: true
            }
            BetterSpinBox {
                from: 1
                to: 360
                stepSize: 1
                value: powerController.lockAfter
                onValueChanged: powerController.lockAfter = this.value
                Layout.preferredWidth: 300
            }
        }

        Item {
            Layout.fillHeight: true
            Layout.fillWidth: true
        }
    }
}
