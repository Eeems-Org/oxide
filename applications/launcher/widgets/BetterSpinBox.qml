import QtQuick 2.10
import QtQuick.Controls 2.4

SpinBox {
    id: control
    property bool upPressed: up.pressed
    property bool downPressed: down.pressed
    leftPadding: padding + (down.indicator ? down.indicator.width : 0)
    up.indicator: Rectangle {
        x: control.mirrored ? 0 : control.width - width
        height: control.height
        implicitWidth: 100
        implicitHeight: 40
        visible: control.enabled && control.value < control.to
        color: "white"
        border.color: "black"

        Text {
            text: "+"
            font.pixelSize: control.font.pixelSize * 2
            color: "black"
            anchors.fill: parent
            fontSizeMode: Text.Fit
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }
    down.indicator: Rectangle {
        x: control.mirrored ? control.width - width : 0
        height: control.height
        implicitWidth: 100
        implicitHeight: 40
        visible: control.enabled && control.value > control.from
        color: "white"
        border.color: "black"

        Text {
            text: "-"
            font.pixelSize: control.font.pixelSize * 2
            color: "black"
            anchors.fill: parent
            fontSizeMode: Text.Fit
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }
}
