#pragma once

#include <stdint.h>
#include <string.h>

using NanoTime = int64_t;

constexpr NanoTime NS_PER_S = 1000*1000*1000;

class PTPBase
{
public:
    PTPBase(bool master_, bool slave_, bool p2p_);
    void begin();
    void update();
    void reset();
    void setKi(double val);
    void setKp(double val);
    NanoTime getOffset();
    NanoTime getDelay();
    void syncMessage();
    void announceMessage();
    void ppsInterruptTriggered(NanoTime pps_ts, NanoTime local_ts);
    int getLockCount();
    int announceInterval = 1000000; //  1 Sec.
    int syncInterval = 500000; //  .5 Sec.
    uint8_t clockID[8];
    int badSyncCount;
    int consecutiveBadCount;
    int sporadicSyncErrorThreshold = 5000; // should be aleast 3000

    uint8_t clockPriority 128;
    
    struct PTPAnnounceMessage {
        uint8_t priority1;
        uint8_t clockClass;
        uint8_t clockAccuracy;
        uint16_t offsetScaledLogVariance;
        uint8_t priority2;
        uint8_t clockIdentity[8];
        uint16_t stepsRemoved;
        uint8_t grandmasterIdentity[8];
    };

    PTPAnnounceMessage localAnnounce;
    PTPAnnounceMessage bestAnnounce;
    
    bool isGrandmaster = false;

    bool isBetterClock(const PTPAnnounceMessage& a, const PTPAnnounceMessage& b);
    void updateBMCA(const PTPAnnounceMessage& incoming);
    void parseAnnounceMessage(const uint8_t *buf);

protected:
    virtual void initSockets()=0;
    virtual void updateSockets()=0;
    virtual void sendPTPMessage(const uint8_t *buf, int size, bool generalMessage)=0;
    
    void parsePTPMessage(const uint8_t *buf, int size, const timespec &recv_ts);
    bool master;
    bool slave;
    bool p2p;
    
private:
	void setT1(NanoTime ts);
	void setT2(NanoTime ts);
	void setT3(NanoTime ts);
	void setT4(NanoTime ts);
    void parseSyncMessage(const uint8_t *buf, const timespec &recv_ts);
    void parseFollowUpMessage(const uint8_t *buf);
    void parseDelayResponseMessage(const uint8_t *buf, const timespec &recv_ts);
    void parseDelayResponseFollowUpMessage(const uint8_t *buf);
    void parseDelayRequestMessage(const uint8_t *buf, const timespec &recv_ts);

    void delayRequestMessage();
    void followUpMessage(const timespec &send_ts);
    void delayResponseMessage(const uint8_t *request_buf, uint16_t sequenceID, const timespec &request_recv_ts);
    void initPTPMessage(uint8_t *buf, const uint16_t messageLength, const uint8_t messageType, const uint16_t sequenceID, const uint8_t controlField);
    void updateController();
    void updateTimer();
    void updatePPS();

    
    bool initialised=false;
    uint16_t delayRequestSequenceID = 0;
	int lockcount=0;
    int driftErrorCount=0;
    elapsedMillis lockTimeout;
    uint16_t syncSequenceID=0;
    uint16_t syncServerSequenceID = 0;
    uint16_t announceServerSequenceID = 0;
    uint16_t followUpSequenceID=0;
    NanoTime t1=-1;
    NanoTime t1last = -1;
    NanoTime t2 = -1;
    NanoTime t2last = -1;
    NanoTime t2new = -1;
    NanoTime t3 = -1;
    NanoTime t4 = -1;
    NanoTime t5 = -1;
    NanoTime t6 = -1;

    NanoTime t1s = -1;
    NanoTime t4s = -1;

    bool t1updated=false;
    bool t2updated=false;
    bool t3updated=false;
    bool t4updated=false;
    bool t5updated=false;
    bool t6updated=false;
    bool t1lastvalid=false;
    bool t2lastvalid=false;
    bool ppsupdated=false;

    NanoTime currentOffset=0;
    NanoTime currentDelay=0;
    int nspsAccu=0;
    double driftNSPS=0;
    double KI=0.5;
    double KP=1.0;
    int updateCounter=0;

    double filteredOffset = 0;
    double filteredDriftNsps = 0;

    // Tuning constant
    const double FILTER_ALPHA = 0.1; // 0.1 = very slow response (smoother)
   
};
