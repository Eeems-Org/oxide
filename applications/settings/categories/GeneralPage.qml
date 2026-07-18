import QtQuick 2.10
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.0
import "qrc:/codes.eeems.oxide"
import "../widgets"

Page {
    id: root
    objectName: "general"
    title: "General"
    background: Rectangle { color: "white" }
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: "Locale"
                font.pixelSize: 32
                Layout.fillWidth: true
            }
            ComboBox {
                id: localeCombo
                Layout.preferredWidth: 400
                Layout.preferredHeight: 48
                flat: true
                font.pixelSize: 32

                delegate: ItemDelegate {
                    width: parent.width
                    text: modelData
                    font.pixelSize: 32
                }
                indicator: Label {
                    text: "\u25BC"
                    font.pixelSize: 24
                    color: "black"
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    verticalAlignment: Text.AlignVCenter
                }

                model: controller.locales
                onActivated: controller.locale = textAt(currentIndex)
                Component.onCompleted: currentIndex = indexOfValue(controller.locale)
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: "Timezone"
                font.pixelSize: 32
                Layout.fillWidth: true
            }
            ComboBox {
                id: timezoneCombo
                Layout.preferredWidth: 400
                Layout.preferredHeight: 48
                flat: true
                font.pixelSize: 32

                delegate: ItemDelegate {
                    width: parent.width
                    text: modelData
                    font.pixelSize: 32
                }
                indicator: Label {
                    text: "\u25BC"
                    font.pixelSize: 24
                    color: "black"
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    verticalAlignment: Text.AlignVCenter
                }

                model: controller.timezones
                onActivated: controller.timezone = textAt(currentIndex)
                Component.onCompleted: currentIndex = indexOfValue(controller.timezone)
            }
        }

        Item {
            Layout.fillHeight: true
            Layout.fillWidth: true
        }
    }
}
