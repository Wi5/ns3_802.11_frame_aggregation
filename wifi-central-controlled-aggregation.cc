/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2017 Jose Saldana, University of Zaragoza (jsaldana@unizar.es)
 *
 * This work has been partially financed by the EU H2020 Wi-5 project (G.A. no: 644262).
 *
 * If you use this code, please cite the next research article:
 *
 * Jose Saldana, Jose Ruiz-Mas, Jose Almodovar, "Frame Aggregation in Central Controlled
 * 802.11 WLANs: the Latency vs. Throughput Trade-off," IEEE Communications Letters,
 * accepted for publication, August 2017.
 * http://dx.doi.org/10.1109/LCOMM.2017.2741940
 * FIXME: (the citation will be updated as soon as the paper is published in paper)
 *
 * http://ieeexplore.ieee.org/document/8013762/
 * Author's self-archive version: http://diec.unizar.es/~jsaldana/personal/amsterdam_2017_in_proc.pdf
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Some parts are inspired on https://www.nsnam.org/doxygen/wifi-aggregation_8cc.html, by Sebastien Deronne
 * Other parts are inspired on https://www.nsnam.org/doxygen/wifi-wired-bridging_8cc_source.html
 * The flow monitor part is inspired on https://www.nsnam.org/doxygen/wifi-hidden-terminal_8cc_source.html
 * The association record is inspired on https://github.com/MOSAIC-UA/802.11ah-ns3/blob/master/ns-3/scratch/s1g-mac-test.cc
 * The hub is inspired on https://www.nsnam.org/doxygen/csma-bridge_8cc_source.html
 *
 * v145
 * Developed and tested for ns-3.26, although the simulation crashes in some cases. One example:
 *    - more than one AP
 *    - set the RtsCtsThreshold below 48000
 *    - AMPDU aggregation On
 *    - Ideal or Minstrel wifi manager
 *
 *  This problem does not exist in ns3-devel (ns-3-dev-444dfd0968eb, Jun 2017).
 */


// PENDING
/*
1) make the 'unset assoc' change the channel to that of the closest AP

Two possibilities:
- WORKING: the STA deassociates by itself. I have to use Yanswifi.
- Not working yet: That it needs the new AP to send beacons to it. In this second option,
  the command 'addoperational channel' should be used on each STA. 
  see https://www.nsnam.org/doxygen/classns3_1_1_spectrum_wifi_phy.html#a948c6d197accf2028529a2842ec68816

2) To separate this file into a number of them, using .h files.

3) Modify the deadlines in ns-3.26/src/internet/model/arp-cache.cc
    I have gone to this file and put 
      - AliveTimeout as 12000 instead of 120
      - DeadTimeout  as 10000 instead of 100

*/

//
// The network scenario includes
// - a number of STA wifi nodes, with different mobility patterns
// - a number of AP wifi nodes. They are distributed in rows and columns in an area
// - a number of servers: each of them communicates with one STA (it is the origin or the destination of the packets)
//
// On each AP node, there is
// - a csma device
// - a wifi device
// - a bridge that binds both interfaces the whole thing into one network
// 
// IP addresses:
// - the STAs have addresses 10.0.0.0 (mask 255.255.0.0)
// - the servers behind the router (only in topology 2) have addresses 10.1.0.0 (mask 255.255.0.0) 
//
// There are three topologies:
//
// topology = 0
//
//              (*)
//            +--|-+                      10.0.0.0
//    (*)     |STA1|           csma     +-----------+  csma  +--------+ 
//  +--|-+    +----+    +---------------|   hub     |--------| single | All the server applications
//  |STA0|              |               +-----------+        | server | are in this node
//  +----+              |                         |          +--------+    
//                      |                     csma|                        
//        +-------------|--+         +------------|--+                 
//        | +----+  +----+ |         | +----+ +----+ |                 
//   ((*))--|WIFI|  |CSMA| |    ((*))--|WIFI| |CSMA| |                 
//        | +----+  +----+ |         | +----+ +----+ |                 
//        |   |       |    |         |   |      |    |                 
//        |  +----------+  |         |  +---------+  |                 
//        |  |  BRIDGE  |  |         |  |  BRIDGE |  |                  
//        |  +----------+  |         |  +---------+  |
//        +----------------+         +---------------+                  
//               AP 0                       AP 1                          
//
//
//
// topology = 1 (DEFAULT)
//
//              (*)
//            +--|-+                      10.0.0.0                       
//    (*)     |STA1|           csma     +-----------+  csma         
//  +--|-+    +----+    +---------------|   hub     |----------------------------------------+
//  |STA0|              |               +-----------+----------------------+                 |
//  +----+              |                         |                        |                 |
//                      |                     csma|                        |                 |
//        +-------------|--+         +------------|--+                 +--------+      +--------+
//        | +----+  +----+ |         | +----+ +----+ |                 | +----+ |      | +----+ |
//   ((*))--|WIFI|  |CSMA| |    ((*))--|WIFI| |CSMA| |                 | |CSMA| |      | |CSMA| |  ...
//        | +----+  +----+ |         | +----+ +----+ |                 | +----+ |      | +----+ |
//        |   |       |    |         |   |      |    |                 |        |      |        |
//        |  +----------+  |         |  +---------+  |                 +--------+      +--------+
//        |  |  BRIDGE  |  |         |  |  BRIDGE |  |                  server 0        server 1
//        |  +----------+  |         |  +---------+  |
//        +----------------+         +---------------+                  talks with      talks with
//               AP 0                       AP 1                          STA 0           STA 1
//
//
//
// topology = 2
//
//              (*)
//            +--|-+                      10.0.0.0                        10.1.0.0
//    (*)     |STA1|           csma     +-----------+  csma  +--------+       point to point
//  +--|-+    +----+    +---------------|   hub     |--------| router |----------------------+
//  |STA0|              |               +-----------+        |        |----+                 |
//  +----+              |                         |          +--------+    |                 |
//                      |                     csma|                        |                 |
//        +-------------|--+         +------------|--+                 +--------+      +--------+
//        | +----+  +----+ |         | +----+ +----+ |                 | +----+ |      | +----+ |
//   ((*))--|WIFI|  |CSMA| |    ((*))--|WIFI| |CSMA| |                 | |CSMA| |      | |CSMA| |  ...
//        | +----+  +----+ |         | +----+ +----+ |                 | +----+ |      | +----+ |
//        |   |       |    |         |   |      |    |                 |        |      |        |
//        |  +----------+  |         |  +---------+  |                 +--------+      +--------+
//        |  |  BRIDGE  |  |         |  |  BRIDGE |  |                  server 0        server 1
//        |  +----------+  |         |  +---------+  |
//        +----------------+         +---------------+                  talks with      talks with
//               AP 0                       AP 1                          STA 0           STA 1
//
//

// When the default aggregation parameters are enabled, the
// maximum A-MPDU size is the one defined by the standard, and the throughput is maximal.
// When aggregation is disabled, the thoughput is lower
//
// Packets in this simulation can be marked with a QosTag so they
// will be considered belonging to  different queues.
// By default, all the packets belong to the BestEffort Access Class (AC_BE).
//
// The user can select many parameters when calling the program. Examples:
// ns-3.26$ ./waf --run "scratch/wifi-central-controlled-aggregation --PrintHelp"
// ns-3.26$ ./waf --run "scratch/wifi-central-controlled-aggregation --number_of_APs=1 --nodeMobility=1 --nodeSpeed=0.1 --simulationTime=10 --distance_between_APs=20"
//
// if you want different results in different runs, use a different seed each time you call the program
// (see https://www.nsnam.org/docs/manual/html/random-variables.html). One example:
//
// ns-3.26$ NS_GLOBAL_VALUE="RngRun=3" ./waf --run "scratch/wifi-central-controlled-aggregation --simulationTime=2 --nodeMobility=3 --verboseLevel=2 --number_of_APs=10 --number_of_APs_per_row=1"
// you can call it with different values of RngRun to obtain different realizations
//
// for being able to see the logs, do ns-3.26$ export NS_LOG=UdpEchoClientApplication=level_all
// or /ns-3-dev$ export 'NS_LOG=ArpCache=level_all'
// or /ns-3-dev$ export 'NS_LOG=ArpCache=level_error' for showing only errors
// see https://www.nsnam.org/docs/release/3.7/tutorial/tutorial_21.html
// see https://www.nsnam.org/docs/tutorial/html/tweaking.html#usinglogging

// Output files
//  You can establish a 'name' and a 'surname' for the output files, using the parameters:
//    --outputFileName=name --outputFileSurname=seed-1
//
//  You can run a battery of tests with the same name and different surname, and you
//  will finally obtain a single file name_average.txt with all the averaged results.
//
//  Example: if you use --outputFileName=name --outputFileSurname=seed-1
//  you will obtain the next output files:
//
//    - name_average.txt                            it integrates all the tests with the same name, even if they have a different surname
//                                                  the file is not deleted, so each test with the same name is added at the bottom
//    - name_seed-1_flows.txt                       information of all the flows of this run
//    - name_seed-1_flow_1_delay_histogram.txt      delay histogram of flow #1
//    - name_seed-1_flow_1_jitter_histogram.txt
//    - name_seed-1_flow_1_packetsize_histogram.txt
//    - name_seed-1_flowmonitor.xml
//    - name_seed-1_AP-0.2.pcap                     pcap file of the device 2 of AP #0
//    - name_seed-1_server-2-1.pcap                 pcap file of the device 1 of server #2
//    - name_seed-1_STA-8-1.pcap                    pcap file of the device 1 of STA #8
//    - name_seed-1_hub.pcap                        pcap file of the hub connecting all the APs


#include "ns3/core-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/bridge-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/nstime.h"
#include "ns3/spectrum-module.h"    // For the spectrum channel
#include <ns3/friis-spectrum-propagation-loss.h>
#include "ns3/ipv4-static-routing-helper.h"
#include <sstream>

//#include "ns3/arp-cache.h"  // If you want to do things with the ARPs
//#include "ns3/arp-header.h"

using namespace ns3;

// Maximum AMPDU size of 802.11n
#define MAXSIZE80211n 65535

// Maximum AMPDU size of 802.11ac
//https://groups.google.com/forum/#!topic/ns-3-users/T_21O5mGlgM
//802.11ac allows the maximum A-MPDU length to range from 8 KB to 1 MB. http://chimera.labs.oreilly.com/books/1234000001739/ch03.html#section-mac-agg
// The maximum transmission length is defined by time, and is a little less than 5.5 microseconds. At the highest data rates for 802.11ac, 
//an aggregate frame can hold almost four and a half megabytes of data. Rather than represent such a large number of bytes in the PLCP header,
//which is transmitted at the lowest possible data rate, 802.11ac shifts the length indication to the MPDU delimiters that are transmitted 
//as part of the high-data-rate payload
// http://www.rfwireless-world.com/Tutorials/802-11ac-MAC-layer.html
// Max. length of A-MPDU = 2^13+Exp -1 bytes 
// Exp can range from 0 to 7, this yields A-MPDU to be of length:
// - 2^13 - 1 = 8191 (8KB)
// - 2^20 - 1 = 1,048,575 (about 1MB)
#define MAXSIZE80211ac 65535  // FIXME. for 802.11ac max ampdu size should be 4692480
                              // You can set it in src/wifi/regular-wifi-mac.cc
                              // http://chimera.labs.oreilly.com/books/1234000001739/ch03.html
                              // https://www.nsnam.org/doxygen/classns3_1_1_sta_wifi_mac.html

// Define a log component
NS_LOG_COMPONENT_DEFINE ("SimpleMpduAggregation");


/********* FUNCTIONS ************/

// Change the frequency of a STA
// Copied from https://groups.google.com/forum/#!topic/ns-3-users/Ih8Hgs2qgeg
// https://10343742895474358856.googlegroups.com/attach/1b7c2a3108d5e/channel-switch-minimal.cc?part=0.1&view=1&vt=ANaJVrGFRkTkufO3dLFsc9u1J_v2-SUCAMtR0V86nVmvXWXGwwZ06cmTSv7DrQUKMWTVMt_lxuYTsrYxgVS59WU3kBd7dkkH5hQsLE8Em0FHO4jx8NbjrPk
void ChangeFrequencyLocal
(NetDeviceContainer deviceslink, uint8_t channel, uint32_t mywifiModel, uint32_t myverbose) {

  for (uint32_t i = 0; i < deviceslink.GetN (); i++)
    {
      Ptr<WifiNetDevice> wifidevice = DynamicCast<WifiNetDevice> (deviceslink.Get(i));

      if (wifidevice == 0) std::cout << "[ChangeFrequencyLocal]\tWARNING: wifidevice IS NULL" << '\n';

      Ptr<WifiPhy> phy0 = wifidevice->GetPhy();

      phy0->SetChannelNumber (channel); //https://www.nsnam.org/doxygen/classns3_1_1_wifi_phy.html#a2d13cf6ae4c185cae8516516afe4a32a
      /*
      if (mywifiModel == 0) {
        Ptr<WifiPhy> phy0 = wifidevice->GetPhy();
        phy0->SetChannelNumber (channel);
      } else {
        Ptr<SpectrumWifiPhy> phy0 = wifidevice->GetPhy()->GetSpectrumPhy();
        phy0->SetChannelNumber (channel);          
      }
*/
      if (myverbose > 1)
        std::cout << Simulator::Now() 
                  << "\t[ChangeFrequencyLocal]\tChanged channel on STA with MAC " << deviceslink.Get (i)->GetAddress () 
                  << "  to:  " << uint16_t(channel) << std::endl;
    }
}


/*********** This part is only for the ARPs. Not used **************/
typedef std::pair<Ptr<Packet>, Ipv4Header> Ipv4PayloadHeaderPair;

static void PrintArpCache (uint32_t nodenumber, Ptr <Node> node, Ptr <NetDevice> nd/*, Ptr <Ipv4Interface> interface*/)
{
  std::cout << "Printing Arp Cache of Node#" << nodenumber << '\n';
  Ptr <ArpL3Protocol> arpL3 = node->GetObject <ArpL3Protocol> ();
  //Ptr <ArpCache> arpCache = arpL3->FindCache (nd);
  //arpCache->Flush ();

  // Get an interactor to Ipv4L3Protocol instance
   Ptr<Ipv4L3Protocol> ip = node->GetObject<Ipv4L3Protocol> ();
   NS_ASSERT(ip !=0);

  // Get interfaces list from Ipv4L3Protocol iteractor
  ObjectVectorValue interfaces;
  ip->GetAttribute("InterfaceList", interfaces);

  // For each interface
  uint32_t l = 0;
  for(ObjectVectorValue::Iterator j = interfaces.Begin(); j !=interfaces.End (); j ++)
  {

    // Get an interactor to Ipv4L3Protocol instance
    Ptr<Ipv4Interface> ipIface = (j->second)->GetObject<Ipv4Interface> ();
    NS_ASSERT(ipIface != 0);

    std::cout << "Interface #" << l << " IP address" <<  /*<< */'\n';
    l++;

    // Get interfaces list from Ipv4L3Protocol iteractor
    Ptr<NetDevice> device = ipIface->GetDevice();
    NS_ASSERT(device != 0);

    if (device == nd) {
    // Get MacAddress assigned to this device
    Mac48Address addr = Mac48Address::ConvertFrom(device->GetAddress ());

    // For each Ipv4Address in the list of Ipv4Addresses assign to this interface...
    for(uint32_t k = 0; k < ipIface->GetNAddresses (); k++)
      {
        // Get Ipv4Address
        Ipv4Address ipAddr = ipIface->GetAddress (k).GetLocal();

        // If Loopback address, go to the next
        if(ipAddr == Ipv4Address::GetLoopback())
        {
          NS_LOG_UNCOND ("[PrintArpCache] Node #" << nodenumber << " " << addr << ", " << ipAddr << "");
        } else {

          NS_LOG_UNCOND ("[PrintArpCache] Node #" << nodenumber << " " << addr << ", " << ipAddr << "");

        Ptr<ArpCache> m_arpCache = nd->GetObject<ArpCache> ();
        m_arpCache = ipIface->GetObject<ArpCache> (); // FIXME: THIS DOES NOT WORK
        //m_arpCache = node->GetObject<ArpCache> ();
        //m_arpCache = nd->GetObject<ArpCache> ();
        //m_arpCache->SetAliveTimeout(Seconds(7));

        //if (m_arpCache != 0) {
          NS_LOG_UNCOND ("[PrintArpCache]       " << nodenumber << " " << addr << ", " << ipAddr << "");
          AsciiTraceHelper asciiTraceHelper;
          Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream ("arpcache.txt");
          m_arpCache->PrintArpCache(stream);
          m_arpCache->Flush();
          //ArpCache::Entry * entry = m_arpCache->Add(ipAddr);
          //entry->MarkWaitReply(0);
          //entry->MarkAlive(addr);
          //}
        }
      }
    }
  }
  Simulator::Schedule (Seconds(1.0), &PrintArpCache, nodenumber, node, nd);
}

void emtpyArpCache()
{
  // Creates ARP Cache object
  Ptr<ArpCache> arp = CreateObject<ArpCache> ();
  uint32_t l = 0;
  for (NodeList::Iterator i = NodeList::Begin(); i != NodeList::End(); ++i)
  {
    std::cout << "Node #" << l << '\n';
    l ++;
    arp = (*i)->GetObject<ArpCache> ();
    arp->SetAliveTimeout (Seconds(3600 * 24 )); // 1-year
    //arp->Flush();
  }
}

void infoArpCache(uint32_t nodenumber, Ptr <Node> mynode, uint32_t myverbose)
{
  // Create ARP Cache object
  Ptr<ArpCache> arp = CreateObject<ArpCache> ();
  Ptr<Ipv4L3Protocol> ip = mynode->GetObject<Ipv4L3Protocol> ();
  if (ip!=0) {
    std::cout << "[infoArpCache] Adding the Arp Cache to Node #" << nodenumber << '\n';

    ObjectVectorValue interfaces;
    ip->GetAttribute("InterfaceList", interfaces);
    for(ObjectVectorValue::Iterator j = interfaces.Begin(); j !=interfaces.End (); j ++)
    {
      Ptr<Ipv4Interface> ipIface = (j->second)->GetObject<Ipv4Interface> ();

      // Get interfaces list from Ipv4L3Protocol iteractor
      Ptr<NetDevice> device = ipIface->GetDevice();
      NS_ASSERT(device != 0);

      arp->SetDevice (device, ipIface); // https://www.nsnam.org/doxygen/classns3_1_1_arp_cache.html#details
      //ipIface->SetAttribute("ArpCache", PointerValue(arp));


      AsciiTraceHelper asciiTraceHelper;
      Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream ("arpcache.txt");
      arp->PrintArpCache(stream);

      if (myverbose) {
        Time mytime = arp->GetAliveTimeout();
        std::cout << "Alive Timeout [s]: " << mytime.GetSeconds() << '\n';

        mytime = arp->GetDeadTimeout();
        std::cout << "Dead Timeout [s]: " << mytime.GetSeconds() << '\n';
      }
    }
  }
  Simulator::Schedule (Seconds(1.0), &infoArpCache, nodenumber, mynode, myverbose);
}

// Taken from here https://github.com/MOSAIC-UA/802.11ah-ns3/blob/master/ns-3/scratch/s1g-mac-test.cc
// Two typos corrected here https://groups.google.com/forum/#!topic/ns-3-users/JRE_BsNEJrY

// It seems this is not feasible: https://www.nsnam.org/bugzilla/show_bug.cgi?id=187
void PopulateArpCache (uint32_t nodenumber, Ptr <Node> mynode)
{
  // Create ARP Cache object
  Ptr<ArpCache> arp = CreateObject<ArpCache> ();

  Ptr<Packet> dummy = Create<Packet> ();

  // Set ARP Timeout
  //arp->SetAliveTimeout (Seconds(3600 * 24 )); // 1-year
  //arp->SetWaitReplyTimeout (Seconds(200));

  // Populates ARP Cache with information from all nodes
  /*for (NodeList::Iterator i = NodeList::Begin(); i != NodeList::End(); ++i)
    {*/

      std::cout << "[PopulateArpCache] Node #" << nodenumber << '\n';

      // Get an interactor to Ipv4L3Protocol instance

      Ptr<Ipv4L3Protocol> ip = mynode->GetObject<Ipv4L3Protocol> ();
      NS_ASSERT(ip !=0);

      // Get interfaces list from Ipv4L3Protocol iteractor
      ObjectVectorValue interfaces;
      ip->GetAttribute("InterfaceList", interfaces);

      // For each interface
      for(ObjectVectorValue::Iterator j = interfaces.Begin(); j !=interfaces.End (); j ++)
        {
          // Get an interactor to Ipv4L3Protocol instance
          Ptr<Ipv4Interface> ipIface = (j->second)->GetObject<Ipv4Interface> ();
          NS_ASSERT(ipIface != 0);

          // Get interfaces list from Ipv4L3Protocol iteractor
          Ptr<NetDevice> device = ipIface->GetDevice();
          NS_ASSERT(device != 0);

          // Get MacAddress assigned to this device
          Mac48Address addr = Mac48Address::ConvertFrom(device->GetAddress ());

          // For each Ipv4Address in the list of Ipv4Addresses assign to this interface...
          for(uint32_t k = 0; k < ipIface->GetNAddresses (); k++)
            {

              // Get Ipv4Address
              Ipv4Address ipAddr = ipIface->GetAddress (k).GetLocal();

              // If Loopback address, go to the next
              if(ipAddr == Ipv4Address::GetLoopback())
                continue;

              std::cout << "[PopulateArpCache] Arp Cache: Adding the pair (" << addr << "," << ipAddr << ")" << '\n';

              // Creates an ARP entry for this Ipv4Address and adds it to the ARP Cache
              Ipv4Header ipHeader;
              ArpCache::Entry * entry = arp->Add(ipAddr);
              //entry->IsPermanent();
              //entry->MarkWaitReply();
              //entry->MarkPermanent();
              //entry->MarkAlive(addr);
              //entry->MarkDead();

              entry->MarkWaitReply (Ipv4PayloadHeaderPair(dummy,ipHeader));
              entry->MarkAlive (addr);
              entry->ClearPendingPacket();
              entry->MarkPermanent ();

              NS_LOG_UNCOND ("[PopulateArpCache] Arp Cache: Added the pair (" << addr << "," << ipAddr << ")");

              AsciiTraceHelper asciiTraceHelper;
              Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream ("arpcache.txt");

              arp->PrintArpCache(stream);

            }
//    }

    // Assign ARP Cache to each interface of each node
    for (NodeList::Iterator i = NodeList::Begin(); i != NodeList::End(); ++i)
    {

      Ptr<Ipv4L3Protocol> ip = (*i)->GetObject<Ipv4L3Protocol> ();
      if (ip!=0) {
        std::cout << "[PopulateArpCache] Adding the Arp Cache to Node #" << nodenumber << '\n';

        ObjectVectorValue interfaces;
        ip->GetAttribute("InterfaceList", interfaces);
        for(ObjectVectorValue::Iterator j = interfaces.Begin(); j !=interfaces.End (); j ++)
        {
          Ptr<Ipv4Interface> ipIface = (j->second)->GetObject<Ipv4Interface> ();

          // Get interfaces list from Ipv4L3Protocol iteractor
          Ptr<NetDevice> device = ipIface->GetDevice();
          NS_ASSERT(device != 0);

          arp->SetDevice (device, ipIface); // https://www.nsnam.org/doxygen/classns3_1_1_arp_cache.html#details

          //ipIface->SetAttribute("ArpCache", PointerValue(arp));


          AsciiTraceHelper asciiTraceHelper;
          Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream ("arpcache.txt");

          arp->PrintArpCache(stream);

          Time mytime = arp->GetAliveTimeout();
          std::cout << "Alive Timeout [s]: " << mytime.GetSeconds() << '\n';
        }
      }
    }
  }
}
/************* END of the ARP part (not used) *************/

