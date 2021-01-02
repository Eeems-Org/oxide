import QtQuick 2.6
import QtQuick.Controls 2.4

MenuBar {
    id: root
    property color backgroundColor: "black"
    property color textColor: "white"
    delegate: MenuBarItem {
        id: menuBarItem
        contentItem: Text {
            text: menuBarItem.text
            font: menuBarItem.font
            opacity: enabled ? 1.0 : 0.3
            color: root.textColor
            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle {
            implicitWidth: 40
            implicitHeight: 40
            opacity: enabled ? 1 : 0.3
            color: root.backgroundColor
        }
    }
    background: Rectangle {
        implicitWidth: 40
        implicitHeight: 40
        color: root.backgroundColor
    }
}
