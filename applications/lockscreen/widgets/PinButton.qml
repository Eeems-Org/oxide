import QtQuick 2.0
import QtQuick.Controls 2.4

Button {
    id: control
    flat: true
    property bool hideBorder: false
    property bool showPress: true
    property color textColor: parent.itemTextColor || "black"
    property color activeTextColor: parent.itemActiveTextColor || "white"
    property color backgroundColor: parent.itemBackgroundColor || "white"
    property color activeBackgroundColor: parent.itemActiveBackgroundColor || "black"
    property color borderColor: parent.itemBorderColor || "black"
    property int borderWidth: parent.itemBorderWidth ?? 3
    implicitWidth: 150
    implicitHeight: implicitWidth
    font.pixelSize: width / 2
    height: width
    contentItem: Text {
        text: control.text
        font: control.font
        color: control.showPress && control.down ? control.activeTextColor : control.textColor
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        anchors.fill: control
        color: control.showPress && control.down && !control.hideBorder ? control.activeBackgroundColor : control.backgroundColor
        border.color: control.hideBorder ? "transparent" : control.borderColor
        border.width: control.borderWidth
        radius: control.width / 2
    }
}
