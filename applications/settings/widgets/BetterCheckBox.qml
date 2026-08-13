import QtQuick 2.10
import QtQuick.Controls 2.4

CheckBox {
    id: control
    indicator: Rectangle {
        implicitWidth: 26
        implicitHeight: 26
        x: control.leftPadding
        y: parent.height / 2 - height / 2
        radius: 3
        border.color: "black"
        Rectangle {
            width: parent.width - 12
            height: parent.height - 12
            x: 6
            y: 6
            radius: 2
            color: "black"
            visible: control.checked
        }
    }

    contentItem: Text {
        text: control.text
        font: control.font
        opacity: enabled ? 1.0 : 0.3
        color: "black"
        verticalAlignment: Text.AlignVCenter
        leftPadding: control.indicator.width + control.spacing
    }
}