// Modify the max AMPDU value of a node
void ModifyAmpdu (uint32_t nodeNumber, uint32_t ampduValue, uint32_t myverbose)
{
  // These are the attributes of regular-wifi-mac: https://www.nsnam.org/doxygen/regular-wifi-mac_8cc_source.html
  // You have to build a line like this (e.g. for node 0):
  // Config::Set("/NodeList/0/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/BE_MaxAmpduSize", UintegerValue(ampduValue));
  // There are 4 queues: VI, VO, BE and BK

  // FIXME: Check if I only have to modify the parameters of all the devices (*), or only some of them.

  // I use an auxiliar string for creating the first argument of Config::Set
  std::ostringstream auxString;

  // VI queue
  auxString << "/NodeList/" << nodeNumber << "/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/VI_MaxAmpduSize";
  // std::cout << auxString.str() << '\n';
  Config::Set(auxString.str(),  UintegerValue(ampduValue));

  // clean the string
  auxString.str(std::string());

  // VO queue
  auxString << "/NodeList/" << nodeNumber << "/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/VO_MaxAmpduSize"; 
  // std::cout << auxString.str() << '\n';
  Config::Set(auxString.str(),  UintegerValue(ampduValue));

  // clean the string
  auxString.str(std::string());

  // BE queue
  auxString << "/NodeList/" << nodeNumber << "/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/BE_MaxAmpduSize"; 
  // std::cout << auxString.str() << '\n';
  Config::Set(auxString.str(),  UintegerValue(ampduValue));

  // clean the string
  auxString.str(std::string());

  // BK queue
  auxString << "/NodeList/" << nodeNumber << "/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/BK_MaxAmpduSize"; 
  //std::cout << auxString.str() << '\n';
  Config::Set(auxString.str(),  UintegerValue(ampduValue));  

  if ( myverbose > 1 )
    std::cout << Simulator::Now() 
              << "\t[ModifyAmpdu] Node #" << nodeNumber 
              << " AMPDU max size changed to " << ampduValue << " bytes" 
              << std::endl;
}


/*
// Not used
// taken from https://www.nsnam.org/doxygen/wifi-ap_8cc.html
// set the position of a node
static void
SetPosition (Ptr<Node> node, Vector position)
{
  Ptr<MobilityModel> mobility = node->GetObject<MobilityModel> ();
  mobility->SetPosition (position);
}*/


// Return a vector with the position of a node
// taken from https://www.nsnam.org/doxygen/wifi-ap_8cc.html
static Vector
GetPosition (Ptr<Node> node)
{
  Ptr<MobilityModel> mobility = node->GetObject<MobilityModel> ();
  return mobility->GetPosition ();
}

// obtain the nearest AP of a STA
static Ptr<Node>
nearestAp (NodeContainer APs, Ptr<Node> mySTA, int myverbose)
{
  // calculate an initial value for the minimum distance (a very high value)
  double mimimumDistance = APs.GetN() * 100000;

  // variable for storing the nearest AP
  Ptr<Node> nearest;

  // vector with the position of the STA
  Vector posSta = GetPosition (mySTA);

  // vector with the position of the AP
  Vector posAp;

  if (myverbose > 3)
    std::cout << (Simulator::Now()) << "\t[nearestAp]\tSTA #" << mySTA->GetId() <<  "\tPosition: "  << posSta.x << "," << posSta.y << std::endl;

  // Check all the APs to find the nearest one
  NodeContainer::Iterator i; 
  for (i = APs.Begin (); i != APs.End (); ++i)
  {
    //(*i)->method ();  // some Node method

    posAp = GetPosition((*i));
    uint32_t distance = sqrt ( ( (posSta.x - posAp.x)*(posSta.x - posAp.x) ) + ( (posSta.y - posAp.y)*(posSta.y - posAp.y) ) );
    if (distance < mimimumDistance ) {
      mimimumDistance = distance;
      nearest = *i;
    }
  }

  if (myverbose > 3)
    std::cout << Simulator::Now()
              << "\t[nearestAp]\t\tNearest AP is AP#" << nearest->GetId() 
              <<  ". Position: "  << GetPosition((nearest)).x 
              << "," << GetPosition((nearest)).y 
              << std::endl;

  return nearest;
}

// Print the position of a node
// taken from https://www.nsnam.org/doxygen/wifi-ap_8cc.html
static void
ReportPosition (Ptr<Node> node, int i, int type, int myverbose, NodeContainer myApNodes)
// type = 0 means it will write 'AP#'
// type = 1 means it will write 'STA#'
{
  Vector pos = GetPosition (node);

  if (myverbose > 2)
    {
      if (type == 0) {
        std::cout << Simulator::Now() 
                  << "\t[ReportPosition] AP  #" << i 
                  <<  " Position: "  << pos.x 
                  << "," << pos.y 
                  << std::endl;
      } else {
        // Find the nearest AP
        Ptr<Node> nearest;
        nearest = nearestAp (myApNodes, node, myverbose);
        std::cout << Simulator::Now() 
                  << "\t[ReportPosition] STA #" << i 
                  <<  " Position: "  << pos.x 
                  << "," << pos.y 
                  << ". The nearest AP is AP#" << (nearest)->GetId()
                  << std::endl;
      }
    }

  // re-schedule in 1 second
  Simulator::Schedule (Seconds (1.0), &ReportPosition, node, i, type, myverbose, myApNodes);
}


// Print the simulation time to std::cout
static void printTime (uint32_t period, std::string myoutputFileName, std::string myoutputFileSurname)
{
  std::cout << Simulator::Now() << "\t" << myoutputFileName << "_" << myoutputFileSurname << '\n';

  // re-schedule 
  Simulator::Schedule (Seconds (period), &printTime, period, myoutputFileName, myoutputFileSurname);
}


// function for tracking mobility changes
static void 
CourseChange (std::string foo, Ptr<const MobilityModel> mobility)
{
  Vector pos = mobility->GetPosition ();
  Vector vel = mobility->GetVelocity ();
  std::cout << Simulator::Now () << "\t[CourseChange] MOBILITY CHANGE. model= " 
            << mobility << ", POS: x=" << pos.x 
            << ", y=" << pos.y
            << ", z=" << pos.z 
            << "; VEL:" << vel.x 
            << ", y=" << vel.y
            << ", z=" << vel.z << std::endl;
}


// Print the statistics to an output file and/or to the screen
void 
print_stats ( FlowMonitor::FlowStats st, 
              double simulationTime, 
              uint32_t mygenerateHistograms, 
              std::string fileName,
              std::string fileSurname,
              uint32_t myverbose,
              std::string flowID,
              uint32_t printColumnTitles ) 
{
  // print the results to a file (they are written at the end of the file)
  if ( fileName != "" ) {

    std::ofstream ofs;
    ofs.open ( fileName + "_flows.txt", std::ofstream::out | std::ofstream::app); // with "trunc" Any contents that existed in the file before it is open are discarded. with "app", all output operations happen at the end of the file, appending to its existing contents

    // Print a line in the output file, with the title of each column
    if ( printColumnTitles == 1 ) {
      ofs << "Flow_ID" << "\t"
          << "Protocol" << "\t"
          << "source_Address" << "\t"
          << "source_Port" << "\t" 
          << "destination_Address" << "\t"
          << "destination_Port" << "\t"
          << "Num_Tx_Packets" << "\t" 
          << "Num_Tx_Bytes" << "\t" 
          << "Tx_Throughput_[bps]" << "\t"  
          << "Num_Rx_Packets" << "\t" 
          << "Num_RX_Bytes" << "\t" 
          << "Num_lost_packets" << "\t" 
          << "Rx_Throughput_[bps]" << "\t"
          << "Average_Latency_[s]" << "\t"
          << "Average_Jitter_[s]" << "\t"
          << "Average_Number_of_hops" << "\t"
          << "Simulation_time_[s]" << "\n";
    }

    // Print a line in the output file, with the data of this flow
    ofs << flowID << "\t" // flowID includes the protocol, IP addresses and ports
        << st.txPackets << "\t" 
        << st.txBytes << "\t" 
        << st.txBytes * 8.0 / simulationTime << "\t"  
        << st.rxPackets << "\t" 
        << st.rxBytes << "\t" 
        << st.txPackets - st.rxPackets << "\t" 
        << st.rxBytes * 8.0 / simulationTime << "\t";

    if (st.rxPackets > 0) 
    { 
      ofs << (st.delaySum.GetSeconds() / st.rxPackets) <<  "\t";

      if (st.rxPackets > 1) { // I need at least two packets for calculating the jitter
        ofs << (st.jitterSum.GetSeconds() / (st.rxPackets - 1.0)) << "\t";
      } else {
        ofs << "\t";
      }

      ofs << st.timesForwarded / st.rxPackets + 1 << "\t"; 

    } else { //no packets arrived
      ofs << "\t" << "\t" << "\t"; 
    }

    ofs << simulationTime << "\n";

    ofs.close();


    // save the histogram to a file
    if ( mygenerateHistograms > 0 ) 
    { 
      std::ofstream ofs_histo;
      ofs_histo.open ( fileName + fileSurname + "_delay_histogram.txt", std::ofstream::out | std::ofstream::trunc);

      ofs_histo << "Flow #" << flowID << "\n";
      ofs_histo << "number\tinit_interval\tend_interval\tnumber_of_samples" << std::endl; 
      for (uint32_t i=0; i < st.delayHistogram.GetNBins (); i++) 
        ofs_histo << i << "\t" << st.delayHistogram.GetBinStart (i) << "\t" << st.delayHistogram.GetBinEnd (i) << "\t" << st.delayHistogram.GetBinCount (i) << std::endl; 
      ofs_histo.close();

      ofs_histo.open ( fileName + fileSurname + "_jitter_histogram.txt", std::ofstream::out | std::ofstream::trunc); // with "trunc", Any contents that existed in the file before it is open are discarded

      ofs_histo << "Flow #" << flowID << "\n";
      ofs_histo << "number\tinit_interval\tend_interval\tnumber_of_samples" << std::endl; 
      for (uint32_t i=0; i < st.jitterHistogram.GetNBins (); i++ ) 
        ofs_histo << i << "\t" << st.jitterHistogram.GetBinStart (i) << "\t" << st.jitterHistogram.GetBinEnd (i) << "\t" << st.jitterHistogram.GetBinCount (i) << std::endl; 
      ofs_histo.close();

      ofs_histo.open ( fileName + fileSurname + "_packetsize_histogram.txt", std::ofstream::out | std::ofstream::trunc); // with "trunc", Any contents that existed in the file before it is open are discarded

      ofs_histo << "Flow #" << flowID << "\n";
      ofs_histo << "number\tinit_interval\tend_interval\tnumber_of_samples"<< std::endl; 
      for (uint32_t i=0; i < st.packetSizeHistogram.GetNBins (); i++ ) 
        ofs_histo << i << "\t" << st.packetSizeHistogram.GetBinStart (i) << "\t" << st.packetSizeHistogram.GetBinEnd (i) << "\t" << st.packetSizeHistogram.GetBinCount (i) << std::endl; 
      ofs_histo.close();
    }
  }

  // print the results by the screen
  if ( myverbose > 0 ) {
    std::cout << " -Flow #" << flowID << "\n";
    if ( mygenerateHistograms > 0) 
      std::cout << "   The name of the output files starts with: " << fileName << fileSurname << "\n";
      std::cout << "   Tx Packets: " << st.txPackets << "\n";
      std::cout << "   Tx Bytes:   " << st.txBytes << "\n";
      std::cout << "   TxOffered:  " << st.txBytes * 8.0 / simulationTime / 1000 / 1000  << " Mbps\n";
      std::cout << "   Rx Packets: " << st.rxPackets << "\n";
      std::cout << "   Rx Bytes:   " << st.rxBytes << "\n";
      std::cout << "   Lost Packets: " << st.txPackets - st.rxPackets << "\n";
      std::cout << "   Throughput: " << st.rxBytes * 8.0 / simulationTime / 1000 / 1000  << " Mbps\n";

    if (st.rxPackets > 0) // some packets have arrived
    { 
      std::cout << "   Mean{Delay}: " << (st.delaySum.GetSeconds() / st.rxPackets); 
      
      if (st.rxPackets > 1) // I need at least two packets for calculating the jitter
      { 
        std::cout << "   Mean{Jitter}: " << (st.jitterSum.GetSeconds() / (st.rxPackets - 1.0 ));
      } else {
        std::cout << "   Mean{Jitter}: only one packet arrived. "; 
      }
      
      std::cout << "   Mean{Hop Count}: " << st.timesForwarded / st.rxPackets + 1 << "\n"; 

    } else { //no packets arrived
      std::cout << "   Mean{Delay}: no packets arrived. ";
      std::cout << "   Mean{Jitter}: no packets arrived. "; 
      std::cout << "   Mean{Hop Count}: no packets arrived. \n"; 
    }

    if (( mygenerateHistograms > 0 ) && ( myverbose > 3 )) 
    { 
      std::cout << "   Delay Histogram" << std::endl; 
      for (uint32_t i=0; i < st.delayHistogram.GetNBins (); i++) 
        std::cout << "  " << i << "(" << st.delayHistogram.GetBinStart (i) 
                  << "-" << st.delayHistogram.GetBinEnd (i) 
                  << "): " << st.delayHistogram.GetBinCount (i) 
                  << std::endl; 

      std::cout << "   Jitter Histogram" << std::endl; 
      for (uint32_t i=0; i < st.jitterHistogram.GetNBins (); i++ ) 
        std::cout << "  " << i << "(" << st.jitterHistogram.GetBinStart (i) 
                  << "-" << st.jitterHistogram.GetBinEnd (i) 
                  << "): " << st.jitterHistogram.GetBinCount (i) 
                  << std::endl; 

      std::cout << "   PacketSize Histogram  "<< std::endl; 
      for (uint32_t i=0; i < st.packetSizeHistogram.GetNBins (); i++ ) 
        std::cout << "  " << i << "(" << st.packetSizeHistogram.GetBinStart (i) 
                  << "-" << st.packetSizeHistogram.GetBinEnd (i) 
                  << "): " << st.packetSizeHistogram.GetBinCount (i) 
                  << std::endl; 
    }

    for (uint32_t i=0; i < st.packetsDropped.size (); i++) 
      std::cout << "    Packets dropped by reason " << i << ": " << st.packetsDropped [i] << std::endl; 
//  for (uint32_t i=0; i<st.bytesDropped.size(); i++) 
//    std::cout << "Bytes dropped by reason " << i << ": " << st.bytesDropped[i] << std::endl;

    std::cout << "\n";
  }
} 


// this class stores a number of records: each one contains a pair AP node id - AP MAC address
// the node id is the one given by ns3 when creating the node
class AP_record
{
  public:
    AP_record ();
    //void SetApRecord (uint16_t thisId, Mac48Address thisMac);
    void SetApRecord (uint16_t thisId, std::string thisMac, uint32_t thisMaxSizeAmpdu);
    //uint16_t GetApid (Mac48Address thisMac);
    uint16_t GetApid ();
    //Mac48Address GetMac (uint16_t thisId);
    std::string GetMac (); 
    uint32_t GetMaxSizeAmpdu ();
    uint8_t GetWirelessChannel();
    void setWirelessChannel(uint8_t thisWirelessChannel);
  private:
    uint16_t apId;
    //Mac48Address apMac;
    std::string apMac;
    uint32_t apMaxSizeAmpdu;
    uint8_t apWirelessChannel;
};

typedef std::vector <AP_record * > AP_recordVector;
AP_recordVector AP_vector;

AP_record::AP_record ()
{
  apId = 0;
  apMac = "02-06-00:00:00:00:00:00";
  apMaxSizeAmpdu = 0;
}

void
//AP_record::SetApRecord (uint16_t thisId, Mac48Address thisMac)
AP_record::SetApRecord (uint16_t thisId, std::string thisMac, uint32_t thisMaxSizeAmpdu)
{
  apId = thisId;
  apMac = thisMac;
  apMaxSizeAmpdu = thisMaxSizeAmpdu;
}

uint16_t
//AP_record::GetApid (Mac48Address thisMac)
AP_record::GetApid ()
{
  return apId;
}

//Mac48Address
std::string
AP_record::GetMac ()
{
  return apMac;
}

uint8_t
AP_record::GetWirelessChannel()
{
  return apWirelessChannel;
}

void
AP_record::setWirelessChannel(uint8_t thisWirelessChannel)
{
  apWirelessChannel = thisWirelessChannel;
}

//MaxSizeAmpdu
uint32_t
AP_record::GetMaxSizeAmpdu ()
{
  return apMaxSizeAmpdu;
}

void
Modify_AP_Record (uint16_t thisId, std::string thisMac, uint32_t thisMaxSizeAmpdu) // FIXME: Can this be done just with Set_AP_Record?
{
  for (AP_recordVector::const_iterator index = AP_vector.begin (); index != AP_vector.end (); index++) {
    //std::cout << Simulator::Now () << " ********************** AP with ID " << (*index)->GetApid() << " has MAC: " << (*index)->GetMac() << " *****" << std::endl;

    if ( (*index)->GetMac () == thisMac ) {
      (*index)->SetApRecord (thisId, thisMac, thisMaxSizeAmpdu);
      //std::cout << Simulator::Now () << "\t[GetAnAP_Id] AP #" << (*index)->GetApid() << " has MAC: " << (*index)->GetMac() << "" << std::endl;
    }
  }
}

uint16_t
GetAnAP_Id (std::string thisMac)
// lists all the STAs associated to an AP, with the MAC of the AP
{
  uint16_t APid = 0;
  //std::cout << Simulator::Now () << " *** Number of STA associated: " << Get_STA_record_num() << " *****" << std::endl;

  for (AP_recordVector::const_iterator index = AP_vector.begin (); index != AP_vector.end (); index++) {
  //std::cout << Simulator::Now () << " ********************** AP with ID " << (*index)->GetApid() << " has MAC: " << (*index)->GetMac() << " *****" << std::endl;

    if ( (*index)->GetMac () == thisMac ) {
      APid = (*index)->GetApid ();
      //std::cout << Simulator::Now () << "\t[GetAnAP_Id] AP #" << (*index)->GetApid() << " has MAC: " << (*index)->GetMac() << "" << std::endl;
    }
  }
  return APid;
}

uint32_t
GetAP_MaxSizeAmpdu (uint16_t thisAPid, uint32_t myverbose)
// returns the max size of the Ampdu of an AP
{
  uint32_t APMaxSizeAmpdu = 0;
  //std::cout << Simulator::Now () << " *** Number of STA associated: " << Get_STA_record_num() << " *****" << std::endl;

  for (AP_recordVector::const_iterator index = AP_vector.begin (); index != AP_vector.end (); index++) {

    if ( (*index)->GetApid () == thisAPid ) {
      APMaxSizeAmpdu = (*index)->GetMaxSizeAmpdu ();
      if ( myverbose > 2 )
        std::cout << Simulator::Now () 
                  << "\t[GetAP_MaxSizeAmpdu] AP #" << (*index)->GetApid() 
                  << " has AMDPU: " << (*index)->GetMaxSizeAmpdu() 
                  << "" << std::endl;
    }
  }
  return APMaxSizeAmpdu;
}

uint8_t
GetAP_WirelessChannel (uint16_t thisAPid, uint32_t myverbose)
// returns the wireless channel of an AP
{
  uint8_t APWirelessChannel = 0;
  //std::cout << Simulator::Now () << " *** Number of STA associated: " << Get_STA_record_num() << " *****" << std::endl;

  for (AP_recordVector::const_iterator index = AP_vector.begin (); index != AP_vector.end (); index++) {

    if ( (*index)->GetApid () == thisAPid ) {
      APWirelessChannel = (*index)->GetWirelessChannel();
      if ( myverbose > 2 )
        std::cout << Simulator::Now () 
                  << "\t[GetAP_WirelessChannel] AP #" << (*index)->GetApid() 
                  << " has channel: " << uint16_t((*index)->GetWirelessChannel())
                  << "" << std::endl;
    }
  }
  return APWirelessChannel;
}

uint32_t
CountAPs (uint32_t myverbose)
// counts all the APs with their id, mac and current value of MaxAmpdu
{
  if (myverbose > 2)
    std::cout << "\n" << Simulator::Now () << "   \t[CountAPs] Report APs" << std::endl;

  uint32_t number = 0;

  for (AP_recordVector::const_iterator index = AP_vector.begin (); index != AP_vector.end (); index++) {
    number ++;
  }
  return number;
}

void
ListAPs (uint32_t myverbose)
// lists all the APs with their id, mac and current value of MaxAmpdu
{
  std::cout << "\n" << Simulator::Now () << "   \t[ListAPs] Report APs. Total " << CountAPs(myverbose) << " APs" << std::endl;

  for (AP_recordVector::const_iterator index = AP_vector.begin (); index != AP_vector.end (); index++) {
    std::cout //<< Simulator::Now ()
              << "                  "
              << "   \t\tAP #" << (*index)->GetApid() 
              << " with MAC " << (*index)->GetMac() 
              << " Max size AMPDU " << (*index)->GetMaxSizeAmpdu() 
              << " Channel " << uint16_t((*index)->GetWirelessChannel())
              << std::endl;
  }
  std::cout << std::endl;
}


// This part, i.e. the association record is taken from https://github.com/MOSAIC-UA/802.11ah-ns3/blob/master/ns-3/scratch/s1g-mac-test.cc

