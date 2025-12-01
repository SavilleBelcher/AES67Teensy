#include <QNEthernet.h>
#include <TeensyID.h>
#include "SDPDiscovery.h"

SDPDiscovery::SDPDiscovery(qindesign::network::EthernetUDP &udp) {
  _udp = &udp;
}

void SDPDiscovery::start(IPAddress Tx_IP, int Tx_Port,int txChannels) {
 localIP = qindesign::network::Ethernet.localIP();
 _Tx_IP = Tx_IP;
 _Tx_Port = Tx_Port;
 _txChannels = txChannels;

/*
 //Ravenna SDPDiscovery 
// Start mDNS with a unique hostname
if (!qindesign::network::MDNS.begin("AES67 Teensy")) {
  Serial.println("mDNS setup failed.");
}
else{
  Serial.println("mDNS responder started.");
}

// Advertise a multicast RTP stream (e.g., RAVENNA/AES67)
qindesign::network::MDNS.addService("_rtp._udp", _Tx_Port, "AES67 Multicast Stream");
*/
}

void SDPDiscovery::stop() {
   _udp->stop();
}

void SDPDiscovery::update() {
  if (_advertiseTimer > _advertiseInterval) {
    _advertiseTimer = 0;
    send();
    //qindesign::network::MDNS.announce();  // Maintain mDNS state
  }
}

void SDPDiscovery::send() {
  uint8_t bufferIndex = 0;
  char writeBuffer[1000];
  writeBuffer[bufferIndex++] = 32;
  writeBuffer[bufferIndex++] = 00;
  writeBuffer[bufferIndex++] = 23; 
  writeBuffer[bufferIndex++] = 23;  
  

// IPv4 address
  writeBuffer[bufferIndex++] = localIP[3];
  writeBuffer[bufferIndex++] = localIP[2];
  writeBuffer[bufferIndex++] = localIP[1];
  writeBuffer[bufferIndex++] = localIP[0];

// Payload Type: application/sdp
  writeBuffer[bufferIndex++] = 0x61;
  writeBuffer[bufferIndex++] = 0x70;
  writeBuffer[bufferIndex++] = 0x70;
  writeBuffer[bufferIndex++] = 0x6c;
  writeBuffer[bufferIndex++] = 0x69;
  writeBuffer[bufferIndex++] = 0x63;
  writeBuffer[bufferIndex++] = 0x61;
  writeBuffer[bufferIndex++] = 0x74;
  writeBuffer[bufferIndex++] = 0x69;
  writeBuffer[bufferIndex++] = 0x6f;
  writeBuffer[bufferIndex++] = 0x6e;
  writeBuffer[bufferIndex++] = 0x2f;
  writeBuffer[bufferIndex++] = 0x73;
  writeBuffer[bufferIndex++] = 0x64;
  writeBuffer[bufferIndex++] = 0x70;
  writeBuffer[bufferIndex++] = 0x00;
  
  sprintf(&writeBuffer[bufferIndex++], 
  "v=0\r\n"
  "o=- 15776096 0 IN IP4 %u.%u.%u.%u\r\n"
	"s=%s\r\n"
	"c=IN IP4 %u.%u.%u.%u/64\r\n"
	"t=0 0\r\n"
	"m=audio %i RTP/AVP 98\r\n"
	"i=%i channels\r\n"//: L, R, C, Mux
  "a=recvonly\r\n"
	"a=rtpmap:98 L16/48000/%i\r\n"
	"a=ptime:1\r\n"
	"a=ts-refclk:ptp=IEEE1588-2008:ff-ff-04-e9-e5-13-4b-c9:0\r\n"
	"a=mediaclk:direct=0\r\n"
  "a=ssrc:1649937450\r\n"
	,localIP[0],localIP[1],localIP[2],localIP[3],streamName.c_str(),_Tx_IP[0],_Tx_IP[1],_Tx_IP[2],_Tx_IP[3],_Tx_Port,_txChannels,_txChannels);
  
  int messagelen = strlen(&writeBuffer[24])+ 24;

  _udp->beginPacket(_advertisementIP, _advertisementPort);
  _udp->write(writeBuffer, messagelen);
  _udp->endPacket();
}