import QtQuick 2.9
import QtQuick.Controls 2.4
import QtQuick.Window 2.2
import QtQuick.Dialogs 1.2

import './components/bootstrap_button' as BootstrapButton
import './components/state_selector' as StateSelector

Item {
    anchors.fill: parent

    Column {
        anchors.fill: parent
        spacing: 8

        Rectangle {
            id: statusBox
            width: parent.width; height: 60
            color: 'black'
            clip: true

            Text {
                id: statusText
                text: 'Create Patch'
                color: 'white'
                font.bold: true
                font.pointSize: 22
                anchors.centerIn: parent
            }
        }

        Rectangle {
            id: cleanRomBox
            width: parent.width; height: 60
            color: 'white'
            clip: true
            property string cleanRomPath: ''
            function osPath(url) { return url.substring('file://'.length, url.length); }
            function baseName(url) { return url.substring(url.lastIndexOf('/')+1, url.length); }
            function baseNameNoExt(url) { return url.substring(url.lastIndexOf('/')+1, url.lastIndexOf('.')); }
            function getExtension(url) { return url.substring(url.lastIndexOf('.'), url.length); }
            function isValidFile(url) {
                var extension = url.substring(url.lastIndexOf('.')+1, url.length);
                switch (extension){
                case 'smc':
                case 'sfc':
                case 'nes':
                case 'gb':
                case 'gbc':
                case 'gba':
                case 'vb':
                case 'sms':
                case 'smd':
                case 'md':
                case 'ngp':
                case 'n64':
                case 'z64':
                    return true;
                default:
                    return false;
                }
            }

            Text {
                id: cleanRomText
                text: '<DROP CLEAN ROM FILE>'
                font.bold: true
                font.pointSize: 22
                anchors.centerIn: parent
            }

            DropArea {
                enabled: parent.enabled
                anchors.fill: parent
                onDropped: {
                    if(cleanRomBox.isValidFile(drop.urls[0])){
                        cleanRomBox.cleanRomPath = cleanRomBox.osPath(drop.urls[0]);
                        cleanRomText.text = cleanRomBox.baseName(cleanRomBox.cleanRomPath);
                    } else {
                        statusText.text = 'Error: Not a valid ROM file.';
                    }
                }
            }
        }

        Rectangle {
            id: hackedRomBox
            width: parent.width; height: 60
            color: (cleanRomBox.cleanRomPath != '' ? 'white' : 'grey')
            clip: true
            property string hackedRomPath: ''
            function osPath(url) { return url.substring('file://'.length, url.length); }
            function baseName(url) { return url.substring(url.lastIndexOf('/')+1, url.length); }
            function baseNameNoExt(url) { return url.substring(url.lastIndexOf('/')+1, url.lastIndexOf('.')); }
            function getExtension(url) { return url.substring(url.lastIndexOf('.'), url.length); }
            function isValidFile(url) {
                var extension = url.substring(url.lastIndexOf('.')+1, url.length);
                switch (extension){
                case 'smc':
                case 'sfc':
                case 'nes':
                case 'gb':
                case 'gbc':
                case 'gba':
                case 'vb':
                case 'sms':
                case 'smd':
                case 'md':
                case 'ngp':
                case 'n64':
                case 'z64':
                    return true;
                default:
                    return false;
                }
            }
            function generateNewPatchPath(urlHackedRom) {
                switch (patchTypeSelector.selectedIndex){
                case 0:
                    return urlHackedRom.substring(0, urlHackedRom.lastIndexOf('/')+1) + hackedRomBox.baseNameNoExt(urlHackedRom) + '.bps';
                case 1:
                    return urlHackedRom.substring(0, urlHackedRom.lastIndexOf('/')+1) + hackedRomBox.baseNameNoExt(urlHackedRom) + '.ips';
                case 2:
                    return urlHackedRom.substring(0, urlHackedRom.lastIndexOf('/')+1) + hackedRomBox.baseNameNoExt(urlHackedRom) + '.ups';
                default:
                    return urlHackedRom.substring(0, urlHackedRom.lastIndexOf('/')+1) + hackedRomBox.baseNameNoExt(urlHackedRom);//no extension
                }
            }

            Text {
                id: hackedRomText
                text: '<DROP HACKED ROM FILE>'
                font.bold: true
                font.pointSize: 22
                anchors.centerIn: parent
                color: (cleanRomBox.cleanRomPath != '' ? 'black' : '#4e4e4e')
            }

            DropArea {
                enabled: (cleanRomBox.cleanRomPath != '')
                anchors.fill: parent
                onDropped: {
                    if(hackedRomBox.isValidFile(drop.urls[0])){
                        hackedRomBox.hackedRomPath = hackedRomBox.osPath(drop.urls[0]);
                        hackedRomText.text = hackedRomBox.baseName(hackedRomBox.hackedRomPath);
                    } else {
                        statusText.text = 'Error: Not a valid ROM file.';
                    }
                }
            }
        }

        StateSelector.Component {
            id: patchTypeSelector
            width: parent.width; height: 120

            label: "Select Patch Type:"
            list: ListModel {
                ListElement { name: "BPS" }
                ListElement { name: "IPS" }
            }
        }

        BootstrapButton.Component {
            id: createPatchButon
            enabled: (cleanRomBox.cleanRomPath != '' && hackedRomBox.hackedRomPath != '')
            text: 'Create Patch'
            anchors.horizontalCenter: parent.horizontalCenter
            type: (cleanRomBox.cleanRomPath != '' && hackedRomBox.hackedRomPath != '' ? 'primary' : 'secondary')
            onClicked: {
                var newPatchPath = hackedRomBox.generateNewPatchPath(hackedRomBox.hackedRomPath);
                if(flipsController.createPatch(patchTypeSelector.selectedIndex, false, cleanRomBox.cleanRomPath, hackedRomBox.hackedRomPath, newPatchPath)){
                    statusText.text = 'PATCH CREATED';
                    cleanRomBox.cleanRomPath = '';
                    cleanRomText.text = '<DROP CLEAN ROM FILE>';
                    hackedRomBox.hackedRomPath = '';
                    hackedRomText.text = '<DROP HACKED ROM FILE>';
                } else {
                    statusText.text = 'ERROR CREATING PATCH';
                }
            }
        }

    }
}
