import QtQuick 2.15
import QtQuick.Window 2.15
import codes.eeems.blight 1.0 as Blight

Item{
    Blight.Surface{
        identifier: parent.objectName
        anchors.fill: parent
    }
    // MouseArea{
    //     anchors.fill: parent
    // }
    // MultiPointTouchArea{
    //     anchors.fill: parent
    // }
    // Keys.onPressed: function(event){
    //     console.log(event);
    //     event.accepted = true;
    // }
    // Keys.onReleased: function(event){
    //     console.log(event);
    //     event.accepted = true;
    // }
}
