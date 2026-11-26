import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "qrc:/codes.eeems.oxide"

OxideWindow {
    id: window
    visible: true
    title: filePath.split('/').pop()
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
        Label { color: window.color; text: window.title; elide: Text.ElideRight; Layout.fillWidth: true }
    ]

    initialItem: ColumnLayout { anchors.fill: parent
        OxideButton {
            text: "\u25b2"
            backgroundColor: window.backgroundColor
            color: window.color
            visible: listView.contentY > 0
            Layout.fillWidth: true
            onClicked: listView.pageUp()
        }
        ListView {
            id: listView
            Layout.fillWidth: true; Layout.fillHeight: true
            clip: true
            snapMode: ListView.SnapOneItem
            interactive: false
            boundsBehavior: Flickable.StopAtBounds
            model: lineModel
            delegate: Text {
                width: listView.width
                text: model.line
                font.family: "monospace"
                font.pixelSize: window.page.font.pixelSize
                verticalAlignment: Text.AlignVCenter
                padding: 4
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            }
            ScrollIndicator.vertical: ScrollIndicator {
                contentItem: Rectangle { color: window.color; implicitWidth: 6; radius: width / 2 }
                background: Rectangle { color: window.backgroundColor; border.color: window.color; border.width: 1; implicitWidth: 6; radius: width / 2 }
            }

            function pageSize() { return height; }

            function pageDown() {
                var nextY = contentY + pageSize();
                var maxY = Math.max(contentHeight - height, 0);
                if (!atYEnd) {
                    contentY = nextY;
                } else {
                    contentY = Math.min(nextY, maxY);
                }
            }

            function pageUp() {
                contentY = Math.max(contentY - pageSize(), 0);
            }

            onContentYChanged: {
                lineModel.ensureLoaded(0, lineModel.count);
            }
        }
        OxideButton {
            text: "\u25bc"
            backgroundColor: window.backgroundColor
            color: window.color
            visible: listView.contentY < listView.contentHeight - listView.height - 1 || !listView.atYEnd
            Layout.fillWidth: true
            onClicked: listView.pageDown()
        }
    }
}
