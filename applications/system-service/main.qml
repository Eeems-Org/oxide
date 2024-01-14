import QtQuick 2.15
import QtQuick.Window 2.15

Window{
    width: Screen.width
    height: Screen.width
    x: 0
    y: 0
    opacity: 0
    visible: true
    Shortcut{
        sequence: "Alt+Tab"
        onActivated: controller.taskSwitcher()
    }
    Shortcut{
        sequences: ["Shift+Option+S", "Shift+Meta+S", "Shift+End+S"]
        onActivated: controller.screenshot()
    }
    Shortcut{
        sequence: ["Ctrl+Shift+Esc", "Ctrl+Shift+1"]
        onActivated: controller.processManager()
    }
    Shortcut{
        sequence: ["Option+Backspace", "Meta+Backspace", "End+Backspace"]
        onActivated: controller.back()
    }
}
