# LinkIt 7697 BLE FOTA
How to use BLE to do FOTA (Firmware update) on LinkIt 7697 HDK

Additinal resource can be found at https://docs.labs.mediatek.com/resource/mt7687-mt7697

```diff
- Note. 
- Linkit 7697 HDK is only supported by LinkIt SDK 4.3 and later version. 
- There might be compile error to use them on older version.
```
## Folder Structure

* `android/ble_fota`: Android studio project files.
* `project/linkit7697_hdk/apps/ble_fota`: LinkIt SDK project files.
* `fota_blink_hi.bin`: A sample "_FOTA ready_" bin for testing firmware update. 
  * This firmware will blink usr led of Linkit 7697 HDK in a Morse code "HI" pattern (4 dot + 2 dot). 
  * Wikipedia [link](https://en.wikipedia.org/wiki/Morse_code) of Morse code.

## How to Build

### Device Side

* Put/Extract the files into SDK root, so that there is `[SDK_root]/project/linkit7697_hdk/apps/ble_fota`
* Execute `./build.sh linkit7697_hdk ble_fota bl` under Linux enviornment
  * note. make sure to add `bl` postfix to rebuild bootloader
* Check generated bin at `[SDK_root]/out/linkit7697_hdk/ble_fota/ble_fota.bin`
* Use Flashtool to download `[SDK_root]/out/linkit7697_hdk/ble_fota/flash_download.ini` into LinkIt 7697 HDK

### Mobile Side

* Extract `android/ble_fota` to anywhere you prefer
* Have Android studio open the project
* Build->Make Project
* Run->Run Application
* Have `fota_blink_hi.bin` downloaded to your phone.

### Firmware for FOTA update

* Check `FOTA ROM Package` tool (can be found in `[SDK_root]/tools/PC_tool.zip` or at Tools section of [Labs](https://docs.labs.mediatek.com/resource/mt7687-mt7697/en/downloads))
* This tool can turn a _normal_ firmware (used by flashtool) into a _FOTA ready_ firmware (used by FOTA).

## How to Run

### Phases

* A. Turn on Device, when it is ready, it will start "BLE Advertising" (appear as BLE_FOTA). Launch Android App, Scan and connects to BLE_FOTA Device.
* B. After connected, the state should become "Connected", Click "LOAD BIN" and select `fota_blink_hi.bin`
* C. Wait for FOTA transmission complete
* D. Check USR LED of LinkIt 7697 HDK. It should blink in a Morse code pattern (4 dot and 2 dot) which means "HI"

### Mobile Side

![Mobile](/images/mobile.png)

### Device Side

Below are log output from UART port of LinkIt 7697 HDK
![Device_1](/images/device_1.png)
![Device_2](/images/device_2.png)
![Device_3](/images/device_3.png)

## Message Sequence Chart

![MSC](/images/msc.png)