// this class stores one record per STA, containing 
// - the information of its association: the MAC of the AP where it is associated
// - the type of application it is running
class STA_record
{
  public:
    STA_record ();
    bool GetAssoc ();
    uint16_t GetStaid ();
    Mac48Address GetMac ();
    uint32_t Gettypeofapplication ();
    uint32_t GetMaxSizeAmpdu ();
    void SetAssoc (std::string context, Mac48Address AP_MAC_address);
    void UnsetAssoc (std::string context, Mac48Address AP_MAC_address);
    void setstaid (uint16_t id);
    void Settypeofapplication (uint32_t applicationid);
    void SetMaxSizeAmpdu (uint32_t MaxSizeAmpdu);
    void SetVerboseLevel (uint32_t myVerboseLevel);
    void SetnumChannels (uint32_t mynumChannels);
    void Setversion80211 (uint32_t myversion80211);
    void SetaggregationAlgorithm (uint32_t myaggregationAlgorithm);
    void SetAmpduSize (uint32_t myAmpduSize);
    void SetmaxAmpduSizeWhenAggregationDisabled (uint32_t mymaxAmpduSizeWhenAggregationDisabled);
    void SetWifiModel (uint32_t mywifiModel);
  private:
    bool assoc;
    uint16_t staid;
    Mac48Address apMac;
    uint32_t typeofapplication; // 0 no application; 1 VoIP upload; 2 VoIP download; 3 TCP upload; 4 TCP download
    uint32_t staRecordMaxSizeAmpdu;
    uint32_t staRecordVerboseLevel;
    uint32_t staRecordNumChannels;
    uint32_t staRecordVersion80211;
    uint32_t staRecordAggregationAlgorithm;
    uint32_t staRecordMaxAmpduSize;
    uint32_t staRecordMaxAmpduSizeWhenAggregationDisabled;
    uint32_t staRecordwifiModel;
};

// this is the constructor. Set the default parameters
STA_record::STA_record ()
{
  assoc = false;
  staid = 0;
  apMac = "00:00:00:00:00:00";
  typeofapplication = 0;
  staRecordMaxSizeAmpdu = 0;
  staRecordVerboseLevel = 0;
  staRecordNumChannels = 0;
  staRecordVersion80211 = 0;
  staRecordAggregationAlgorithm = 0;
  staRecordMaxAmpduSize = 0;
  staRecordMaxAmpduSizeWhenAggregationDisabled = 0;
  staRecordwifiModel = 0;
}

void
STA_record::setstaid (uint16_t id)
{
  staid = id;
}

typedef std::vector <STA_record * > STA_recordVector;
STA_recordVector assoc_vector;

uint32_t
Get_STA_record_num ()
// counts the number or STAs associated
{
  uint32_t AssocNum = 0;
  for (STA_recordVector::const_iterator index = assoc_vector.begin (); index != assoc_vector.end (); index++) {
    if ((*index)->GetAssoc ()) {
      AssocNum++;
    }
  }
  return AssocNum;
}

void
List_STA_record ()
// lists all the STAs, with the MAC of the AP if they are associated to it
{
  std::cout << "\n" << Simulator::Now () << "\t[List_STA_record] Report STAs. Total associated: " << Get_STA_record_num() << "" << std::endl;

  for (STA_recordVector::const_iterator index = assoc_vector.begin (); index != assoc_vector.end (); index++) {
    if ((*index)->GetAssoc ()) {

      // auxiliar string
      std::ostringstream auxString;
      // create a string with the MAC
      auxString << "02-06-" << (*index)->GetMac();
      std::string myaddress = auxString.str();

      std::cout //<< Simulator::Now () 
                << "\t\t\t\tSTA #" << (*index)->GetStaid() 
                << "\tassociated to AP #" << GetAnAP_Id(myaddress) 
                << "\twith MAC " << (*index)->GetMac() 
                << "\ttype of application " << (*index)->Gettypeofapplication()
                << "\tValue of Max AMPDU " << (*index)->GetMaxSizeAmpdu()
                << std::endl;
    } else {
      std::cout //<< Simulator::Now () 
                << "\t\t\t\tSTA #" << (*index)->GetStaid()
                << "\tnot associated to any AP \t\t\t" 
                << "\ttype of application " << (*index)->Gettypeofapplication()
                << "\tValue of Max AMPDU " << (*index)->GetMaxSizeAmpdu()
                << std::endl;      
    }
  }
}

// This is called with a callback every time a STA is associated to an AP
void
STA_record::SetAssoc (std::string context, Mac48Address AP_MAC_address)
{
  // 'context' is something like "/NodeList/9/DeviceList/1/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/$ns3::StaWifiMac/Assoc"

  // update the data in the STA_record structure
  assoc = true;
  apMac = AP_MAC_address;

  // I have this info available in the STA record:
  //  staid
  //  typeofapplication
  //  staRecordMaxSizeAmpdu

  // auxiliar string
  std::ostringstream auxString;
  // create a string with the MAC
  auxString << "02-06-" << AP_MAC_address;
  std::string myaddress = auxString.str();

  uint8_t apChannel = GetAP_WirelessChannel ( GetAnAP_Id(myaddress), staRecordVerboseLevel );

  if (staRecordVerboseLevel > 0)
    std::cout << Simulator::Now () 
              << "\t[SetAssoc] STA #" << staid 
              << "\twith AMPDU size " << staRecordMaxSizeAmpdu 
              << "\trunning application " << typeofapplication 
              << "\thas associated to AP #" << GetAnAP_Id(myaddress)
              << " with MAC " << apMac  
              << " with channel " <<  uint16_t (apChannel)
              << "" << std::endl;

  // This part only runs if the aggregation algorithm is activated
  if (staRecordAggregationAlgorithm == 1) {
    // check if the STA associated to the AP is running VoIP. In this case, I have to disable aggregation:
    // - in the AP
    // - in all the associated STAs
    if ( typeofapplication == 1 || typeofapplication == 2 ) {

      // disable aggregation in the AP

      // check if the AP is aggregating
      if ( GetAP_MaxSizeAmpdu ( GetAnAP_Id(myaddress), staRecordVerboseLevel ) > 0 ) {

        // I modify the A-MPDU of this AP
        ModifyAmpdu ( GetAnAP_Id(myaddress), staRecordMaxAmpduSizeWhenAggregationDisabled, 1 );

        // Modify the data in the table of APs
        //for (AP_recordVector::const_iterator index = AP_vector.begin (); index != AP_vector.end (); index++) {
          //if ( (*index)->GetMac () == myaddress ) {
            Modify_AP_Record ( GetAnAP_Id(myaddress), myaddress, staRecordMaxAmpduSizeWhenAggregationDisabled);
            //std::cout << Simulator::Now () << "\t[GetAnAP_Id] AP #" << (*index)->GetApid() << " has MAC: " << (*index)->GetMac() << "" << std::endl;
        //  }
        //}

        if (staRecordVerboseLevel > 0)
          std::cout << Simulator::Now () 
                    << "\t[SetAssoc] Aggregation in AP #" << GetAnAP_Id(myaddress) 
                    << "\twith MAC: " << myaddress 
                    << "\tset to " << staRecordMaxAmpduSizeWhenAggregationDisabled 
                    << "\t(disabled)" << std::endl;

        // disable aggregation in all the STAs associated to that AP
        for (STA_recordVector::const_iterator index = assoc_vector.begin (); index != assoc_vector.end (); index++) {

          // I only have to disable aggregation for TCP STAs
          if ((*index)->Gettypeofapplication () > 2) {

            // if the STA is associated
            if ((*index)->GetAssoc ()) {

              // if the STA is associated to this AP
              if ((*index)->GetMac() == AP_MAC_address ) {

                ModifyAmpdu ((*index)->GetStaid(), staRecordMaxAmpduSizeWhenAggregationDisabled, 1);   // modify the AMPDU in the STA node
                (*index)->SetMaxSizeAmpdu(staRecordMaxAmpduSizeWhenAggregationDisabled);               // update the data in the STA_record structure

                if (staRecordVerboseLevel > 0)
                  std::cout << Simulator::Now () 
                            << "\t[SetAssoc] Aggregation in STA #" << (*index)->GetStaid() 
                            << ", associated to AP #" << GetAnAP_Id(myaddress) 
                            << "\twith MAC " << (*index)->GetMac() 
                            << "\tset to " << staRecordMaxAmpduSizeWhenAggregationDisabled 
                            << "\t(disabled)" << std::endl;
              }
            }
          }
        }
      }

    // If this associated STA is using TCP
    } else {

      // If the new AP is not aggregating
      if ( GetAP_MaxSizeAmpdu ( GetAnAP_Id(myaddress), staRecordVerboseLevel ) == 0) {

        // Disable aggregation in this STA
        ModifyAmpdu (staid, staRecordMaxAmpduSizeWhenAggregationDisabled, 1);  // modify the AMPDU in the STA node
        staRecordMaxSizeAmpdu = staRecordMaxAmpduSizeWhenAggregationDisabled;        // update the data in the STA_record structure

        if (staRecordVerboseLevel > 0)
          std::cout << Simulator::Now () 
                    << "\t[SetAssoc] Aggregation in STA #" << staid 
                    << ", associated to AP #" << GetAnAP_Id(myaddress) 
                    << "\twith MAC " << apMac
                    << "\tset to " << staRecordMaxSizeAmpdu 
                    << "\t(disabled)" << std::endl;

  /*      for (STA_recordVector::const_iterator index = assoc_vector.begin (); index != assoc_vector.end (); index++) {

          if ( (*index)->GetMac () == AP_MAC_address ) {
              ModifyAmpdu ((*index)->GetStaid(), 0, 1);  // modify the AMPDU in the STA node
              (*index)->SetMaxSizeAmpdu(0);// update the data in the STA_record structure

              if (staRecordVerboseLevel > 0)
                std::cout << Simulator::Now () 
                          << "\t[SetAssoc] Aggregation in STA #" << (*index)->GetStaid() 
                          << ", associated to AP #" << GetAnAP_Id(myaddress) 
                          << "\twith MAC " << (*index)->GetMac() 
                          << "\tset to " << 0 
                          << "\t(disabled)" << std::endl;
          }
        }*/

      // If the new AP is aggregating, I have to enable aggregation in this STA
     
      } else {
        // Enable aggregation in this STA
        ModifyAmpdu (staid, staRecordMaxAmpduSize, 1);  // modify the AMPDU in the STA node
        staRecordMaxSizeAmpdu = staRecordMaxAmpduSize;        // update the data in the STA_record structure

        if (staRecordVerboseLevel > 0)
          std::cout << Simulator::Now () 
                    << "\t[SetAssoc] Aggregation in STA #" << staid 
                    << ", associated to AP #" << GetAnAP_Id(myaddress) 
                    << "\twith MAC " << apMac
                    << "\tset to " << staRecordMaxSizeAmpdu 
                    << "(enabled)" << std::endl;
  /*      // Enable aggregation in the STA
          for (STA_recordVector::const_iterator index = assoc_vector.begin (); index != assoc_vector.end (); index++) {
            if ( (*index)->GetMac () == AP_MAC_address ) {
              ModifyAmpdu ((*index)->GetStaid(), maxAmpduSize, 1);  // modify the AMPDU in the STA node
              (*index)->SetMaxSizeAmpdu(maxAmpduSize);// update the data in the STA_record structure

              if (myverbose > 0)
                std::cout << Simulator::Now () 
                          << "\t[SetAssoc] Aggregation in STA #" << (*index)->GetStaid() 
                          << ", associated to AP #" << GetAnAP_Id(myaddress) 
                          << "\twith MAC " << (*index)->GetMac() 
                          << "\tset to " << maxAmpduSize 
                          << "\t(enabled)" << std::endl;
          }
        }*/
      }
    }
  }
  if (staRecordVerboseLevel > 0) {
    List_STA_record ();
    ListAPs (staRecordVerboseLevel);
  }
}

// This is called with a callback every time a STA is de-associated from an AP
void
STA_record::UnsetAssoc (std::string context, Mac48Address AP_MAC_address)
{
  // 'context' is something like "/NodeList/9/DeviceList/1/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/$ns3::StaWifiMac/Assoc"

  // update the data in the STA_record structure
  assoc = false;
  apMac = "00:00:00:00:00:00";
   
  // auxiliar string
  std::ostringstream auxString;
  // create a string with the MAC
  auxString << "02-06-" << AP_MAC_address;
  std::string myaddress = auxString.str();

  uint8_t apChannel = GetAP_WirelessChannel ( GetAnAP_Id(myaddress), staRecordVerboseLevel );

  if (staRecordVerboseLevel > 0)
    std::cout << Simulator::Now () 
              << "\t[UnsetAssoc] STA #" << staid
              << "\twith AMPDU size " << staRecordMaxSizeAmpdu               
              << "\trunning application " << typeofapplication 
              << "\tde-associated from AP #" << GetAnAP_Id(myaddress)
              << " with MAC " << AP_MAC_address 
              << " with channel " <<  uint16_t (apChannel)
              << "" << std::endl;

  // This only runs if the aggregation algorithm is running
  if(staRecordAggregationAlgorithm == 1) {

    // check if there is some VoIP STA already associated to the AP. In this case, I have to enable aggregation:
    // - in the AP
    // - in all the associated STAs
    if ( typeofapplication == 1 || typeofapplication == 2 ) {

      // check if the AP is not aggregating
      /*if ( GetAP_MaxSizeAmpdu ( GetAnAP_Id(myaddress), staRecordVerboseLevel ) == 0 ) {
        if (staRecordVerboseLevel > 0)
          std::cout << Simulator::Now () 
                    << "\t[UnsetAssoc] This AP is not aggregating" 
                    << std::endl;*/

        // check if there is no STA running VoIP associated
        bool anyStaWithVoIPAssociated = false;

        // Check all the associated STAs, except the one de-associating
        for (STA_recordVector::const_iterator index = assoc_vector.begin (); index != assoc_vector.end (); index++) {

          // Only consider STAs associated to this AP
          if ( (*index)->GetMac () == AP_MAC_address ) {

            // Only consider VoIP STAs
            if ( ( (*index)->Gettypeofapplication() == 1 ) || ( (*index)->Gettypeofapplication() == 2 ) ) {

              // It cannot be the one being de-associated
              if ( (*index)->GetStaid() != staid)

                anyStaWithVoIPAssociated = true;
            }
          }
        }

        // If there is no remaining STA running VoIP associated
        if ( anyStaWithVoIPAssociated == false ) {
          // enable aggregation in the AP
          // Modify the A-MPDU of this AP
          ModifyAmpdu (GetAnAP_Id(myaddress), staRecordMaxAmpduSize, 1);
          Modify_AP_Record (GetAnAP_Id(myaddress), myaddress, staRecordMaxAmpduSize);

          if (staRecordVerboseLevel > 0)
            std::cout << Simulator::Now () 
                      << "\t[UnsetAssoc]\tAggregation in AP #" << GetAnAP_Id(myaddress) 
                      << "\twith MAC: " << myaddress 
                      << "\tset to " << staRecordMaxAmpduSize 
                      << "\t(enabled)" << std::endl;

          // enable aggregation in all the STAs associated to that AP
          for (STA_recordVector::const_iterator index = assoc_vector.begin (); index != assoc_vector.end (); index++) {

            // if the STA is associated
            if ((*index)->GetAssoc()) {

              // if the STA is associated to this AP
              if ( (*index)->GetMac() == AP_MAC_address ) {

                // if the STA is not running VoIP. NOT NEEDED. IF I AM HERE IT MEANS THAT ALL THE STAs ARE TCP
                //if ((*index)->Gettypeofapplication () > 2) {

                  ModifyAmpdu ((*index)->GetStaid(), staRecordMaxAmpduSize, 1);  // modify the AMPDU in the STA node
                  (*index)->SetMaxSizeAmpdu(staRecordMaxAmpduSize);// update the data in the STA_record structure

                  if (staRecordVerboseLevel > 0)  
                    std::cout << Simulator::Now () 
                              << "\t[UnsetAssoc] Aggregation in STA #" << (*index)->GetStaid() 
                              << "\tassociated to AP #" << GetAnAP_Id(myaddress) 
                              << "\twith MAC " << (*index)->GetMac() 
                              << "\tset to " << staRecordMaxAmpduSize 
                              << "\t(enabled)" << std::endl;
                //}
              }
            }
          }

        // there is still some VoIP STA associatedm so aggregation cannot be enabled
        } else {
          if (staRecordVerboseLevel > 0)
            std::cout << Simulator::Now () 
                      << "\t[UnsetAssoc] There is still at least a VoIP STA in this AP " << GetAnAP_Id(myaddress) 
                      << " so aggregation cannot be enabled" << std::endl;
        }
      //}

    // If the STA de-associated is using TCP
    } else {

      // If the AP is not aggregating
      if ( GetAP_MaxSizeAmpdu ( GetAnAP_Id(myaddress), staRecordVerboseLevel ) == staRecordMaxAmpduSizeWhenAggregationDisabled) {

        // Enable aggregation in this STA
        ModifyAmpdu (staid, staRecordMaxAmpduSize, 1);  // modify the AMPDU in the STA node
        staRecordMaxSizeAmpdu = staRecordMaxAmpduSize;  // update the data in the STA_record structure

        if (staRecordVerboseLevel > 0)
          std::cout << Simulator::Now () 
                    << "\t[UnsetAssoc] Aggregation in STA #" << staid 
                    << ", de-associated from AP #" << GetAnAP_Id(myaddress) 
                    << "\twith MAC " << apMac
                    << "\tset to " << staRecordMaxSizeAmpdu 
                    << "\t(enabled)" << std::endl;

  /*    // Enable aggregation in the STA

      for (STA_recordVector::const_iterator index = assoc_vector.begin (); index != assoc_vector.end (); index++) {

        // if the STA is associated to this AP
        if ( (*index)->GetMac() == AP_MAC_address ) {

          // if the STA is not running VoIP
          if ((*index)->Gettypeofapplication () > 2) {

            ModifyAmpdu ((*index)->GetStaid(), maxAmpduSize, 1);  // modify the AMPDU in the STA node
            (*index)->SetMaxSizeAmpdu(maxAmpduSize);// update the data in the STA_record structure

            if (staRecordVerboseLevel > 0)
              std::cout << Simulator::Now () 
                        << "\t[UnsetAssoc] Aggregation in STA #" << (*index)->GetStaid() 
                        << ", de-associated from AP #" << GetAnAP_Id(myaddress) 
                        << "\twith MAC " << (*index)->GetMac() 
                        << "\tset to " << maxAmpduSize 
                        << "\t(enabled)" << std::endl; 
          }
        }*/      
      }
    }
  }
  if (staRecordVerboseLevel > 0) {
    List_STA_record ();
    ListAPs (staRecordVerboseLevel);
  }

/*
  // If wifiModel==1, I don't need to manually change the channel. It will do it automatically
  // If wifiModel==0, I have to manually set the channel of the STA to that of the nearest AP
  if (staRecordwifiModel == 0) {  // staRecordwifiModel is the local version of the variable wifiModel
*/
    // Put the STA in the channel of the nearest AP
    if (staRecordNumChannels > 1) {
      // Only for wifiModel = 0. With WifiModel = 1 it is supposed to scan for other APs in other channels 
      //if (staRecordwifiModel == 0) {
        // Put all the APs in a nodecontainer
        // and get a pointer to the STA
        Ptr<Node> mySTA;
        NodeContainer APs;
        uint32_t numberAPs = CountAPs (staRecordVerboseLevel);

        for (NodeList::Iterator i = NodeList::Begin(); i != NodeList::End(); ++i) {
          uint32_t identif;
          identif = (*i)->GetId();
       
          if ( (identif >= 0) && (identif < numberAPs) ) {
            APs.Add(*i);
          } else if ( identif == staid) {
            mySTA = (*i);
          }
        }

        // Find the nearest AP
        Ptr<Node> nearest;
        nearest = nearestAp (APs, mySTA, staRecordVerboseLevel);

        // Move this STA to the channel of the AP identified as the nearest one
        NetDeviceContainer thisDevice;
        thisDevice.Add( (mySTA)->GetDevice(1) ); // this adds the device to the NetDeviceContainer. It has to be device 1, not device 0. I don't know why
     
        uint8_t newChannel = GetAP_WirelessChannel ( (nearest)->GetId(), staRecordVerboseLevel );

        ChangeFrequencyLocal (thisDevice, newChannel, staRecordwifiModel, staRecordVerboseLevel);

        if (staRecordVerboseLevel > 0)
          std::cout << Simulator::Now () 
                    << "\t[UnsetAssoc] STA #" << staid 
                    << " de-associated from AP #" << GetAnAP_Id(myaddress) 
                    << ". Channel set to " << uint16_t (newChannel) 
                    << ", i.e. the channel of the nearest AP (AP #" << (nearest)->GetId()
                    << ")" << std::endl << std::endl;

      //}
    } else { // numChannels == 1
      if (staRecordVerboseLevel > 0)
        std::cout << Simulator::Now () 
                  << "\t[UnsetAssoc] STA #" << staid 
                  << " de-associated from AP #" << GetAnAP_Id(myaddress) 
                  << "\tnot modified because numChannels=" << staRecordNumChannels 
                  << "\tchannel is still " << uint16_t (apChannel) 
                  << std::endl << std::endl;
    }
/*  } else { // wifiModel = 1
    if (staRecordVerboseLevel > 0)
      std::cout << Simulator::Now () 
                  << "\t[UnsetAssoc] STA #" << staid 
                  << " de-associated from AP #" << GetAnAP_Id(myaddress) 
                  << "\tnot modified because wifimodel=" << staRecordwifiModel
                  << "\tchannel is still " << uint16_t (apChannel) 
                  << std::endl << std::endl;
  }*/
}


void
STA_record::Settypeofapplication (uint32_t applicationid)
{
  typeofapplication = applicationid;
}

void
STA_record::SetMaxSizeAmpdu (uint32_t MaxSizeAmpdu)
{
  staRecordMaxSizeAmpdu = MaxSizeAmpdu;
}

void
STA_record::SetVerboseLevel (uint32_t myVerboseLevel)
{
  staRecordVerboseLevel = myVerboseLevel;
}

void
STA_record::SetnumChannels (uint32_t mynumChannels)
{
  staRecordNumChannels = mynumChannels;
}

void
STA_record::Setversion80211 (uint32_t myversion80211)
{
  staRecordVersion80211 = myversion80211;
}

void
STA_record::SetaggregationAlgorithm (uint32_t myaggregationAlgorithm)
{
  staRecordAggregationAlgorithm = myaggregationAlgorithm;
}

void
STA_record::SetAmpduSize (uint32_t myAmpduSize)
{
  staRecordMaxAmpduSize = myAmpduSize;
}

void
STA_record::SetmaxAmpduSizeWhenAggregationDisabled (uint32_t mymaxAmpduSizeWhenAggregationDisabled)
{
  staRecordMaxAmpduSizeWhenAggregationDisabled = mymaxAmpduSizeWhenAggregationDisabled;
}

void
STA_record::SetWifiModel (uint32_t mywifiModel)
{
  staRecordwifiModel = mywifiModel;
}

bool
STA_record::GetAssoc ()
// returns true or false depending whether the STA is associated or not
{
  return assoc;
}

