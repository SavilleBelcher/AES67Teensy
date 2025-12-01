#include <Audio.h>
#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <util/atomic.h>

#include "QNEthernet.h"
#include "RTPAudio.h"
#include "SDPDiscovery.h"
#include "t41-ptp.h" 

#define USBBAUD          115200


// Max 4 channel count for TX and RX aes67 audio

// ====== AUDIO
// AudioInputs
AudioSynthWaveform        waveform1;   
AudioInputI2SQuad         LRCmic;
AudioMixer4               memicmixer;
const int txChannels = 4; // desired number of channels
AudioRecordQueue          txQueue[txChannels]; // Max 4 channels
AudioConnection           patchCord1(LRCmic, 0, memicmixer, 0);
AudioConnection           patchCord2(LRCmic, 1, memicmixer, 1);
AudioConnection           patchCord3(LRCmic, 2, memicmixer, 2);
AudioConnection           patchCord4(LRCmic, 0, txQueue[0], 0);
AudioConnection           patchCord5(LRCmic, 1, txQueue[1], 0);
AudioConnection           patchCord6(LRCmic, 2, txQueue[2], 0);
//AudioConnection           patchCord7(memicmixer, 0, txQueue[3], 0); // changed to send the revicing audio back to sending device

// AudioOutput
AudioOutputI2S            Speakers;
AudioMixer4               rxLeftMixer;
AudioMixer4               rxRightMixer;
AudioMixer4               rxRetunMixer;
const int rxChannels = 4; // desired number of channels
AudioPlayQueue            rxQueue[rxChannels]; // Max 4 channels
AudioConnection           patchCord8(rxQueue[0], 0, rxLeftMixer, 0);
AudioConnection           patchCord9(rxQueue[1], 0, rxLeftMixer, 1);
AudioConnection           patchCord10(rxQueue[2], 0, rxRightMixer, 0);
AudioConnection           patchCord11(rxQueue[3], 0, rxRightMixer, 1);
AudioConnection           patchCord12(rxLeftMixer, 0, Speakers, 0);
AudioConnection           patchCord13(rxRightMixer, 0, Speakers, 1);
AudioConnection           patchCord14(rxQueue[0], 0, rxRetunMixer, 0);
AudioConnection           patchCord15(rxQueue[1], 0, rxRetunMixer, 1);
AudioConnection           patchCord16(rxQueue[2], 0, rxRetunMixer, 2);
AudioConnection           patchCord17(rxQueue[3], 0, rxRetunMixer, 3);
AudioConnection           patchCord18(rxRetunMixer, 0, txQueue[3], 0);
//AudioConnection           patchCord18(waveform1, 0, txQueue[3], 0);

// ====== ETHERNET
byte mac[6];
IPAddress staticIP{10, 0, 69, 18};
IPAddress subnetMask{255, 255, 0, 0};
IPAddress gateway{10, 0, 69, 10};

qindesign::network::EthernetUDP udp;

// ====== VARIABLES
int previous = 0;
String SData;
uint32_t driftCheckInterval = 500000; // Audio PLL drift check interval in 500000uS = 500mS. 
uint32_t enetTimerValue = 1000000; //  RTP timer interval in 1000000nS = 1mS

bool timerState = false;
elapsedMillis status;
elapsedMillis unLockedTimeout;
bool ptpLocked = false;

// Constants
const uint32_t SAMPLE_RATE = 48000; // 48 kHz audio
const uint32_t FRAMES_PER_PACKET = 48; // Your fixed packet design


IntervalTimer syncTimer;
IntervalTimer announceTimer;
uint32_t syncTimerValue = 500000; //Sync interval in 500000uS = 500mS
uint32_t announceTimerValue = 1000000; //Announce interval in 1000000uS = 1000mS = 1S

bool master=false; // false for slave mode.
bool p2p=false; // Delay mech false for E2E 

// ====== CLASSES
l3PTP ptp(master,!master,p2p); // ptp(master_bool,slave_bool,p2p_bool)
RTPAudio RTPAudio(rxQueue, rxChannels, txQueue, txChannels, udp);
SDPDiscovery SDPDiscovery(udp);
qindesign::network::EthernetIEEE1588Class::TimerChannelModes enetTimerMode = qindesign::network::EthernetIEEE1588Class::TimerChannelModes::kSetOnCompare;

//RTP send data 
void sendRTP(){
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE){
    RTPAudio.sendRTPData();
  }
}

//Processor temp
extern float tempmonGetTemp(void);

