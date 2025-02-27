/*
* Copyright (c) 2017 University of Padova
*
* SPDX-License-Identifier: GPL-2.0-only
*
* Author: Matteo Perin <matteo.perin.2@studenti.unipd.2>
*/

#ifndef LORAWAN_RL_ADR_H
#define LORAWAN_RL_ADR_H

#include "network-controller-components.h"
#include "network-status.h"

#include "ns3/ai-module.h"
#include "ns3/log.h"
#include "ns3/object.h"
#include "ns3/packet.h"

namespace ns3
{
namespace lorawan
{

struct AiAdrStatesStruct
{
    int8_t managerId; // Proabably the AI manager that is responsible for the param setting
    int8_t type;        // if we have different operations and for every op we need to share the stats with the python model and get actions we use this param to identify and relate the op to the stats and the action
    uint8_t spreadingFactor; // May also be available in the received packet info. So we might not need to pass it through here if the dtype of the lists are convertible via the python binder  
    uint8_t txPower;
    double snr;     // The SNR is caculated from the RSSI anyway. So we might not need to explicitly pass it to the AI script unless we find a dynamic way to calculate it inside NS3
    double rssi;    // The RSSI is already available in the received packet info. So we might not need to pass it to the AI script explicitly
    EndDeviceStatus::GatewayList gwList;                        // Conatins the list of time of reception paired with received power
    EndDeviceStatus::ReceivedPacketList packetList;             // We might not be able to pass this to the python directly. Might need some modification************. Contains gwList, sf, frequency, BW
    EndDeviceStatus::ReceivedPacketInfo lastReceivedPacket;     // Might need to extract relavant info to be used in python model
    double batteryLevel;                                        // From energy model or loraenergyhelper 

    // Initialize the struct
    AiAdrStatesStruct()
        : managerId(0), 
            spreadingFactor(0),
            txPower(0),
            snr(0),
            rssi(0),
            gwList(EndDeviceStatus::GatewayList()),
            packetList(EndDeviceStatus::ReceivedPacketList()),
            lastReceivedPacket(EndDeviceStatus::ReceivedPacketInfo())

    {
    }
};

struct AiAdrActionStruct
{
    int8_t managerId;
    uint8_t spreadingFactor;  
    uint8_t txPower;

    // Initialize the struct
    AiAdrActionStruct()
        : managerId(0),
            spreadingFactor(0),
            txPower(0)
    {
    }
};

/**
 * \ingroup lorawan
 *
 * LinkAdrRequest commands management
 */
class LorawanRlAdr : public NetworkControllerComponent
{
    /**
     * Available policies for combining radio metrics in packet history.
     */
    enum CombiningMethod
    {
        AVERAGE,
        MAXIMUM,
        MINIMUM,
    };



public:
    /**
     *  Register this type.
     *  \return The object TypeId.
     */
    static TypeId GetTypeId();

    LorawanRlAdr();           //!< Default constructor
    ~LorawanRlAdr() override; //!< Destructor

    void OnReceivedPacket(Ptr<const Packet> packet,
                        Ptr<EndDeviceStatus> status,
                        Ptr<NetworkStatus> networkStatus) override;

    void BeforeSendingReply(Ptr<EndDeviceStatus> status, Ptr<NetworkStatus> networkStatus) override;

    void OnFailedReply(Ptr<EndDeviceStatus> status, Ptr<NetworkStatus> networkStatus) override;

private:
    /**
     * Implementation of the default Adaptive Data Rate (ADR) procedure.
     *
     * ADR is meant to optimize radio modulation parameters of end devices to improve energy
     * consuption and radio resource utilization. For more details see
     * https://doi.org/10.1109/NOMS.2018.8406255 .
     *
     * \param newDataRate [out] new data rate value selected for the end device.
     * \param newTxPower [out] new tx power value selected for the end device.
     * \param status State representation of the current end device.
     */
    void AdrImplementation(uint8_t* newDataRate, uint8_t* newTxPower, Ptr<EndDeviceStatus> status);

    /**
     * Convert spreading factor values [7:12] to respective data rate values [0:5].
     *
     * \param sf The spreading factor value.
     * \return Value of the data rate as uint8_t.
     */
    uint8_t SfToDr(uint8_t sf);