uint16_t
STA_record::GetStaid ()
// returns the id of the Sta
{
  return staid;
}

Mac48Address
STA_record::GetMac ()
{
  return apMac;
}

uint32_t
STA_record::Gettypeofapplication ()
// returns the id of the Sta
{
  return typeofapplication;
}

uint32_t
STA_record::GetMaxSizeAmpdu ()
// returns the id of the Sta
{
  return staRecordMaxSizeAmpdu;
}

uint32_t
Get_STA_record_num_AP_app (Mac48Address apMac, uint32_t typeofapplication)
// counts the number or STAs associated to an AP, with a type of application
{
  uint32_t AssocNum = 0;
  for (STA_recordVector::const_iterator index = assoc_vector.begin (); index != assoc_vector.end (); index++) {
    if (((*index)->GetAssoc ()) && ((*index)->GetMac() == apMac) && ( (*index)->Gettypeofapplication()==typeofapplication) ) {
      AssocNum++;
    }
  }
  return AssocNum;
}

/* I don't need this function
uint32_t
GetstaRecordMaxSizeAmpdu (uint16_t thisSTAid, uint32_t myverbose)
// returns the max size of the Ampdu of an AP
{
  uint32_t staRecordMaxSizeAmpdu = 0;
  //std::cout << Simulator::Now () << " *** Number of STA associated: " << Get_STA_record_num() << " *****" << std::endl;

  for (STA_recordVector::const_iterator index = assoc_vector.begin (); index != assoc_vector.end (); index++) {
  //std::cout << Simulator::Now () << " ********************** AP with ID " << (*index)->GetApid() << " has MAC: " << (*index)->GetMac() << " *****" << std::endl;

    if ( (*index)->GetStaid () == thisSTAid ) {
      staRecordMaxSizeAmpdu = (*index)->GetMaxSizeAmpdu ();
      if ( myverbose > 0 )
        std::cout << Simulator::Now () 
                  << "\t[GetstaRecordMaxSizeAmpdu]\tAP #" << (*index)->GetMac() 
                  << " has AMDPU: " << (*index)->GetMaxSizeAmpdu() 
                  << "" << std::endl;
    }
  }
  return staRecordMaxSizeAmpdu;
}
*/




int main (int argc, char *argv[]) {

  //bool populatearpcache = false; // Provisional variable FIXME: It should not be necessary

  // Variables to store some fixed parameters
  static uint32_t VoIPg729PayoladSize = 32; // Size of the UDP payload (also includes the RTP header) of a G729a packet with 2 samples
  static double VoIPg729IPT = 0.02; // Time between g729a packets (50 pps)

  static uint32_t initial_port = 1000; // port to be used by the VoIP uplink application. Subsequent ones will be used by the other applications
  static uint32_t initial_time_interval = 1.0; // time before the applications start (seconds). The same amount of time is added at the end

  static double x_position_first_AP = 0.0;
  static double y_position_first_AP = 0.0;

  static double x_distance_STA_to_AP = 0.0; // initial X distance from the first STA to the first AP
  static double y_distance_STA_to_AP = 5.0; // initial Y distance from the first STA to the first AP

  static double pause_time = 2.0;           // maximum pause time for the random waypoint mobility model


  // Variables to store the input parameters. I add the default value
  uint32_t number_of_APs = 4;
  uint32_t number_of_APs_per_row = 2;
  double distance_between_APs = 50.0; // X-axis and Y-axis distance between APs (meters)
  uint32_t number_of_STAs_per_row = number_of_APs_per_row;
  double distance_between_STAs = distance_between_APs;
  double distanceToBorder = 0.6 * distance_between_APs; // It is used for establishing the coordinates of the square where the STA move randomly

  uint32_t nodeMobility = 0;
  double constantSpeed = 1.5;  // X-axis speed (m/s) in the case the constant speed model is used (https://en.wikipedia.org/wiki/Preferred_walking_speed)

  double rateAPsWithAMPDUenabled = 1.0; // rate of APs with A-MPDU enabled at the beginning of the simulation

  uint32_t aggregationAlgorithm = 1;  // Set this to 1 in order to make the central control algorithm run
  uint32_t maxAmpduSizeWhenAggregationDisabled = 0;  // Only for TCP. Minimum size (to be used when aggregation is 'disabled')

  uint16_t topology = 1;    // 0: all the server applications are in a single server
                            // 1: each server application is in a node connected to the hub
                            // 2: each server application is in a node behind the router, connected to it with a P2P connection

  uint32_t TcpPayloadSize = 1448; //bytes. Prevent fragmentation. Taken from https://www.nsnam.org/doxygen/codel-vs-pfifo-asymmetric_8cc_source.html
  std::string TcpVariant = "TcpNewReno"; // other options "TcpHighSpeed", "TcpWestwoodPlus"

  double simulationTime = 10.0; //seconds

  uint32_t numberVoIPupload = 0;
  uint32_t numberVoIPdownload = 0;
  uint32_t numberTCPupload = 0;
  uint32_t numberTCPdownload = 0;

  //Using different priorities for VoIP and TCP
  //https://groups.google.com/forum/#!topic/ns-3-users/J3BvzGVJhXM
  //https://groups.google.com/forum/#!topic/ns-3-users/n8h8VbIekoQ
  //http://code.nsnam.org/ns-3-dev/file/06676d0e299f/src/wifi/doc/source/wifi-user.rst
  // the selection of the Access Category (AC) for an MSDU is based on the
  // value of the DS field in the IP header of the packet (ToS field in case of IPv4).
  // You can see the values in WireShark:
  //   IEEE 802.11 QoS data
  //     QoS Control
  uint32_t prioritiesEnabled = 0;
  uint8_t VoIpPriorityLevel = 0xc0;
  uint8_t TcpPriorityLevel = 0x00;

  uint32_t RtsCtsThreshold = 999999;  // RTS/CTS is disabled by defalult

  double powerLevel = 30.0;  // in dBm

  std::string rateModel = "Ideal"; // Model for 802.11 rate control (Constant; Ideal; Minstrel)

  bool writeMobility = false;
  bool enablePcap = 0; // set this to 1 and .pcap files will be generated (in the ns-3.26 folder)
  uint32_t verboseLevel = 0; // verbose level.
  uint32_t printSeconds = 0; // print the time every 'printSeconds' simulation seconds
  uint32_t generateHistograms = 0; // generate histograms
  std::string outputFileName; // the beginning of the name of the output files to be generated during the simulations
  std::string outputFileSurname; // this will be added to certain files
  bool saveXMLFile = false; // save per-flow results in an XML file

  uint32_t numChannels = 4; // by default, 4 different channels are used in the APs

  uint32_t channelWidth = 20;

  // https://www.nsnam.org/doxygen/wifi-spectrum-per-example_8cc_source.html
  uint32_t wifiModel = 0;

  uint32_t version80211 = 0; // 0 means 802.11n; 1 means 802.11ac

  uint32_t propagationLossModel = 0; // 0: LogDistancePropagationLossModel (default); 1: FriisPropagationLossModel; 2: FriisSpectrumPropagationLossModel

  uint32_t errorRateModel = 0; // 0 means NistErrorRateModel (default); 1 means YansErrorRateModel

  uint32_t maxAmpduSize;     // taken from https://www.nsnam.org/doxygen/minstrel-ht-wifi-manager-example_8cc_source.html

  // Assign the selected value of the MAX AMPDU
  if (version80211 == 0) {
    maxAmpduSize = MAXSIZE80211n;
  } else {
    maxAmpduSize = MAXSIZE80211ac;
  }

  // declaring the command line parser (input parameters)
  CommandLine cmd;
  cmd.AddValue ("number_of_APs", "Number of wifi APs", number_of_APs);
  cmd.AddValue ("number_of_APs_per_row", "Number of wifi APs per row", number_of_APs_per_row);
  cmd.AddValue ("distance_between_APs", "Distance in meters between the APs", distance_between_APs);
  cmd.AddValue ("distanceToBorder", "Distance in meters between the AP and the border of the scenario", distanceToBorder);

  cmd.AddValue ("number_of_STAs_per_row", "Number of wifi STAs per row", number_of_STAs_per_row);
  cmd.AddValue ("distance_between_STAs", "Initial distance in meters between the STAs (only for static and linear mobility)", distance_between_STAs);

  cmd.AddValue ("nodeMobility", "Kind of movement of the nodes (0 static; 1 linear; 2 Random Walk 2d; 3 Random Waypoint)", nodeMobility);
  cmd.AddValue ("constantSpeed", "Speed of the nodes (in linear and random mobility), default 1.5 m/s", constantSpeed);

  cmd.AddValue ("rateAPsWithAMPDUenabled", "Initial rate of APs with AMPDU aggregation enabled", rateAPsWithAMPDUenabled);
  cmd.AddValue ("aggregationAlgorithm", "Is the algorithm controlling AMPDU aggregation enabled?", aggregationAlgorithm);
  cmd.AddValue ("maxAmpduSize", "Maximum value of the AMPDU (bytes)", maxAmpduSize);
  cmd.AddValue ("maxAmpduSizeWhenAggregationDisabled", "Max AMPDU size to use when aggregation is disabled", maxAmpduSizeWhenAggregationDisabled);

  cmd.AddValue ("TcpPayloadSize", "Payload size in bytes", TcpPayloadSize);
  cmd.AddValue ("TcpVariant", "TCP variant: TcpNewReno (default), TcpHighSpped, TcpWestwoodPlus", TcpVariant);

  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);

  cmd.AddValue ("numberVoIPupload", "Number of nodes running VoIP up", numberVoIPupload);
  cmd.AddValue ("numberVoIPdownload", "Number of nodes running VoIP down", numberVoIPdownload);
  cmd.AddValue ("numberTCPupload", "Number of nodes running TCP up", numberTCPupload);
  cmd.AddValue ("numberTCPdownload", "Number of nodes running TCP down", numberTCPdownload);
  cmd.AddValue ("prioritiesEnabled", "Use different 802.11 priorities for VoIP / TCP (0: no, default; 1: yes)", prioritiesEnabled);

  cmd.AddValue ("version80211", "Version of 802.11 (0: 802.11n; 1: 802.11ac)", version80211);
  cmd.AddValue ("numChannels", "Number of different channels to use on the APs: 1, 4 (default), 9, 16", numChannels);
  cmd.AddValue ("channelWidth", "Width of the wireless channels: 20 (default), 40, 80, 160", channelWidth);
  cmd.AddValue ("wifiModel", "WiFi model: 0: YansWifiPhy (default); 1: SpectrumWifiPhy with MultiModelSpectrumChannel", wifiModel);
  // Path loss exponent in LogDistancePropagationLossModel is 3 and in Friis it is supposed to be lower maybe 2.
  cmd.AddValue ("propagationLossModel", "Propagation loss model: 0: LogDistancePropagationLossModel (default); 1: FriisPropagationLossModel; 2: FriisSpectrumPropagationLossModel (not working yet)", propagationLossModel);
  cmd.AddValue ("errorRateModel", "Error Rate model: 0: NistErrorRateModel (default); 1: YansErrorRateModel", errorRateModel);

  cmd.AddValue ("rateModel", "Model for 802.11 rate control (Constant; Ideal; Minstrel)", rateModel);  
//cmd.AddValue ("enableRtsCts", "Enable RTS/CTS? 0: no (default); 1: yes; 2: only for packets above 500 bytes", enableRtsCts);
  cmd.AddValue ("RtsCtsThreshold", "Threshold for using RTS/CTS (bytes). Examples. 0: always; 500: only 500 bytes-packes or higher will require RTS/CTS; 999999: never (default)", RtsCtsThreshold);
  cmd.AddValue ("powerLevel", "Power level of the wireless interfaces (dBm, Default 30)", powerLevel);

  cmd.AddValue ("writeMobility", "Write mobility trace", writeMobility);
  cmd.AddValue ("enablePcap", "Enable/disable pcap file generation", enablePcap);
  cmd.AddValue ("verboseLevel", "Tell echo applications to log if true", verboseLevel);
  cmd.AddValue ("printSeconds", "Periodically print simulation time", printSeconds);
  cmd.AddValue ("generateHistograms", "Generate histograms?", generateHistograms);
  cmd.AddValue ("outputFileName", "First characters to be used in the name of the output files", outputFileName);
  cmd.AddValue ("outputFileSurname", "Other characters to be used in the name of the output files (not in the average one)", outputFileSurname);
  cmd.AddValue ("saveXMLFile", "Save per-flow results to an XML file?", saveXMLFile);

  cmd.AddValue ("topology", "Topology: (0: all server applications in a server; 1: all the servers connected to the hub (default); 2: all the servers behind a router)", topology);

  cmd.Parse (argc, argv);


  // Other variables
  uint32_t number_of_STAs = numberVoIPupload + numberVoIPdownload + numberTCPupload + numberTCPdownload;   // One STA runs each application
  double x_position_first_STA = x_position_first_AP + x_distance_STA_to_AP;
  double y_position_first_STA = y_position_first_AP + y_distance_STA_to_AP; // by default, the first STA is located some meters above the first AP
  uint32_t number_of_Servers = number_of_STAs;  // the number of servers is the same as the number of STAs. Each server attends a STA

  //the list of channels is here: https://www.nsnam.org/docs/models/html/wifi-user.html
  // see https://en.wikipedia.org/wiki/List_of_WLAN_channels
  uint8_t availableChannels20MHz[34] = {36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112,
                                        116, 120, 124, 128, 132, 136, 140, 144, 149, 153, 157, 
                                        161, 165, 169, 173, 184, 188, 192, 196, 8, 12, 16};

  uint8_t availableChannels40MHz[12] = {38, 46, 54, 62, 102, 110, 118, 126, 134, 142, 151, 159}; 

  uint8_t availableChannels80MHz[6] = {42, 58, 106, 122, 138, 155}; 

  uint8_t availableChannels160MHz[2] = {50, 114};

  uint32_t i, j;  //FIXME: remove these variables and declare them when needed


  // One server interacts with each STA
  if (topology == 0) {
    number_of_Servers = 0;
  } else {
    number_of_Servers = number_of_STAs;
  }


  // Test some conditions before starting
  if (version80211 == 0) {
    if ( maxAmpduSize > MAXSIZE80211n ) {
      std::cout << "INPUT PARAMETER ERROR: Too high AMPDU size. Limit: " << MAXSIZE80211n <<". Stopping the simulation." << '\n';
      return 0;      
    }
  } else {
    if ( maxAmpduSize > MAXSIZE80211ac ) {
      std::cout << "INPUT PARAMETER ERROR: Too high AMPDU size. Limit: " << MAXSIZE80211ac <<". Stopping the simulation." << '\n';
      return 0;  
    }
  }

  if ( maxAmpduSizeWhenAggregationDisabled > maxAmpduSize ) {
      std::cout << "INPUT PARAMETER ERROR: The Max AMPDU size to use when aggregation is disabled (" << maxAmpduSizeWhenAggregationDisabled << ") has to be smaller or equal than the Max AMPDU size (" << maxAmpduSize << "). Stopping the simulation." << '\n';      
      return 0;        
  }

  if ((rateModel != "Constant") && (rateModel != "Ideal") && (rateModel != "Minstrel")) {
    std::cout << "INPUT PARAMETER ERROR: The wifi rate model MUST be 'Constant', 'Ideal' or 'Minstrel'. Stopping the simulation." << '\n';
    return 0;
  }

  if (number_of_APs % number_of_APs_per_row != 0) {
    std::cout << "INPUT PARAMETER ERROR: The number of APs MUST be a multiple of the number of APs per row. Stopping the simulation." << '\n';
    return 0;
  }

  if ((nodeMobility ==0) || (nodeMobility == 1)) {
    if (number_of_APs % number_of_APs_per_row != 0) {
      std::cout << "INPUT PARAMETER ERROR: With static and linear mobility, the number of STAs MUST be a multiple of the number of STAs per row. Stopping the simulation." << '\n';
      return 0;
    }
  }

  if ((aggregationAlgorithm == 1 ) && (rateAPsWithAMPDUenabled < 1.0 )) {
    std::cout << "INPUT PARAMETER ERROR: The algorithm has to start with all the APs with A-MPDU enabled (--rateAPsWithAMPDUenabled=1.0). Stopping the simulation." << '\n';
    return 0;
  }

  // check if the channel width is correct
  if ((channelWidth != 20) && (channelWidth != 40) && (channelWidth != 80) && (channelWidth != 160)) {
    std::cout << "INPUT PARAMETER ERROR: The witdth of the channels has to be 20, 40, 80 or 160. Stopping the simulation." << '\n';
    return 0;    
  }

  if ((channelWidth == 20) && (numChannels > 34) ) {
    std::cout << "INPUT PARAMETER ERROR: The maximum number of 20 MHz channels is 16. Stopping the simulation." << '\n';
    return 0;    
  }

  if ((channelWidth == 40) && (numChannels > 12) ) {
    std::cout << "INPUT PARAMETER ERROR: The maximum number of 40 MHz channels is 12. Stopping the simulation." << '\n';
    return 0;    
  }

  if ((channelWidth == 80) && (numChannels > 6) ) {
    std::cout << "INPUT PARAMETER ERROR: The maximum number of 80 MHz channels is 6. Stopping the simulation." << '\n';
    return 0;    
  }

  if ((channelWidth == 160) && (numChannels > 2) ) {
    std::cout << "INPUT PARAMETER ERROR: The maximum number of 160 MHz channels is 12. Stopping the simulation." << '\n';
    return 0;    
  }

  uint8_t availableChannels[numChannels];
  for (uint32_t i = 0; i < numChannels; ++i) {
    if (channelWidth == 20)
      availableChannels[i] = availableChannels20MHz[i];
    else if (channelWidth == 40)
      availableChannels[i] = availableChannels40MHz[i];
    else if (channelWidth == 80)
      availableChannels[i] = availableChannels80MHz[i];
    else if (channelWidth == 160)
      availableChannels[i] = availableChannels160MHz[i];
  }

  // Show the parameters by the screen
  if (verboseLevel > 0) {
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("SimpleMpduAggregation", LOG_LEVEL_INFO);

    // write the input parameters to the screen
    std::cout << "Number of APs: " << number_of_APs << '\n';    
    std::cout << "Number of APs per row: " << number_of_APs_per_row << '\n'; 
    std::cout << "Distance between APs: " << distance_between_APs << " meters" << '\n';
    std::cout << "Distance between the AP and the border of the scenario: " << distanceToBorder << " meters" << '\n';
    std::cout << "Total number of STAs: " << number_of_STAs << '\n'; 
    std::cout << "Number of STAs per row: " << number_of_STAs_per_row << '\n';
    std::cout << "Initial distance between STAs (only for static and linear mobility): " << distance_between_STAs << " meters" << '\n';
    std::cout << "Node mobility (0 static; 1 linear; 2 Random Walk 2d; 3 Random Waypoint): " << nodeMobility << '\n';
    std::cout << "Speed of the nodes (in linear and random mobility): " << constantSpeed << " m/s"<< '\n';
    std::cout << "Topology (0: all server applications in a server; 1: all the servers connected to the hub; 2: all the servers behind a router): " << topology << '\n';
    std::cout << '\n';
    std::cout << "Initial rate of APs with AMPDU aggregation enabled: " << rateAPsWithAMPDUenabled << '\n';
    std::cout << "Is the algorithm controlling AMPDU aggregation enabled?: " << aggregationAlgorithm << '\n';
    std::cout << "Maximum value of the AMPDU size: " << maxAmpduSize << " bytes" << '\n';
    std::cout << "Maximum value of the AMPDU size when aggregation is disabled: " << maxAmpduSizeWhenAggregationDisabled << " bytes" << '\n';
    std::cout << '\n'; 
    std::cout << "TCP Payload size: " << TcpPayloadSize << " bytes"  << '\n';
    std::cout << "TCP variant: " << TcpVariant << '\n';
    std::cout << "Simulation Time: " << simulationTime <<" sec" << '\n';
    std::cout << '\n'; 
    std::cout << "Number of nodes running VoIP up: " << numberVoIPupload << '\n';
    std::cout << "Number of nodes running VoIP down: " << numberVoIPdownload << '\n';
    std::cout << "Number of nodes running TCP up: " << numberTCPupload << '\n';
    std::cout << "Number of nodes running TCP down: " << numberTCPdownload << '\n';
    std::cout << "Use different 802.11 priorities for VoIP / TCP? (0: no; 1: yes): " << prioritiesEnabled << '\n';
    //std::cout << '\n'; 
    std::cout << "Version of 802.11 (0: 802.11n; 1: 802.11ac): " << version80211 << '\n';
    std::cout << "Number of different channels to use on the APs: " << numChannels << '\n';
    std::cout << "Channels being used: ";
    for (uint32_t i = 0; i < numChannels; ++i) {
      std::cout << uint16_t (availableChannels[i]) << " ";
    }
    std::cout << '\n'; 
    std::cout << "Width of the wireless channels: " << channelWidth << '\n';
    std::cout << "WiFi model (0: YansWifiPhy; 1: SpectrumWifiPhy with MultiModelSpectrumChannel): " << wifiModel << '\n';
    std::cout << "Propagation loss model: 0: LogDistancePropagationLossModel (default); 1: FriisPropagationLossModel; 2: FriisSpectrumPropagationLossModel: " << propagationLossModel << '\n';

    std::cout << "Error Rate model: 0: NistErrorRateModel; 1: YansErrorRateModel: " << errorRateModel << '\n';
    std::cout << '\n';
    std::cout << "Model for 802.11 rate control (Constant; Ideal; Minstrel): " << rateModel << '\n';  
    std::cout << "Threshold for using RTS/CTS (Examples. 0:always; 500:only 500 bytes-packes or higher will require RTS/CTS; 999999:never): " << RtsCtsThreshold << " bytes" << '\n';
    std::cout << "Power level of the wireless interfaces: " << powerLevel << " dBm" << '\n';
    std::cout << '\n';
    std::cout << "pcap generation enabled ?: " << enablePcap << '\n';
    std::cout << "verbose level: " << verboseLevel << '\n';
    std::cout << "Periodically print simulation time every " << printSeconds << " seconds" << '\n';    
    std::cout << "Generate histograms (delay, jitter, packet size): " << generateHistograms << '\n';
    std::cout << "First characters to be used in the name of the output file: " << outputFileName << '\n';
    std::cout << "Other characters to be used in the name of the output file (not in the average one): " << outputFileSurname << '\n';
    std::cout << "Save per-flow results to an XML file?: " << saveXMLFile << '\n';
    std::cout << '\n'; 
  }


