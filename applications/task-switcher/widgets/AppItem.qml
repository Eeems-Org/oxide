import QtQuick 2.10
import QtQuick.Window 2.3
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.0

Item {
    id: root
    clip: true
    width: height
    signal clicked;
    property string source: "qrc:/img/icon.png"
    property string text: ""

    Image {
        fillMode: Image.PreserveAspectFit
        height: root.height * 0.75
        width: root.width
        source: root.source
    }
    Text {
        text: root.text
        width: root.width
        height: root.height * 0.25
        font.pointSize: 100
        minimumPointSize: 1
        anchors.bottom: root.bottom
        fontSizeMode: Text.Fit
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter
    }
    MouseArea {
        anchors.fill: root
        enabled: root.enabled
        onClicked: root.clicked()
    }
}
