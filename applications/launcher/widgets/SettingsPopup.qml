import QtQuick 2.10
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.0
import "qrc:/codes.eeems.oxide"

Item {
    id: root
    x: (parent.width / 2) - (settings.width / 2)
    y: (parent.height / 2) - (settings.height / 2)
    signal closed
    Popup {
        id: settings
        width: 1000
        height: 1400
        closePolicy: Popup.NoAutoClose
        onClosed: parent.closed()
        visible: parent.visible
        GridLayout {
            columns: 3
            rows: 20
            anchors.fill: parent
            RowLayout {
                Layout.columnSpan: parent.columns
                Label {
                    text: "Locale"
                    Layout.columnSpan: 1
                    Layout.fillWidth: true
                }
                ComboBox {
                    Layout.columnSpan: parent.columns - 1
                    Layout.fillWidth: true
                    flat: true

                    model: controller.locales
                    onActivated: controller.locale = textAt(currentIndex)
                    Component.onCompleted: currentIndex = indexOfValue(controller.locale)
                }
            }
            RowLayout {
                Layout.columnSpan: parent.columns
                Label {
                    text: "Timezone"
                    Layout.columnSpan: 1
                    Layout.fillWidth: true
                }
                ComboBox {
                    Layout.columnSpan: parent.columns - 1
                    Layout.fillWidth: true
                    flat: true

                    model: controller.timezones
                    onActivated: controller.timezone = textAt(currentIndex)
                    Component.onCompleted: currentIndex = indexOfValue(controller.timezone)
                }
            }
            RowLayout {
                Layout.columnSpan: parent.columns
                Label {
                    text: "Lock after sleep"
                    Layout.columnSpan: parent.columns - 1
                    Layout.fillWidth: true
                }
                BetterCheckBox {
                    tristate: false
                    checkState: controller.lockOnSuspend ? Qt.Checked : Qt.Unchecked
                    onClicked: {
                        controller.lockOnSuspend = this.checkState === Qt.Checked
                    }
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                    Layout.fillWidth: false
                }
            }
            RowLayout {
                Layout.columnSpan: parent.columns
                Label {
                    text: "Automatic sleep"
                    Layout.columnSpan: parent.columns - 1
                    Layout.fillWidth: true
                }
                BetterCheckBox {
                    tristate: false
                    checkState: controller.automaticSleep ? Qt.Checked : Qt.Unchecked
                    onClicked: {
                        controller.automaticSleep = this.checkState === Qt.Checked
                        controller.sleepAfter = sleepAfterSpinBox.value
                    }
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                    Layout.fillWidth: false
                }
            }
            RowLayout {
                Layout.columnSpan: parent.columns
                Layout.preferredWidth: parent.width
                enabled: controller.automaticSleep
                Label {
                    text: "Sleep After (minutes)"
                    Layout.fillWidth: true
                }
                BetterSpinBox {
                    id: sleepAfterSpinBox
                    objectName: "sleepAfterSpinBox"
                    from: 1
                    to: 360
                    stepSize: 1
                    value: controller.sleepAfter
                    onDownPressedChanged: {
                        if(this.value <= 10){
                            this.stepSize = 1;
                            return;
                        }
                        if(this.value <= 60){
                            this.stepSize = 5;
                            return;
                        }
                        this.stepSize = 15;
                    }
                    onUpPressedChanged: {
                        if(this.value < 10){
                            this.stepSize = 1;
                            return;
                        }
                        if(this.value < 60){
                            this.stepSize = 5;
                            return;
                        }
                        this.stepSize = 15;
                    }
                    onValueChanged: {
                        controller.sleepAfter = this.value;
                    }
                    Layout.preferredWidth: 300
                }
            }
            RowLayout {
                Layout.columnSpan: parent.columns
                Label {
                    text: "Automatic lock"
                    Layout.columnSpan: parent.columns - 1
                    Layout.fillWidth: true
                }
                BetterCheckBox {
                    tristate: false
                    checkState: controller.automaticLock ? Qt.Checked : Qt.Unchecked
                    onClicked: {
                        controller.automaticLock = this.checkState === Qt.Checked
                        controller.lockAfter = lockAfterSpinBox.value
                    }
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                    Layout.fillWidth: false
                }
            }
            RowLayout {
                Layout.columnSpan: parent.columns
                Layout.preferredWidth: parent.width
                enabled: controller.automaticLock
                Label {
                    text: "Lock After (minutes)"
                    Layout.fillWidth: true
                }
                BetterSpinBox {
                    id: lockAfterSpinBox
                    objectName: "lockAfterSpinBox"
                    from: 1
                    to: 360
                    stepSize: 1
                    value: controller.lockAfter
                    onDownPressedChanged: {
                        if(this.value <= 10){
                            this.stepSize = 1;
                            return;
                        }
                        if(this.value <= 60){
                            this.stepSize = 5;
                            return;
                        }
                        this.stepSize = 15;
                    }
                    onUpPressedChanged: {
                        if(this.value < 10){
                            this.stepSize = 1;
                            return;
                        }
                        if(this.value < 60){
                            this.stepSize = 5;
                            return;
                        }
                        this.stepSize = 15;
                    }
                    onValueChanged: {
                        controller.lockAfter = this.value;
                    }
                    Layout.preferredWidth: 300
                }
            }
            RowLayout {
                Layout.columnSpan: parent.columns
                Label {
                    text: "Show Battery Percent"
                    Layout.columnSpan: parent.columns - 1
                    Layout.fillWidth: true
                }
                BetterCheckBox {
                    tristate: false
                    checkState: controller.showBatteryPercent ? Qt.Checked : Qt.Unchecked
                    onClicked: controller.showBatteryPercent = this.checkState === Qt.Checked
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                    Layout.fillWidth: false
                }
            }
            RowLayout {
                Layout.columnSpan: parent.columns
                Label {
                    text: "Show Battery Temperature"
                    Layout.columnSpan: parent.columns - 1
                    Layout.fillWidth: true
                }
                BetterCheckBox {
                    tristate: false
                    checkState: controller.showBatteryTemperature ? Qt.Checked : Qt.Unchecked
                    onClicked: controller.showBatteryTemperature = this.checkState === Qt.Checked
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                    Layout.fillWidth: false
                }
            }
            RowLayout {
                Layout.columnSpan: parent.columns
                Label {
                    text: "Show Wifi DB"
                    Layout.columnSpan: parent.columns - 1
                    Layout.fillWidth: true
                }
                BetterCheckBox {
                    tristate: false
                    checkState: controller.showWifiDb ? Qt.Checked : Qt.Unchecked
                    onClicked: controller.showWifiDb = this.checkState === Qt.Checked
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                    Layout.fillWidth: false
                }
            }
            RowLayout {
                Layout.columnSpan: parent.columns
                Layout.preferredWidth: parent.width
                Label {
                    text: "Show Date"
                    Layout.fillWidth: true
                }
                BetterCheckBox {
                    tristate: false
                    checkState: controller.showDate ? Qt.Checked : Qt.Unchecked
                    onClicked: controller.showDate = this.checkState === Qt.Checked
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                    Layout.fillWidth: false
                }
            }
            RowLayout {
                Layout.columnSpan: parent.columns
                Layout.preferredWidth: parent.width
                Label {
                    text: "Home Screen Columns"
                    Layout.fillWidth: true
                }
                BetterSpinBox {
                    id: columnsSpinBox
                    objectName: "columnsSpinBox"
                    from: 2
                    to: 10
                    stepSize: 2
                    value: controller.columns
                    onValueChanged: controller.columns = this.value
                    Layout.preferredWidth: 300
                }
            }
            RowLayout {
                Layout.columnSpan: parent.columns
                Layout.preferredWidth: parent.width
                Label {
                    text: "Right Swipe Length (pixels)"
                    Layout.fillWidth: true
                }
                BetterSpinBox {
                    id: swipeLengthRightSpinBox
                    objectName: "swipeLengthRightSpinBox"
                    from: 10
                    to: controller.maxTouchWidth
                    stepSize: 10
                    value: controller.swipeLengthRight
                    onValueChanged: controller.swipeLengthRight = this.value
                    Layout.preferredWidth: 300
                }
            }
            RowLayout {
                Layout.columnSpan: parent.columns
                Layout.preferredWidth: parent.width
                Label {
                    text: "Left Swipe Length (pixels)"
                    Layout.fillWidth: true
                }
                BetterSpinBox {
                    id: swipeLengthLeftSpinBox
                    objectName: "swipeLengthLeftSpinBox"
                    from: 10
                    to: controller.maxTouchWidth
                    stepSize: 10
                    value: controller.swipeLengthLeft
                    onValueChanged: controller.swipeLengthLeft = this.value
                    Layout.preferredWidth: 300
                }
            }
            RowLayout {
                Layout.columnSpan: parent.columns
                Layout.preferredWidth: parent.width
                Label {
                    text: "Up Swipe Length (pixels)"
                    Layout.fillWidth: true
                }
                BetterSpinBox {
                    id: swipeLengthUpSpinBox
                    objectName: "swipeLengthUpSpinBox"
                    from: 10
                    to: controller.maxTouchHeight
                    stepSize: 10
                    value: controller.swipeLengthUp
                    onValueChanged: controller.swipeLengthUp = this.value
                    Layout.preferredWidth: 300
                }
            }
            RowLayout {
                Layout.columnSpan: parent.columns
                Layout.preferredWidth: parent.width
                Label {
                    text: "Down Swipe Length (pixels)"
                    Layout.fillWidth: true
                }
                BetterSpinBox {
                    id: swipeLengthDownSpinBox
                    objectName: "swipeLengthDownSpinBox"
                    from: 10
                    to: controller.maxTouchHeight
                    stepSize: 10
                    value: controller.swipeLengthDown
                    onValueChanged: controller.swipeLengthDown = this.value
                    Layout.preferredWidth: 300
                }
            }
            Item {
                Layout.rowSpan: 6
                Layout.columnSpan: parent.columns
                Layout.fillHeight: true
                Layout.fillWidth: true
            }
            OxideButton {
                text: "Reset"
                color: "black"
                Layout.columnSpan: parent.columns
                Layout.fillWidth: true
                onClicked: {
                    controller.breadcrumb("settings.reset", "click", "ui");
                    controller.loadSettings();
                }
            }
            OxideButton {
                text: "Close"
                color: "black"
                Layout.columnSpan: parent.columns
                Layout.fillWidth: true
                onClicked: {
                    controller.breadcrumb("settings.close", "click", "ui");
                    controller.saveSettings();
                    settings.close();
                }
            }
        }
    }
}
