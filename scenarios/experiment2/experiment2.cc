/*
This experiment will evaluate the coverage of a gateway in a forest scenario.
For each distance evaluate throughput, delay, PDR
*/

#include "ns3/end-device-lora-phy.h"
#include "ns3/gateway-lora-phy.h"
#include "ns3/end-device-lora-mac.h"
#include "ns3/gateway-lora-mac.h"
#include "ns3/simulator.h"
#include "ns3/log.h"
#include "ns3/node-list.h"
#include "ns3/core-module.h"
#include "ns3/constant-position-mobility-model.h"
#include "ns3/constant-velocity-mobility-model.h"
#include "ns3/lora-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/node-container.h"
#include "ns3/position-allocator.h"
#include "ns3/one-shot-sender-helper.h"
#include "ns3/command-line.h"
#include "ns3/periodic-sender-helper.h"
#include "ns3/alfa-friis-loss-model.h"
#include "ns3/propagation-loss-model.h"
#include <algorithm>
#include <ctime>
#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Experiment2");

double l_frequency = 867e+06;
double interval;
double nNodes = 1000;
int radius = 2600; // Max radius of a SF7
int sf = 5; // data rate goes from 0 to 5
double nRcv = 0;
double nTx = 0;
std::string filename;


/**************************************
* GLOBAL CALLBACKS                    *
**************************************/

enum PacketOutcome {
  RECEIVED,
  INTERFERED,
  NO_MORE_RECEIVERS,
  UNDER_SENSITIVITY,
  UNSET
};

struct PacketStatus {
  Ptr<Packet const> packet;
  uint32_t senderId;
  int outcomeNumber;
  std::vector<enum PacketOutcome> outcomes;
  int64_t txTime;
  Vector senderPos;
};

std::map<Ptr<Packet const>, PacketStatus> packetTracker;

void
TransmissionCallback (Ptr<Packet const> packet, uint32_t systemId)
{
  nTx += 1;
}

void
PacketReceptionCallback (Ptr<Packet const> packet, uint32_t systemId)
{
  nRcv += 1;
} 


