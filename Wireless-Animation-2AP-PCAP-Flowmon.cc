/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Wireless + P2P + CSMA + Animation + PCAP Example (2 AP + STA Groups)
 * AP1 → OnOffHelper
 * AP2 → OnOffApplication
 */

#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

#include <iomanip>   // std::fixed, std::setprecision, std::setw
#include <map>
#include <vector>
#include <string>
#include <cstdint>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WirelessAnimation2APExample");

// ============== Global untuk FlowMonitor per detik ==============
FlowMonitorHelper flowmonHelper;
Ptr<FlowMonitor> monitor;
Ptr<Ipv4FlowClassifier> classifier;
std::ofstream timeseries;

// --- state terakhir untuk hitung delta per-interval (baru ditambahkan) ---
std::map<FlowId, uint64_t> lastRxBytes;
std::map<FlowId, uint64_t> lastTxPackets;
std::map<FlowId, uint64_t> lastRxPackets;
std::map<FlowId, Time>     lastDelaySum;

// Fungsi record per detik (dimodifikasi untuk delta per-interval)
void RecordFlowStatsPerSecond(double time)
{
  monitor->CheckForLostPackets();
  auto stats = monitor->GetFlowStats();

  for (auto iter = stats.begin(); iter != stats.end(); ++iter)
  {
    FlowId fid = iter->first;
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(fid);

    if (t.sourceAddress == Ipv4Address("10.1.3.1") ||
        t.sourceAddress == Ipv4Address("10.1.5.1"))
    {
      // current cumulative values
      uint64_t curRxBytes = iter->second.rxBytes;
      uint64_t curTxPackets = iter->second.txPackets;
      uint64_t curRxPackets = iter->second.rxPackets;
      Time curDelaySum = iter->second.delaySum;

      // compute delta from last stored values (interval ~1s)
      uint64_t rxBytesDelta = 0;
      uint64_t txPacketsDelta = 0;
      uint64_t rxPacketsDelta = 0;
      Time delayDelta = Seconds(0);

      if (lastRxBytes.find(fid) != lastRxBytes.end())
      {
        // safe subtract: hanya jika cur >= last, kalau tidak ambil cur (menghindari underflow)
        rxBytesDelta = (curRxBytes >= lastRxBytes[fid]) ? (curRxBytes - lastRxBytes[fid]) : curRxBytes;
        txPacketsDelta = (curTxPackets >= lastTxPackets[fid]) ? (curTxPackets - lastTxPackets[fid]) : curTxPackets;
        rxPacketsDelta = (curRxPackets >= lastRxPackets[fid]) ? (curRxPackets - lastRxPackets[fid]) : curRxPackets;
        delayDelta = (curDelaySum >= lastDelaySum[fid]) ? (curDelaySum - lastDelaySum[fid]) : curDelaySum;
      }
      else
      {
        // first sample: treat last values as zero -> delta = current
        rxBytesDelta = curRxBytes;
        txPacketsDelta = curTxPackets;
        rxPacketsDelta = curRxPackets;
        delayDelta = curDelaySum;
      }

      // interval length (sekitar 1 detik karena scheduling tiap 1s)
      double interval = 1.0;

      // hitung metrik per-interval (jika ada traffic)
      double throughput = 0.0;
      double pdr = 0.0;
      double loss = 0.0;
      double delay = 0.0;

      if (interval > 0.0)
      {
        throughput = (rxBytesDelta * 8.0 / interval) / 1e6; // Mbps
      }

      if (txPacketsDelta > 0)
      {
        pdr = (double)rxPacketsDelta / (double)txPacketsDelta * 100.0;
        if (std::isnan(pdr) || std::isinf(pdr)) pdr = 0.0;
        if (pdr > 100.0) pdr = 100.0;
        if (pdr < 0.0) pdr = 0.0;

        loss = 100.0 - pdr;
        if (std::isnan(loss) || std::isinf(loss)) loss = 0.0;
        if (loss > 100.0) loss = 100.0;
        if (loss < 0.0) loss = 0.0;
      }
      else
      {
        pdr = 0.0;
        loss = 0.0;
      }

      if (rxPacketsDelta > 0)
      {
        delay = (delayDelta.GetSeconds() / (double)rxPacketsDelta) * 1000.0; // ms
        if (std::isnan(delay) || std::isinf(delay)) delay = 0.0;
      }
      else
      {
        delay = 0.0;
      }

      // Nama flow (AP1 / AP2)
      std::string apName = (t.sourceAddress == Ipv4Address("10.1.3.1")) ? "AP1 (helper)" : "AP2 (apps)";

      if (txPacketsDelta > 0 || rxBytesDelta > 0)
      {
        timeseries << (int)time << "," << apName << ","
                   << std::fixed << std::setprecision(2)
                   << throughput << "," << pdr << ","
                   << loss << "," << delay << std::endl;
      }

      // Update last-state
      lastRxBytes[fid] = curRxBytes;
      lastTxPackets[fid] = curTxPackets;
      lastRxPackets[fid] = curRxPackets;
      lastDelaySum[fid] = curDelaySum;
    }
  }

  if (time < 20.0)
    Simulator::Schedule(Seconds(1.0), &RecordFlowStatsPerSecond, time + 1.0);
}