void setup() {
  pinMode(13, OUTPUT);
  
  Serial.begin(USBBAUD);
  Serial.printf("Teensy AES 67 %s Mode\n", master ? "Master" : "Slave");
  if ( Serial && CrashReport ) {
    // Output a crash report on reboot
    Serial.print(CrashReport);
  }
  
  // Setup audio board
  Serial.println("Setup audio");
  AudioMemory(1000); //needs to be above 700 for the current channel count buffers 

  for(int i = 0; i < rxChannels; i++) {
    rxQueue[i].setBehaviour(AudioPlayQueue::NON_STALLING);
    rxQueue[i].setMaxBuffers(8); // was 8
  }
  waveform1.pulseWidth(0.5);
  waveform1.begin(0.4, 220, WAVEFORM_PULSE);

  // Setup networking
  Serial.println("Setup network");
  qindesign::network::Ethernet.setHostname("AES67_Teensy");
  qindesign::network::Ethernet.macAddress(mac);
  qindesign::network::Ethernet.begin(staticIP, subnetMask, gateway);
  qindesign::network::EthernetIEEE1588.begin();
  qindesign::network::Ethernet.onLinkState([](bool state) {
    Serial.printf("[Ethernet] Link %s\n", state ? "ON" : "OFF");
    if (state) {
      ptp.begin();
      ptp.setKp(1);
      ptp.setKi(.05);
      ptp.sporadicSyncErrorThreshold = 3000;
      //RTPAudio.Rx_multicastIP = {239, 1, 4, 19}; // set IP addresse before starting default is 239.1.4.3 in RTPAudio.h
      RTPAudio.Tx_multicastIP = {239, 1, 4, 18}; // set IP address before starting default is 239.1.4.10 in RTPAudio.h
      RTPAudio.start();
      SDPDiscovery.streamName = "AES67_Teensy";
      SDPDiscovery.start(RTPAudio.Tx_multicastIP,RTPAudio._multicastPort,txChannels);
      if (master){
        syncTimer.begin(syncInterrupt, syncTimerValue); //Sync interval in 500000uS = 500mS
        announceTimer.begin(announceInterrupt, announceTimerValue); //Announce interval in 1000000uS = 1000mS
      }
      
      Serial.printf("Started teensy AES 67  %d TX Channels and %d RX Channels\n",txChannels,rxChannels);
      timerState = qindesign::network::EthernetIEEE1588.setChannelCompareValue(0,enetTimerValue);
      Serial.printf("Enet timer Value %s", timerState ? "accepted" : "Error");
      Serial.println();
      timerState = qindesign::network::EthernetIEEE1588.setChannelMode(0,enetTimerMode);
      Serial.printf("Enet timer %s", timerState ? "enabled" : "Error");
      Serial.println();
      timerState = false;
      for(int i = 0; i < txChannels; i++) {
        txQueue[i].begin();
      }
      
    }
    else {
      RTPAudio.stop();
      SDPDiscovery.stop();
      ptp.reset();
      if (master){
        announceTimer.end();
        syncTimer.end();
      }
      for(int i = 0; i < txChannels; i++) {
        txQueue[i].end(); 
      }
      
    }
  }); 
  
  Serial.printf("Mac address:   %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.printf("Audio samples: %d\n", AUDIO_BLOCK_SAMPLES);
  Serial.print( "IP:            "); Serial.println(qindesign::network::Ethernet.localIP());
  Serial.println();

}

bool sync=false;
bool announce=false;

void loop() {

  if (qindesign::network::Ethernet.linkState()){
    RTPAudio.readAudio();
    ptp.update();
    SDPDiscovery.update();
    RTPAudio.readPackets();
    if((timerState = qindesign::network::EthernetIEEE1588.getAndClearChannelStatus(0))){
      sendRTP();
    }
    if (timerState == true){
      if (enetTimerValue >= 999000000){enetTimerValue = 0;}
      else{enetTimerValue += 1000000;}
      timerState = qindesign::network::EthernetIEEE1588.setChannelCompareValue(0,enetTimerValue);
      //Serial.printf("Enet timer Value %s", timerState ? "updated" : "Error");
      //Serial.println ("New timmer vaule is ");
      //Serial.println(enetTimerValue);
      timerState = false;
    }
    if(announce){
      announce=false;
      ptp.announceMessage();
    }
    if(sync){
      sync=false;
      ptp.syncMessage();
    }
    if (ptp.getLockCount() > 20){
    unLockedTimeout = 0;
    ptpLocked = true;
    
    }
    if (unLockedTimeout > 3000){
    ptpLocked = false;
    } 

    RTPAudio.ptpSynced = ptpLocked;
    
    digitalWrite(13, ptp.getLockCount() > 10 ? HIGH : LOW);

  }


 if(status > 2000) { // print every few seconds
    printGeneralStats(); 
    status = 0;
  }
  
  if(!master && RTPAudio.I2SSampleCount >= (SAMPLE_RATE*(driftCheckInterval*.000001))){
    checkDriftAndCorrect();
  }

}

void syncInterrupt() {
  sync=true; 
}

void announceInterrupt() {
  announce=true;
}

void printGeneralStats(){
  Serial.print("Processor (Curr/Max) = ");  Serial.print(AudioProcessorUsage());
  Serial.print("/"); Serial.print(AudioProcessorUsageMax());   
  Serial.print(". Audio Memory (Curr/Max) = ");  Serial.print(AudioMemoryUsage());
  Serial.print("/"); Serial.print(AudioMemoryUsageMax());
  Serial.print(" Processor Temp:"); Serial.print(tempmonGetTemp());Serial.println(" C");
  if (!master){
    Serial.printf("PTP Locked count:%d %s  ", ptp.getLockCount(), ptpLocked ? "Locked" : "Not Locked"); 
    Serial.print("Offset:");Serial.print(ptp.getOffset());Serial.print("  BadSynccount:");Serial.println(ptp.badSyncCount);Serial.println();
  }
          
}

// Slave Audio PLL adjustment to align with the Enet PLL
// Drift tracking variables
uint64_t ptp_start_time_ns = 0;
uint32_t pll_audio_num_current = 0;

const float GAIN_P = 1.5;  // Proportional gain - more aggressive
const float GAIN_I = 0.45; // Integral gain - to wipe out slow drift
volatile float integral_error = 0;

void checkDriftAndCorrect() {
  timespec ptp_now_ts; 
  qindesign::network::EthernetIEEE1588.readTimer(ptp_now_ts);
  uint64_t ptp_now = ((uint64_t)ptp_now_ts.tv_sec * 1000000000ull) + ptp_now_ts.tv_nsec;

  if (ptp_start_time_ns == 0 || ptp.getLockCount() < 20 || ptp.consecutiveBadCount > 0) {
    ptp_start_time_ns = ptp_now;
    RTPAudio.I2SSampleCount = 0;
    return;
  }
  
  if ((ptp_now_ts.tv_nsec < 20000000) || (ptp_now_ts.tv_nsec > 980000000)) {
    Serial.println("Skipping correction near PTP second boundary");
    ptp_start_time_ns = ptp_now;
    RTPAudio.I2SSampleCount = 0;
    return;
  }


  int64_t elapsed_ns = (int64_t)ptp_now - (int64_t)ptp_start_time_ns;
  float elapsed_sec = elapsed_ns / 1e9f;
  Serial.printf("Δt: %llu ns (%.3f sec)  ", elapsed_ns, elapsed_sec);

  float expected_samples = elapsed_sec * SAMPLE_RATE; 
  Serial.printf("I2S Sample Count: %lu (Mesured ~%.8f )  ", RTPAudio.I2SSampleCount, expected_samples);

  float drift_ppm = ((float)RTPAudio.I2SSampleCount - expected_samples) / expected_samples * 1e6f;

  // Ignore outliers: skip controller update if absurd drift
  if (fabs(drift_ppm) > 3000.0f) {
      Serial.printf("Skipping correction: drift too high (%.2f ppm)\n", drift_ppm);
      RTPAudio.I2SSampleCount = 0;
      ptp_start_time_ns = ptp_now;
      return;
  }

  // Filter drift to smooth noise
  static float filtered_drift_ppm = 0;
  static bool filter_initialized = false;

  // Reset filter if large jump detected
  if (!filter_initialized || fabs(drift_ppm - filtered_drift_ppm) > 200.0f) {
      filtered_drift_ppm = drift_ppm;
      integral_error = 0;
      filter_initialized = true;
  } else {
      filtered_drift_ppm = 0.2f * drift_ppm + 0.8f * filtered_drift_ppm;
  }

  // Accumulate error for I term
  integral_error += filtered_drift_ppm * (driftCheckInterval / 1000.0f); // ms → sec

  // Clamp integral to ±200
  if (integral_error > 200) integral_error = 200;
  if (integral_error < -200) integral_error = -200;

  float correction_ppm = (filtered_drift_ppm * GAIN_P) + (integral_error * GAIN_I);

  // Clamp correction to ±200
  if (correction_ppm > 1000) correction_ppm = 1000;
  if (correction_ppm < -1000) correction_ppm = -1000;

  Serial.printf("raw:%.2f ppm  filtered:%.2f  correction:%.2f  ",drift_ppm, filtered_drift_ppm, correction_ppm);
  
  correctAudioPLL(correction_ppm);

  RTPAudio.I2SSampleCount = 0;
  ptp_start_time_ns = ptp_now;
}


void correctAudioPLL(float drift_ppm) {
  const float GAIN = 1; // Gain factor to make corrections smoother (optional tuning)
  uint32_t pll_audio_num = CCM_ANALOG_PLL_AUDIO_NUM;
  uint32_t pll_audio_denom = CCM_ANALOG_PLL_AUDIO_DENOM;

  if (pll_audio_denom == 0) return; // avoid div/0

  // Calculate correction: GAIN * drift
  int32_t correction = (int32_t)((drift_ppm * GAIN / 1e6) * pll_audio_denom);

  // Limit correction range (safety)
  if (correction > 200) correction = 200;
  if (correction < -200) correction = -200;

  int32_t new_num = (int32_t)pll_audio_num - correction;

  // Bound NUM inside legal 22-bit range
  if (new_num < 0) new_num = 0;
  if (new_num > 0x3FFFFF) new_num = 0x3FFFFF;

  // Apply the new setting
  CCM_ANALOG_PLL_AUDIO_NUM = (uint32_t)new_num;
  pll_audio_num_current = new_num;

  Serial.printf("[PLL] NUM adjusted to: %lu (correction: %ld)\n", new_num, correction);
}