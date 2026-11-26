import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "qrc:/codes.eeems.oxide"

OxideWindow {
    id: window
    visible: true
    title: Qt.application.displayName + " - " + filePath.split('/').pop()
    headerBackgroundColor: "white"
    backgroundColor: "white"
    color: "black"
    reserveSystemSpace: true

    leftMenu: [
        OxideButton {
            text: "⬅️"
            onClicked: Qt.quit()
            color: window.color
            backgroundColor: window.backgroundColor
            borderColor: window.backgroundColor
        }
    ]
    centerMenu: [
        Label {
            color: window.color
            text: window.title
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
    ]

    initialItem: Item {
        anchors.fill: parent
        Label {
            visible: image.status == Image.Loading
            anchors.centerIn: parent
            text: "Loading..."
            color: window.color
        }
        Image {
            id: image
            anchors.fill: parent
            fillMode: Image.PreserveAspectFit
            asynchronous: true
            source: 'file:' + filePath
            sourceSize.width: width
            sourceSize.height: height
            visible: status != Image.Loading
        }
    }
}
