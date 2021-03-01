import QtQuick 2.10
import QtQuick.Window 2.3
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.0

Item {
    id: root
    clip: true
    width: height
    enabled: visible
    signal clicked;
    signal longPress;
    property string source: "qrc:/img/icon.png"
    property string text: ""

    Item {
        id: spacer
        height: root.height * 0.05
    }
    Image {
        id: icon
        fillMode: Image.PreserveAspectFit
        anchors.top: spacer.bottom
        height: root.height * (root.text ? 0.70 : 0.90)
        width: root.width
        source: root.source
    }
    Text {
        text: root.text
        width: root.width
        height: root.text ? root.height * 0.20 : 0
        font.pointSize: 100
        minimumPointSize: 1
        anchors.top: icon.bottom
        fontSizeMode: Text.Fit
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter
    }
    MouseArea {
        anchors.fill: root
        enabled: root.enabled
        onClicked: root.clicked()
        onPressAndHold: root.longPress();
    }
}
