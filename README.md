# AES67Teensy
An AES67 Audio-over-IP implemented on the Teensy 4.1 using I2S audio input/output
48k 16bit 1ms packet time. 

Big thanks to the work of the following libraries. 
- Modified version of https://github.com/ssilverman/QNEthernet/tree/ieee1588-2.
- Modified version of https://github.com/IMS-AS-LUH/t41-ptp. 
- optional https://github.com/alex6679/teensy-4-usbAudio.

## Features
- PTP timing Master or Slave 
- 4x4 4-channel RTP audio streaming
- Slave mode with Real-time Audio PLL sync to PTP clock correction
- RTP/SDP discovery for Tx Stream 


## Hardware 
- Teensy 4.1
- Audio Board (Rev D) - optional
- Ethernet kit 

## 
- if usbAudio used select 48KHz from the tool dropdown settings. 
- if usdAudio not used. Need to change AUDIO_SAMPLE_RATE_EXACT in AudioStream.h to 48000.0f
  - location example C:\Users\****\AppData\Local\Arduino15\packages\teensy\hardware\avr\1.59.0\cores\teensy4\AudioStream.h
- tested up to 4 channels Tx and Rx 
- tested with: 
  - Magewell AES67 Pro convert master and slave 
  - Lawo Virtual Patch Bay
  - Clear-Com IPA card
	 
