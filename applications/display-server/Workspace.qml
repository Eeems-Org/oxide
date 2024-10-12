import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15

ApplicationWindow {
    id: workspace
    objectName: "Workspace"
    width: Screen.width
    height: Screen.height
    x: 0
    y: 0
    visible: true
    function loadComponent(url: string, props): string{
        var comp = Qt.createComponent(url, Component.PreferSynchronous);
        if(comp.status === Component.Error){
            return comp.errorString();
        }
        return comp.createObject(workspace, props).objectName;
    }
    background: Rectangle{
        color: "white"
        anchors.fill: parent
    }
    contentData: []
}
