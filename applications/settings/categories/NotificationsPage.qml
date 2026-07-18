import QtQuick 2.10
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.0
import "qrc:/codes.eeems.oxide"
import "../widgets"

Page {
    id: root
    objectName: "notifications"
    title: "Notifications"
    background: Rectangle { color: "white" }
    Component.onCompleted: notificationsController.init()
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: "Display time (seconds)"
                font.pixelSize: 32
                Layout.fillWidth: true
            }
            BetterSpinBox {
                from: 1
                to: 60
                stepSize: 1
                value: notificationsController.notificationDisplayTime
                onValueChanged: notificationsController.notificationDisplayTime = this.value
                Layout.preferredWidth: 300
            }
        }

        Item {
            Layout.fillHeight: true
            Layout.fillWidth: true
        }
    }
}
