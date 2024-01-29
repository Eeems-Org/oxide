import QtQuick 2.15
import QtQuick.Window 2.15
import "qrc:/codes.eeems.oxide"

OxideWindow{
    id: window
    objectName: "notification"
    width: notification.width
    height: notification.width
    x: landscape ? 0 : width
    y: height
    backgroundColor: "transparent"
    visible: false
    initialItem: Rectangle{
        id: notification
        color: "white"
        border.color: "black"
        border.width: 1
        radius: 10
        clip: true

        Image{
            id: image
            objectName: "notificationIcon"

            anchors.left: parent.left
            anchors.leftMargin: 10
            anchors.topMargin: 10
            anchors.bottomMargin: 10
        }
        Text{
            objectName: "notificationText"

            anchors.margins: 10
            anchors.left: image.right
            anchors.right: parent.right

            wrapMode: Text.Wrap
            padding: 10
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignLeft
        }
    }
}
