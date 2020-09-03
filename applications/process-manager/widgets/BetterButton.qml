import QtQuick 2.0
import QtQuick.Controls 2.4

Button {
    id: control
    flat: true
    property color backgroundColor: "black"
    property color borderColor: "black"
    property color color: "white"
    contentItem: Text {
        text: control.text
        font: control.font
        opacity: enabled ? 1.0 : 0.3
        color: control.color
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        anchors.fill: control
        implicitWidth: 100
        implicitHeight: 40
        opacity: enabled ? 1 : 0.3
        color: control.backgroundColor
        border.color: control.borderColor
        border.width: 1
        radius: 2
    }
}
