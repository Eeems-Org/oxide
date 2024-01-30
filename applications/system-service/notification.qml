import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import "qrc:/codes.eeems.oxide"

Window{
    property alias text: text.text
    property alias image: image.source
    property bool landscape: Oxide.landscape

    id: window
    objectName: "notification"
    flags: Qt.FramelessWindowHint
    visible: false
    x: landscape ? 0 : width - background.border.width
    y: height - background.border.width
    width: Screen.width / 2
    height: Screen.height / 2
    function orientationWidth(){ return landscape ? height : width; }
    function orientationHeight(){ return landscape ? width : height; }
    contentOrientation: landscape ? Qt.LandscapeOrientation : Qt.PortraitOrientation
    color: "transparent"

    Item{
        id: notification
        // Must centerIn and specify width/height to force rotation to actually work
        width: window.orientationWidth()
        height: window.orientationHeight()
        anchors.centerIn: parent
        rotation: landscape ? 90 : 0
        clip: true

        Rectangle{
            id: background
            color: "white"
            border.color: "black"
            border.width: 2
            radius: 10
            anchors.fill: rowLayout
        }
        RowLayout{
            id: rowLayout
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            Image{
                id: image
                visible: status === Image.Ready
                Layout.preferredHeight: 50
                Layout.preferredWidth: 50
                Layout.alignment: Qt.AlignCenter
                Layout.leftMargin: 10
                Layout.topMargin: 10
                Layout.bottomMargin: 10
            }
            Label{
                id: text
                visible: this.text.length
                color: "black"
                Layout.margins: 10
                Layout.alignment: Qt.AlignCenter
            }
        }
    }
}
