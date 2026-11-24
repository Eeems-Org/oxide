import QtQuick 2.9
import QtQuick.Layouts 1.0

Item {
    id: root
    property var keyboard: null
    property real size: 1
    property int basesize: keyboard.basesize || 90

    Layout.fillHeight: true
    implicitWidth: basesize * size
}
