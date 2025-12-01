#include <QNEthernet.h>
#include "RTPAudio.h"


RTPAudio::RTPAudio(AudioPlayQueue* rxQueue, int rxChannels,  AudioRecordQueue* txQueue, int txChannels, qindesign::network::EthernetUDP &udp) {
  _rxChannels = rxChannels;
  _txChannels = txChannels;
  _rxQueue = rxQueue;
  _txQueue = txQueue;
  _udp = &udp;
  _startupTimestamp = millis();
  _rxRtpPacketSize = (12 + rxChannels *_numSamples * 2);
  _txRtpPacketSize = (12 + txChannels *_numSamples * 2);
 
}

void RTPAudio::start() {
  _udp->beginMulticast(Rx_multicastIP, _multicastPort, true);
   
}

void RTPAudio::stop() {
   _udp->stop();
   _outputTimestamp = 0;
   syncedOneShot = false;

}

void RTPAudio::readPackets() {
  int size = _udp->parsePacket();
  if (size < 1) {
    return;
  }
  _udp->read(_packetStreamBuffer, _rxRtpPacketSize);

  unsigned short sequenceNumber = (_packetStreamBuffer[2] << 8) | (_packetStreamBuffer[3] & 0xff);
  uint32_t incomingAudioPacketTimestamp = ((uint32_t) _packetStreamBuffer[4]) << 24;
    incomingAudioPacketTimestamp += ((uint32_t) _packetStreamBuffer[5]) << 16;
    incomingAudioPacketTimestamp += ((uint32_t) _packetStreamBuffer[6]) << 8;
    incomingAudioPacketTimestamp += _packetStreamBuffer[7];

  
  // Each frame = 4 bytes: 2 bytes ch1 + 2 bytes ch2
  int sampleIdx = 12;
  while (sampleIdx + (_rxChannels * 2) <= size) {
    for (int ch = 0; ch < _rxChannels; ++ch) {
      _incomingAudioBuffer[ch][_incomingAudioBufferIndex] =
          _packetStreamBuffer[sampleIdx + (ch * 2) + 1];
      _incomingAudioBuffer[ch][_incomingAudioBufferIndex + 1] =
          _packetStreamBuffer[sampleIdx + (ch * 2)];
    }

    _incomingAudioBufferIndex += 2;
    sampleIdx += (_rxChannels * 2);

    if (_incomingAudioBufferIndex >= 256) {
      _incomingAudioBufferIndex = 0;
      for (int ch = 0; ch < _rxChannels; ++ch) {
        int16_t* buff = _rxQueue[ch].getBuffer();
        memcpy(buff, _incomingAudioBuffer[ch], 256);
        _rxQueue[ch].playBuffer();
      }
    }
  }
 
}


void RTPAudio::sendRTPData() {
  if (_OutputQueue.size() <= 0) return;

   //Serial.print("Seq: ");
   //Serial.print(_sequenceNo);
   //Serial.print("    Act: ");
   //Serial.print((writeBuffer[2] << 8) | (writeBuffer[3] & 0xff));
   //Serial.print("    Index: ");
   //Serial.print(bufferIndex);
   //Serial.print("    Time: ");
   //Serial.println(_outputTimestamp - _outputLastTimestamp);

  _udp->beginPacket(Tx_multicastIP, _multicastPort);
  _udp->setPacketDiffServ(46);
  _udp->write(writeBuffer, _txRtpPacketSize);
  _udp->endPacket();
  _sequenceNo++;
 
}

void RTPAudio::readAudio() {

  int allAvailable = 0;
  for (int ch = 0; ch < _txChannels; ++ch) {
    if (_txQueue[ch].available() >= 2) {
      allAvailable++;
    }
  }
  
  if (allAvailable == _txChannels) {
    int16_t* buffers[_txChannels];  // hold pointers to all channels
    for (int ch = 0; ch < _txChannels; ++ch) {
      buffers[ch] = _txQueue[ch].readBuffer();  
    }

    for (int i = 0; i < AUDIO_BLOCK_SAMPLES; ++i) {
      for (int ch = 0; ch < _txChannels; ++ch) {
        _outputBuffer[_outputBufferWriterIndex][_bufferIndex + 6] = buffers[ch][i];
        _bufferIndex++;
      }
      I2SSampleCount++;

      if (_bufferIndex >= (_numSamples * _txChannels)) {
        _OutputQueue.push(_outputBufferWriterIndex);
        _bufferIndex = 0;
        _outputBufferWriterIndex++;
        if (_outputBufferWriterIndex >= _numOutputBuffers) {
          _outputBufferWriterIndex = 0;
        }
      }
    }

    for (int ch = 0; ch < _txChannels; ++ch) {
      _txQueue[ch].freeBuffer(); 
    }
    
  }

  
  //sync tx timestamp to ptp time after ptp sync
  if (!syncedOneShot && ptpSynced) {
    timespec ptpTime;
    qindesign::network::EthernetIEEE1588.readTimer(ptpTime);
    uint64_t now_ns = ((uint64_t)ptpTime.tv_sec * 1000000000ull) + ptpTime.tv_nsec;
    // Convert PTP time to 48KHz RTP timestamp base (common for AES67)
    _outputTimestamp = (uint32_t)((now_ns * _numSamples) / 1000000); // ns to 48kHz 
    syncedOneShot = true;
  }
  if (!ptpSynced){syncedOneShot = false;}
  
  if (_OutputQueue.size() > 0 && (_sequenceNo  != previoussequenceNo)) {
     
    bufferIndex = _OutputQueue.front();
    _OutputQueue.pop();
    previoussequenceNo = _sequenceNo;
    _outputTimestamp +=  _numSamples;

    _outputBuffer[bufferIndex][0] = _rtpPayloadType;
    _outputBuffer[bufferIndex][1] = _sequenceNo;
    _outputBuffer[bufferIndex][2] = (_outputTimestamp) >> 16;
    _outputBuffer[bufferIndex][3] = (_outputTimestamp);
    _outputBuffer[bufferIndex][4] = _ssrc >> 16;
    _outputBuffer[bufferIndex][5] = 0;
       
    // This uses big endian, which is wrong
    //memcpy(writeBuffer, _outputBuffer[bufferIndex], _rtpPacketSize);
    // So we swap the byte order manually
    int i = 0;
    for (i = 0; i < (_txRtpPacketSize/2); i++) {
      writeBuffer[(i * 2) + 1] = (_outputBuffer[bufferIndex][i] >> 0) & 0xFF;
      writeBuffer[(i * 2)] = (_outputBuffer[bufferIndex][i] >> 8) & 0xFF;
    }
  }

  
}
