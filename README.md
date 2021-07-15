# UHD-5G-MIMO
This code realized 4 channel sending for 5G signal which is helpful to future MIMO technology
The four-channel transceiver simulation of this blog is based on the UHD basic example: tx_waveforms basic writing, so first introduce the function and code structure of the tx_waveforms example.
The CPP file of the relevant example is under the path: XXX/uhd/host/example. After modifying the corresponding CPP file, you need to recompile.

sudo make
sudo make install
sudo ldconfig

This example is written based on the standard example tx_waveforms. The main framework is the same as that in Part 3. The specific program logic can refer to Figure 3.

In this example, some help prompts have been modified to make it easier for users to understand. At the same time, there are a few English comments in the code.

--file arg supports sending user-defined waveform and signal information. (The file suffix needs to be of type .bin) Users can use Matlab to generate signal waveforms, and then send them through USRP and connect to an oscilloscope for performance verification.

--otw arg represents the type of data transmitted on the cable (Over the wire type)

USRP N310 operating status
When the red light is on, it means that the channel is activated and in the running state
![image](https://user-images.githubusercontent.com/61801817/125753626-cc90cc5b-1c08-4b21-8c91-ab12cc187f55.png)

