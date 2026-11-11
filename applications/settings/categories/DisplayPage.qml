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
    StackView.onActivated: displayController.activate()
    StackView.onDeactivating: displayController.deactivate()
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

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: "Notification display time (seconds)"
                font.pixelSize: 32
                Layout.fillWidth: true
            }
            BetterSpinBox {
                from: 1
                to: 60
                stepSize: 1
                value: displayController.notificationDisplayTime
                onValueChanged: displayController.notificationDisplayTime = this.value
                Layout.preferredWidth: 300
            }
        }

        Label {
            text: "Brightness"
            visible: displayController.hasFrontlight
            font.pixelSize: 32
            Layout.fillWidth: true
        }
        Slider {
            id: brightnessSlider
            visible: displayController.hasFrontlight
            Layout.fillWidth: true
            Layout.preferredHeight: 60
            Layout.leftMargin: root.width * 0.15
            Layout.rightMargin: root.width * 0.15
            from: 0
            to: 100
            value: displayController.brightness
            onMoved: displayController.brightness = value
            background: Rectangle {
                x: brightnessSlider.leftPadding
                y: brightnessSlider.topPadding + brightnessSlider.availableHeight / 2 - height / 2
                width: brightnessSlider.availableWidth
                height: 8
                color: "black"
            }
            handle: Rectangle {
                x: brightnessSlider.leftPadding + brightnessSlider.visualPosition * (brightnessSlider.availableWidth - width)
                y: brightnessSlider.topPadding + brightnessSlider.availableHeight / 2 - height / 2
                width: 50
                height: 50
                radius: 25
                color: "white"
                border.color: "black"
                border.width: 2
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

        Item {
            Layout.fillHeight: true
            Layout.fillWidth: true
        }
    }
}
