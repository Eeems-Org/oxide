import QtQuick 2.0
import QtQuick.Controls 2.4

Button {
    id: control
    flat: true
    height: control.width
    contentItem: Text {
        text: control.text
        font: control.font
        opacity: enabled ? 1.0 : 0.3
        color: control.down ? "black" : "white"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        anchors.fill: control
        implicitWidth: 100
        implicitHeight: 100
        opacity: enabled ? 1 : 0.3
        color: control.down ? "white" : "black"
        border.color: "white"
        border.width: 1
        radius: control.width / 2
    }
}
