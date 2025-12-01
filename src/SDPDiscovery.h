#ifndef teensy_aes67_SDPDiscovery_h_
#define teensy_aes67_SDPDiscovery_h_

#include <Audio.h>
#include <QNEthernet.h>
//#include <audioBoard.h>

class SDPDiscovery {
  public:
    void start(IPAddress Tx_IP, int Tx_Port, int txChannels);
    void update();
    void stop();
    SDPDiscovery(qindesign::network::EthernetUDP &Udp);
    String streamName = "Teensy_41";
  private:
    elapsedMillis _advertiseTimer;
    uint16_t _advertiseInterval = 30000; // 10s
    IPAddress _advertisementIP{239, 255, 255, 255};
    IPAddress localIP; // local IP of device
    IPAddress _Tx_IP; // Ip of Transmit stream 
    int _Tx_Port; // Port of Transmit stream
    uint16_t _advertisementPort = 9875;
    int _txChannels = 0;
    qindesign::network::EthernetUDP *_udp;
    void send();
};

#endif
