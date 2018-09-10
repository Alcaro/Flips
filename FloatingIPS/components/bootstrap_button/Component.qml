import QtQuick 2.7
import QtGraphicalEffects 1.0
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.11

Item {
    id: root
    width: text.width + 26; height: text.height + 26
    property alias text: text.text
    property alias font: text.font
    property string type: 'primary'
    signal clicked

    Button {
        id: button
        enabled: parent.enabled
        width: parent.width; height: parent.height
        background: Rectangle { color: "#00000000" }
        onClicked: root.clicked()

        property var colorStyle: {
            'primary'   : { 'normal' : '#0073ff', 'hovered' : '#005fd4', 'down' : '#00469c', 'border' : '#80b9ff' },
            'secondary' : { 'normal' : '#606970', 'hovered' : '#50565c', 'down' : '#3c4145', 'border' : '#b0b4b8' },
            'success'   : { 'normal' : '#249c3c', 'hovered' : '#1e7d31', 'down' : '#155722', 'border' : '#91cc9d' },
            'danger'    : { 'normal' : '#d62f3d', 'hovered' : '#bf212e', 'down' : '#82161d', 'border' : '#eb969d' },
            'warning'   : { 'normal' : '#ffb60a', 'hovered' : '#db9a02', 'down' : '#9e6f02', 'border' : '#ffdb87' },
            'info'      : { 'normal' : '#1797ad', 'hovered' : '#1797ad', 'down' : '#0e5966', 'border' : '#8bcbd6' },
            'dark'      : { 'normal' : '#2c3136', 'hovered' : '#2c3136', 'down' : '#1d2024', 'border' : '#949799' }
        }

        Rectangle {
            anchors.fill: parent
            border.width: button.hovered ? 3 : 0
            border.color: button.colorStyle[root.type]['border']
            radius: 6
            color: button.hovered ?
                       ( button.down ? button.colorStyle[root.type]['down'] :
                                       button.colorStyle[root.type]['hovered'] ) :
                       button.colorStyle[root.type]['normal'];
        }

        Text {
            id: text
            anchors.centerIn: button
            text: "CLICK ME"
            font.family: "Helvetica"
            font.pixelSize: 15
            font.bold: true
            color: "white"
        }
    }
}


