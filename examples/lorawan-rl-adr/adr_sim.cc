#include "ns3/basic-energy-source-helper.h"
#include "ns3/lora-radio-energy-model-helper.h"

#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/core-module.h"
#include "ns3/forwarder-helper.h"
#include "ns3/gateway-lora-phy.h"
#include "ns3/hex-grid-position-allocator.h"
#include "ns3/log.h"
#include "ns3/lora-channel.h"
#include "ns3/lora-device-address-generator.h"
#include "ns3/lora-helper.h"
#include "ns3/lora-phy-helper.h"
#include "ns3/lorawan-mac-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/network-module.h"
#include "ns3/network-server-helper.h"
#include "ns3/periodic-sender-helper.h"
#include "ns3/periodic-sender.h"
#include "ns3/point-to-point-module.h"
#include "ns3/random-variable-stream.h"
#include "ns3/rectangle.h"
#include "ns3/string.h"

#include "ns3/building-allocator.h"
#include "ns3/building-penetration-loss.h"
#include "ns3/buildings-helper.h"
#include "ns3/correlated-shadowing-propagation-loss-model.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <ctime>
#include <sstream>

#include "ns3/li-ion-energy-source-helper.h"

using namespace ns3;
using namespace lorawan;

NS_LOG_COMPONENT_DEFINE("RLAdrTest");

/**
 * Record a change in the data rate setting on an end device.
 *
 * \param oldDr The previous data rate value.
 * \param newDr The updated data rate value.
 */
void
OnDataRateChange(uint8_t oldDr, uint8_t newDr)
{
    NS_LOG_DEBUG("DR" << unsigned(oldDr) << " -> DR" << unsigned(newDr));
}

/**
 * Record a change in the transmission power setting on an end device.
 *
 * \param oldTxPower The previous transmission power value.
 * \param newTxPower The updated transmission power value.
 */
void
OnTxPowerChange(double oldTxPower, double newTxPower)
{
    NS_LOG_DEBUG(oldTxPower << " dBm -> " << newTxPower << " dBm");
}

/*
* Returns the timestamp in YYYY-MM-DD_HH-MM-SS format
*/
std::string
GetTimestamp() {
    // Get the current time
    std::time_t now = std::time(nullptr);
    std::tm* localTime = std::localtime(&now);

    // Format the time as YYYY-MM-DD_HH-MM-SS
    std::ostringstream timestamp;
    timestamp << (1900 + localTime->tm_year) << "-"
            << (1 + localTime->tm_mon) << "-"
            << localTime->tm_mday << "_"
            << localTime->tm_hour << "-"
            << localTime->tm_min << "-"
            << localTime->tm_sec;
    return timestamp.str();
}


// Global file stream for output
std::ofstream energyFile;

// Function to log energy consumption
void LogEnergyConsumption(Ptr<EnergySourceContainer> energySources, Time samplePeriod) {
    Time currentTime = Simulator::Now();  // Gets the current simulation time for the timestamping of the data
    energyFile << currentTime.GetSeconds() << " ";  // 1st column : Simulation timestamp
    for (uint32_t i = 0; i < energySources->GetN(); ++i) {
        Ptr<EnergySource> source = energySources->Get(i);
        energyFile << source->GetRemainingEnergy() << " ";
    }
    energyFile << std::endl;

    // Schedule the next logging
    Simulator::Schedule(samplePeriod, &LogEnergyConsumption, energySources, samplePeriod);
}