//  if ( verboseLevel > 1 )
//    ArpCache.EnableLogComponents ();  // Turn on all Arp logging
//    LogComponentEnable("ArpCache", LOG_LEVEL_ALL);

  /******** create the node containers *********/
  NodeContainer apNodes;
  // apNodes have:
  //  - a csma device
  //  - a wifi device
  //  - a bridgeApDevices connecting them
  //  - I do not install the IP stack, as they do not need it

  // wireless STAs. They have a mobility pattern 
  NodeContainer staNodes;

  // server that interacts with the STAs
  NodeContainer singleServerNode;

  // servers that interact with the STAs
  NodeContainer serverNodes;

  // a router connecting to the servers' network
  NodeContainer routerNode;

  // a single csma hub that connects everything
  NodeContainer csmaHubNode;


  /******** create the nodes *********/
  // The order in which you create the nodes is important
  apNodes.Create (number_of_APs);

  staNodes.Create (number_of_STAs);

  if (topology == 0) {
    // create a single server
    singleServerNode.Create(1);

  } else {
    // create one server per STA
    serverNodes.Create (number_of_Servers);

    if (topology == 2)
      // in this case, I also need a router
      routerNode.Create(1);
  }

  // Create a hub
  // Inspired on https://www.nsnam.org/doxygen/csma-bridge_8cc_source.html
  csmaHubNode.Create (1);


  /************ Install Internet stack in the nodes ***************/
  InternetStackHelper stack;

  //stack.Install (apNodes); // I do not install it because they do not need it

  stack.Install (staNodes);

  if (topology == 0) {
    // single server
    stack.Install (singleServerNode);

  } else {
    // one server per STA
    stack.Install (serverNodes);

    if (topology == 2)
      // in this case, I also need a router
      stack.Install (routerNode);
  }



  /******** create the net device containers *********/
  NetDeviceContainer apCsmaDevices;
  std::vector<NetDeviceContainer> apWiFiDevices;
  std::vector<NetDeviceContainer> staDevices;
  NetDeviceContainer serverDevices;
  NetDeviceContainer singleServerDevices;
  NetDeviceContainer csmaHubDevices;                // each network card of the hub
  //NetDeviceContainer bridgeApDevices;             // A bridge container for the bridge of each AP node
  NetDeviceContainer routerDeviceToAps, routerDeviceToServers;


  /******* IP interfaces *********/
  //Ipv4InterfaceContainer apInterfaces;
  std::vector<Ipv4InterfaceContainer> staInterfaces;
  Ipv4InterfaceContainer serverInterfaces;
  Ipv4InterfaceContainer singleServerInterfaces;
  //Ipv4InterfaceContainer csmaHubInterfaces;
  Ipv4InterfaceContainer routerInterfaceToAps, routerInterfaceToServers;


  /********* IP addresses **********/
  Ipv4AddressHelper ipAddressesSegmentA;
  ipAddressesSegmentA.SetBase ("10.0.0.0", "255.255.0.0"); // If you use 255.255.255.0, you can only use 256 nodes

  Ipv4AddressHelper ipAddressesSegmentB;    // The servers are behind the router, so they are in other network
  ipAddressesSegmentB.SetBase ("10.1.0.0", "255.255.0.0");


  /******** mobility *******/
  MobilityHelper mobility;

  // mobility of the backbone nodes (i.e. APs and servers): constant position
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                "MinX", DoubleValue (x_position_first_AP),
                                "MinY", DoubleValue (y_position_first_AP),
                                "DeltaX", DoubleValue (distance_between_APs),
                                "DeltaY", DoubleValue (distance_between_APs),
                                "GridWidth", UintegerValue (number_of_APs_per_row), // size of the row
                                "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
//mobility.Install (backboneNodes);
  mobility.Install (apNodes);


  if (verboseLevel > 2) {
    for (uint32_t i = 0; i < number_of_APs; ++i) {
      //ReportPosition (backboneNodes.Get(i), i, 0, 1, apNodes); this would report the position every second
      //Vector pos = GetPosition (backboneNodes.Get (i));
      Vector pos = GetPosition (apNodes.Get (i));
      std::cout << "AP#" << i << " Position: " << pos.x << "," << pos.y << '\n';
    }
  }


  // Set the positions and the mobility of the STAs
  // Taken from https://www.nsnam.org/docs/tutorial/html/building-topologies.html#building-a-wireless-network-topology

  // STAs do not move
  if (nodeMobility == 0) {
    mobility.SetPositionAllocator ( "ns3::GridPositionAllocator",
                                    "MinX", DoubleValue (x_position_first_STA),
                                    "MinY", DoubleValue (y_position_first_STA),
                                    "DeltaX", DoubleValue (distance_between_STAs),
                                    "DeltaY", DoubleValue (distance_between_STAs),
                                    "GridWidth", UintegerValue (number_of_STAs_per_row),  // size of the row
                                    "LayoutType", StringValue ("RowFirst"));

    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (staNodes);

  // STAs linear mobility: constant speed to the right
  } else if (nodeMobility == 1) {
    mobility.SetPositionAllocator ( "ns3::GridPositionAllocator",
                                    "MinX", DoubleValue (x_position_first_STA),
                                    "MinY", DoubleValue (y_position_first_STA),
                                    "DeltaX", DoubleValue (distance_between_STAs),
                                    "DeltaY", DoubleValue (distance_between_STAs),
                                    "GridWidth", UintegerValue (number_of_STAs_per_row),  // size of the row
                                    "LayoutType", StringValue ("RowFirst"));

    Ptr<ConstantVelocityMobilityModel> mob;
    Vector m_velocity = Vector(constantSpeed, 0.0, 0.0);

    // https://www.nsnam.org/doxygen/classns3_1_1_constant_velocity_mobility_model.html#details
    mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
    mobility.Install (staNodes);

    for ( j = 0; j < staNodes.GetN(); ++j) {
      // https://www.nsnam.org/doxygen/classns3_1_1_constant_velocity_mobility_model.html#details
      mob = staNodes.Get(j)->GetObject<ConstantVelocityMobilityModel>();
      mob->SetVelocity(m_velocity);
    }

  // STAs random walk 2d mobility mobility
  } else if (nodeMobility == 2) { 

    // Each instance moves with a speed and direction choosen at random with the user-provided random variables until either a fixed distance has been walked or until a fixed amount of time. If we hit one of the boundaries (specified by a rectangle), of the model, we rebound on the boundary with a reflexive angle and speed. This model is often identified as a brownian motion model.

/*  // https://www.nsnam.org/doxygen/classns3_1_1_rectangle.html
    // Rectangle (double _xMin, double _xMax, double _yMin, double _yMax)
    mobility.SetMobilityModel ( "ns3::RandomWalk2dMobilityModel",
                                //"Mode", StringValue ("Time"),
                                //"Time", StringValue ("2s"),
                                //"Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"),
                                "Bounds", RectangleValue (Rectangle ( x_position_first_AP - distanceToBorder, 
                                                                      ((number_of_APs_per_row -1) * distance_between_APs) + distanceToBorder,
                                                                      y_position_first_AP - distanceToBorder,
                                                                      ((number_of_APs / number_of_APs_per_row) -1 ) * distance_between_APs + distanceToBorder)));
*/
/*  Config::SetDefault ("ns3::RandomWalk2dMobilityModel::Mode", StringValue ("Time"));
    Config::SetDefault ("ns3::RandomWalk2dMobilityModel::Time", StringValue ("2s"));
    Config::SetDefault ("ns3::RandomWalk2dMobilityModel::Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"));
    Config::SetDefault ("ns3::RandomWalk2dMobilityModel::Bounds", StringValue ("0|200|0|200"));
*/

    // auxiliar string
    std::ostringstream auxString;

    // create a string with the X boundaries
    auxString << "ns3::UniformRandomVariable[Min=" << x_position_first_AP - distanceToBorder 
              << "|Max=" << ((number_of_APs_per_row -1) * distance_between_APs) + distanceToBorder << "]"; 
    std::string XString = auxString.str();
    if ( verboseLevel > 1 )
      std::cout << "Limits for X: " << XString << '\n';

    // clean the string
    auxString.str(std::string());

    // create a string with the Y boundaries
    auxString   << "ns3::UniformRandomVariable[Min=" << y_position_first_AP - distanceToBorder 
                << "|Max=" << ((number_of_APs / number_of_APs_per_row) -1 ) * distance_between_APs + distanceToBorder << "]"; 
    std::string YString = auxString.str();
    if ( verboseLevel > 1 )
      std::cout << "Limits for Y: " << YString << '\n';

    // Locate the STAs initially
    mobility.SetPositionAllocator ( "ns3::RandomRectanglePositionAllocator",
                                    "X", StringValue (XString),
                                    "Y", StringValue (YString));

    // clean the string
    auxString.str(std::string());

    // create a string with the time between speed modifications
    auxString << "5s";
    std::string timeString = auxString.str();
    if ( verboseLevel > 1 )
      std::cout << "The STAs will change their trajectory every " << timeString << '\n';

    // clean the string
    auxString.str(std::string());

    // create a string with the speed
    auxString  << "ns3::ConstantRandomVariable[Constant=" << constantSpeed << "]";
    std::string speedString = auxString.str();
    if ( verboseLevel > 1 )
      std::cout << "Speed with which the STAs move: " << speedString << '\n';

    // clean the string
    auxString.str(std::string());

    // create a string like this: "0|200|0|200"
    auxString << x_position_first_AP - distanceToBorder << "|" 
              << ((number_of_APs_per_row -1) * distance_between_APs) + distanceToBorder << "|"
              << y_position_first_AP - distanceToBorder << "|"
              << ((number_of_APs / number_of_APs_per_row) -1 ) * distance_between_APs + distanceToBorder;
    std::string boundsString = auxString.str();
    if ( verboseLevel > 1 )
      std::cout << "Rectangle where the STAs move: " << boundsString << '\n';

    mobility.SetMobilityModel ( "ns3::RandomWalk2dMobilityModel",
                                "Mode", StringValue ("Time"),
                                "Time", StringValue (timeString),   // the node will modify its speed every 'timeString' seconds
                                "Speed", StringValue (speedString), // the speed is always between [-speedString,speedString]
                                //"Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"),
                                "Bounds", StringValue (boundsString));
                                //"Bounds", StringValue ("-50|250|-50|250"));

    mobility.Install (staNodes);

  // STAs random waypoint model
  } else if (nodeMobility == 3) {
    // https://www.nsnam.org/doxygen/classns3_1_1_random_waypoint_mobility_model.html#details
    // Each object starts by pausing at time zero for the duration governed by the random variable 
    // "Pause". After pausing, the object will pick a new waypoint (via the PositionAllocator) and a 
    // new random speed via the random variable "Speed", and will begin moving towards the waypoint 
    // at a constant speed. When it reaches the destination, the process starts over (by pausing).

    // auxiliar string
    std::ostringstream auxString;

    // create a string with the X boundaries
    auxString << "ns3::UniformRandomVariable[Min="
              << x_position_first_AP - distanceToBorder 
              << "|Max=" << ((number_of_APs_per_row -1) * distance_between_APs) + distanceToBorder 
              << "]";

    std::string XString = auxString.str();

    if ( verboseLevel > 2 )
      std::cout << "Limits for X: " << XString << '\n';

    // clean the string
    auxString.str(std::string());

    // create a string with the Y boundaries
    auxString   << "ns3::UniformRandomVariable[Min=" << y_position_first_AP - distanceToBorder 
                << "|Max=" << ((number_of_APs / number_of_APs_per_row) -1 ) * distance_between_APs + distanceToBorder 
                << "]"; 

    std::string YString = auxString.str();

    if ( verboseLevel > 2 )
      std::cout << "Limits for Y: " << YString << '\n';


    ObjectFactory pos;
    pos.SetTypeId ( "ns3::RandomRectanglePositionAllocator");

    pos.Set ("X", StringValue (XString));
    pos.Set ("Y", StringValue (YString));

    // clean the string
    auxString.str(std::string());

    // create a string with the speed
    auxString  << "ns3::UniformRandomVariable[Min=0.0|Max=" << constantSpeed << "]";
    std::string speedString = auxString.str();
    if ( verboseLevel > 2 )
      std::cout << "Speed with which the STAs move: " << speedString << '\n';


    // clean the string
    auxString.str(std::string());

    // create a string with the pause time
    auxString  << "ns3::UniformRandomVariable[Min=0.0|Max=" << pause_time << "]";
    std::string pauseTimeString = auxString.str();
    if ( verboseLevel > 2 )
      std::cout << "The STAs will pause during " << pauseTimeString << '\n';

    Ptr<PositionAllocator> taPositionAlloc = pos.Create ()->GetObject<PositionAllocator> ();
    mobility.SetMobilityModel ( "ns3::RandomWaypointMobilityModel",
                                "Speed", StringValue (speedString),
                                //"Speed", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=10.0]"),
                                "Pause", StringValue (pauseTimeString),
                                //"Pause", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=10.0]"),
                                "PositionAllocator", PointerValue (taPositionAlloc));
    mobility.SetPositionAllocator (taPositionAlloc);
    mobility.Install (staNodes);
  }

/* 
  if (verboseLevel > 0)
    for ( j = 0; j < number_of_STAs; ++j) {
      Vector pos = GetPosition (staNodes.Get (j));
      std::cout << "STA#" << number_of_APs + number_of_Servers + j << " Position: " << pos.x << "," << pos.y << '\n';
    }
*/


  // Periodically report the positions of all the STAs
  if (verboseLevel > 2) {
    for ( j = 0; j < number_of_STAs; ++j) {
      Simulator::Schedule (Seconds (initial_time_interval), &ReportPosition, staNodes.Get(j), number_of_APs + number_of_Servers + j , 1, verboseLevel, apNodes);
    }

    // This makes a callback every time a STA changes its course
    // see trace sources in https://www.nsnam.org/doxygen/classns3_1_1_random_walk2d_mobility_model.html
    Config::Connect ( "/NodeList/*/$ns3::MobilityModel/CourseChange", MakeCallback (&CourseChange));
  }

  // Add an empty record per AP
  for (uint16_t i = 0; i < number_of_APs; i++ ){
    AP_record *m_AP_record = new AP_record;
    AP_vector.push_back(m_AP_record);
  }


  /******** create the channels (wifi, csma and point to point) *********/

  // create the wifi phy layer, using 802.11n in 5GHz

  // create the wifi channel

  // if wifiModel == 0
  // we use the "yans" model. See https://www.nsnam.org/doxygen/classns3_1_1_yans_wifi_channel_helper.html#details
  // it is described in this paper: http://cutebugs.net/files/wns2-yans.pdf
  // The yans name stands for "Yet Another Network Simulator"
  //YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();

  // if wifiModel == 1
  // we use the spectrumWifi model
  SpectrumWifiPhyHelper spectrumPhy = SpectrumWifiPhyHelper::Default ();

  if (wifiModel == 0) {

    wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
    YansWifiChannelHelper wifiChannel;

    // propagation models: https://www.nsnam.org/doxygen/group__propagation.html
    if (propagationLossModel == 0) {
      wifiChannel.AddPropagationLoss ("ns3::LogDistancePropagationLossModel");
    } else if (propagationLossModel == 1) {
      wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
    }

    wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");

    wifiPhy.SetChannel (wifiChannel.Create ());
    wifiPhy.Set ("TxPowerStart", DoubleValue (powerLevel)); // a value of '1' means 1 dBm (1.26 mW)
    wifiPhy.Set ("TxPowerEnd", DoubleValue (powerLevel));
    // Experiences:   at 5GHz,  with '-15' the coverage is less than 70 m
    //                          with '-10' the coverage is about 70 m (recommended for an array with distance 50m between APs)

    wifiPhy.Set ("ShortGuardEnabled", BooleanValue (false));
    wifiPhy.Set ("ChannelWidth", UintegerValue (channelWidth));
    //wifiPhy.Set ("ChannelNumber", UintegerValue(ChannelNo)); //This is done later

    wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);

    if (errorRateModel == 0) { // Nist
      wifiPhy.SetErrorRateModel ("ns3::NistErrorRateModel");
    } else { // errorRateModel == 1 (Yans)
      wifiPhy.SetErrorRateModel ("ns3::YansErrorRateModel");      
    }

/*     //FIXME: Can this be done with YANS?

    //Ptr<wifiPhy> myphy = node->GetObject<wifiPhy> ();
    if (numChannels > 1) 
      for (uint32_t k = 0; k < numChannels; k++)
        wifiPhy.AddOperationalChannel ( availableChannels[k] );
*/

  // wifiModel == 1
  } else {

    //Bug 2460: CcaMode1Threshold default should be set to -62 dBm when using Spectrum
    //Config::SetDefault ("ns3::WifiPhy::CcaMode1Threshold", DoubleValue (-62.0));

    // Use multimodel spectrum channel, https://www.nsnam.org/doxygen/classns3_1_1_multi_model_spectrum_channel.html
    Ptr<MultiModelSpectrumChannel> spectrumChannel = CreateObject<MultiModelSpectrumChannel> ();

    // propagation models: https://www.nsnam.org/doxygen/group__propagation.html
    if (propagationLossModel == 0) {
      //spectrumChannel.AddPropagationLoss ("ns3::LogDistancePropagationLossModel");
      Ptr<LogDistancePropagationLossModel> lossModel = CreateObject<LogDistancePropagationLossModel> ();
      spectrumChannel->AddPropagationLossModel (lossModel);
    } else if (propagationLossModel == 1) {
      //spectrumChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
      Ptr<FriisPropagationLossModel> lossModel = CreateObject<FriisPropagationLossModel> ();
      spectrumChannel->AddPropagationLossModel (lossModel);
    } else if (propagationLossModel == 2) {
      //spectrumChannel.AddPropagationLoss ("ns3::FriisSpectrumPropagationLossModel");
      Ptr<FriisSpectrumPropagationLossModel> lossModel = CreateObject<FriisSpectrumPropagationLossModel> ();
      spectrumChannel->AddSpectrumPropagationLossModel(lossModel);
    }

    // delay model
    Ptr<ConstantSpeedPropagationDelayModel> delayModel = CreateObject<ConstantSpeedPropagationDelayModel> ();
    spectrumChannel->SetPropagationDelayModel (delayModel);

    spectrumPhy.SetChannel (spectrumChannel);
    if (errorRateModel == 0) { //Nist
      spectrumPhy.SetErrorRateModel ("ns3::NistErrorRateModel");
    } else { // errorRateModel == 1 (Yans)
      spectrumPhy.SetErrorRateModel ("ns3::YansErrorRateModel");      
    }


    //spectrumPhy.Set ("Frequency", UintegerValue (5180));
    //spectrumPhy.Set ("ChannelNumber", UintegerValue (ChannelNo)); //This is done later
    spectrumPhy.Set ("TxPowerStart", DoubleValue (powerLevel));  // a value of '1' means 1 dBm (1.26 mW)
    spectrumPhy.Set ("TxPowerEnd", DoubleValue (powerLevel));

    spectrumPhy.Set ("ShortGuardEnabled", BooleanValue (false));
    spectrumPhy.Set ("ChannelWidth", UintegerValue (channelWidth));

    Ptr<SpectrumWifiPhy> m_phy;
    m_phy = CreateObject<SpectrumWifiPhy> ();

    // Test of the Addoperationalchannel functionality
    //(*m_phy).DoChannelSwitch (uint8_t(40) ); It is private
    // Add a channel number to the list of operational channels.
    // https://www.nsnam.org/doxygen/classns3_1_1_spectrum_wifi_phy.html#a948c6d197accf2028529a2842ec68816

    // https://groups.google.com/forum/#!topic/ns-3-users/Ih8Hgs2qgeg

/*    // This does not work, i.e. the STA does not scan in other channels
    if (numChannels > 1) 
      for (uint32_t k = 0; k < numChannels; k++)
        (*m_phy).AddOperationalChannel ( availableChannels[k] );*/
  }


  // Create a Wifi helper in an empty state: all its parameters must be set before calling ns3::WifiHelper::Install.
  // https://www.nsnam.org/doxygen/classns3_1_1_wifi_helper.html
  // The default state is defined as being an Adhoc MAC layer with an ARF rate control
  // algorithm and both objects using their default attribute values. By default, configure MAC and PHY for 802.11a.
  WifiHelper wifi;

  if ( verboseLevel > 3 )
    wifi.EnableLogComponents ();  // Turn on all Wifi logging

  // define the standard to follow (see https://www.nsnam.org/doxygen/group__wifi.html#ga1299834f4e1c615af3ca738033b76a49)
  if (version80211 == 0) {
    wifi.SetStandard (WIFI_PHY_STANDARD_80211n_5GHZ);
    //wifi.SetStandard (WIFI_PHY_STANDARD_80211n_2_4GHZ); // This is also valid
  } else {
    wifi.SetStandard (WIFI_PHY_STANDARD_80211ac);
  }


  // The SetRemoteStationManager method tells the helper the type of rate control algorithm to use.
  // constant_rate_wifi_manager uses always the same transmission rate for every packet sent.
  // https://www.nsnam.org/doxygen/classns3_1_1_constant_rate_wifi_manager.html#details
  //wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("HtMcs7"), "ControlMode", StringValue ("HtMcs0"));

  // Select the most appropriate wifimanager
  // ARF Rate control algorithm.
  // arf_wifi_manager implements the so-called ARF algorithm which was initially described in WaveLAN-II: A High-performance wireless LAN for the unlicensed band, by A. Kamerman and L. Monteban. in Bell Lab Technical Journal, pages 118-133, Summer 1997.
  // https://www.nsnam.org/doxygen/classns3_1_1_arf_wifi_manager.html
  // This RAA (Rate Adaptation Algorithm) does not support HT, VHT nor HE modes and will error exit if the user tries to configure 
  // this RAA with a Wi-Fi MAC that has VhtSupported, HtSupported or HeSupported set
  // wifi.SetRemoteStationManager ("ns3::ArfWifiManager");

  // The next line is not necessary, as it only defines the default WifiRemoteStationManager
  //Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", 
  //                    enableRtsCts ? StringValue ("0") : StringValue ("999999")); // if enableRtsCts is true, I select the first option


  // MinstrelHt and Ideal do support HT/VHT (i.e. 802.11n and above)
  if (rateModel == "Constant") {
    // more rates here https://www.nsnam.org/doxygen/wifi-spectrum-per-example_8cc_source.html
    wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                  "DataMode", StringValue ("HtMcs7"),
                                  "ControlMode", StringValue ("HtMcs0"),
                                  "RtsCtsThreshold", UintegerValue (RtsCtsThreshold));

