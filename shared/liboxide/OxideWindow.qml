import QtQuick 2.10
import QtQuick.Window 2.3
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.0

ApplicationWindow {
    id: window
    property alias page: page
    property alias focus: page.focus
    property alias leftMenu: leftMenu.children
    property alias rightMenu: rightMenu.children
    property alias centerMenu: centerMenu.children
    property alias pageContent: page.contentData
    property bool landscape: false
    property bool disableQuitShortcut: false
    function orientationWidth(){ return landscape ? height : width; }
    function orientationHeight(){ return landscape ? width : height;  }
    signal keyPressed(var event)
    signal keyReleased(var event)
    width: Screen.width
    height: Screen.height
    Page {
        id: page
        title: window.title
        visible: window.visible
        rotation: landscape ? 90 : 0
        // Must centerIn and specify width/height to force rotation to actually work
        anchors.centerIn: parent
        width: window.orientationWidth()
        height: window.orientationHeight()
        header: Rectangle {
            color: "black"
            RowLayout {
                id: leftMenu
                anchors.left: parent.left
                height: parent.height
            }
            RowLayout {
                id: rightMenu
                anchors.right: parent.right
                height: parent.height
            }
            RowLayout {
                id: centerMenu
                anchors.centerIn: parent
                height: parent.height
            }
        }
        background: Rectangle {
            id: background
            color: "black"
        }
        Shortcut {
            sequence: StandardKey.Quit
            onActivated: !disableQuitShortcut && Qt.quit()
        }
        Keys.onPressed: (event) => window.keyPressed(event)
        Keys.onReleased: (event) => window.keyReleased(event)
    }
}