    /**
     * Convert reception power values [dBm] to Signal to Noise Ratio (SNR) values [dB].
     *
     * The conversion comes from the formula \f$P_{rx}=-174+10\log_{10}(B)+SNR+NF\f$ where
     * \f$P_{rx}\f$ is the received transmission power, \f$B\f$ is the transmission bandwidth and
     * \f$NF\f$ is the noise figure of the receiver. The constant \f$-174\f$ is the thermal noise
     * [dBm] in 1 Hz of bandwidth and is influenced the temperature of the receiver, assumed
     * constant in this model. For more details see the SX1301 chip datasheet.
     *
     * \param transmissionPower Value of received transmission power.
     * \return SNR value as double.
     */
    double RxPowerToSNR(double transmissionPower) const;

    /**
     * Get the min RSSI (dBm) among gateways receiving the same transmission.
     *
     * \param gwList List of gateways paired with reception information.
     * \return Min RSSI of transmission as double.
     */
    double GetMinRxFromGateways(EndDeviceStatus::GatewayList gwList);
    /**
     * Get the max RSSI (dBm) among gateways receiving the same transmission.
     *
     * \param gwList List of gateways paired with packet reception information.
     * \return Max RSSI of transmission as double.
     */
    double GetMaxRxFromGateways(EndDeviceStatus::GatewayList gwList);
    /**
     * Get the average RSSI (dBm) of gateways receiving the same transmission.
     *
     * \param gwList List of gateways paired with packet reception information.
     * \return Average RSSI of transmission as double.
     */
    double GetAverageRxFromGateways(EndDeviceStatus::GatewayList gwList);
    /**
     * Get RSSI metric for a transmission according to chosen gateway aggregation policy.
     *
     * \param gwList List of gateways paired with packet reception information.
     * \return RSSI of tranmsmission as double.
     */
    double GetReceivedPower(EndDeviceStatus::GatewayList gwList);

    /**
     * Get the min Signal to Noise Ratio (SNR) of the receive packet history.
     *
     * \param packetList History of received packets with reception information.
     * \param historyRange Number of packets to consider going back in time.
     * \return Min SNR among packets as double.
     */
    double GetMinSNR(EndDeviceStatus::ReceivedPacketList packetList, int historyRange);
    /**
     * Get the max Signal to Noise Ratio (SNR) of the receive packet history.
     *
     * \param packetList History of received packets with reception information.
     * \param historyRange Number of packets to consider going back in time.
     * \return Max SNR among packets as double.
     */
    double GetMaxSNR(EndDeviceStatus::ReceivedPacketList packetList, int historyRange);
    /**
     * Get the average Signal to Noise Ratio (SNR) of the received packet history.
     *
     * \param packetList History of received packets with reception information.
     * \param historyRange Number of packets to consider going back in time.
     * \return Average SNR of packets as double.
     */
    double GetAverageSNR(EndDeviceStatus::ReceivedPacketList packetList, int historyRange);

    /**
     * Get the LoRaWAN protocol TXPower configuration index from the Equivalent Isotropically
     * Radiated Power (EIRP) in dBm.
     *
     * \param txPower Transission EIRP configuration.
     * \return TXPower index as int.
     */
    int GetTxPowerIndex(int txPower);

    enum CombiningMethod tpAveraging;      //!< TX power from gateways policy
    int historyRange;                      //!< Number of previous packets to consider
    enum CombiningMethod historyAveraging; //!< Received SNR history policy

    const int min_spreadingFactor = 7;    //!< Spreading factor lower limit
    const int min_transmissionPower = 2;  //!< Minimum transmission power (dBm) (Europe)
    const int max_transmissionPower = 14; //!< Maximum transmission power (dBm) (Europe)
    // const int offset = 10;                //!< Device specific SNR margin (dB)
    const int B = 125000; //!< Bandwidth (Hz)
    const int NF = 6;     //!< Noise Figure (dB)
    double threshold[6] = {
        -20.0,
        -17.5,
        -15.0,
        -12.5,
        -10.0,
        -7.5}; //!< Vector containing the required SNR for the 6 allowed spreading factor
            //!< levels ranging from 7 to 12 (the SNR values are in dB).

    bool m_toggleTxPower; //!< Whether to control transmission power of end devices or not
};
} // namespace lorawan
} // namespace ns3

#endif