/*  This is another option:
    wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                  "DataMode", DataRate,
                                  "ControlMode", DataRate,
                                  "RtsCtsThreshold", UintegerValue (ctsThr));
*/

  } else if (rateModel == "Ideal") {
    // Ideal Wifi Manager, https://www.nsnam.org/doxygen/classns3_1_1_ideal_wifi_manager.html#details
    wifi.SetRemoteStationManager ("ns3::IdealWifiManager",
                                  //"MaxSlrc", UintegerValue (7)  // 7 is the default value
                                  "RtsCtsThreshold", UintegerValue (RtsCtsThreshold));
/*  This is another option:
    wifi.SetRemoteStationManager ("ns3::IdealWifiManager",
                                  //"MaxSlrc", UintegerValue (7)  // 7 is the default value
                                  "RtsCtsThreshold", enableRtsCts ? UintegerValue (RtsCtsThreshold) : UintegerValue (999999)); // if enableRtsCts is true, I select the first option
*/

  } else if (rateModel == "Minstrel") {
    // I obtain some errors when running Minstrel
    // https://www.nsnam.org/bugzilla/show_bug.cgi?id=1797
    // https://www.nsnam.org/doxygen/classns3_1_1_minstrel_ht_wifi_manager.html
    wifi.SetRemoteStationManager ("ns3::MinstrelHtWifiManager",
                                  "RtsCtsThreshold", UintegerValue (RtsCtsThreshold),
                                  "PrintStats", BooleanValue (false)); // if you set this to true, you will obtain a file with the stats
  }


  // Create the MAC helper
  // https://www.nsnam.org/doxygen/classns3_1_1_wifi_mac_helper.html
  WifiMacHelper wifiMac;


  // connect the APs to the wifi
  for ( i = 0 ; i < number_of_APs ; ++i ) {

    // I use an auxiliary device container and an auxiliary interface container
    NetDeviceContainer apWiFiDev;

    // create an ssid for each wifi AP
    std::ostringstream oss;
    oss << "wifi-default-" << i; // Each AP will have a different SSID
    Ssid apssid = Ssid (oss.str ());

    // setup the APs. Install one wifiMac or another depending on a random variable
    Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable> ();
    
    if ( uv->GetValue () < rateAPsWithAMPDUenabled ) {
      // Enable AMPDU
      wifiMac.SetType ( "ns3::ApWifiMac",
                        "Ssid", SsidValue (apssid),
                        "QosSupported", BooleanValue (true), // FIXME: check if this is important
                        "BeaconGeneration", BooleanValue (true),  // Beacon generation is necessary in an AP
                        "BE_MaxAmpduSize", UintegerValue (maxAmpduSize),
                        "BK_MaxAmpduSize", UintegerValue (maxAmpduSize),
                        "VI_MaxAmpduSize", UintegerValue (maxAmpduSize),
                        "VO_MaxAmpduSize", UintegerValue (maxAmpduSize)); 

      if ( verboseLevel > 0 )
        std::cout << "AP     #" << i << "\tAMPDU enabled" << '\n';

    } else {
      // Disable AMPDU
      // - don't use aggregation (both A-MPDU and A-MSDU are disabled);
      wifiMac.SetType ( "ns3::ApWifiMac",
                        "Ssid", SsidValue (apssid),
                        "QosSupported", BooleanValue (true),    // FIXME: check if this is important
                        "BeaconGeneration", BooleanValue (true),  // Beacon generation is necessary in an AP
                        "BE_MaxAmpduSize", UintegerValue (0),     //Disable A-MPDU
                        "BK_MaxAmpduSize", UintegerValue (0),     //Disable A-MPDU
                        "VI_MaxAmpduSize", UintegerValue (0),     //Disable A-MPDU
                        "VO_MaxAmpduSize", UintegerValue (0));    //Disable A-MPDU

      if ( verboseLevel > 0 )
        std::cout << "AP     #" << i << "\tAMPDU disabled" << '\n';
    }

/*  Other options:
      // - enable A-MSDU (with maximum size of 8 kB) but disable A-MPDU;
      wifiMac.SetType ( "ns3::ApWifiMac",
                        "Ssid", SsidValue (apssid),
                        "BeaconGeneration", BooleanValue (true),
                        "BE_MaxAmpduSize", UintegerValue (0), //Disable A-MPDU
                        "BE_MaxAmsduSize", UintegerValue (7935)); //Enable A-MSDU with the highest maximum size allowed by the standard (7935 bytes)

      // - use two-level aggregation (A-MPDU with maximum size of 32 kB and A-MSDU with maximum size of 4 kB).
      wifiMac.SetType ( "ns3::ApWifiMac",
                        "Ssid", SsidValue (apssid),
                        "BeaconGeneration", BooleanValue (true),
                        "BE_MaxAmpduSize", UintegerValue (32768), //Enable A-MPDU with a smaller size than the default one
                        "BE_MaxAmsduSize", UintegerValue (3839)); //Enable A-MSDU with the smallest maximum size allowed by the standard (3839 bytes)
*/

    // install the wifi in the APs
    uint8_t ChannelNoForThisAP = availableChannels[0];

    // Use the available channels in turn
    ChannelNoForThisAP = availableChannels[i % numChannels];

    // Yans wifi
    if (wifiModel == 0) {    
      wifiPhy.Set ("ChannelNumber", UintegerValue(ChannelNoForThisAP));
      AP_vector[i]->setWirelessChannel(ChannelNoForThisAP);

      apWiFiDev = wifi.Install (wifiPhy, wifiMac, apNodes.Get (i));

    // spectrumwifi
    } else {
      spectrumPhy.Set ("ChannelNumber", UintegerValue(ChannelNoForThisAP));
      AP_vector[i]->setWirelessChannel(ChannelNoForThisAP);

      apWiFiDev = wifi.Install (spectrumPhy, wifiMac, apNodes.Get (i));
    }


    // print the IP and the MAC address
    if (verboseLevel > 0) {
      //std::cout << "AP     #" << i << "\tIP address: " << addr << '\n';
      std::cout << "        " << "\tMAC address: " << apWiFiDev.Get(0)->GetAddress() << '\n'; 
      std::cout << "        " << "\tWi-Fi channel: " << uint32_t(ChannelNoForThisAP) << '\n'; // convert to uint32_t in order to print it
    }

    // save everything in containers (add a line to the vector of containers, including the new AP device and interface)
    apWiFiDevices.push_back (apWiFiDev);
  }


  uint32_t k = 0;
  // this creates a record with the IDs and the MACs of the APs
  for (AP_recordVector::const_iterator index = AP_vector.begin (); index != AP_vector.end (); index++) {

    // auxiliar string
    std::ostringstream auxString;

    // create a string with the MAC
    auxString << apWiFiDevices[k].Get(0)->GetAddress();

    std::string myaddress = auxString.str();

    if (verboseLevel > 3 )
      std::cout << "AP with MAC " << myaddress << " added to the list of APs" << '\n';

    if (aggregationAlgorithm == 0) {
      (*index)->SetApRecord (k, myaddress, 0); // The algorithm is not activated, so I put a 0     
    } else {
      (*index)->SetApRecord (k, myaddress, maxAmpduSize); // The algorithm has to start with all the APs with A-MPDU enabled
    }

    //(*index)->SetMaxSizeAmpdu (0); // Not to be done here

    k++;
  }


  // Connect the STAs to the wifi

  // An ssid variable for the STAs
  Ssid stassid; // If you leave it blank, the STAs will send broadcast assoc requests

  // connect the STAs to the wifi channel
  for (uint32_t j = 0; j < number_of_STAs; j++) {

    // I use an auxiliary device container and an auxiliary interface container
    NetDeviceContainer staDev;
    Ipv4InterfaceContainer staInterface;

    // If the aggregation algorithm is NOT enabled, all the STAs aggregate
    if ( aggregationAlgorithm == 0 ) {
      wifiMac.SetType ( "ns3::StaWifiMac",
                        "Ssid", SsidValue (stassid));
      
    // If the aggregation algorithm is enabled, install 
    // - non aggregation in the VoIP STAs
    // - aggregation in the TCP STAs
    } else {
      // The VoIP STAs do NOT aggregate
      if ( j < numberVoIPupload + numberVoIPdownload ) {
        wifiMac.SetType ( "ns3::StaWifiMac",
                          "Ssid", SsidValue (stassid),
                          "QosSupported", BooleanValue (true),
                          "ActiveProbing", BooleanValue (true),
                          "BE_MaxAmpduSize", UintegerValue (0),
                          "BK_MaxAmpduSize", UintegerValue (0),
                          "VI_MaxAmpduSize", UintegerValue (0),
                          "VO_MaxAmpduSize", UintegerValue (0)); //Disable A-MPDU in the STAs
      // The TCP STAs do aggregate
      } else {
        wifiMac.SetType ( "ns3::StaWifiMac",
                          "Ssid", SsidValue (stassid),
                          "QosSupported", BooleanValue (true),
                          "ActiveProbing", BooleanValue (true),
                          "BE_MaxAmpduSize", UintegerValue (maxAmpduSize),
                          "BK_MaxAmpduSize", UintegerValue (maxAmpduSize),
                          "VI_MaxAmpduSize", UintegerValue (maxAmpduSize),
                          "VO_MaxAmpduSize", UintegerValue (maxAmpduSize));
      }

/*    // Other options (Enable AMSDU, and also enable AMSDU and AMPDU at the same time)
      wifMac.SetType ( "ns3::StaWifiMac",
                        "Ssid", SsidValue (stassid),
                        "BE_MaxAmpduSize", UintegerValue (0), //Disable A-MPDU
                        "BE_MaxAmsduSize", UintegerValue (7935)); //Enable A-MSDU with the highest maximum size allowed by the standard (7935 bytes)

      wifMac.SetType ( "ns3::StaWifiMac",
                        "Ssid", SsidValue (stassid),
                        "BE_MaxAmpduSize", UintegerValue (32768), //Enable A-MPDU with a smaller size than the default one
                        "BE_MaxAmsduSize", UintegerValue (3839)); //Enable A-MSDU with the smallest maximum size allowed by the standard (3839 bytes)
*/
    }


    // install the wifi in the STAs
    uint8_t ChannelNoForThisSTA = availableChannels[0];
    Ptr<Node> myNearestAp;
    uint32_t myNearestApId;

    // If there are different channels
    if ( numChannels > 1 ) {
      myNearestAp = nearestAp (apNodes, staNodes.Get(j), verboseLevel);
      myNearestApId = (myNearestAp)->GetId();
      ChannelNoForThisSTA = GetAP_WirelessChannel (myNearestApId, verboseLevel);
      if (verboseLevel > 2)
        std::cout << "The nearest AP for STA#" << j 
                  << " is AP#" <<  myNearestApId 
                  << ", in channel " << uint16_t(ChannelNoForThisSTA) << '\n';

    // All the APs are in the same channel
    } else {
      myNearestAp = nearestAp (apNodes, staNodes.Get(j), verboseLevel);
      myNearestApId = (myNearestAp)->GetId();
      ChannelNoForThisSTA = GetAP_WirelessChannel (myNearestApId, verboseLevel);
      if (verboseLevel > 2)
        std::cout << "The nearest AP for STA#" << j 
                  << " is AP#" <<  myNearestApId 
                  << ", in channel " << uint16_t(availableChannels[0]) 
                  << " (only a channel is used)" << '\n';     
    }

    // Yans wifi
    if (wifiModel == 0) {
      wifiPhy.Set ("ChannelNumber", UintegerValue(ChannelNoForThisSTA));
      //AP_vector[i]->setWirelessChannel(ChannelNoForThisAP);

      staDev = wifi.Install (wifiPhy, wifiMac, staNodes.Get(j));

    // spectrumwifi
    } else {
      spectrumPhy.Set ("ChannelNumber", UintegerValue(ChannelNoForThisSTA));
      //AP_vector[i]->setWirelessChannel(ChannelNoForThisAP);

      staDev = wifi.Install (spectrumPhy, wifiMac, staNodes.Get(j));   
    }

    // add this device
    staDevices.push_back (staDev);

    // add an IP address (10.0.0.0) to this interface
    staInterface = ipAddressesSegmentA.Assign (staDev);
    staInterfaces.push_back (staInterface);

    if (verboseLevel > 0) {
      Ptr<Node> node;
      Ptr<Ipv4> ipv4;
      Ipv4Address addr;
      node = staNodes.Get (j); // Get pointer to ith node in container
      ipv4 = node->GetObject<Ipv4> (); // Get Ipv4 instance of the node
      //addr = ipv4->GetAddress (0, 0).GetLocal (); // This returns 127.0.0.1
      addr = ipv4->GetAddress (1, 0).GetLocal ();

      // I print the node identifier
      std::cout << "STA    #" << node->GetId() << "\tIP address: " << addr << '\n';
      std::cout << "        " << "\tMAC address: " << staDevices[j].Get(0)->GetAddress() << '\n';
      std::cout << "        " << "\tchannel: " << uint32_t(ChannelNoForThisSTA) << '\n';

      // I print the position 
      Vector pos = GetPosition (node);
      std::cout << "        " << "\tPosition: " << pos.x << "," << pos.y << '\n';     
      std::cout << "        " << "\tInitially near AP# " << myNearestApId << '\n';      
    }
  }



  // EXPERIMENTAL: on each STA, add support for other operational channels
  // This is only needed if more than 1 channel is in use, and if wifiModel == 1
  if (numChannels > 1 && wifiModel == 1) {
    Ptr<SpectrumWifiPhy> wifiPhyPtrClient;
    for (uint32_t j = 0; j < number_of_STAs; j++) {
      wifiPhyPtrClient = staDevices[j].Get(0)->GetObject<WifiNetDevice>()->GetPhy()->GetObject<SpectrumWifiPhy>();

      std::cout << "STA\t#" << staNodes.Get(j)->GetId()
                << "\tAdded operational channels: ";

      for (uint32_t k = 0; k < numChannels; k++) {
        (*wifiPhyPtrClient).AddOperationalChannel ( availableChannels[k] );
        std::cout << uint16_t(availableChannels[k]) << " "; 
      }
      std::cout << '\n'; 
    }
  }


  // Set channel width
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue (channelWidth));


  // wired connections
  // create the ethernet channel for connecting the APs and the router
  CsmaHelper csma;
  //csma.SetChannelAttribute ("DataRate", StringValue ("100000Mbps")); // to avoid this being the bottleneck
  csma.SetChannelAttribute ("DataRate", DataRateValue (100000000000)); // 100 gbps
  // set the speed-of-light delay of the channel to 6560 nano-seconds (arbitrarily chosen as 1 nanosecond per foot over a 100 meter segment)
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));



  // https://www.nsnam.org/doxygen/classns3_1_1_bridge_helper.html#aba53f6381b7adda00d9163840b072fa6
  // This method creates an ns3::BridgeNetDevice with the attributes configured by BridgeHelper::SetDeviceAttribute, 
  // adds the device to the node (first argument), and attaches the given NetDevices (second argument) as ports of the bridge.
  // In this case, it adds it to the backbonenodes and attaches the backboneDevices of apWiFiDev
  // Returns a container holding the added net device.

  // taken from https://www.nsnam.org/doxygen/csma-bridge-one-hop_8cc_source.html
  //   +----------------+
  //   |   csmaHubNode  |                         The node named csmaHubNode
  //   +----------------+                         has a number CSMA net devices that are bridged
  //   CSMA   CSMA   CSMA   csmaHubDevices        together using a BridgeNetDevice (created with a bridgehelper) called 'bridgehub'.
  //    1|      |      |
  //     |      |      |                          The bridge node talks over three CSMA channels
  //    0|      |      |                          to three other CSMA net devices
  //   CSMA   CSMA   CSMA
  //   +--+   +--+  +------+
  //   |AP|   |AP|  |router|  (the router only appears in topology = 2)
  //   +--+   +--+  +------+
  //   wifi   wifi   CSMA
  //                   |
  //                   |
  //                   |
  //                 CSMA    


  // install a csma channel between the ith AP node and the bridge (csmaHubNode) node
  for ( uint32_t i = 0; i < number_of_APs; i++) {
    NetDeviceContainer link = csma.Install (NodeContainer (apNodes.Get(i), csmaHubNode));
    apCsmaDevices.Add (link.Get(0));
    csmaHubDevices.Add (link.Get(1));
  }

  if (topology == 0) {
    // install a csma channel between the singleServer and the bridge (csmaHubNode) node
    NetDeviceContainer link = csma.Install (NodeContainer (singleServerNode.Get(0), csmaHubNode));
    singleServerDevices.Add (link.Get(0));
    csmaHubDevices.Add (link.Get(1));

    // Assign an IP address (10.0.0.0) to the single server
    singleServerInterfaces = ipAddressesSegmentA.Assign (singleServerDevices);

  } else if (topology == 1) {
    // install a csma channel between the ith server node and the bridge (csmaHubNode) node
    for ( uint32_t i = 0; i < number_of_Servers; i++) {
      NetDeviceContainer link = csma.Install (NodeContainer (serverNodes.Get(i), csmaHubNode));
      serverDevices.Add (link.Get(0));
      csmaHubDevices.Add (link.Get(1));
    }  
      
    // Assign IP addresses (10.0.0.0) to the servers
    serverInterfaces = ipAddressesSegmentA.Assign (serverDevices);

  } else { // if (topology == 2)
    // install a csma channel between the router and the bridge (csmaHubNode) node
    NetDeviceContainer link = csma.Install (NodeContainer (routerNode.Get(0), csmaHubNode));
    routerDeviceToAps.Add (link.Get(0));
    csmaHubDevices.Add (link.Get(1));

    // Assign an IP address (10.0.0.0) to the router (AP part)
    routerInterfaceToAps = ipAddressesSegmentA.Assign (routerDeviceToAps);
  }

  // on each AP, I install a bridge between two devices: the WiFi device and the csma device
  BridgeHelper bridgeAps, bridgeHub;
  for (uint32_t i = 0; i < number_of_APs; i++) {
    // Create a bridge between two devices of the same node: the AP
    bridgeAps.Install (apNodes.Get (i), NetDeviceContainer ( apWiFiDevices[i].Get(0), apCsmaDevices.Get (i) ) );

    // If needed, I could assign an IP address to the bridge (not to the wifi or the csma devices)
    //apInterface = ipAddressesSegmentA.Assign (bridgeApDevices);
  }


  //Create the bridge netdevice, which will do the packet switching.  The
  // bridge lives on the node csmaHubNode.Get(0) and bridges together the csmaHubDevices and the routerDeviceToAps
  // which are the CSMA net devices 
  bridgeHub.Install (csmaHubNode.Get(0), csmaHubDevices );


  // create a point to point helper for connecting the servers with the router (if topology == 2)
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10000Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("1ms"));


  //if (populatearpcache)
    // Assign IP address (10.0.0.0) to the bridge
    //csmaHubInterfaces = ipAddressesSegmentA.Assign (bridgeApDevices);

  if (topology == 2) {

    // connect each server to the router with a point to point connection
    for ( uint32_t i = 0; i < number_of_Servers; i++) {
      // Create a point to point link between the router and the server
      NetDeviceContainer devices;
      devices = p2p.Install ( NodeContainer (routerNode.Get (0), serverNodes.Get (i))); // this returns two devices. Add them to the router and the server
      routerDeviceToServers.Add (devices.Get(0));
      serverDevices.Add (devices.Get(1));
    }

    // Assign IP addresses (10.1.0.0) to the servers
    serverInterfaces = ipAddressesSegmentB.Assign (serverDevices);

    // Assign an IP address (10.1.0.0) to the router
    routerInterfaceToServers = ipAddressesSegmentB.Assign (routerDeviceToServers);
  }


  if (verboseLevel > 0) {
    Ptr<Node> node;
    Ptr<Ipv4> ipv4;
    Ipv4Address addr;

    if (topology == 0) { 
      // print the IP and the MAC addresses of the server
      node = singleServerNode.Get (0); // Get pointer to ith node in container
      ipv4 = node->GetObject<Ipv4> (); // Get Ipv4 instance of the node
      //addr = ipv4->GetAddress (0, 0).GetLocal (); // This returns 127.0.0.1
      addr = ipv4->GetAddress (1, 0).GetLocal ();
      // I print the node identifier
      std::cout << "Server\t" << "\tIP address: " << addr << '\n';
      std::cout << "      \t" << "\tMAC address: " << singleServerDevices.Get(0)->GetAddress() << '\n';
      
    } else if (topology == 1) {
      // print the IP and the MAC addresses of each server
      for ( uint32_t i = 0; i < number_of_Servers; i++) {
        node = serverNodes.Get (i); // Get pointer to ith node in container
        ipv4 = node->GetObject<Ipv4> (); // Get Ipv4 instance of the node      
        //addr = ipv4->GetAddress (0, 0).GetLocal (); // This returns 127.0.0.1
        addr = ipv4->GetAddress (1, 0).GetLocal (); 
        std::cout << "Server #" << node->GetId() << "\tIP address: " << addr << '\n';
        std::cout << "        " << "\tMAC address: " << serverDevices.Get(i)->GetAddress() << '\n';  
      }

    } else { //(topology == 2)
      // print the IP and the MAC addresses of the router
      node = routerNode.Get (0); // Get pointer to ith node in container
      ipv4 = node->GetObject<Ipv4> (); // Get Ipv4 instance of the node
      //addr = ipv4->GetAddress (0, 0).GetLocal (); // This returns 127.0.0.1
      addr = ipv4->GetAddress (1, 0).GetLocal ();
      // I print the node identifier
      std::cout << "Router(AP side)" << "\tIP address: " << addr << '\n';
      std::cout << "               " << "\tMAC address: " << routerDeviceToAps.Get(0)->GetAddress() << '\n';

      // print the IP and the MAC addresses of the router and its corresponding server
      for ( uint32_t i = 0; i < number_of_Servers; i++) {
        // print the IP and the MAC of the p2p devices of the router
        node = routerNode.Get(0); // Get pointer to the router
        ipv4 = node->GetObject<Ipv4> (); // Get Ipv4 instance of the router      
        //addr = ipv4->GetAddress (0, 0).GetLocal (); // This returns 127.0.0.1
        addr = ipv4->GetAddress ( i + 2, 0).GetLocal (); // address 0 is local, and address 1 is for the AP side 
        std::cout << "Router IF #" << i + 2 << "\tIP address: " << addr << '\n';
        std::cout << "        " << "\tMAC address: " << routerDeviceToServers.Get(i)->GetAddress() << '\n';
      }

      // print the IP and the MAC addresses of the router and its corresponding server
      for ( uint32_t i = 0; i < number_of_Servers; i++) {
        // addresses of the server
        node = serverNodes.Get (i); // Get pointer to ith node in container
        ipv4 = node->GetObject<Ipv4> (); // Get Ipv4 instance of the node      
        //addr = ipv4->GetAddress (0, 0).GetLocal (); // This returns 127.0.0.1
        addr = ipv4->GetAddress (1, 0).GetLocal (); 
        std::cout << "Server #" << node->GetId() << "\tIP address: " << addr << '\n';
        std::cout << "        " << "\tMAC address: " << serverDevices.Get(i)->GetAddress() << '\n'; 
      }
    } 
  }


  // print a blank line after printing IP addresses
  if (verboseLevel > 0)
    std::cout << "\n";


  // Fill the routing tables. It is necessary in topology 2, which includes two different networks
  if(topology == 2) {
    //NS_LOG_INFO ("Enabling global routing on all nodes");
    //Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
    // I obtain this error: aborted. msg="ERROR: L2 forwarding loop detected!", file=../src/internet/model/global-router-interface.cc, line=1523
    // Global routing helper does not work in this case
    // As a STA can connect through many APs, loops appear
    // Therefore, I have to add the routes manually
    // https://www.nsnam.org/doxygen/classns3_1_1_ipv4_static_routing.html

    // get the IPv4 address of the router interface to the APs
    Ptr<Node> node;
    Ptr<Ipv4> ipv4;
    Ipv4Address addrRouterAPs, addrRouterSrv;
    node = routerNode.Get (0); // Get pointer to ith node in container
    ipv4 = node->GetObject<Ipv4> (); // Get Ipv4 instance of the node
    addrRouterAPs = ipv4->GetAddress (1, 0).GetLocal ();

    // On each Sta, add a route to the network of the servers
    for (uint32_t i = 0; i < number_of_STAs; i++) {
      // get the ipv4 instance of the STA
      Ptr<Node> node;
      Ptr<Ipv4> ipv4;
      Ipv4Address addr;
      node = staNodes.Get (i); // Get pointer to ith node in container
      ipv4 = node->GetObject<Ipv4> (); // Get Ipv4 instance of the node
      Ipv4StaticRoutingHelper ipv4RoutingHelper;
      Ptr<Ipv4StaticRouting> staticRouting = ipv4RoutingHelper.GetStaticRouting (ipv4);

      // Add a route to the network of the servers
      staticRouting->AddNetworkRouteTo (Ipv4Address ("10.1.0.0"), Ipv4Mask ("255.255.0.0"), addrRouterAPs, 1);

      if (verboseLevel > 0) {
        std::cout << "Routing in STA #" << staNodes.Get(i)->GetId() << " with IP address "<< ipv4->GetAddress (1, 0).GetLocal() << ": ";
        std::cout << "\tadded route to network 10.1.0.0/255.255.0.0 through gateway " << addrRouterAPs << '\n';
      }
    }

    // On each server, add a route to the network of the Stas
    for (uint32_t i = 0; i < number_of_Servers; i++) {
      // get the ipv4 instance of the server
      Ptr<Node> node;
      Ptr<Ipv4> ipv4;
      Ipv4Address addr;
      node = serverNodes.Get (i); // Get pointer to ith node in container
      ipv4 = node->GetObject<Ipv4> (); // Get Ipv4 instance of the node
      Ipv4StaticRoutingHelper ipv4RoutingHelper;
      Ptr<Ipv4StaticRouting> staticRouting = ipv4RoutingHelper.GetStaticRouting (ipv4);

      // get the ipv4 instance of the router
      Ptr<Ipv4> ipv4Router;
      ipv4Router = routerNode.Get (0)->GetObject<Ipv4> (); // Get Ipv4 instance of the router

      // Add a route to the network of the Stas
      staticRouting->AddNetworkRouteTo (Ipv4Address ("10.0.0.0"), Ipv4Mask ("255.255.0.0"), ipv4Router->GetAddress ( i + 2, 0).GetLocal(), 1);

      if (verboseLevel > 0) {
        std::cout << "Routing in server #" << serverNodes.Get(i)->GetId() << " with IP address "<< ipv4->GetAddress (1, 0).GetLocal() << ": ";
        std::cout << "\tadded route to network 10.0.0.0/255.255.0.0 through gateway " << ipv4Router->GetAddress ( i + 2, 0).GetLocal() << '\n';        
      }
    }

    // On the router, add a route to each server
    for (uint32_t i = 0; i < number_of_Servers; i++) {
      // get the ipv4 instance of the server
      Ptr<Node> node;
      Ptr<Ipv4> ipv4;
      Ipv4Address addrServer;
      node = serverNodes.Get (i); // Get pointer to ith node in container
      ipv4 = node->GetObject<Ipv4> (); // Get Ipv4 instance of the node
      addrServer = ipv4->GetAddress (1, 0).GetLocal ();

      // get the ipv4 instance of the router
      Ptr<Ipv4> ipv4Router;
      ipv4Router = routerNode.Get (0)->GetObject<Ipv4> (); // Get Ipv4 instance of the router
      Ipv4StaticRoutingHelper ipv4RoutingHelper;
      Ptr<Ipv4StaticRouting> staticRouting = ipv4RoutingHelper.GetStaticRouting (ipv4Router);

      // Tell the interface to be used for going to this server
      staticRouting->AddHostRouteTo (addrServer, i + 2, 1); // use interface 'i + 2': interface #0 is localhost, and interface #1 is for talking with the APs

      if (verboseLevel > 0) {
        std::cout << "Routing in the router (id #" << routerNode.Get(0)->GetId() << "): " ;
        std::cout << "\tadded route to host " << addrServer << " through interface with IP address " << ipv4Router->GetAddress ( i + 2, 0).GetLocal() << '\n';
      }      
    }

    // print a blank line after printing routing information
    if (verboseLevel > 0)
      std::cout << "\n";
  }


  // Create a STA_record per STA, in order to store its association parameters
  NodeContainer::Iterator mynode;
  uint32_t l = 0;
  for (mynode = staNodes.Begin (); mynode != staNodes.End (); ++mynode) { // run this for all the STAs

    // This calls the constructor, i.e. the function that creates a record to store the association of each STA
    STA_record *m_STArecord = new STA_record();

    // Set the value of the id of the STA in the record
    m_STArecord->setstaid ((*mynode)->GetId());

    // Establish the type of application
    if ( l < numberVoIPupload ) {
      m_STArecord->Settypeofapplication (1);  // VoIP upload
      m_STArecord->SetMaxSizeAmpdu (0);       // No aggregation
    } else if (l < numberVoIPupload + numberVoIPdownload ) {
      m_STArecord->Settypeofapplication (2);  // VoIP download
      m_STArecord->SetMaxSizeAmpdu (0);       // No aggregation
    } else if (l < numberVoIPupload + numberVoIPdownload + numberTCPupload) {
      m_STArecord->Settypeofapplication (3);               // TCP upload
      m_STArecord->SetMaxSizeAmpdu (maxAmpduSize);         // aggregation enabled
    } else {
      m_STArecord->Settypeofapplication (4);                // TCP download
      m_STArecord->SetMaxSizeAmpdu (maxAmpduSize);         // aggregation enabled
    }

    // Establish the verbose level in the STA record
    m_STArecord->SetVerboseLevel (verboseLevel);

    // Establish the rest of the private variables of the STA record
    m_STArecord->SetnumChannels (numChannels);
    m_STArecord->Setversion80211 (version80211);
    m_STArecord->SetaggregationAlgorithm (aggregationAlgorithm);
    m_STArecord->SetAmpduSize (maxAmpduSize);
    m_STArecord->SetmaxAmpduSizeWhenAggregationDisabled (maxAmpduSizeWhenAggregationDisabled);
    m_STArecord->SetWifiModel (wifiModel);

    l++;

    // Set a callback function to be called each time a STA gets associated to an AP
    std::ostringstream STA;
    STA << (*mynode)->GetId();
    std::string strSTA = STA.str();

    // Check if we are using the algoritm for deactivating / activating aggregation
    //if ( aggregationAlgorithm == 1) {

      // This makes a callback every time a STA gets associated to an AP
      // see trace sources in https://www.nsnam.org/doxygen/classns3_1_1_sta_wifi_mac.html#details
      // trace association. Taken from https://github.com/MOSAIC-UA/802.11ah-ns3/blob/master/ns-3/scratch/s1g-mac-test.cc
      // some info here: https://groups.google.com/forum/#!msg/ns-3-users/zqdnCxzYGM8/MdCshgYKAgAJ
      Config::Connect ( "/NodeList/"+strSTA+"/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/$ns3::StaWifiMac/Assoc", 
                        MakeCallback (&STA_record::SetAssoc, m_STArecord));

      // Set a callback function to be called each time a STA gets de-associated from an AP
      Config::Connect ( "/NodeList/"+strSTA+"/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/$ns3::StaWifiMac/DeAssoc", 
                        MakeCallback (&STA_record::UnsetAssoc, m_STArecord));
    //}

    // Add the new record to the vector of STA associations
    assoc_vector.push_back (m_STArecord);
  }


  /************* Setting applications ***********/

  // Variable for setting the port of each communication
  uint32_t port = initial_port;

  // VoIP upload
  // UDPClient runs in the STA and UDPServer runs in the server
  // traffic goes STA -> server
  UdpServerHelper myVoipUpServer;
  ApplicationContainer VoipUpServer;

  for (uint32_t i = 0 ; i < numberVoIPupload ; i++ ) {
    myVoipUpServer = UdpServerHelper(port); // Each UDP connection requires a different port

    if (topology == 0) {
      VoipUpServer = myVoipUpServer.Install (singleServerNode.Get(0));
    } else {
      VoipUpServer = myVoipUpServer.Install (serverNodes.Get(i));
    }

    VoipUpServer.Start (Seconds (0.0));
    VoipUpServer.Stop (Seconds (simulationTime + initial_time_interval));

    // UdpClient runs in the STA, so I must create a UdpClient per STA
    UdpClientHelper myVoipUpClient;
    ApplicationContainer VoipUpClient;

    // I associate all the servers (all running in the Stas) to each server application

    Ipv4Address myaddress;

    if (topology == 0) {
      myaddress = singleServerInterfaces.GetAddress (0);
    } else {
      myaddress = serverInterfaces.GetAddress (i);
    }

    InetSocketAddress destAddress (myaddress, port);

    // If priorities ARE NOT enabled, VoIP traffic will have TcpPriorityLevel
    if (prioritiesEnabled == 0) {
      destAddress.SetTos (TcpPriorityLevel);
    // If priorities ARE enabled, VoIP traffic will have a higher priority (VoIpPriorityLevel)
    } else {
      destAddress.SetTos (VoIpPriorityLevel);
    }

    myVoipUpClient = UdpClientHelper(destAddress);

    myVoipUpClient.SetAttribute ("MaxPackets", UintegerValue (4294967295u));
    //myVoipUpClient.SetAttribute ("Interval", TimeValue (Time ("0.02")));
    myVoipUpClient.SetAttribute ("Interval", TimeValue (Seconds (VoIPg729IPT))); //packets/s
    myVoipUpClient.SetAttribute ("PacketSize", UintegerValue ( VoIPg729PayoladSize ));

    VoipUpClient = myVoipUpClient.Install (staNodes.Get(i));
    VoipUpClient.Start (Seconds (initial_time_interval));
    VoipUpClient.Stop (Seconds (simulationTime + initial_time_interval));
    if (verboseLevel > 0) {
      if (topology == 0) {
        std::cout << "Application VoIP upload   from STA    #" << staNodes.Get(i)->GetId()
                  << "\t with IP address " << staInterfaces[i].GetAddress(0)
                  << "\t-> to the server"
                  << "\t with IP address " << singleServerInterfaces.GetAddress (0) 
                  << "\t and port " << port
                  << '\n';  
      } else {
        std::cout << "Application VoIP upload   from STA    #" << staNodes.Get(i)->GetId()
                  << "\t with IP address " << staInterfaces[i].GetAddress(0)
                  << "\t-> to server #" << serverNodes.Get(i)->GetId()
                  << "\t with IP address " << serverInterfaces.GetAddress (i) 
                  << "\t and port " << port
                  << '\n';
      }
    }
    port ++; // Each UDP connection requires a different port
  }


  // VoIP download
  // UDPClient in the AP and UDPServer in the STA
  // traffic goes AP -> STA
  // I have taken this as an example: https://groups.google.com/forum/#!topic/ns-3-users/ej8LaxQO1Gc
  // UdpServer runs in each STA. It waits for input UDP packets and uses the
  // information carried into their payload to compute delay and to determine
  // if some packets are lost. https://www.nsnam.org/doxygen/classns3_1_1_udp_server_helper.html#details

  UdpServerHelper myVoipDownServer;

  ApplicationContainer VoipDownServer;

  for (uint32_t i = numberVoIPupload ; i < numberVoIPupload + numberVoIPdownload ; i++ ) {
    myVoipDownServer = UdpServerHelper(port);
    VoipDownServer = myVoipDownServer.Install (staNodes.Get(i));
    VoipDownServer.Start (Seconds (0.0));
    VoipDownServer.Stop (Seconds (simulationTime + initial_time_interval));

    // UdpClient runs in the AP, so I must create a UdpClient per STA
    UdpClientHelper myVoipDownClient;
    ApplicationContainer VoipDownClient;
    // I associate all the servers (all running in the Stas) to each server application
    // GetAddress() will return the address of the UdpServer

    InetSocketAddress destAddress (InetSocketAddress (staInterfaces[i].GetAddress(0), port));
    
    // If priorities ARE NOT enabled, VoIP traffic will have TcpPriorityLevel
    if (prioritiesEnabled == 0) {
      destAddress.SetTos (TcpPriorityLevel);
    // If priorities ARE enabled, VoIP traffic will have a higher priority (VoIpPriorityLevel)
    } else {
      destAddress.SetTos (VoIpPriorityLevel);
    }

    myVoipDownClient = UdpClientHelper(destAddress);

    myVoipDownClient.SetAttribute ("MaxPackets", UintegerValue (4294967295u));
    //myVoipDownClient.SetAttribute ("Interval", TimeValue (Time ("0.02"))); //packets/s
    myVoipDownClient.SetAttribute ("Interval", TimeValue (Seconds (VoIPg729IPT))); //packets/s
    myVoipDownClient.SetAttribute ("PacketSize", UintegerValue ( VoIPg729PayoladSize ));

    //VoipDownClient = myVoipDownClient.Install (wifiApNodesA.Get(0));
    if (topology == 0) {
      VoipDownClient = myVoipDownClient.Install (singleServerNode.Get(0));
    } else {
      VoipDownClient = myVoipDownClient.Install (serverNodes.Get (i));
    }

    VoipDownClient.Start (Seconds (initial_time_interval));
    VoipDownClient.Stop (Seconds (simulationTime + initial_time_interval));

    if (verboseLevel > 0) {
      if (topology == 0) {
        std::cout << "Application VoIP download from the server"
                  << "\t with IP address " << singleServerInterfaces.GetAddress (0) 
                  << "\t-> to STA    #" << staNodes.Get(i)->GetId() 
                  << "\t\t with IP address " << staInterfaces[i].GetAddress(0) 
                  << "\t and port " << port
                  << '\n';          
      } else {
        std::cout << "Application VoIP download from server #" << serverNodes.Get(i)->GetId()
                  << "\t with IP address " << serverInterfaces.GetAddress (i) 
                  << "\t-> to STA    #" << staNodes.Get(i)->GetId()
                  << "\t\t with IP address " << staInterfaces[i].GetAddress(0)  
                  << "\t and port " << port
                  << '\n';     
      }     
    }
    port ++;
  }


  // Configurations for TCP

  // This is necessary, or the packets will not be of this size
  // Taken from https://www.nsnam.org/doxygen/codel-vs-pfifo-asymmetric_8cc_source.html
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (TcpPayloadSize));

  // The next lines seem to be useless
  // 4 MB of TCP buffer
  //Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue (1 << 21));
  //Config::SetDefault ("ns3::TcpSocket::SndBufSize", UintegerValue (1 << 21));

  // Activate TCP selective acknowledgement (SACK)
  // bool sack = true;
  // Config::SetDefault ("ns3::TcpSocketBase::Sack", BooleanValue (sack));

  // TCP variants, see 
  //  https://www.nsnam.org/docs/models/html/tcp.html
  //  https://www.nsnam.org/doxygen/tcp-variants-comparison_8cc_source.html

  // TCP NewReno is the default in ns3, and also in this script
  if (TcpVariant.compare ("TcpNewReno") == 0)
  {
    Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpNewReno::GetTypeId ()));
  } 
  else if (TcpVariant.compare ("TcpHighSpeed") == 0) {
    Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpHighSpeed::GetTypeId ()));
  } 
  else if (TcpVariant.compare ("TcpWestwoodPlus") == 0) {
    Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpWestwood::GetTypeId ()));
    Config::SetDefault ("ns3::TcpWestwood::ProtocolType", EnumValue (TcpWestwood::WESTWOODPLUS));
    Config::SetDefault ("ns3::TcpWestwood::FilterType", EnumValue (TcpWestwood::TUSTIN));
  } else {
    std::cout << "INPUT PARAMETER ERROR: Bad TCP variant. Supported: TcpNewReno, TcpHighSpeed, TcpWestwoodPlus. Stopping the simulation." << '\n';
    return 0;  
  }

  // Activate the log of BulkSend application
  if (verboseLevel > 2) {
    LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
  }


  //TCP upload
  // Create a PacketSinkApplication and install it on the remote nodes
  ApplicationContainer PacketSinkTcpUp;

  // Create a BulkSendApplication and install it on all the staNodes
  // it will send traffic to the servers
  ApplicationContainer BulkSendTcpUp;

  for (uint32_t i = numberVoIPupload + numberVoIPdownload; i < numberVoIPupload + numberVoIPdownload + numberTCPupload; i++) {

    PacketSinkHelper myPacketSinkTcpUp ("ns3::TcpSocketFactory",
                                        InetSocketAddress (Ipv4Address::GetAny (), port));

    myPacketSinkTcpUp.SetAttribute ("Protocol", TypeIdValue (TcpSocketFactory::GetTypeId ()));

    if (topology == 0) {
      PacketSinkTcpUp.Add(myPacketSinkTcpUp.Install (singleServerNode.Get(0)));
    } else {
      PacketSinkTcpUp.Add(myPacketSinkTcpUp.Install (serverNodes.Get (i)));
    }

    Ipv4Address myaddress;

    if (topology == 0) {
      myaddress = singleServerInterfaces.GetAddress (0); 
    } else {
      myaddress = serverInterfaces.GetAddress (i);
    }

    InetSocketAddress destAddress (InetSocketAddress (myaddress, port));

    // TCP will have TcpPriorityLevel, whether priorities are enabled or not
    destAddress.SetTos (TcpPriorityLevel);

    BulkSendHelper myBulkSendTcpUp ( "ns3::TcpSocketFactory", destAddress);

    // Set the amount of data to send in bytes. Zero is unlimited.
    myBulkSendTcpUp.SetAttribute ("MaxBytes", UintegerValue (0));
    myBulkSendTcpUp.SetAttribute ("SendSize", UintegerValue (TcpPayloadSize));
    // You must also add the Config::SetDefalut  SegmentSize line, or the previous line will have no effect

    // install the application on every staNode
    BulkSendTcpUp = myBulkSendTcpUp.Install (staNodes.Get(i));

    if (verboseLevel > 0) {
      if (topology == 0) {
        std::cout << "Application TCP upload    from STA    #" << staNodes.Get(i)->GetId() 
                  << "\t with IP address " << staInterfaces[i].GetAddress(0) 
                  << "\t-> to the server"
                  << "\t with IP address " << singleServerInterfaces.GetAddress (0) 
                  << "\t and port " << port
                  << '\n';
      } else {
        std::cout << "Application TCP upload    from STA    #" << staNodes.Get(i)->GetId() 
                  << "\t with IP address " << staInterfaces[i].GetAddress(0) 
                  << "\t-> to server #" << serverNodes.Get(i)->GetId() 
                  << "\t with IP address " << serverInterfaces.GetAddress (i) 
                  << "\t and port " << port
                  << '\n';
      } 
    }
    port++;
  }
  PacketSinkTcpUp.Start (Seconds (0.0));
  PacketSinkTcpUp.Stop (Seconds (simulationTime + initial_time_interval));

  BulkSendTcpUp.Start (Seconds (initial_time_interval));
  BulkSendTcpUp.Stop (Seconds (simulationTime + initial_time_interval));


  // TCP download

  // Create a PacketSink Application and install it on the wifi STAs
  ApplicationContainer PacketSinkTcpDown;

  // Create a BulkSendApplication and install it on all the servers
  // it will send traffic to the STAs
  ApplicationContainer BulkSendTcpDown;

  for (uint32_t i = numberVoIPupload + numberVoIPdownload + numberTCPupload; 
                i < numberVoIPupload + numberVoIPdownload + numberTCPupload + numberTCPdownload; 
                i++) {

    // Install a sink on each STA
    // Each sink will have a different port
    PacketSinkHelper myPacketSinkTcpDown ( "ns3::TcpSocketFactory",
                                            InetSocketAddress (Ipv4Address::GetAny (), port ));

    myPacketSinkTcpDown.SetAttribute ("Protocol", TypeIdValue (TcpSocketFactory::GetTypeId ()));

    PacketSinkTcpDown = myPacketSinkTcpDown.Install(staNodes.Get (i));
    PacketSinkTcpDown.Start (Seconds (0.0));
    PacketSinkTcpDown.Stop (Seconds (simulationTime + initial_time_interval));

    // Install a sender on the sender node
    InetSocketAddress destAddress (InetSocketAddress (staInterfaces[i].GetAddress(0), port ));

    // TCP will have TcpPriorityLevel, whether priorities are enabled or not
    destAddress.SetTos (TcpPriorityLevel);
    
    BulkSendHelper myBulkSendTcpDown ( "ns3::TcpSocketFactory", destAddress);

    // Set the amount of data to send in bytes.  Zero is unlimited.
    myBulkSendTcpDown.SetAttribute ("MaxBytes", UintegerValue (0));
    myBulkSendTcpDown.SetAttribute ("SendSize", UintegerValue (TcpPayloadSize));
    // You must also add the Config::SetDefalut  SegmentSize line, or the previous line will have no effect

    // Install a sender on the sender
    if (topology == 0) {
      BulkSendTcpDown.Add(myBulkSendTcpDown.Install (singleServerNode.Get(0)));
    } else {
      BulkSendTcpDown.Add(myBulkSendTcpDown.Install (serverNodes.Get (i)));
    }

    if (verboseLevel > 0) {
      if (topology == 0) {
        std::cout << "Application TCP download  from the server"
                  << "\t with IP address " << singleServerInterfaces.GetAddress (0) 
                  << "\t-> to STA    #" << staNodes.Get(i)->GetId() 
                  << "\t\t with IP address " << staInterfaces[i].GetAddress(0) 
                  << "\t and port " << port
                  << '\n';    
      } else {
        std::cout << "Application TCP download  from server #" << serverNodes.Get(i)->GetId()
                  << "\t with IP address " << serverInterfaces.GetAddress (i) 
                  << "\t-> to STA    #" << staNodes.Get(i)->GetId() 
                  << "\t\t with IP address " << staInterfaces[i].GetAddress(0) 
                  << "\t and port " << port
                  << '\n';
      }
    }
    port++;
  }

  BulkSendTcpDown.Start (Seconds (initial_time_interval));
  BulkSendTcpDown.Stop (Seconds (simulationTime + initial_time_interval));

  // print a blank line after printing the info about the applications
  if (verboseLevel > 0)
    std::cout << "\n";


  // Enable the creation of pcap files
  if (enablePcap) {

    // pcap trace of the APs and the STAs
    if (wifiModel == 0) {

      wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);

      for (uint32_t i=0; i< number_of_APs; i++)
        wifiPhy.EnablePcap (outputFileName + "_" + outputFileSurname + "_AP", apWiFiDevices[i]);

      for (uint32_t i=0; i < number_of_STAs; i++)
        wifiPhy.EnablePcap (outputFileName + "_" + outputFileSurname + "_STA", staDevices[i]);

    } else {

      spectrumPhy.SetPcapDataLinkType (SpectrumWifiPhyHelper::DLT_IEEE802_11_RADIO);

      for (uint32_t i=0; i< number_of_APs; i++)
        spectrumPhy.EnablePcap (outputFileName + "_" + outputFileSurname + "_AP", apWiFiDevices[i]);

      // pcap trace of the STAs
      for (uint32_t i=0; i < number_of_STAs; i++)
        spectrumPhy.EnablePcap (outputFileName + "_" + outputFileSurname + "_STA", staDevices[i]);    
    }

    // pcap trace of the server(s)
    if (topology == 0) {
      // pcap trace of the single server
      csma.EnablePcap (outputFileName + "_" + outputFileSurname + "_single_server", singleServerDevices.Get(0));

    } else {
      // pcap trace of the servers
      for (uint32_t i=0; i < number_of_Servers; i++)
        p2p.EnablePcap (outputFileName + "_" + outputFileSurname + "_server", serverDevices.Get(i));
    }


    // pcap trace of the hub ports. not necessary
    if (false) {
      if (topology == 0) {
        // pcap trace of the hub: it has number_of_APs + 1 devices (the one connected to the server)
        for (uint32_t i=0; i < number_of_APs + 1; i++)
          csma.EnablePcap (outputFileName + "_" + outputFileSurname + "_hub", csmaHubDevices.Get(i));

      } else if (topology == 1) {
         // pcap trace of the hub: it has number_of_APs + number_of_server devices
        for (uint32_t i=0; i < number_of_APs + number_of_Servers; i++)
          csma.EnablePcap (outputFileName + "_" + outputFileSurname + "_hub", csmaHubDevices.Get(i)); 

      } else if (topology == 2) {
        // pcap trace of the hub: it has number_of_APs + 1 devices (the one connected to the router)
        for (uint32_t i=0; i < number_of_APs + 1; i++)
          csma.EnablePcap (outputFileName + "_" + outputFileSurname + "_hub", csmaHubDevices.Get(i));    
      }
    }
  }


  // Install FlowMonitor on the nodes
  // see https://www.nsnam.org/doxygen/wifi-hidden-terminal_8cc_source.html
  // and https://www.nsnam.org/docs/models/html/flow-monitor.html
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor;

  // It is not necessary to monitor the APs, because I am not getting statistics from them
  if (false)
    monitor = flowmon.Install(apNodes);

  // install monitor in the STAs
  monitor = flowmon.Install(staNodes);

  // install monitor in the server(s)
  if (topology == 0) {
    monitor = flowmon.Install(singleServerNode);
  } else {
    monitor = flowmon.Install(serverNodes);
  }


  // mobility trace
  if (writeMobility) {
    AsciiTraceHelper ascii;
    MobilityHelper::EnableAsciiAll (ascii.CreateFileStream (outputFileName + "_" + outputFileSurname + "-mobility.txt"));
  }
 


