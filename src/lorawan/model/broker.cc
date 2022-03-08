#include "ns3/broker.h"
#include "ns3/net-device.h"
#include "ns3/point-to-point-net-device.h"
#include "ns3/packet.h"
#include "ns3/lorawan-mac-header.h"
#include "ns3/lora-frame-header.h"
#include "ns3/lora-device-address.h"
#include "ns3/network-status.h"
#include "ns3/lora-frame-header.h"
#include "ns3/node-container.h"
#include "ns3/class-a-end-device-lorawan-mac.h"
#include "ns3/mac-command.h"
#include "ns3/mqtt-tag.h"

namespace ns3 {
namespace lorawan {

NS_LOG_COMPONENT_DEFINE ("BrokerServer");

NS_OBJECT_ENSURE_REGISTERED (BrokerServer);

TypeId
BrokerServer::GetTypeId (void)
{
  static TypeId tid =
      TypeId ("ns3::BrokerServer")
          .SetParent<Application> ()
          .AddConstructor<BrokerServer> ()
          .AddTraceSource ("ReceivedPacket",
                           "Trace source that is fired when a packet arrives at the Broker Server",
                           MakeTraceSourceAccessor (&BrokerServer::m_receivedPacket),
                           "ns3::Packet::TracedCallback")
          .AddAttribute ("pubDelay", "Delay between publish", DoubleValue (0.0),
                         MakeDoubleAccessor (&BrokerServer::m_delay), MakeDoubleChecker<double> ())
          .SetGroupName ("lorawan");
  return tid;
}

BrokerServer::BrokerServer ()
    : m_status (Create<NetworkStatus> ()),
      m_controller (Create<NetworkController> (m_status)),
      m_scheduler (Create<NetworkScheduler> (m_status, m_controller))
{
  NS_LOG_FUNCTION_NOARGS ();
}

BrokerServer::~BrokerServer ()
{
  NS_LOG_FUNCTION_NOARGS ();
}

void
BrokerServer::StartApplication (void)
{
  NS_LOG_FUNCTION_NOARGS ();
}

void
BrokerServer::StopApplication (void)
{
  NS_LOG_FUNCTION_NOARGS ();
}

void
BrokerServer::AddGateway (Ptr<Node> gateway, Ptr<NetDevice> netDevice)
{
  NS_LOG_FUNCTION (this << gateway);

  // Get the PointToPointNetDevice
  Ptr<PointToPointNetDevice> p2pNetDevice;
  for (uint32_t i = 0; i < gateway->GetNDevices (); i++)
    {
      p2pNetDevice = gateway->GetDevice (i)->GetObject<PointToPointNetDevice> ();
      if (p2pNetDevice != 0)
        {
          // We found a p2pNetDevice on the gateway
          break;
        }
    }

  // Get the gateway's LoRa MAC layer (assumes gateway's MAC is configured as first device)
  Ptr<GatewayLorawanMac> gwMac = gateway->GetDevice (0)
                                     ->GetObject<LoraNetDevice> ()
                                     ->GetMac ()
                                     ->GetObject<GatewayLorawanMac> ();
  NS_ASSERT (gwMac != 0);

  // Get the Address
  Address gatewayAddress = p2pNetDevice->GetAddress ();

  // Create new gatewayStatus
  Ptr<GatewayStatus> gwStatus = Create<GatewayStatus> (gatewayAddress, netDevice, gwMac);

  m_status->AddGateway (gatewayAddress, gwStatus);
}

void
BrokerServer::AddClients (NodeContainer nodes)
{
  NS_LOG_FUNCTION_NOARGS ();

  // For each node in the container, call the function to add that single node
  NodeContainer::Iterator it;
  for (it = nodes.Begin (); it != nodes.End (); it++)
    {
      AddClient (*it);
    }
}

void
BrokerServer::AddClient (Ptr<Node> node)
{
  NS_LOG_FUNCTION (this << node);

  // Get the LoraNetDevice
  Ptr<LoraNetDevice> loraNetDevice;
  for (uint32_t i = 0; i < node->GetNDevices (); i++)
    {
      loraNetDevice = node->GetDevice (i)->GetObject<LoraNetDevice> ();
      if (loraNetDevice != 0)
        {
          // We found a LoraNetDevice on the node
          break;
        }
    }

  // Get the MAC
  Ptr<EndDeviceLorawanMac> edLorawanMac =
      loraNetDevice->GetMac ()->GetObject<EndDeviceLorawanMac> ();

  // Update the NetworkStatus about the existence of this node
  m_status->AddNode (edLorawanMac);
}

bool
BrokerServer::Receive (Ptr<NetDevice> device, Ptr<const Packet> packet, uint16_t protocol,
                       const Address &address)
{
  NS_LOG_FUNCTION (this << packet << protocol << address);

  // Create a copy of the packet
  Ptr<Packet> myPacket = packet->Copy ();
  Ptr<Packet> npacket = packet->Copy ();

  // Fire the trace source
  m_receivedPacket (packet);

  // Inform the scheduler of the newly arrived packet
  m_scheduler->OnReceivedPacket (packet);

  // Inform the status of the newly arrived packet
  m_status->OnReceivedPacket (packet, address);

  // Inform the controller of the newly arrived packet
  m_controller->OnNewPacket (packet);

  // Here remove MQTT TAG and inspect Type of message
  mqttTag mqtt;
  myPacket->RemovePacketTag (mqtt);
  double messageType;
  messageType = mqtt.GetType ();

  //Remove Frame Header to find ED address
  LorawanMacHeader macHdr;
  myPacket->RemoveHeader (macHdr);
  LoraFrameHeader frameHdr;
  frameHdr.SetAsUplink ();
  myPacket->RemoveHeader (frameHdr);
  LoraDeviceAddress edAddr = frameHdr.GetAddress ();

  std::string topic;
  std::string MSG;

  uint8_t *buffer = new uint8_t[packet->GetSize ()];
  packet->CopyData (buffer, packet->GetSize ());
  std::string data = std::string (buffer, buffer + packet->GetSize ());
  data.erase (0, 9);
  std::string w = "";
  for (auto x : data)
    {
      if (x == ',')
        {
          topic = w;
          w = "";
        }
      else
        {
          w = w + x;
        }
    }
  MSG = w;
    // NS_LOG_UNCOND ("TOPIC: " << topic);
    // NS_LOG_UNCOND ("MSG: " << MSG);
  //   NS_LOG_UNCOND ("TYPE: " << messageType);

  switch (int (messageType))
    {
    case 0: // Subscribe
      SubscribeToTopic (topic, edAddr);
      break;
    case 1: // Publish
      SendToSubscribers (topic, MSG, npacket);
      break;
    }

  return true;
}

void
BrokerServer::AddComponent (Ptr<NetworkControllerComponent> component)
{
  NS_LOG_FUNCTION (this << component);

  m_controller->Install (component);
}

Ptr<NetworkStatus>
BrokerServer::GetNetworkStatus (void)
{
  return m_status;
}

void
BrokerServer::SubscribeToTopic (std::string topic, LoraDeviceAddress address)
{

  std::vector<LoraDeviceAddress> add;
  add.push_back (address);
  std::map<std::string, std::vector<LoraDeviceAddress>>::iterator it = m_addresses.find (topic);
  std::vector<LoraDeviceAddress> &addvec = it->second;
  // Check if Topic exists; If so add address to list
  if (m_addresses.find (topic) != m_addresses.end ())
    {
      addvec.push_back (address);
    }
  else
    {
      // Create new topic and List of Addresses associated
      m_addresses.insert ({topic, add});
    }
  //   NS_LOG_UNCOND ("Topic: \"" << topic << "\" List of adresses: ");
  //   for (size_t i = 0; i < addvec.size (); i++)
  //     {
  //       NS_LOG_UNCOND (addvec[i]);
  //     }
  //   NS_LOG_UNCOND ("\n");
}

void
BrokerServer::SendToSubscribers (std::string topic, std::string MSG, Ptr<Packet> data)
{
  //Check if topic exists, if not error
  if (m_addresses.find (topic) == m_addresses.end ())
    {
      NS_LOG_UNCOND (
          "You tried to Publish to a topic but it doesn't exist.");
    }else{
  std::map<std::string, std::vector<LoraDeviceAddress>>::iterator it = m_addresses.find (topic);
  std::vector<LoraDeviceAddress> &addresses = it->second;
  std::string Msg;
  Msg.append (topic);
  Msg.append (",");
  Msg.append (MSG);
  for (size_t i = 0; i < addresses.size (); i++)
    {
      //  if (address = Lora)
      // {
      // Create new packet to send with MSG inside

      uint8_t *buffer = (uint8_t *) &Msg[0];
      uint32_t size = Msg.length ();
      Ptr<Packet> packet =
          Create<Packet> (buffer, size); //reinterpret_cast<const uint8_t *> (MSG), MSG.length ()
      // Send through Lora
      //data->RemoveAllPacketTags();
      Simulator::Schedule (Seconds ((i * m_delay)), &BrokerServer::SendLora, this, packet,
                           addresses[i]);
      // }
      // else if (address = CSMA)
      // {
      //     // Send through CSMA
      //     Simulator::Schedule(Seconds(0), &BrokerServer::SendCSMA, this, packetCopy, LoraDeviceAddress(addresses[i]));
      // }
    }}
}

void
BrokerServer::SendLora (Ptr<Packet> data, LoraDeviceAddress deviceAddress)
{
  NS_LOG_FUNCTION (this << data << deviceAddress);
  m_scheduler->DoSend (data, deviceAddress, 2);
  // NS_LOG_UNCOND("All Packets should have been sent");
}

// void BrokerServer::SendCSMA(Ptr<Packet> data, LoraDeviceAddress deviceAddress)
// {
//     NS_LOG_FUNCTION(this << data << deviceAddress);
//     m_scheduler->DoSend(data, deviceAddress, 2);
//     // NS_LOG_UNCOND("All Packets should have been sent");
// }

} // namespace lorawan
} // namespace ns3