int
main(int argc, char* argv[])
{
    bool verbose = false;
    bool adrEnabled = true;
    bool initializeSF = false;
    int nDevices = 1;
    int nPeriodsOf20Minutes = 2;
    double mobileNodeProbability = 0;
    double sideLengthMeters = 1000;
    int gatewayDistanceMeters = 5000;
    double maxRandomLossDB = 10;
    double minSpeedMetersPerSecond = 2;
    double maxSpeedMetersPerSecond = 16;
    std::string adrType = "ns3::AdrRL";
    bool realisticChannelModel = true;
    bool printBuildingInfo = true;

    //OUtput folder
    std::string outPath = "local_experiments/";
    std::string subFolder = "/";

    CommandLine cmd(__FILE__);
    cmd.AddValue("verbose", "Whether to print output or not", verbose);
    cmd.AddValue("MultipleGwCombiningMethod", "ns3::AdrComponent::MultipleGwCombiningMethod");
    cmd.AddValue("MultiplePacketsCombiningMethod",
                "ns3::AdrComponent::MultiplePacketsCombiningMethod");  // Whether to average SNRs from multiple packets or to use the maximum: "avg", "max", "min"
    cmd.AddValue("HistoryRange", "ns3::AdrComponent::HistoryRange");   // Number of packets to use for averaging
    cmd.AddValue("MType", "ns3::EndDeviceLorawanMac::MType");
    cmd.AddValue("EDDRAdaptation", "ns3::EndDeviceLorawanMac::EnableEDDataRateAdaptation");
    cmd.AddValue("ChangeTransmissionPower", "ns3::AdrComponent::ChangeTransmissionPower");
    cmd.AddValue("AdrEnabled", "Whether to enable Adaptive Data Rate (ADR)", adrEnabled);
    cmd.AddValue("nDevices", "Number of devices to simulate", nDevices);
    cmd.AddValue("PeriodsToSimulate", "Number of periods (20m) to simulate", nPeriodsOf20Minutes);
    cmd.AddValue("MobileNodeProbability",
                "Probability of a node being a mobile node",
                mobileNodeProbability);
    cmd.AddValue("sideLength",
                "Length (m) of the side of the rectangle nodes will be placed in",
                sideLengthMeters);
    cmd.AddValue("maxRandomLoss",
                "Maximum amount (dB) of the random loss component",
                maxRandomLossDB);
    cmd.AddValue("gatewayDistance", "Distance (m) between gateways", gatewayDistanceMeters);
    cmd.AddValue("initializeSF", "Whether to initialize the SFs", initializeSF);
    cmd.AddValue("MinSpeed", "Minimum speed (m/s) for mobile devices", minSpeedMetersPerSecond);
    cmd.AddValue("MaxSpeed", "Maximum speed (m/s) for mobile devices", maxSpeedMetersPerSecond);
    cmd.AddValue("MaxTransmissions", "ns3::EndDeviceLorawanMac::MaxTransmissions");
    cmd.AddValue("outPath", "Output folder", outPath);
    cmd.AddValue("subFolder", "Subfolder of the Output folder (add a / after)", subFolder);
    cmd.AddValue("RealChannel", "Realistic channel with buildings and shadowing effects", realisticChannelModel);
    cmd.Parse(argc, argv);

    // Check/Creat result output path
    // Combine the paths
    std::string timeStamp = GetTimestamp();
    std::string fullPath = outPath + timeStamp + subFolder;

    // Check if the subfolder exists, and create it if it doesn't
    if (!std::filesystem::exists(fullPath)) {
        try {
            std::filesystem::create_directories(fullPath); // Create directories (including intermediate ones)
            std::cout << "Subfolder created at: " << fullPath << std::endl;
        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "Error creating subfolder: " << e.what() << std::endl;
        }
    } else {
        std::cout << "Subfolder already exists at: " << fullPath << std::endl;
    }


    int gatewayRings = 2 + (std::sqrt(2) * sideLengthMeters) / (gatewayDistanceMeters);
    int nGateways = 3 * gatewayRings * gatewayRings - 3 * gatewayRings + 1;

    /*************************************************************************************************************
 ******************************************* Logging **********************************************************
    ****************************************************************************************************//////////

    LogComponentEnable("RLAdrTest", LOG_LEVEL_ALL);
    // LogComponentEnable ("LoraPacketTracker", LOG_LEVEL_ALL);
    // LogComponentEnable ("NetworkServer", LOG_LEVEL_ALL);
    // LogComponentEnable ("NetworkController", LOG_LEVEL_ALL);
    // LogComponentEnable ("NetworkScheduler", LOG_LEVEL_ALL);
    // LogComponentEnable ("NetworkStatus", LOG_LEVEL_ALL);
    // LogComponentEnable ("EndDeviceStatus", LOG_LEVEL_ALL);
    // LogComponentEnable("AdrComponent", LOG_LEVEL_ALL);
    // LogComponentEnable("ClassAEndDeviceLorawanMac", LOG_LEVEL_ALL);
    // LogComponentEnable ("LogicalLoraChannelHelper", LOG_LEVEL_ALL);
    // LogComponentEnable ("MacCommand", LOG_LEVEL_ALL);
    // LogComponentEnable ("AdrExploraSf", LOG_LEVEL_ALL);
    // LogComponentEnable ("AdrExploraAt", LOG_LEVEL_ALL);
    // LogComponentEnable ("EndDeviceLorawanMac", LOG_LEVEL_ALL);
    LogComponentEnable ("LorawanRlAdr", LOG_LEVEL_ALL);
    LogComponentEnableAll(LOG_PREFIX_FUNC);
    LogComponentEnableAll(LOG_PREFIX_NODE);
    LogComponentEnableAll(LOG_PREFIX_TIME);

    // Set the end devices to allow data rate control (i.e. adaptive data rate) from the network
    // server
    Config::SetDefault("ns3::EndDeviceLorawanMac::DRControl", BooleanValue(true));

    /**********************++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +++++++++++++++++++++++++++*  Creating EDs and GWs *+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    **********************++++++++++++++++++++++++++++++++++++++++++++++++++++***********************************/
    // Helpers
    //////////

    // End device mobility
    MobilityHelper mobilityEd;
    MobilityHelper mobilityGw;
    mobilityEd.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                    "X",
                                    PointerValue(CreateObjectWithAttributes<UniformRandomVariable>(
                                        "Min",
                                        DoubleValue(-sideLengthMeters * 2),
                                        "Max",
                                        DoubleValue(sideLengthMeters * 2))),
                                    "Y",
                                    PointerValue(CreateObjectWithAttributes<UniformRandomVariable>(
                                        "Min",
                                        DoubleValue(-sideLengthMeters * 2),
                                        "Max",
                                        DoubleValue(sideLengthMeters * 2))));

    // // Gateway mobility
    // Ptr<ListPositionAllocator> positionAllocGw = CreateObject<ListPositionAllocator> ();
    // positionAllocGw->Add (Vector (0.0, 0.0, 15.0));
    // positionAllocGw->Add (Vector (-5000.0, -5000.0, 15.0));
    // positionAllocGw->Add (Vector (-5000.0, 5000.0, 15.0));
    // positionAllocGw->Add (Vector (5000.0, -5000.0, 15.0));
    // positionAllocGw->Add (Vector (5000.0, 5000.0, 15.0));
    // mobilityGw.SetPositionAllocator (positionAllocGw);
    // mobilityGw.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    Ptr<HexGridPositionAllocator> hexAllocator =
        CreateObject<HexGridPositionAllocator>(gatewayDistanceMeters / 2);
    mobilityGw.SetPositionAllocator(hexAllocator);
    mobilityGw.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    ////////////////
    // Create gateways //
    ////////////////

    NodeContainer gateways;
    gateways.Create(nGateways);
    mobilityGw.Install(gateways);

    // Create end devices
    /////////////

    NodeContainer endDevices;
    endDevices.Create(nDevices);

    // Install mobility model on fixed nodes
    mobilityEd.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    int fixedPositionNodes = double(nDevices) * (1 - mobileNodeProbability);
    for (int i = 0; i < fixedPositionNodes; ++i)
    {
        mobilityEd.Install(endDevices.Get(i));
    }
    // Install mobility model on mobile nodes
    mobilityEd.SetMobilityModel(
        "ns3::RandomWalk2dMobilityModel",
        "Bounds",
        RectangleValue(
            Rectangle(-sideLengthMeters, sideLengthMeters, -sideLengthMeters, sideLengthMeters)),
        "Distance",
        DoubleValue(1000),
        "Speed",
        PointerValue(CreateObjectWithAttributes<UniformRandomVariable>(
            "Min",
            DoubleValue(minSpeedMetersPerSecond),
            "Max",
            DoubleValue(maxSpeedMetersPerSecond))));
    for (int i = fixedPositionNodes; i < (int)endDevices.GetN(); ++i)
    {
        mobilityEd.Install(endDevices.Get(i));
    }

    // Set the height of the nodes are at a certain height > 0
    for (auto j = endDevices.Begin(); j != endDevices.End(); ++j)
    {
        Ptr<MobilityModel> mobility = (*j)->GetObject<MobilityModel>();
        Vector position = mobility->GetPosition();
        position.z = 1.2;
        mobility->SetPosition(position);
    }

    // Set the height of the gateways are at a certain height > 0
    for (auto g = gateways.Begin(); g != gateways.End(); ++g)
    {
        Ptr<MobilityModel> mobility = (*g)->GetObject<MobilityModel>();
        Vector position = mobility->GetPosition();
        position.z = 15.0;
        mobility->SetPosition(position);
    }


    /**********************++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
     +++++++++++++++++++++++++++*  Handle buildings  *+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    **********************++++++++++++++++++++++++++++++++++++++++++++++++++++***********************************/

    double xLength = 130;  //the length of the wall of each building
    double deltaX = 32;     //Space between buildings
    double yLength = 64;
    double deltaY = 17;
    int gridWidth = 2 * sideLengthMeters / (xLength + deltaX);  // Number of objects laid out in a line on the X axis
    int gridHeight = 2 * sideLengthMeters / (yLength + deltaY);
    if (!realisticChannelModel)
    {
        gridWidth = 0;
        gridHeight = 0;
    }
    // Creating a random variable for generating random heights
    Ptr<UniformRandomVariable> heightRandomVar = CreateObject<UniformRandomVariable>();
    heightRandomVar->SetAttribute("Min", DoubleValue(3.0)); // Minimum height (in meters)
    heightRandomVar->SetAttribute("Max", DoubleValue(10.0)); // Maximum height (in meters)

    Ptr<GridBuildingAllocator> gridBuildingAllocator;
    gridBuildingAllocator = CreateObject<GridBuildingAllocator>();
    gridBuildingAllocator->SetAttribute("GridWidth", UintegerValue(gridWidth));
    gridBuildingAllocator->SetAttribute("LengthX", DoubleValue(xLength));
    gridBuildingAllocator->SetAttribute("LengthY", DoubleValue(yLength));
    gridBuildingAllocator->SetAttribute("DeltaX", DoubleValue(deltaX));
    gridBuildingAllocator->SetAttribute("DeltaY", DoubleValue(deltaY));
    gridBuildingAllocator->SetAttribute("Height", DoubleValue(6));
    // // Generate buildings with random heights
    // for (uint32_t i = 0; i < numberOfBuildings; ++i) {
    //     double randomHeight = heightRandomVar->GetValue(); // Get random height
    //     gridBuildingAllocator->SetAttribute("Height", DoubleValue(randomHeight)); // Set height
    //     // Allocate a building with this height
    //     gridBuildingAllocator->Create(1);
    // }
    gridBuildingAllocator->SetBuildingAttribute("NRoomsX", UintegerValue(2));
    gridBuildingAllocator->SetBuildingAttribute("NRoomsY", UintegerValue(4));
    gridBuildingAllocator->SetBuildingAttribute("NFloors", UintegerValue(2));
    gridBuildingAllocator->SetAttribute(
        "MinX",
        DoubleValue(-gridWidth * (xLength + deltaX) / 2 + deltaX / 2));
    gridBuildingAllocator->SetAttribute(
        "MinY",
        DoubleValue(-gridHeight * (yLength + deltaY) / 2 + deltaY / 2));
    BuildingContainer bContainer = gridBuildingAllocator->Create(gridWidth * gridHeight);

    BuildingsHelper::Install(endDevices);
    BuildingsHelper::Install(gateways);

    // Print the buildings
    if (printBuildingInfo)
    {
        std::ofstream myfile;
        myfile.open(fullPath+"buildings.txt");
        std::vector<Ptr<Building>>::const_iterator it;
        int j = 1;
        for (it = bContainer.Begin(); it != bContainer.End(); ++it, ++j)
        {
            Box boundaries = (*it)->GetBoundaries();
            myfile << "set object " << j << " rect from " << boundaries.xMin << ","
                << boundaries.yMin << " to " << boundaries.xMax << "," << boundaries.yMax << ","
                << boundaries.zMax << std::endl;
        }
        myfile.close();
    }

    /*************************************************************************************************************
     ************************************* Create the wireless channel *******************************+++++++++++++
    *************************************************************************************************************/

    Ptr<LogDistancePropagationLossModel> loss = CreateObject<LogDistancePropagationLossModel>();
    loss->SetPathLossExponent(3.76);
    loss->SetReference(1, 7.7);

    if (realisticChannelModel)
    {
        // Create the correlated shadowing component
        Ptr<CorrelatedShadowingPropagationLossModel> shadowing =
            CreateObject<CorrelatedShadowingPropagationLossModel>();

        // Aggregate shadowing to the logdistance loss
        loss->SetNext(shadowing);

        // Add the effect to the channel propagation loss
        Ptr<BuildingPenetrationLoss> buildingLoss = CreateObject<BuildingPenetrationLoss>();

        shadowing->SetNext(buildingLoss);
    }

    Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable>();
    x->SetAttribute("Min", DoubleValue(0.0));
    x->SetAttribute("Max", DoubleValue(maxRandomLossDB));

    Ptr<RandomPropagationLossModel> randomLoss = CreateObject<RandomPropagationLossModel>();
    randomLoss->SetAttribute("Variable", PointerValue(x));

    loss->SetNext(randomLoss);

    Ptr<PropagationDelayModel> delay = CreateObject<ConstantSpeedPropagationDelayModel>();

    Ptr<LoraChannel> channel = CreateObject<LoraChannel>(loss, delay);

    /*********************************************************************************************************
 ***************************************** Continue configuring EDs and GWs *******************************
    *********************************************************************************************************/
    // Create the LoraPhyHelper
    LoraPhyHelper phyHelper = LoraPhyHelper();
    phyHelper.SetChannel(channel);

    // Create the LorawanMacHelper
    LorawanMacHelper macHelper = LorawanMacHelper();

    // Create the LoraHelper
    LoraHelper helper = LoraHelper();
    helper.EnablePacketTracking();

    // Create the LoraNetDevices of the gateways
    phyHelper.SetDeviceType(LoraPhyHelper::GW);
    macHelper.SetDeviceType(LorawanMacHelper::GW);
    helper.Install(phyHelper, macHelper, gateways);


    // Create a LoraDeviceAddressGenerator
    uint8_t nwkId = 54;
    uint32_t nwkAddr = 1864;
    Ptr<LoraDeviceAddressGenerator> addrGen =
        CreateObject<LoraDeviceAddressGenerator>(nwkId, nwkAddr);

    // Create the LoraNetDevices of the end devices
    phyHelper.SetDeviceType(LoraPhyHelper::ED);
    macHelper.SetDeviceType(LorawanMacHelper::ED_A);
    macHelper.SetAddressGenerator(addrGen);
    macHelper.SetRegion(LorawanMacHelper::EU);
    NetDeviceContainer endDevicesNetDevices = helper.Install(phyHelper, macHelper, endDevices);

    // Install applications in end devices
    int appPeriodSeconds = 1200; // One packet every 20 minutes
    PeriodicSenderHelper appHelper = PeriodicSenderHelper();
    appHelper.SetPeriod(Seconds(appPeriodSeconds));
    ApplicationContainer appContainer = appHelper.Install(endDevices);

    // Do not set spreading factors up: we will wait for the network server to do this
    if (initializeSF)
    {
        LorawanMacHelper::SetSpreadingFactorsUp(endDevices, gateways, channel);
    }

    /******************************************************************************************************
 ************************************* Create network server *******************************************
    ******************************************************************************************************/
    Ptr<Node> networkServer = CreateObject<Node>();

    // PointToPoint links between gateways and server
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));
    // Store network server app registration details for later
    P2PGwRegistration_t gwRegistration;
    for (auto gw = gateways.Begin(); gw != gateways.End(); ++gw)
    {
        auto container = p2p.Install(networkServer, *gw);
        auto serverP2PNetDev = DynamicCast<PointToPointNetDevice>(container.Get(0));
        gwRegistration.emplace_back(serverP2PNetDev, *gw);
    }

    // Install the NetworkServer application on the network server
    NetworkServerHelper networkServerHelper;
    networkServerHelper.EnableAdr(adrEnabled);
    networkServerHelper.SetAdr(adrType);
    networkServerHelper.SetGatewaysP2P(gwRegistration);
    networkServerHelper.SetEndDevices(endDevices);
    networkServerHelper.Install(networkServer);

    // Install the Forwarder application on the gateways
    ForwarderHelper forwarderHelper;
    forwarderHelper.Install(gateways);

    /**********************************************************************************************************
     *************************************** Install Energy Model *********************************************
    **********************************************************************************************************/
    BasicEnergySourceHelper basicSourceHelper; // we can also use LiIonEnergySourceHelper or BasicEnergyHarvester
    LoraRadioEnergyModelHelper radioEnergyHelper;

    // configure energy source
    basicSourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(10000)); // Energy in J
    basicSourceHelper.Set("BasicEnergySupplyVoltageV", DoubleValue(3.3));

    // add a logic so that the current or voltage or energy consumption changes according to the tx power or duration
    radioEnergyHelper.Set("StandbyCurrentA", DoubleValue(0.0014));
    radioEnergyHelper.Set("TxCurrentA", DoubleValue(0.028));
    radioEnergyHelper.Set("SleepCurrentA", DoubleValue(0.0000015));
    radioEnergyHelper.Set("RxCurrentA", DoubleValue(0.0112));

    radioEnergyHelper.SetTxCurrentModel("ns3::ConstantLoraTxCurrentModel",
                                        "TxCurrent",
                                        DoubleValue(0.028));

    // install source on end devices' nodes
    EnergySourceContainer ergSources = basicSourceHelper.Install(endDevices);
    Names::Add("/Names/EnergySource", ergSources.Get(0));

    // install device model
    DeviceEnergyModelContainer deviceModels =
        radioEnergyHelper.Install(endDevicesNetDevices, ergSources);

    // Get/save output: the remaining energy of the nodes


    //++++++++++++++++++++++++++++++++++++End of energy module configuration+++++++++++++++++++++++++++++++++++++//

    // Connect our traces
    Config::ConnectWithoutContext(
        "/NodeList/*/DeviceList/0/$ns3::LoraNetDevice/Mac/$ns3::EndDeviceLorawanMac/TxPower",
        MakeCallback(&OnTxPowerChange));
    Config::ConnectWithoutContext(
        "/NodeList/*/DeviceList/0/$ns3::LoraNetDevice/Mac/$ns3::EndDeviceLorawanMac/DataRate",
        MakeCallback(&OnDataRateChange));

    // Activate printing of end device MAC parameters
    Time stateSamplePeriod = Seconds(1200);
    helper.EnablePeriodicDeviceStatusPrinting(endDevices,
                                            gateways,
                                            fullPath+"nodeData.txt",
                                            stateSamplePeriod);
    helper.EnablePeriodicPhyPerformancePrinting(gateways, fullPath+"phyPerformance.txt", stateSamplePeriod);
    helper.EnablePeriodicGlobalPerformancePrinting(fullPath+"globalPerformance.txt", stateSamplePeriod);

    LoraPacketTracker& tracker = helper.GetPacketTracker();

    /****************************************** Start simulation ***********************************************
 ************************************************************************************************************/
    // Open the file for writing energy_data
    energyFile.open(fullPath + "energy_data.txt");

    // Start logging energy consumption with the sampling period
    Simulator::Schedule(stateSamplePeriod, &LogEnergyConsumption, &ergSources, stateSamplePeriod);

    Time simulationTime = Seconds(1200 * nPeriodsOf20Minutes);
    Simulator::Stop(simulationTime);
    Simulator::Run();

    //************************************Run time data collection *******************************************
    // +++++++++++++++++++++ Code here ++++++++++++++
    //

    // Closing the file for energy_data
    energyFile.close();

    Simulator::Destroy();

    std::cout << tracker.CountMacPacketsGlobally(Seconds(1200 * (nPeriodsOf20Minutes - 2)),
                                                Seconds(1200 * (nPeriodsOf20Minutes - 1)))
            << std::endl;


    //++++++++++++++++++++++++++++++++++ Collecting the gateway positions +++++++++++++++++++++++++++++++++++++++++
    std::ofstream gwPosFile(fullPath + "gw_positions.txt");
    for (auto gw = gateways.Begin(); gw != gateways.End(); ++gw)
    {
        uint32_t gwId = (*gw)->GetId();
        NS_LOG_INFO("Saving gateway position gwPosFile.txt");
        Ptr<MobilityModel> mob = (*gw)->GetObject<MobilityModel>();
        Vector pos = mob->GetPosition();
        gwPosFile << gwId << " " << pos.x << " " << pos.y << std::endl;
    }


    return 0;
}