// =========================== MAIN ==============================
int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  NodeContainer allNodes;

  // ================= WiFi Group 1 (AP1 + STA1) =================
  NodeContainer wifiStaNodes1;
  wifiStaNodes1.Create(1);
  allNodes.Add(wifiStaNodes1);

  NodeContainer wifiApNode1;
  wifiApNode1.Create(1);
  allNodes.Add(wifiApNode1);

  YansWifiChannelHelper channel1;
  channel1.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  channel1.AddPropagationLoss("ns3::FriisPropagationLossModel");

  YansWifiPhyHelper phy1;
  phy1.SetChannel(channel1.Create());

  WifiHelper wifi1;
  wifi1.SetStandard(WIFI_STANDARD_80211a);
  wifi1.SetRemoteStationManager("ns3::AarfWifiManager");

  WifiMacHelper mac1;
  Ssid ssid1 = Ssid("Wifi-NS-Rhmn-AP1");

  mac1.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid1),
               "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevices1 = wifi1.Install(phy1, mac1, wifiStaNodes1);

  mac1.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid1));
  NetDeviceContainer apDevices1 = wifi1.Install(phy1, mac1, wifiApNode1);

  NodeContainer routerNode;
  routerNode.Create(1);
  allNodes.Add(routerNode);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));
  NodeContainer p2pNodes1;
  p2pNodes1.Add(wifiApNode1);
  p2pNodes1.Add(routerNode);
  NetDeviceContainer p2pDevices1 = pointToPoint.Install(p2pNodes1);

  // ================= WiFi Group 2 (AP2 + STA2) =================
  NodeContainer wifiStaNodes2;
  wifiStaNodes2.Create(1);
  allNodes.Add(wifiStaNodes2);

  NodeContainer wifiApNode2;
  wifiApNode2.Create(1);
  allNodes.Add(wifiApNode2);

  YansWifiChannelHelper channel2;
  channel2.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  channel2.AddPropagationLoss("ns3::FriisPropagationLossModel");

  YansWifiPhyHelper phy2;
  phy2.SetChannel(channel2.Create());

  WifiHelper wifi2;
  wifi2.SetStandard(WIFI_STANDARD_80211a);
  wifi2.SetRemoteStationManager("ns3::AarfWifiManager");

  WifiMacHelper mac2;
  Ssid ssid2 = Ssid("Wifi-NS-Rhmn-AP2");

  mac2.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid2),
               "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevices2 = wifi2.Install(phy2, mac2, wifiStaNodes2);

  mac2.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid2));
  NetDeviceContainer apDevices2 = wifi2.Install(phy2, mac2, wifiApNode2);

  NodeContainer p2pNodes2;
  p2pNodes2.Add(wifiApNode2);
  p2pNodes2.Add(routerNode.Get(0));
  NetDeviceContainer p2pDevices2 = pointToPoint.Install(p2pNodes2);

  NodeContainer csmaNodes;
  csmaNodes.Add(routerNode.Get(0));
  csmaNodes.Create(1);
  allNodes.Add(csmaNodes.Get(1));

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
  csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));
  NetDeviceContainer csmaDevices = csma.Install(csmaNodes);

  MobilityHelper mobilitySta1;
  mobilitySta1.SetPositionAllocator("ns3::GridPositionAllocator",
                                    "MinX", DoubleValue(5.0),
                                    "MinY", DoubleValue(5.0),
                                    "DeltaX", DoubleValue(5.0),
                                    "DeltaY", DoubleValue(5.0),
                                    "GridWidth", UintegerValue(5),
                                    "LayoutType", StringValue("RowFirst"));
  mobilitySta1.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue(Rectangle(-50, 50, -25, 50)));
  mobilitySta1.Install(wifiStaNodes1);

  MobilityHelper mobilitySta2;
  mobilitySta2.SetPositionAllocator("ns3::GridPositionAllocator",
                                    "MinX", DoubleValue(15.0),
                                    "MinY", DoubleValue(45.0),
                                    "DeltaX", DoubleValue(5.0),
                                    "DeltaY", DoubleValue(5.0),
                                    "GridWidth", UintegerValue(5),
                                    "LayoutType", StringValue("RowFirst"));
  mobilitySta2.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue(Rectangle(-50, 50, -25, 50)));
  mobilitySta2.Install(wifiStaNodes2);

  MobilityHelper mobilityFixed;
  mobilityFixed.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobilityFixed.Install(wifiApNode1);
  mobilityFixed.Install(wifiApNode2);
  mobilityFixed.Install(routerNode);
  mobilityFixed.Install(csmaNodes.Get(1));

  InternetStackHelper stack;
  stack.Install(allNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces1 = address.Assign(p2pDevices1);
  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer csmaInterfaces = address.Assign(csmaDevices);
  address.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces1 = address.Assign(staDevices1);
  Ipv4InterfaceContainer apInterface1 = address.Assign(apDevices1);
  address.SetBase("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces2 = address.Assign(p2pDevices2);
  address.SetBase("10.1.5.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces2 = address.Assign(staDevices2);
  Ipv4InterfaceContainer apInterface2 = address.Assign(apDevices2);

  uint16_t port = 5000;
  Address serverAddress(InetSocketAddress(csmaInterfaces.GetAddress(1), port));
  PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", serverAddress);
  ApplicationContainer serverApp = sinkHelper.Install(csmaNodes.Get(1));
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(20.0));

  OnOffHelper onoff1("ns3::UdpSocketFactory", serverAddress);
  onoff1.SetAttribute("DataRate", StringValue("2Mbps"));
  onoff1.SetAttribute("PacketSize", UintegerValue(512));
  onoff1.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff1.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  ApplicationContainer client1 = onoff1.Install(wifiStaNodes1.Get(0));
  client1.Start(Seconds(2.0));
  client1.Stop(Seconds(20.0));

  Ptr<Socket> sta2Socket = Socket::CreateSocket(wifiStaNodes2.Get(0), UdpSocketFactory::GetTypeId());
  Ptr<OnOffApplication> onoff2 = CreateObject<OnOffApplication>();
  onoff2->SetAttribute("DataRate", DataRateValue(DataRate("2Mbps")));
  onoff2->SetAttribute("PacketSize", UintegerValue(512));
  onoff2->SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff2->SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onoff2->SetAttribute("Remote", AddressValue(serverAddress));
  wifiStaNodes2.Get(0)->AddApplication(onoff2);
  onoff2->SetStartTime(Seconds(2.0));
  onoff2->SetStopTime(Seconds(20.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  phy1.EnablePcap("scratch/pcap_file/onoffhelp_ap1", apDevices1.Get(0), true, true);
  phy2.EnablePcap("scratch/pcap_file/onoffapp_ap2", apDevices2.Get(0), true, true);

  AnimationInterface anim("wireless-animation-2AP.xml");
  anim.SetConstantPosition(wifiApNode1.Get(0), 20, 20);
  anim.SetConstantPosition(routerNode.Get(0), 25, 30);
  anim.SetConstantPosition(wifiApNode2.Get(0), 20, 40);
  anim.SetConstantPosition(csmaNodes.Get(1), 50, 30);

  anim.UpdateNodeDescription(wifiStaNodes1.Get(0), "STA1-1");
  anim.UpdateNodeColor(wifiStaNodes1.Get(0), 255, 0, 0);
  anim.UpdateNodeDescription(wifiStaNodes2.Get(0), "STA2-1");
  anim.UpdateNodeColor(wifiStaNodes2.Get(0), 0, 255, 255);
  anim.UpdateNodeDescription(wifiApNode1.Get(0), "AP1");
  anim.UpdateNodeColor(wifiApNode1.Get(0), 0, 0, 255);
  anim.UpdateNodeDescription(wifiApNode2.Get(0), "AP2");
  anim.UpdateNodeColor(wifiApNode2.Get(0), 255, 128, 0);
  anim.UpdateNodeDescription(routerNode.Get(0), "Router");
  anim.UpdateNodeColor(routerNode.Get(0), 0, 255, 0);
  anim.UpdateNodeDescription(csmaNodes.Get(1), "ISP");
  anim.UpdateNodeColor(csmaNodes.Get(1), 255, 255, 0);
  anim.EnablePacketMetadata(true);
  anim.EnableIpv4RouteTracking("routingtable-wireless-2AP.xml", Seconds(0), Seconds(5), Seconds(0.25));

  monitor = flowmonHelper.InstallAll();
  classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());

  timeseries.open("scratch/pcap_file/flow_timeseries.csv");
  timeseries << "Time,Flow,Throughput(Mbps),PDR(%),Loss(%),Delay(ms)" << std::endl;

  Simulator::Schedule(Seconds(2.0), &RecordFlowStatsPerSecond, 2.0);
  Simulator::Stop(Seconds(20.0));
  Simulator::Run();

  monitor->SerializeToXmlFile("scratch/pcap_file/flowmon-results.xml", true, true);
  timeseries.close();

   // ================= Rekap rata-rata di terminal dengan narasi =================
  std::cout << "\n=== Pengujian Performance OnOffHelper vs OnOffApps dengan Variable Beban ===\n";
  std::cout << "DataRate = 2Mbps, PacketSize = 512 bytes, Protocol = UDP\n\n";
  
  std::cout << std::left
            << std::setw(15) << "#Flow Process"
            << std::setw(18) << "Throughput(Mbps)"
            << std::setw(12) << "PDR(%)"
            << std::setw(12) << "Loss(%)"
            << std::setw(12) << "Delay(ms)" << std::endl;

  auto stats = monitor->GetFlowStats();
  for (auto iter = stats.begin(); iter != stats.end(); ++iter)
  {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);

    if (t.sourceAddress == Ipv4Address("10.1.3.1") ||
        t.sourceAddress == Ipv4Address("10.1.5.1"))
    {
      double throughput = (iter->second.rxBytes * 8.0 /
                           (iter->second.timeLastRxPacket.GetSeconds() -
                            iter->second.timeFirstTxPacket.GetSeconds())) / 1e6;

      double pdr = (iter->second.txPackets > 0) ?
                   (double)iter->second.rxPackets / iter->second.txPackets * 100 : 0;
      double loss = (iter->second.txPackets > 0) ?
                    (double)(iter->second.txPackets - iter->second.rxPackets) /
                    iter->second.txPackets * 100 : 0;
      double delay = (iter->second.rxPackets > 0) ?
                     (iter->second.delaySum.GetSeconds() / iter->second.rxPackets) * 1000 : 0;

      std::string apName = (t.sourceAddress == Ipv4Address("10.1.3.1")) ? "AP 1 (helper)" : "AP 2 (apps)";

      std::cout << std::left
                << std::setw(15) << apName
                << std::setw(18) << std::fixed << std::setprecision(5) << throughput
                << std::setw(12) << std::fixed << std::setprecision(5) << pdr
                << std::setw(12) << std::fixed << std::setprecision(5) << loss
                << std::setw(12) << std::fixed << std::setprecision(5) << delay
                << std::endl;
    }
  }

  Simulator::Destroy();
  return 0;
}
