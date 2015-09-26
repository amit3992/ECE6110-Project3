// CAD Project 3
// Amit Kulkarni, Rashmi Mehere, Urvi Sharma, Rasika Subramanian

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include <math.h>

#include "ns3/queue.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"
#include "ns3/applications-module.h"
#include "ns3/random-variable-stream.h"
#include "ns3/olsr-module.h"
#include "ns3/aodv-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/wifi-module.h"
#include "ns3/traced-value.h"
#include "ns3/trace-source-accessor.h"
   
using namespace ns3;
using namespace std;
   
NS_LOG_COMPONENT_DEFINE ("P3");

uint32_t bytesTransmitted=0;  // global variable for transmitted bytes
uint32_t bytesReceived=0;   // global variable for received bytes

void
Trace (Ptr<const Packet> oldValue)
{
	bytesTransmitted += 512;
}

int main (int argc, char *argv[])
{

  // Set defaults and varibles.
  RngSeedManager::SetSeed(1234); // random seed to generate random pairs.
  	  
  uint32_t nNodes = 20; // default
  double intensity = 0.1;	// Traffic intensity to determine data rate
  double maxRate = 11000000; // 11Mbps
  double powerTx = 1.0; // in mW
  double area = 1000; // Area of topology
  double nodeDensity = 0.00002; // default for 20 nodes and area of 1000 m
  double rate;
  long double efficiency;   // Efficiency of the network

  string Mode ("DsssRate1Mbps");
  string protocol="AODV";
  string str_area = "1000";
  string animFile = "P3.xml" ; // Name of file for animation output

  Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("2200")); // Fragmentation disabled for frames below 2200
  Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode",StringValue (Mode)); // data rate of non-unicast same as unicast

  CommandLine cmd;
  cmd.AddValue("nodeDensity", "Select node density", nodeDensity);
  cmd.AddValue("area", "Select area of topology", area);
  cmd.AddValue("powerTx", "Power in for the nodes (mW)", powerTx);
  cmd.AddValue("protocol", "Type of routing protocol to use(aodv or olsr)", protocol);	
  cmd.AddValue("intensity","Traffic intensity in the network",intensity);

  cmd.Parse (argc, argv);

  Ptr<UniformRandomVariable> rv = CreateObject<UniformRandomVariable> ();         // Random variable to determine random peers
  Ptr<UniformRandomVariable> rv_start = CreateObject<UniformRandomVariable>();    // Random variable to determine start times
  rv_start->SetAttribute ("Min", DoubleValue (0));
  rv_start->SetAttribute ("Max", DoubleValue (2.0));
  rv->SetAttribute ("Min", DoubleValue (0));
  rv->SetAttribute ("Max", DoubleValue (nNodes-1)); 

  nNodes =  nodeDensity * area * area; // calculate number of nodes based on nodeDensity.
  rate=(intensity* maxRate)/nNodes;  //Calculate data rate from given value of traffic intensity
  
  double start[nNodes]; // store start times of peers
  uint32_t peers[nNodes];  // Array to form pairs
  
  if (area == 2000)
    str_area = "2000";

  NodeContainer nodes;
  nodes.Create (nNodes);

  // Place nodes randomly in a square.
  MobilityHelper mobility;
  string areaRandom = "ns3::UniformRandomVariable[Min=0.0|Max="+ str_area + "]";
  mobility.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
  "X", StringValue (areaRandom),
  "Y", StringValue (areaRandom));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  // Set wireless properties.

  WifiHelper wireless;
  double dbmTx = 10 * log10 (powerTx); // Convert power from mW to dbM
  YansWifiPhyHelper wirelessPhy =  YansWifiPhyHelper::Default ();
  wirelessPhy.Set ("RxGain", DoubleValue (0) );  
  wirelessPhy.Set("TxPowerStart",DoubleValue(dbmTx));
  wirelessPhy.Set("TxPowerEnd", DoubleValue(dbmTx));

  YansWifiChannelHelper wirelessChannel;
  wirelessChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wirelessChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel"); 
  wirelessPhy.SetChannel (wirelessChannel.Create ());

  NqosWifiMacHelper wirelessMac = NqosWifiMacHelper::Default ();
  wireless.SetStandard (WIFI_PHY_STANDARD_80211b);
  wireless.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
  "DataMode",StringValue (Mode),
  "ControlMode",StringValue (Mode));
  
  // Set it to adhoc mode
  wirelessMac.SetType ("ns3::AdhocWifiMac");
  NetDeviceContainer wirelessDevices = wireless.Install (wirelessPhy, wirelessMac, nodes);

  // Select routing protocol. 
  Ipv4ListRoutingHelper list;
  InternetStackHelper stack; // Install internet stack
  
  if (protocol == "AODV") 
    {
       AodvHelper aodv;
       list.Add (aodv, 100);
       stack.SetRoutingHelper (list);
    }
    
  else if (protocol == "OLSR") 
    {
      OlsrHelper olsr;
      list.Add (olsr, 100);
      stack.SetRoutingHelper (list);
    } 
    else 
    {
      NS_ABORT_MSG ("Invalid protocol: Use --protocol=AODV or --protocol=OLSR");
    }    
    stack.Install(nodes);
  
  // Assigning IP addresses to all devices in the container wirelessDevices.
    NS_LOG_INFO ("Assign IP Addresses."); 

    Ipv4AddressHelper address;
    address.SetBase ("10.1.0.0", "255.255.0.0");
    Ipv4InterfaceContainer interfaces;
    interfaces = address.Assign (wirelessDevices);
 
  //Create pairs and install applications

    uint32_t port = 9;
    ApplicationContainer sourceApp[nNodes]; 
    ApplicationContainer sinkApp[nNodes];

    for (uint32_t i=0; i< nNodes; i++)
    {

      start[i] = rv_start->GetValue();
      OnOffHelper UDPHelper("ns3::UdpSocketFactory",Address());
      UDPHelper.SetConstantRate(DataRate(rate));
      peers[i]=rv->GetValue();

      if(peers[i]!=i)           
      {
          AddressValue Addr (InetSocketAddress(interfaces.GetAddress(peers[i]),port));  // install only if sources are not same.
          UDPHelper.SetAttribute("Remote",Addr);

      } 


      
      else
      {
          peers[i] = peers[0];      // change value if same.
      }  

      sourceApp[i] = UDPHelper.Install(nodes.Get(i)); 
      sourceApp[i].Start(Seconds(start[i]));
      sourceApp[i].Stop(Seconds (10.0));    

      PacketSinkHelper UDPSink("ns3::UdpSocketFactory",InetSocketAddress(Ipv4Address::GetAny(),port));
      sinkApp[i]= UDPSink.Install(nodes.Get(i));
      sinkApp[i].Start(Seconds(0.0));
      sinkApp[i].Stop(Seconds(10.0));

      Ptr<OnOffApplication> source1 = DynamicCast<OnOffApplication> (sourceApp[i].Get(0));
      source1->TraceConnectWithoutContext ("Tx", MakeCallback(&Trace));   // Call back method to count transmitted bytes
   }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Create the animation object and configure for specified output
    AnimationInterface anim (animFile);


  Simulator::Stop(Seconds(10.0));
  Simulator::Run ();
  
 
  for(uint32_t i = 0;i < nNodes; i++)
  {
    Ptr<PacketSink> sink1 = DynamicCast<PacketSink> (sinkApp[i].Get(0));
    bytesReceived += sink1->GetTotalRx();
  }  

  efficiency = (double)bytesReceived/(double)bytesTransmitted;
  
  cout<<"Nodes\t"<<"Area\t"<<"Intensity\t"<<"Protocol\t"<<"PowerTx(mW)\t"<<"Efficiency\t"<<endl;
  cout<<nNodes<<"\t"<<area<<"\t"<<intensity<<"\t\t"<<protocol<<"\t\t"<<powerTx<<"\t\t"<<efficiency<<endl;

  Simulator::Destroy ();
  return 0;
}