int
main (int argc, char* argv[]){


	  CommandLine cmd;
    cmd.AddValue ("DataRate", "Rate of Lora devices (0 to 5)", sf);
    cmd.AddValue ("nNodes", "Number of LoRa nodes", nNodes);
    cmd.AddValue ("Interval", "Space between packets", interval );

    cmd.Parse (argc, argv);

	/*****************************
	* LOGGING                    *
	*****************************/
	// LogComponentEnable ("Experiment1", LOG_LEVEL_INFO);
 //    LogComponentEnable ("LoraChannel", LOG_LEVEL_DEBUG);
       //LogComponentEnable ("EndDeviceLoraPhy", LOG_LEVEL_INFO);
       //LogComponentEnable ("GatewayLoraPhy", LOG_LEVEL_INFO);
 //    LogComponentEnable ("GatewayLoraMac", LOG_LEVEL_INFO);
 //    LogComponentEnable ("WeissbergerPropagationLossModel", LOG_LEVEL_DEBUG);
 //    LogComponentEnable ("PropagationLossModel", LOG_LEVEL_DEBUG);

	  LogComponentEnableAll (LOG_PREFIX_FUNC);
  	LogComponentEnableAll (LOG_PREFIX_NODE);
  	LogComponentEnableAll (LOG_PREFIX_TIME);

  	// filename = "exp2.rate" + std::to_string(sf) + ".out.txt"; 
  	// std::ofstream out(filename);
   //  std::streambuf *coutbuf = std::cout.rdbuf();
   //  std::cout.rdbuf(out.rdbuf());

   //  std::cout << "Node  |   time   |   delay   |    throughput   |     x_coordinate\n" << std::endl;
   //  std::cout << std::to_string(sf) << std::endl;

  	/************************
    *  Create the channel  *
    ************************/

    NS_LOG_INFO ("Creating the channel...");

    // Create the lora channel object. The Lora channel has an alfa Friis propagation and a constant delay (default to speed of light)

    Ptr<AlfaFriisPropagationLossModel> medLoss = CreateObject<AlfaFriisPropagationLossModel> ();

    medLoss->SetFrequency(l_frequency);
    medLoss->SetAlfa(3.29);

    Ptr<PropagationDelayModel> delay = CreateObject<ConstantSpeedPropagationDelayModel> ();

    Ptr<LoraChannel> channel = CreateObject<LoraChannel> (medLoss, delay);

    /************************
    *  Create the helpers  *
    ************************/

    NS_LOG_INFO ("Setting up helpers...");

    MobilityHelper mobility;

    // Create the LoraPhyHelper
    LoraPhyHelper phyHelper = LoraPhyHelper ();
    phyHelper.SetChannel (channel);

    // Create the LoraMacHelper
    LoraMacHelper macHelper = LoraMacHelper ();

    // Create the LoraHelper
    LoraHelper helper = LoraHelper ();

    /************************
    *  Create End Devices  *
    ************************/

    NS_LOG_INFO ("Creating end devices...");

    // Create a set of nodes
    NodeContainer endDevices;
    endDevices.Create (nNodes);

    // Assign a mobility model to the node
    Ptr<UniformDiscPositionAllocator> nodesAllocator = CreateObject<UniformDiscPositionAllocator> ();
    nodesAllocator->SetRho(radius);

    mobility.SetPositionAllocator (nodesAllocator);
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (endDevices);

    // Create the LoraNetDevices of the end devices
    phyHelper.SetDeviceType (LoraPhyHelper::ED);
    macHelper.SetDeviceType (LoraMacHelper::ED);
    helper.Install (phyHelper, macHelper, endDevices);

    for (NodeContainer::Iterator j = endDevices.Begin (); j != endDevices.End (); ++j)
    {
      Ptr<Node> node = *j;

      Ptr<LoraNetDevice> loraNetDevice = node->GetDevice (0)->GetObject<LoraNetDevice> ();
      Ptr<EndDeviceLoraMac> mac = loraNetDevice->GetMac ()->GetObject<EndDeviceLoraMac> ();
      mac -> SetDataRate(sf);
    }

    /*********************
    *  Create Gateways  *
    *********************/

    NS_LOG_INFO ("Creating the gateway...");
    NodeContainer gateways;
    gateways.Create (1);
  
    Ptr<ListPositionAllocator> gwAllocator = CreateObject<ListPositionAllocator> ();
    gwAllocator->Add(Vector(0,0,60));
  
    mobility.SetPositionAllocator (gwAllocator);
    mobility.Install (gateways);

    // Create a netdevice for each gateway
    phyHelper.SetDeviceType (LoraPhyHelper::GW);
    macHelper.SetDeviceType (LoraMacHelper::GW);
    helper.Install (phyHelper, macHelper, gateways);

    // Set the data rates for different spreading factors
    //macHelper.SetSpreadingFactorsUp (endDevices, gateways, channel);

    /*******************************
    * Setup Trace sinks            *
    *******************************/

    
    for (NodeContainer::Iterator j = endDevices.Begin (); j != endDevices.End (); ++j)
    {
      Ptr<Node> node = *j;
      Ptr<LoraNetDevice> loraNetDevice = node->GetDevice (0)->GetObject<LoraNetDevice> ();
      Ptr<LoraPhy> phy = loraNetDevice->GetPhy ();
      phy->TraceConnectWithoutContext ("StartSending",
                                       MakeCallback (&TransmissionCallback));
    }

    for (NodeContainer::Iterator j = gateways.Begin (); j != gateways.End (); j++)
    {

      Ptr<Node> object = *j;

      Ptr<NetDevice> netDevice = object->GetDevice (0);
      Ptr<LoraNetDevice> loraNetDevice = netDevice->GetObject<LoraNetDevice> ();
      NS_ASSERT (loraNetDevice != 0);
      Ptr<GatewayLoraPhy> gwPhy = loraNetDevice->GetPhy ()->GetObject<GatewayLoraPhy> ();

      // Global callbacks (every gateway)
      gwPhy->TraceConnectWithoutContext ("ReceivedPacket",
                                         MakeCallback (&PacketReceptionCallback));
    }

    

    /********************
    * Install Application*
    ********************/

    PeriodicSenderHelper appHelper = PeriodicSenderHelper ();
    appHelper.SetPeriod (Seconds (interval));
    ApplicationContainer appContainer = appHelper.Install (endDevices);

    appContainer.Start (Seconds (2));
    appContainer.Stop (Minutes (5)); 

    Simulator::Stop (Hours (2));

    Simulator::Run ();

    Simulator::Destroy ();

    //std::cout.rdbuf(coutbuf);
    std::cout << "SF\tnNodes\tInterval\tPDR" << std::endl;
    std::cout << sf << "\t" << nNodes << "\t" << interval << "\t" << nRcv/nTx << std::endl;

    return 0;
}