// FIXME ***************Trial: Change the parameters of the AP (disable A-MPDU) during the simulation
// how to change attributes: https://www.nsnam.org/docs/manual/html/attributes.html
// https://www.nsnam.org/doxygen/regular-wifi-mac_8cc_source.html
//Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/VI_MaxAmpduSize", UintegerValue(0));
//Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/VO_MaxAmpduSize", UintegerValue(0));
//Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/BE_MaxAmpduSize", UintegerValue(0));
//Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/BK_MaxAmpduSize", UintegerValue(0));
//Config::Set("/NodeList/0/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/BK_MaxAmpduSize", UintegerValue(0));
  if(false) {
    for ( uint32_t j = 0 ; j < number_of_APs ; ++j ) {
      std::cout << "scheculing AMPDU=0 in second 4.0 for node " << j << '\n';
      Simulator::Schedule (Seconds (4.0), &ModifyAmpdu, j, 0, verboseLevel );

      std::cout << "scheculing AMPDU= " << maxAmpduSize << " in second 6.0 for node " << j << '\n';
      Simulator::Schedule (Seconds (6.0), &ModifyAmpdu, j, maxAmpduSize, verboseLevel );
    }
    for ( uint32_t j = 0 ; j < number_of_STAs ; ++j ) {
      std::cout << "scheculing AMPDU=0 in second 4.0 for node " << number_of_APs + number_of_STAs + j << '\n';
      Simulator::Schedule (Seconds (4.0), &ModifyAmpdu, number_of_APs + number_of_STAs + j, 0, verboseLevel );

      std::cout << "scheculing AMPDU= " << maxAmpduSize << " in second 6.0 for node " << number_of_APs + number_of_STAs + j << '\n';
      Simulator::Schedule (Seconds (6.0), &ModifyAmpdu, number_of_APs + number_of_STAs + j, maxAmpduSize, verboseLevel );
    }
  }
