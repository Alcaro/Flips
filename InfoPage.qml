import QtQuick 2.9
import QtQuick.Controls 2.4
import QtQuick.Window 2.2
import QtQuick.Dialogs 1.2

import './components/bootstrap_button' as BootstrapButton

Item {
    anchors.fill: parent

    Rectangle {
        anchors.fill: parent
        color: '#4e4e4e'

        Flickable {
            anchors.fill: parent
            contentHeight: parent.height + 350

            flickableDirection: Flickable.VerticalFlick
            ScrollBar.vertical: ScrollBar {
                width: 15
                active: true
                onActiveChanged: {
                    if (!active)
                        active = true;//Keep scrollbar always visible
                }
            }

            TextArea {
                 id: textArea
                 clip: true
                 readOnly: true
                 width: parent.width - 15; height: parent.height
                 anchors.margins: 2
                 anchors.centerIn: parent
                 wrapMode: TextEdit.Wrap
                 color: "white"
                 font.family: "Helvetica"
                 font.bold: true
                 font.pointSize: 22
                 textFormat: Text.RichText
                 onLinkActivated: Qt.openUrlExternally(link)
                 text: '<h1>About</h1>
    <p>This program is a <a href="https://www.qt.io/">qt5</a> GUI wrapper
    around the Floating IPS library written by
    <a href="https://github.com/Alcaro">Alcaro</a> on Github.</p>

    <p>Alcaro\'s repo can be found here: <a href="https://github.com/Alcaro/Flips">https://github.com/Alcaro/Flips</a></p>

    <p>The qt5 GUI wrapper is written by me, <a href="https://github.com/covarianttensor">covarianttensor</a>, on Github.</p>

    <p>My fork repo can be found here: <a href="https://github.com/covarianttensor/Flips">https://github.com/covarianttensor/Flips</a></p>

   <p>This software is licensed under the GPL3 and full legal text can be found <a href="https://raw.githubusercontent.com/Alcaro/Flips/master/COPYING.gpl3">here</a>.</p>

   <p>An interpretation of the license can be found <a href="https://raw.githubusercontent.com/Alcaro/Flips/master/COPYING">here</a>.</p>

    <p>Special thanks to <a href="https://github.com/Alcaro">Alcaro</a> for creating this library.</p>

    <p>This program (GUI wrapper) was created with the purpose of running on OS X, but can be compiled to run on any platform that supports qt5.</p>

<p><a href="https://www.flaticon.com/free-icon/inclined-patch_84538#term=patch&page=1&position=14">patch.png</a> Icon made by <a href="https://www.flaticon.com/authors/freepik">Freepik</a> from <a href="https://www.flaticon.com/">www.flaticon.com</a> </p>

<p><a href="https://www.flaticon.com/free-icon/create-new-pencil-button_60785#term=create&page=1&position=3">create.png</a> Icon made by <a href="https://www.flaticon.com/authors/google">Google</a> from <a href="https://www.flaticon.com/">www.flaticon.com</a> </p>'
            }
        }
    }
}
