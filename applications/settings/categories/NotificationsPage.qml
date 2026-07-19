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
    StackView.onActivated: notificationsController.activate()
    StackView.onDeactivating: notificationsController.deactivate()
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 8

        ListView {
            id: notifications
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: notificationsController.notificationsList
            delegate: Rectangle {
                id: item
                width: notifications.width - scrollbar.width
                height: contentRow.implicitHeight + 16
                visible: !!model.display
                color: "white"
                RowLayout {
                    id: contentRow
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 8
                    Image {
                        source: {
                            if (!model.display || !model.display.icon) {
                                return "qrc:/img/notifications/black.png";
                            }
                            var icon = model.display.icon;
                            if (!icon.startsWith("qrc:") && !icon.startsWith("file:")) {
                                return "file:" + icon;
                            }
                            return icon;
                        }
                        Layout.preferredHeight: 64
                        Layout.preferredWidth: 64
                    }
                    Label {
                        text: (model.display && model.display.text) || "Notification"
                        font.pixelSize: 24
                        Layout.alignment: Qt.AlignLeft
                        Layout.fillWidth: true
                    }
                    OxideButton {
                        text: "X"
                        color: "black"
                        Layout.preferredWidth: 50
                        onClicked: {
                            if (!model.display) return;
                            notificationsController.notificationsList.remove(model.display.identifier);
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
        }

        OxideButton {
            text: "Clear All"
            color: "black"
            Layout.fillWidth: true
            enabled: notifications.count > 0
            onClicked: {
                notificationsController.notificationsList.clear();
            }
        }
    }
}
