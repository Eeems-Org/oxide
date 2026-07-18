import QtQuick 2.10
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.11
import "qrc:/codes.eeems.oxide"
import "categories"
import "widgets"

OxideWindow {
    id: window
    visible: true
    reserveSystemSpace: true
    headerBackgroundColor: "white"
    backgroundColor: "white"
    color: "black"
    title: "Settings"
    FontLoader { id: iconFont; source: "/font/icomoon.ttf" }
    Connections {
        target: controller
        function onCategoryRequested(name) {
            var component = categoryPages[name];
            if (component === undefined) {
                return;
            }
            if (stack.depth > 1) {
                stack.pop(null); // Pop back to category list
            }
            // Create once, reuse same instance on every push
            if (!categoryInstances[name]) {
                categoryInstances[name] = component.createObject(window);
            }
            stack.push(categoryInstances[name]);
        }
    }

    property var categoryPages: ({
        "general": generalPageComponent,
        "display": displayPageComponent,
        "power": powerPageComponent,
        "wifi": wifiPageComponent,
        "notifications": notificationsPageComponent
    })
    property var categoryInstances: ({})

    leftMenu: [
        OxideButton {
            text: "⬅️"
            color: window.color
            backgroundColor: window.backgroundColor
            borderColor: window.backgroundColor
            onClicked: {
                controller.breadcrumb("settings.back", "click", "ui");
                if (stack.depth <= 1) {
                    Qt.quit();
                    return;
                }
                stack.pop();
            }
        }
    ]
    centerMenu: [
        Label {
            text: stack.depth > 1 && stack.currentItem.title ? stack.currentItem.title : "Settings"
            color: window.color
            font.pixelSize: 32
            font.bold: true
        }
    ]

    initialItem: ListView {
        id: categoryList
        clip: true
        interactive: false
        model: ListModel {
            ListElement {
                name: "general"
                label: "General"
                description: "Locale, Timezone"
            }
            ListElement {
                name: "display"
                label: "Display"
                description: "Battery, Wifi, Date, Brightness, Columns"
            }
            ListElement {
                name: "power"
                label: "Power"
                description: "Sleep, Lock, Suspend"
            }
            ListElement {
                name: "wifi"
                label: "WiFi"
                description: "Networks, Connect, Disconnect"
            }
            ListElement {
                name: "notifications"
                label: "Notifications"
                description: "Display Time"
            }
        }
        delegate: Rectangle {
            width: categoryList.width
            height: 120
            color: "white"
            border.color: "black"
            border.width: 1
            RowLayout {
                anchors.fill: parent
                anchors.margins: 16
                ColumnLayout {
                    Layout.fillWidth: true
                    Label {
                        text: model.label
                        font.pixelSize: 32
                        font.bold: true
                        color: "black"
                    }
                    Label {
                        text: model.description
                        font.pixelSize: 22
                        color: "black"
                        Layout.fillWidth: true
                    }
                }
                Label {
                    text: "\u203a"
                    font.pixelSize: 48
                    color: "black"
                }
            }
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    controller.breadcrumb("settings.category", model.name, "ui");
                    controller.navigateToCategory(model.name);
                }
            }
        }
    }

    Component {
        id: generalPageComponent
        GeneralPage {}
    }
    Component {
        id: displayPageComponent
        DisplayPage {}
    }
    Component {
        id: powerPageComponent
        PowerPage {}
    }
    Component {
        id: wifiPageComponent
        WifiPage {}
    }
    Component {
        id: notificationsPageComponent
        NotificationsPage {}
    }
}
