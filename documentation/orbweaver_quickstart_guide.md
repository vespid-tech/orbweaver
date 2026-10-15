# Configuration with Windows

## What you'll need
1) Orbweaver
2) desktop or laptop computer running Windows
2) USB cable
3) antenna

## Loading Firmware
1) Download the latest Orbweaver binary: [Orbweaver binary](https://github.com/vespid-tech/orbweaver/releases/download/orbweaver-v0.0.1/orbweaver_v0.0.1.d810f2a.bin).
2) Download the latest Orbweaver configurator: [Orbweaver configurator](https://github.com/vespid-tech/orbweaver/releases/download/configurator-v0.0.1/orbweaver_configurator_v0.0.1.exe)
3) Run the Configurator. You should see the following user interface:

<p align="center">
  <img src="./images/configurator.jpg"
       alt="Configurator GUI">
</p>

4) **Connect the Orbweaver to the PC via a USB cable**. PWR LED should illuminate indicating Orbwaever is powered. If Orbeaver firmware has not been loaded, LEDs 3, 2, and 1, should cycle on/off in that order. This indicates the Orbweaver bootloader is running. If Orbweaver firmware has previously been loaded, LED 3 will blink, and LEDs 1 and 2 should stay off. This indcates Orbweaver application is running.
5) In the Configurator next to Com port: dropdown, **press the Refresh button**. The dropdown should populate the comport dropdown Orbweaver is using. **Make sure the comport in use is selected**.
6) If the Orbweaver bootloader is running select **Load Mode: Bootloader**. If the Orbweaver application is running select **Load Mode: Application**. Refer to step 3 to determine which mode is active.
7) **Click the Browse button** and browse to the Orbweaver binary downloaded in step 1.
8) Click **"Load Binary"**. Loading should complete after several seconds. Once loading is complete, a load complete dialog box should pop up. **Press OK** on the Load Complete dialog box. The binary is now loaded.
9) If you wish to configure Orbweaver proceed to the Configuration section of this document.

## Configuration

1) In the Configurator, assign an address to the Orbweaver **enter an address in the Address (1-254): box**. We recommend using address 1 as the first Orbweaver address in your network. The second Orbweaver should be address 2, and so on. **Do not use the same address as that displayed in  Source Address (1-254):**, which defaults to 254.
2) **Press "Set New Address"**.
3) **Select the Frequency Band** you plan to operate Orbweaver in. In North and South America, use ITU region 2. In Europe, Africa, and West Asia, use ITU region 1.
4) **Press "Set Frequency Band"**.
5) Press "Get Configuration" and ensure the Output matches the settings you selected above.
6) Orbweaver is now configured and ready for use.

## Further Configuration and Networking

For more information on Orbweaver configuration (including the other configuration options), refer to the following document: [Orbweaver Networks] 