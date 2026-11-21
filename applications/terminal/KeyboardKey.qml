import QtQuick 2.9
import QtQuick.Controls 2.15
import "."
Item {
    id: root
    clip: true
    property var keyboard: null
    property string text: ""
    property string shifttext: text
    property string alttext: text
    property string ctrltext: text
    property string metatext: text
    property string shiftalttext: shifttext
    property string shiftctrltext: shifttext
    property string shiftmetatext: shifttext
    property string altctrltext: alttext
    property string altmetatext: alttext
    property string ctrlmetatext: alttext
    property string shiftaltctrltext: shiftalttext
    property string shiftaltmetatext: shiftalttext
    property string shiftctrlmetatext: shiftctrltext
    property string shiftaltctrlmetatext: shiftalttext
    property bool repeatOnHold: true
    property int holdInterval: 100
    property int key: 0
    property string value: null
    property int size: 1
    property int basesize: 60
    property int fontsize: 8
    signal click(Item item)
    signal hold(Item item)
    signal release(Item item)
    property bool toggle: false
    width: size * basesize
    height: basesize
    function getText(){
        if (keyboard.hasShift && keyboard.hasAlt && keyboard.hasCtrl) {
            return shiftaltctrltext;
        }
        if (keyboard.hasShift && keyboard.hasAlt) {
            return shiftalttext;
        }
        if (keyboard.hasShift && keyboard.hasCtrl) {
            return shiftctrltext;
        }
        if (keyboard.hasAlt && keyboard.hasCtrl) {
            return altctrltext;
        }
        if (keyboard.hasShift) {
            return shifttext;
        }
        if (keyboard.hasAlt) {
            return alttext;
        }
        if (keyboard.hasCtrl) {
            return ctrltext;
        }
        return text;
    }
    function doClick(){
        if (root.click(root) !== false) {
            var modifiers = Qt.NoModifier;
            if (keyboard.hasShift) {
                modifiers = modifiers | Qt.ShiftModifier;
            }
            if (keyboard.hasAlt) {
                modifiers = modifiers | Qt.AltModifier;
            }
            if (keyboard.hasCtrl) {
                modifiers = modifiers | Qt.ControlModifier;
            }
            if (keyboard.hasMeta) {
                modifiers = modifiers | Qt.MetaModifier;
            }
            if (root.key > 0) {
                handler.keyPress(root.key, modifiers, root.value);
            } else {
                var t = root.getText();
                if (t.length > 1) {
                    handler.stringPress(t, modifiers);
                } else {
                    handler.charPress(t, modifiers);
                }
            }
            if (!root.toggle) {
                keyboard.hasShift = false;
                keyboard.hasAlt = false;
                keyboard.hasCtrl = false;
                keyboard.hasMeta = false;
            }
        }
    }
    Button {
        id: button
        text: root.getText()
        anchors.fill: parent
        onClicked: root.doClick()
    }
    Timer {
        id: timer
        repeat: true
        interval: root.holdInterval
        onTriggered: root.doClick()
    }
}
