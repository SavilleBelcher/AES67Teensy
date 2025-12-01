#ifndef teensy_aes67_audio_h_
#define teensy_aes67_audio_h_

#include <Audio.h>
#include <QNEthernet.h>
#include <queue>


class RTPAudio {
  public:
    IPAddress Rx_multicastIP{239, 1, 4, 3}; // Magewell
    IPAddress Tx_multicastIP{239, 1, 4, 10}; //
    int _multicastPort = 5004;
    uint32_t incomingSampleCount = 0;
    uint32_t I2SSampleCount = 0;
    bool ptpSynced = 0;
    void start();
    void stop();
    void readPackets();
    void readAudio();
    void sendRTPData();
    RTPAudio(AudioPlayQueue* rxQueue, int rxChannels, AudioRecordQueue* txQueue, int txChannels, qindesign::network::EthernetUDP &Udp);
  private:
    int _rxChannels = 0;
    int _txChannels = 0;
    static const int maxChannels = 4; // For AES67 max 4ch
    AudioPlayQueue *_rxQueue;
    AudioRecordQueue *_txQueue;
    qindesign::network::EthernetUDP *_udp;
    unsigned long _packetTimestamp = 0;
    unsigned long _startupTimestamp = 0;
    uint8_t _incomingAudioBuffer[maxChannels][256]; // 4 audio channels
    uint16_t _incomingAudioBufferIndex = 0;
    int _rxRtpPacketSize = 0;// 108 for 1 ch 204 for 2 ch
    int _txRtpPacketSize = 0;// 108 for 1 ch 396 for 4ch  
    std::queue <int> _OutputQueue;
    uint16_t _outputBuffer[48][198];
    int _numOutputBuffers = 48;
    static const int _numSamples = 48; //was 44 
    uint8_t _packetStreamBuffer[12 + maxChannels *_numSamples * 2];
    int _outputBufferReaderIndex = 0;
    int _outputBufferWriterIndex = 0;
    int _bufferIndex = 0;
    int bufferIndex;
    uint32_t _outputTimestamp = 0;
    unsigned long _outputLastTimestamp = 0;
    unsigned long _ssrc = 1649937450;
    unsigned short _rtpPayloadType = 0x8062;//was 32864
    unsigned short _sequenceNo = 1;//was 0
    unsigned short previoussequenceNo = 0; 
    uint8_t writeBuffer[12 + maxChannels *_numSamples*2];
    bool syncedOneShot = 0;
};

#endif
