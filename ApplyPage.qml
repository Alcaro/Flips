import QtQuick 2.9
import QtQuick.Controls 2.4
import QtQuick.Window 2.2
import QtQuick.Dialogs 1.2

import './components/bootstrap_button' as BootstrapButton
import './components/state_selector' as StateSelector

Item {
    anchors.fill: parent

    ErrorDialog { id: errDiag }
    SuccessDialog { id: succDiag }

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
                text: 'Apply Patch (*.bps, *.ips)'
                color: 'white'
                font.bold: true
                font.pointSize: 22
                anchors.centerIn: parent
            }
        }

        Rectangle {
            id: patchBox
            width: parent.width; height: 60
            color: 'white'
            clip: true
            property string patchPath: ''
            function osPath(url) { return url.substring('file://'.length, url.length); }
            function baseName(url) { return url.substring(url.lastIndexOf('/')+1, url.length); }
            function isValidFile(url) {
                var extension = url.substring(url.lastIndexOf('.')+1, url.length);
                switch (extension){
                case 'bps':
                case 'ips':
                case 'ups':
                    return true;
                default:
                    return false;
                }
            }

            Text {
                id: patchText
                text: '<DROP PATCH FILE>'
                font.bold: true
                font.pointSize: 22
                anchors.centerIn: parent
            }

            DropArea {
                enabled: parent.enabled
                anchors.fill: parent
                onDropped: {
                    if(patchBox.isValidFile(drop.urls[0])){
                        patchBox.patchPath = patchBox.osPath(drop.urls[0]);
                        patchText.text = patchBox.baseName(patchBox.patchPath);
                    } else {
                        errDiag.text = 'Not a valid patch file.';
                        errDiag.open();
                    }
                }
            }
        }

        Rectangle {
            id: romBox
            width: parent.width; height: 60
            color: (patchBox.patchPath != '' ? 'white' : 'grey')
            clip: true
            property string romPath: ''
            property string newRomPath: ''
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
            function generateNewRomPath(urlOldRom, urlPatch) {
                return urlPatch.substring(0, urlPatch.lastIndexOf('/')+1) + romBox.baseNameNoExt(urlPatch) + romBox.getExtension(urlOldRom);
            }

            Text {
                id: romText
                text: '<DROP CLEAN ROM FILE>'
                font.bold: true
                font.pointSize: 22
                anchors.centerIn: parent
                color: (patchBox.patchPath != '' ? 'black' : '#4e4e4e')
            }

            DropArea {
                enabled: (patchBox.patchPath != '')
                anchors.fill: parent
                onDropped: {
                    if(romBox.isValidFile(drop.urls[0])){
                        romBox.romPath = romBox.osPath(drop.urls[0]);
                        romBox.newRomPath = romBox.generateNewRomPath(romBox.romPath, patchBox.patchPath);
                        romText.text = romBox.baseName(romBox.romPath);
                    } else {
                        errDiag.text = 'Not a valid ROM file.';
                        errDiag.open();
                    }
                }
            }
        }

        BootstrapButton.Component {
            id: applyPatchButon
            enabled: (patchBox.patchPath != '' && romBox.romPath != '')
            text: 'Apply Patch'
            anchors.horizontalCenter: parent.horizontalCenter
            type: (patchBox.patchPath != '' && romBox.romPath != '' ? 'primary' : 'secondary')
            onClicked: {
                var result = flipsController.applyPatch(patchBox.patchPath, romBox.romPath, romBox.newRomPath);
                if(result["success"]){
                    patchBox.patchPath = '';
                    patchText.text = '<DROP PATCH FILE>';
                    romBox.romPath = '';
                    romBox.newRomPath = '';
                    romText.text = '<DROP CLEAN ROM FILE>';

                    succDiag.text = "Patch applied successfully!";
                    succDiag.open();
                } else {
                    errDiag.text = result['errorMessage'];
                    errDiag.open();
                }
            }
        }

    }
}
