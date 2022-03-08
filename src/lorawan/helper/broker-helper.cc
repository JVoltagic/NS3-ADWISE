#include "ns3/broker-helper.h"
#include "ns3/network-controller-components.h"
#include "ns3/adr-component.h"
#include "ns3/double.h"
#include "ns3/string.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/simulator.h"
#include "ns3/log.h"

namespace ns3 {
namespace lorawan {

NS_LOG_COMPONENT_DEFINE ("BrokerServerHelper");

BrokerServerHelper::BrokerServerHelper ()
{
  m_factory.SetTypeId ("ns3::BrokerServer");
  p2pHelper.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2pHelper.SetChannelAttribute ("Delay", StringValue ("2ms"));
  SetAdr ("ns3::AdrComponent");
}

BrokerServerHelper::~BrokerServerHelper ()
{
}

void
BrokerServerHelper::SetAttribute (std::string name, const AttributeValue &value)
{
  m_factory.Set (name, value);
}

void
BrokerServerHelper::SetGateways (NodeContainer gateways)
{
  m_gateways = gateways;
}

void
BrokerServerHelper::SetEndDevices (NodeContainer endDevices)
{
  m_endDevices = endDevices;
}

ApplicationContainer
BrokerServerHelper::Install (Ptr<Node> node)
{
  return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer
BrokerServerHelper::Install (NodeContainer c)
{
  ApplicationContainer apps;
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      apps.Add (InstallPriv (*i));
    }

  return apps;
}

Ptr<Application>
BrokerServerHelper::InstallPriv (Ptr<Node> node)
{
  NS_LOG_FUNCTION (this << node);
  Ptr<BrokerServer> app = m_factory.Create<BrokerServer> ();
  app->SetNode (node);
  node->AddApplication (app);

  // Cycle on each gateway
  for (NodeContainer::Iterator i = m_gateways.Begin ();
       i != m_gateways.End ();
       i++)
    {
      // Add the connections with the gateway
      // Create a PointToPoint link between gateway and NS
      NetDeviceContainer container = p2pHelper.Install (node, *i);

      // Add the gateway to the NS list
      app->AddGateway (*i, container.Get (0));
    }

  // Link the BrokerServer to its NetDevices
  for (uint32_t i = 0; i < node->GetNDevices (); i++)
    {
      Ptr<NetDevice> currentNetDevice = node->GetDevice (i);
      currentNetDevice->SetReceiveCallback (MakeCallback
                                              (&BrokerServer::Receive,
                                              app));
}
  // Add the end devices
  app->AddClients (m_endDevices);
  // Add components to the BrokerServer
  InstallComponents (app);
  return app;
}

void
BrokerServerHelper::EnableAdr (bool enableAdr)
{
  NS_LOG_FUNCTION (this << enableAdr);

  m_adrEnabled = enableAdr;
}

void
BrokerServerHelper::SetAdr (std::string type)
{
  NS_LOG_FUNCTION (this << type);

  m_adrSupportFactory = ObjectFactory ();
  m_adrSupportFactory.SetTypeId (type);
}

void
BrokerServerHelper::InstallComponents (Ptr<BrokerServer> netServer)
{
  NS_LOG_FUNCTION (this << netServer);

  // Add Confirmed Messages support
  Ptr<ConfirmedMessagesComponent> ackSupport =
    CreateObject<ConfirmedMessagesComponent> ();
  netServer->AddComponent (ackSupport);

  // Add LinkCheck support
  Ptr<LinkCheckComponent> linkCheckSupport = CreateObject<LinkCheckComponent> ();
  netServer->AddComponent (linkCheckSupport);

  // Add Adr support
  if (m_adrEnabled)
    {
      netServer->AddComponent (m_adrSupportFactory.Create<NetworkControllerComponent> ());
    }
}
}
} // namespace ns3