// FIXME *** end of the trial ***


  if ( (verboseLevel > 0) && (aggregationAlgorithm == 1) ) {
    Simulator::Schedule(Seconds(0.0), &List_STA_record);
    Simulator::Schedule(Seconds(0.0), &ListAPs, verboseLevel);
  }

  if (printSeconds > 0) {
    Simulator::Schedule(Seconds(0.0), &printTime, printSeconds, outputFileName, outputFileSurname);
  }

  // Start ARP trial (Failure so far)
  if (false) {
    for ( uint32_t j = 0 ; j < number_of_Servers ; ++j )
      Simulator::Schedule(Seconds(0.0), &PopulateArpCache, j + number_of_APs, serverNodes.Get(j));

    for ( uint32_t j = 0 ; j < number_of_STAs ; ++j )
      Simulator::Schedule(Seconds(0.0), &PopulateArpCache, j + number_of_APs + number_of_Servers, staNodes.Get(j));


    for ( uint32_t j = 0 ; j < number_of_Servers ; ++j )
      Simulator::Schedule(Seconds(0.0), &infoArpCache, j + number_of_APs, serverNodes.Get(j), verboseLevel);  

    for ( uint32_t j = 0 ; j < number_of_STAs ; ++j )
      Simulator::Schedule(Seconds(0.0), &infoArpCache, j + number_of_APs + number_of_Servers, staNodes.Get(j), verboseLevel);

    //Simulator::Schedule(Seconds(2.0), &emtpyArpCache);

    for ( uint32_t j = number_of_APs ; j < number_of_APs + number_of_STAs ; ++j )
      Simulator::Schedule(Seconds(0.5), &PrintArpCache, j, staNodes.Get(j), staDevices[j].Get(0));

    for ( uint32_t j = number_of_APs ; j < number_of_APs + number_of_Servers ; ++j )
      Simulator::Schedule(Seconds(0.5), &PrintArpCache, number_of_STAs + j, serverNodes.Get(j), serverDevices.Get(j));


    //  Time mytime2 = Time (10.0);
    //    Config::SetDefault ("ns3::ArpL3Protocol::AliveTimeout", TimeValue(mytime2));

    // Modify the parameters of the ARP caches of the STAs and servers
    // see the parameters of the arp cache https://www.nsnam.org/doxygen/classns3_1_1_arp_cache.html#details
    // I only run this for the STAs and the servers. The APs and the hub do not have the IP stack
    for ( uint32_t j = number_of_APs ; j < number_of_APs + number_of_Servers + number_of_STAs; ++j ) {

      // I use an auxiliar string for creating the first argument of Config::Set
      std::ostringstream auxString;

      // Modify the number of retries
      //auxString << "/NodeList/" << j << "/$ns3::Ipv4L3Protocol/InterfaceList/1/ArpCache::MaxRetries";
      auxString << "/NodeList/" << j << "/$ns3::ArpL3Protocol/CacheList/*::MaxRetries";
      // std::cout << auxString.str() << '\n';
      Config::Set(auxString.str(),  UintegerValue(1));
      // clean the string
      auxString.str(std::string());

      // Modify the size of the queue for packets pending an arp reply
      auxString << "/NodeList/" << j << "/$ns3::Ipv4L3Protocol/InterfaceList/0/ArpCache::PendingQueueSize";
      // std::cout << auxString.str() << '\n';
      Config::Set(auxString.str(),  UintegerValue(1));
      // clean the string
      auxString.str(std::string());

      // Modify the AliveTimeout
      Time mytime = Time (10.0);
      //auxString << "/NodeList/" << j << "/$ns3::Ipv4L3Protocol/InterfaceList/1/ArpCache::AliveTimeout";
      //auxString << "  /NodeList/" << j << "/$ns3::ArpL3Protocol/CacheList/*::AliveTimeout";
      auxString << "/NodeList/*/$ns3::ArpL3Protocol/CacheList/*::AliveTimeout";
      Config::Set(auxString.str(), TimeValue(mytime)); // see https://www.nsnam.org/doxygen/classns3_1_1_time.html#addbf69c7aec0f3fd8c0595426d88622e
      // clean the string
      auxString.str(std::string());

      // Modify the DeadTimeout
      mytime = Time (1.0);
      //auxString << "/NodeList/" << j << "/$ns3::Ipv4L3Protocol/InterfaceList/0/ArpCache::DeadTimeout";
      auxString << "/NodeList/" << j << "/$ns3::ArpL3Protocol/CacheList/*::DeadTimeout";
      Config::Set(auxString.str(), TimeValue(mytime)); // see https://www.nsnam.org/doxygen/classns3_1_1_time.html#addbf69c7aec0f3fd8c0595426d88622e
    }

    std::ostringstream auxString;
    // Modify the AliveTimeout
    Time mytime = Time (10.0);
    //auxString << "/NodeList/" << j << "/$ns3::Ipv4L3Protocol/InterfaceList/1/ArpCache::AliveTimeout";
    //auxString << "  /NodeList/" << j << "/$ns3::ArpL3Protocol/CacheList/*::AliveTimeout";
    auxString << "/NodeList/*/ns3::ArpL3Protocol/CacheList/*::AliveTimeout";
    Config::Set(auxString.str(), TimeValue(mytime)); // see https://www.nsnam.org/doxygen/classns3_1_1_time.html#addbf69c7aec0f3fd8c0595426d88622e
    //Config::Connect(auxString.str(), MakeCallback (&hello) );
    // clean the string
    auxString.str(std::string());
  }
  // End ARP trial (failure so far)


// Trial of channel swithing of a STA
//https://10343742895474358856.googlegroups.com/attach/1b7c2a3108d5e/channel-switch-minimal.cc?part=0.1&view=1&vt=ANaJVrGFRkTkufO3dLFsc9u1J_v2-SUCAMtR0V86nVmvXWXGwwZ06cmTSv7DrQUKMWTVMt_lxuYTsrYxgVS59WU3kBd7dkkH5hQsLE8Em0FHO4jx8NbjrPk
if(false) {
  NetDeviceContainer devices;
  devices.Add (apWiFiDevices[0]);
  devices.Add (staDevices[0]);
  ChangeFrequencyLocal (devices, 44, wifiModel, verboseLevel); // This works since it is executed before the simulation starts.

  Simulator::Schedule(Seconds(2.0), &ChangeFrequencyLocal, devices, 44, wifiModel, verboseLevel); // This does not work with SpectrumWifiPhy. But IT WORKS WITH YANS!!!

  //nearestAp (apNodes, staNodes.Get(0), verboseLevel);
  //Simulator::Schedule(Seconds(3.0), &nearestAp, apNodes, staNodes.Get(0), verboseLevel);
  // This does not work because nearestAp returns a value and you cannot schedule it
}



  if (verboseLevel > 0) {
    NS_LOG_INFO ("Run Simulation");
    NS_LOG_INFO ("");
  }

  Simulator::Stop (Seconds (simulationTime + initial_time_interval));
  Simulator::Run ();

  if (verboseLevel > 0)
    NS_LOG_INFO ("Simulation finished. Writing results");


  /***** Obtain per flow and aggregate statistics *****/

  // This part is inspired on https://www.nsnam.org/doxygen/wifi-hidden-terminal_8cc_source.html
  // and also on https://groups.google.com/forum/#!msg/ns-3-users/iDs9HqrQU-M/ryoVRz4M_fYJ

  monitor->CheckForLostPackets (); // Check right now for packets that appear to be lost.

  // FlowClassifier provides a method to translate raw packet data into abstract flow identifier and packet identifier parameters
  // see https://www.nsnam.org/doxygen/classns3_1_1_flow_classifier.html
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ()); // Returns a pointer to the FlowClassifier object


  // Save the results of flowmon to an XML file
  if (saveXMLFile)
    flowmon.SerializeToXmlFile (outputFileName + "_" + outputFileSurname + "_flowmonitor.xml", true, true);


  // variables used for calculating the averages
  std::string proto; 
  uint32_t this_is_the_first_flow = 1;        // used to add the titles on the first line of the output file

  uint32_t number_of_UDP_upload_flows = 0;      // this index is used for the cumulative calculation of the average
  uint32_t number_of_UDP_download_flows = 0;
  uint32_t number_of_TCP_upload_flows = 0;      // this index is used for the cumulative calculation of the average
  uint32_t number_of_TCP_download_flows = 0;
  
  uint32_t total_UDP_upload_tx_packets = 0;
  uint32_t total_UDP_upload_rx_packets = 0;
  double total_UDP_upload_latency = 0.0;
  double total_UDP_upload_jitter = 0.0;

  uint32_t total_UDP_download_tx_packets = 0;
  uint32_t total_UDP_download_rx_packets = 0;
  double total_UDP_download_latency = 0.0;
  double total_UDP_download_jitter = 0.0;

  double total_TCP_upload_throughput = 0.0;
  double total_TCP_download_throughput = 0.0; // average throughput of all the download TCP flows

  // for each flow
  std::map< FlowId, FlowMonitor::FlowStats > stats = monitor->GetFlowStats(); 
  for (std::map< FlowId, FlowMonitor::FlowStats >::iterator flow=stats.begin(); flow!=stats.end(); flow++) 
  {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow->first); 

    switch(t.protocol) 
    { 
      case(6): 
        proto = "TCP"; 
      break; 

      case(17): 
        proto = "UDP"; 
      break; 

      default: 
        std::cout << "Protocol unknown" << std::endl;
        exit(1);
      break;

    }

    // create a string with the characteristics of the flow
    std::ostringstream flowID;

    flowID  << flow->first << "\t"      // identifier of the flow (a number)
            << proto << "\t"
            << t.sourceAddress << "\t"
            << t.sourcePort << "\t" 
            << t.destinationAddress << "\t"
            << t.destinationPort; 

    // create a string with the name of the output file
    std::ostringstream nameFlowFile, surnameFlowFile;

    nameFlowFile  << outputFileName
                  << "_"
                  << outputFileSurname;

    surnameFlowFile << "_flow_"
                    << flow->first;

    // Print the statistics of this flow to an output file and to the screen
    print_stats ( flow->second, simulationTime, generateHistograms, nameFlowFile.str(), surnameFlowFile.str(), verboseLevel, flowID.str(), this_is_the_first_flow );

    // the first time, print_stats will print a line with the title of each column
    // put the flag to 0
    if ( this_is_the_first_flow == 1 )
      this_is_the_first_flow = 0;

    // calculate and print the average of each kind of applications
    // calculate it in a cumulative way

    // UDP upload flows
    if (  (t.destinationPort >= initial_port ) && 
          (t.destinationPort <  initial_port + numberVoIPupload )) {

        total_UDP_upload_tx_packets = total_UDP_upload_tx_packets + flow->second.txPackets;
        total_UDP_upload_rx_packets = total_UDP_upload_rx_packets + flow->second.rxPackets;
        total_UDP_upload_latency = total_UDP_upload_latency + flow->second.delaySum.GetSeconds();
        total_UDP_upload_jitter = total_UDP_upload_jitter + flow->second.jitterSum.GetSeconds();
        number_of_UDP_upload_flows ++;

    // UDP download flows
    } else if ( (t.destinationPort >= initial_port + numberVoIPupload ) && 
                (t.destinationPort <  initial_port + numberVoIPupload + numberVoIPdownload )) { 

        total_UDP_download_tx_packets = total_UDP_download_tx_packets + flow->second.txPackets;
        total_UDP_download_rx_packets = total_UDP_download_rx_packets + flow->second.rxPackets;
        total_UDP_download_latency = total_UDP_download_latency + flow->second.delaySum.GetSeconds();
        total_UDP_download_jitter = total_UDP_download_jitter + flow->second.jitterSum.GetSeconds();
        number_of_UDP_download_flows ++;

    // TCP upload flows
    } else if ( (t.destinationPort >= initial_port + numberVoIPupload + numberVoIPdownload ) && 
                (t.destinationPort <  initial_port + numberVoIPupload + numberVoIPdownload + numberTCPupload )) { 

        total_TCP_upload_throughput = total_TCP_upload_throughput + ( flow->second.rxBytes * 8.0 / simulationTime );
        number_of_TCP_upload_flows ++;

    // TCP download flows
    } else if ( (t.destinationPort >= initial_port + numberVoIPupload + numberVoIPdownload + numberTCPupload ) && 
                (t.destinationPort <  initial_port + numberVoIPupload + numberVoIPdownload + numberTCPupload + numberTCPdownload )) { 

        total_TCP_download_throughput = total_TCP_download_throughput + ( flow->second.rxBytes * 8.0 / simulationTime );                                          
        number_of_TCP_download_flows ++;
    } 
  }

  if (verboseLevel > 0) {
    std::cout << "\n" 
      << "The next figures are averaged per packet, not per flow:" << std::endl;

    if ( total_UDP_upload_rx_packets > 0 ) {
      std::cout << " Average UDP upload latency [s]:\t" << total_UDP_upload_latency / total_UDP_upload_rx_packets << std::endl;
      std::cout << " Average UDP upload jitter [s]:\t\t" << total_UDP_upload_jitter / total_UDP_upload_rx_packets << std::endl;
    } else {
      std::cout << " Average UDP upload latency [s]:\tno packets received" << std::endl;
      std::cout << " Average UDP upload jitter [s]:\tno packets received" << std::endl;      
    }
    if ( total_UDP_upload_tx_packets > 0 ) {
      std::cout << " Average UDP upload loss rate:\t\t" 
                <<  1.0 - ( double(total_UDP_upload_rx_packets) / double(total_UDP_upload_tx_packets) )
                << std::endl;
    } else {
      std::cout << " Average UDP upload loss rate:\t\tno packets sent" << std::endl;     
    }


    if ( total_UDP_download_rx_packets > 0 ) {
      std::cout << " Average UDP download latency [s]:\t" << total_UDP_download_latency / total_UDP_download_rx_packets 
                << std::endl;
      std::cout << " Average UDP download jitter [s]:\t" << total_UDP_download_jitter / total_UDP_download_rx_packets 
                << std::endl;
    } else {
      std::cout << " Average UDP download latency [s]:\tno packets received" << std::endl;
      std::cout << " Average UDP download jitter [s]:\tno packets received" << std::endl;      
    }
    if ( total_UDP_download_tx_packets > 0 ) {
      std::cout << " Average UDP download loss rate:\t" 
                <<  1.0 - ( double(total_UDP_download_rx_packets) / double(total_UDP_download_tx_packets) )
                << std::endl;
    } else {
     std::cout << " Average UDP download loss rate:\tno packets sent" << std::endl;     
    }

    std::cout << "\n" 
              << " Number TCP upload flows\t\t"
              << number_of_TCP_upload_flows << "\n"
              << " Total TCP upload throughput [bps]\t"
              << total_TCP_upload_throughput << "\n"

              << " Number TCP download flows\t\t"
              << number_of_TCP_download_flows << "\n"
              << " Total TCP download throughput [bps]\t"
              << total_TCP_download_throughput << "\n";
  }

  // save the average values to a file 
  std::ofstream ofs;
  ofs.open ( outputFileName + "_average.txt", std::ofstream::out | std::ofstream::app); // with "app", all output operations happen at the end of the file, appending to its existing contents
  ofs << outputFileSurname << "\t"
      << "Number UDP upload flows" << "\t"
      << number_of_UDP_upload_flows << "\t";
  if ( total_UDP_upload_rx_packets > 0 ) {
    ofs << "Average UDP upload latency [s]" << "\t"
        << total_UDP_upload_latency / total_UDP_upload_rx_packets << "\t"
        << "Average UDP upload jitter [s]" << "\t"
        << total_UDP_upload_jitter / total_UDP_upload_rx_packets << "\t";
  } else {
    ofs << "Average UDP upload latency [s]" << "\t"
        << "\t"
        << "Average UDP upload jitter [s]" << "\t"
        << "\t";
  }
  if ( total_UDP_upload_tx_packets > 0 ) {
    ofs << "Average UDP upload loss rate" << "\t"
        << 1.0 - ( double(total_UDP_upload_rx_packets) / double(total_UDP_upload_tx_packets) ) << "\t";
  } else {
    ofs << "Average UDP upload loss rate" << "\t"
        << "\t";
  }

  ofs << "Number UDP download flows" << "\t"
      << number_of_UDP_download_flows << "\t";
  if ( total_UDP_download_rx_packets > 0 ) {
    ofs << "Average UDP download latency [s]" << "\t"
        << total_UDP_download_latency / total_UDP_download_rx_packets << "\t"
        << "Average UDP download jitter [s]" << "\t"
        << total_UDP_download_jitter / total_UDP_download_rx_packets << "\t";
  } else {
    ofs << "Average UDP download latency [s]" << "\t"
        << "\t"
        << "Average UDP download jitter [s]" << "\t"
        << "\t";
  }
  if ( total_UDP_download_tx_packets > 0 ) {
    ofs << "Average UDP download loss rate" << "\t"
        << 1.0 - ( double(total_UDP_download_rx_packets) / double(total_UDP_download_tx_packets) ) << "\t";
  } else {
    ofs << "Average UDP download loss rate" << "\t"
        << "\t";
  }

  ofs << "Number TCP upload flows" << "\t"
      << number_of_TCP_upload_flows << "\t"
      << "Total TCP upload throughput [bps]" << "\t"
      << total_TCP_upload_throughput << "\t"

      << "Number TCP download flows" << "\t"
      << number_of_TCP_download_flows << "\t"
      << "Total TCP download throughput [bps]" << "\t"
      << total_TCP_download_throughput << "\t";

  ofs << "Duration of the simulation [s]" << "\t"
      << simulationTime << "\n";

  ofs.close();

  // Cleanup
  Simulator::Destroy ();
  if (verboseLevel > 0)
    NS_LOG_INFO ("Done");

  return 0;
}
