import QtQuick 2.0
import QtQuick.Controls 2.4

ToolButton {
    flat: true
    contentItem: Text {
        text: parent.text
        font: parent.font
        opacity: enabled ? 1.0 : 0.3
        color: parent.down ? "black" : "white"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle{
        implicitWidth: 40
        implicitHeight: 40
        color: parent.down ? "white" : "black"
        opacity: parent.enabled ? 1 : 0.3
        visible: parent.down || (parent.enabled && (parent.checked || parent.highlighted))
    }
}
