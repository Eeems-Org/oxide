import QtQuick 2.0
import QtQuick.Controls 2.4

Button {
    id: control
    flat: true
    property bool hideBorder: false
    property bool showPress: true
    implicitWidth: 150
    implicitHeight: implicitWidth
    font.pixelSize: width / 2
    height: width
    contentItem: Text {
        text: control.text
        font: control.font
        color: control.showPress && control.down || control.hideBorder ? "black" : "white"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        anchors.fill: control
        color: control.showPress && control.down && !control.hideBorder ? "white" : "black"
        border.color: control.hideBorder ? "transparent" : "white"
        border.width: 3
        radius: control.width / 2
    }
}
