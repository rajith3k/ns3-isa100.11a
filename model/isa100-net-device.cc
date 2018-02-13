/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2017 The University Of Calgary- FISHLAB
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Geoffrey Messier <gmessier@ucalgary.ca>
 *         Hazem Gomaa <gomaa.hazem@gmail.com>
 *         Michael Herrmann <mjherrma@gmail.com>
 *
 * Based on the Lr-wpan-net-device by Tom Henderson <thomas.r.henderson@boeing.com>
*/


#include "ns3/log.h"
#include "ns3/abort.h"

#include "isa100-error-model.h"
#include "ns3/spectrum-channel.h"
#include "ns3/mobility-model.h"
#include "ns3/node.h"
#include "ns3/isa100-dl.h"
#include "ns3/zigbee-phy.h"
#include "ns3/isa100-battery.h"
#include "ns3/isa100-processor.h"
#include "ns3/packet.h"

#include "ns3/isa100-net-device.h"

NS_LOG_COMPONENT_DEFINE ("Isa100NetDevice");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (Isa100NetDevice);

TypeId
Isa100NetDevice::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::Isa100NetDevice")
    .SetParent<NetDevice> ()
    .AddConstructor<Isa100NetDevice> ()
    .AddAttribute (
    		"Channel", "The channel attached to this device",
    		PointerValue (),
				MakePointerAccessor (&Isa100NetDevice::DoGetChannel),
				MakePointerChecker<SpectrumChannel> ())
    .AddAttribute (
    		"Phy", "The PHY layer attached to this device.",
				PointerValue (),
				MakePointerAccessor (&Isa100NetDevice::GetPhy,&Isa100NetDevice::SetPhy),
				MakePointerChecker<ZigbeePhy> ())
	  .AddAttribute (
	  		"Dl", "The DL layer attached to this device.",
				PointerValue (),
				MakePointerAccessor (&Isa100NetDevice::GetDl,&Isa100NetDevice::SetDl),
				MakePointerChecker<Isa100Dl> ())
  ;
  return tid;
}


Isa100NetDevice::Isa100NetDevice ()
  : m_configComplete (false)
{
  NS_LOG_FUNCTION (this);
  m_dl = CreateObject<Isa100Dl> ();
  m_phy = CreateObject<ZigbeePhy> ();
  m_battery = 0;
  m_processor = 0;

  CompleteConfig ();

}


Isa100NetDevice::~Isa100NetDevice ()
{
  NS_LOG_FUNCTION (this);
}

void
Isa100NetDevice::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  m_dl->Dispose ();
  m_phy->Dispose ();
  if(m_battery)
  	m_battery->Dispose();
  if(m_processor)
  	m_processor->Dispose();
  m_phy = 0;
  m_dl = 0;
  m_battery = 0;
  m_processor = 0;
  m_sensor = 0;
  m_node = 0;

  // chain up.
  NetDevice::DoDispose ();
}

void
Isa100NetDevice::DoInitialize (void)
{
  NS_LOG_FUNCTION (this);
  m_phy->Initialize ();
  m_dl->Initialize ();
  NetDevice::DoInitialize ();
}


void Isa100NetDevice::CompleteConfig (void)
{
  NS_LOG_FUNCTION (this);
  if (m_dl == 0
      || m_phy == 0
      || m_node == 0
      || m_configComplete)
    {
      return;
    }


  // PHY helper objects
  m_phy->SetMobility (m_node->GetObject<MobilityModel> ());
  Ptr<Isa100ErrorModel> model = CreateObject<Isa100ErrorModel> ();
  m_phy->SetErrorModel (model);

  // Callbacks for DL to PHY communication.
  m_dl->SetPdDataRequestCallback( MakeCallback(&ZigbeePhy::PdDataRequest, m_phy) );
  m_dl->SetPlmeCcaRequestCallback( MakeCallback(&ZigbeePhy::PlmeCcaRequest, m_phy) );
  m_dl->SetPlmeSetTrxStateRequestCallback( MakeCallback(&ZigbeePhy::PlmeSetTRXStateRequest, m_phy) );
  m_dl->SetPlmeSetAttributeCallback( MakeCallback(&ZigbeePhy::PlmeSetAttributeRequest, m_phy) );


  // Callbacks for PHY to DL communication.
  m_phy->SetPdDataIndicationCallback (MakeCallback (&Isa100Dl::PdDataIndication, m_dl));
  m_phy->SetPdDataConfirmCallback (MakeCallback (&Isa100Dl::PdDataConfirm, m_dl));
  m_phy->SetPlmeCcaConfirmCallback (MakeCallback (&Isa100Dl::PlmeCcaConfirm, m_dl));
  m_phy->SetPlmeSetTRXStateConfirmCallback (MakeCallback (&Isa100Dl::PlmeSetTrxStateConfirm, m_dl));

  m_configComplete = true;
}

void Isa100NetDevice::SetDl (Ptr<Isa100Dl> dl)
{
  NS_LOG_FUNCTION (this);
  m_dl = dl;
  CompleteConfig ();
}

void Isa100NetDevice::SetPhy (Ptr<ZigbeePhy> phy)
{
  NS_LOG_FUNCTION (this);
  m_phy = phy;
  CompleteConfig ();
}

void Isa100NetDevice::SetBattery (Ptr<Isa100Battery> battery)
{
  NS_LOG_FUNCTION (this);
  m_battery = battery;
}

void Isa100NetDevice::SetProcessor (Ptr<Isa100Processor> processor)
{
  NS_LOG_FUNCTION (this);
  m_processor = processor;
}

void Isa100NetDevice::SetSensor (Ptr<Isa100Sensor> sensor)
{
  NS_LOG_FUNCTION (this);
  m_sensor = sensor;
}


void Isa100NetDevice::SetChannel (Ptr<SpectrumChannel> channel)
{
  NS_LOG_FUNCTION (this << channel);
  m_phy->SetChannel (channel);
  channel->AddRx (m_phy);
  CompleteConfig ();
}

Ptr<Isa100Dl> Isa100NetDevice::GetDl (void) const
{
  NS_LOG_FUNCTION (this);
  return m_dl;
}

Ptr<ZigbeePhy> Isa100NetDevice::GetPhy (void) const
{
  NS_LOG_FUNCTION (this);
  return m_phy;
}

Ptr<Isa100Battery> Isa100NetDevice::GetBattery (void) const
{
  NS_LOG_FUNCTION (this);
  return m_battery;
}

Ptr<Isa100Processor> Isa100NetDevice::GetProcessor (void) const
{
  NS_LOG_FUNCTION (this);
  return m_processor;
}

Ptr<Isa100Sensor> Isa100NetDevice::GetSensor (void) const
{
  NS_LOG_FUNCTION (this);
  return m_sensor;
}

void
Isa100NetDevice::SetIfIndex (const uint32_t index)
{
  NS_LOG_FUNCTION (this << index);
  m_ifIndex = index;
}

uint32_t
Isa100NetDevice::GetIfIndex (void) const
{
  NS_LOG_FUNCTION (this);
  return m_ifIndex;
}

Ptr<Channel>
Isa100NetDevice::GetChannel (void) const
{
  NS_LOG_FUNCTION (this);
  return m_phy->GetChannel ();
}

void
Isa100NetDevice::LinkUp (void)
{
  NS_LOG_FUNCTION (this);
  m_linkUp = true;
  m_linkChanges ();
}
void
Isa100NetDevice::LinkDown (void)
{
  NS_LOG_FUNCTION (this);
  m_linkUp = false;
  m_linkChanges ();
}


Ptr<SpectrumChannel>
Isa100NetDevice::DoGetChannel (void) const
{
  NS_LOG_FUNCTION (this);
  return m_phy->GetChannel ();
}
void
Isa100NetDevice::SetAddress (Address address)
{
  NS_LOG_FUNCTION (this);
  m_dl->SetAttribute("Address",Mac16AddressValue(Mac16Address::ConvertFrom (address)));
}

Address
Isa100NetDevice::GetAddress (void) const
{
  NS_LOG_FUNCTION (this);

  Mac16AddressValue addressValue;
  m_dl->GetAttribute ("Address", addressValue);

  return Address(addressValue.Get());
}

bool
Isa100NetDevice::SetMtu (const uint16_t mtu)
{
	NS_ABORT_MSG ("Unsupported");
  return false;
}

uint16_t
Isa100NetDevice::GetMtu (void) const
{
  NS_ABORT_MSG ("Unsupported");
  return 0;
}

bool
Isa100NetDevice::IsLinkUp (void) const
{
  NS_LOG_FUNCTION (this);
  return m_phy != 0 && m_linkUp;
}

void
Isa100NetDevice::AddLinkChangeCallback (Callback<void> callback)
{
  NS_LOG_FUNCTION (this);
  m_linkChanges.ConnectWithoutContext (callback);
}

bool
Isa100NetDevice::IsBroadcast (void) const
{
  NS_LOG_FUNCTION (this);
  return true;
}

Address
Isa100NetDevice::GetBroadcast (void) const
{
  NS_ABORT_MSG ("Unsupported; add me");
  return Address ();
}

bool
Isa100NetDevice::IsMulticast (void) const
{
  NS_LOG_FUNCTION (this);
  return false;
}

Address
Isa100NetDevice::GetMulticast (Ipv4Address multicastGroup) const
{
  NS_ABORT_MSG ("Unsupported");
  return Address ();
}

Address
Isa100NetDevice::GetMulticast (Ipv6Address addr) const
{
  NS_ABORT_MSG ("Unsupported");
  return Address ();
}

bool
Isa100NetDevice::IsBridge (void) const
{
  NS_LOG_FUNCTION (this);
  return false;
}

bool
Isa100NetDevice::IsPointToPoint (void) const
{
  NS_LOG_FUNCTION (this);
  return false;
}

bool
Isa100NetDevice::Send (Ptr<Packet> packet, const Address& dest, uint16_t protocolNumber)
{
  // This method basically assumes an 802.3-compliant device, but a raw
  // 802.15.4 device does not have an ethertype, and requires specific
  // McpsDataRequest parameters.
  // For further study:  how to support these methods somehow, such as
  // inventing a fake ethertype and packet tag for McpsDataRequest


	 NS_ABORT_MSG ("Isa100NetDevice::Send -> Unsupported; use McpsDataRequest instead");
	 return false;
}

bool
Isa100NetDevice::SendFrom (Ptr<Packet> packet, const Address& source, const Address& dest, uint16_t protocolNumber)
{
  NS_ABORT_MSG ("Isa100NetDevice::SendFrom -> Unsupported; use McpsDataRequest instead");
  return false;
}

Ptr<Node>
Isa100NetDevice::GetNode (void) const
{
  NS_LOG_FUNCTION (this);
  return m_node;
}

void
Isa100NetDevice::SetNode (Ptr<Node> node)
{
  NS_LOG_FUNCTION (this);
  m_node = node;
  CompleteConfig ();
}

bool
Isa100NetDevice::NeedsArp (void) const
{
  NS_ABORT_MSG ("Unsupported");
  return false;
}

void
Isa100NetDevice::SetReceiveCallback (ReceiveCallback cb)
{
  // This method basically assumes an 802.3-compliant device, but a raw
  // 802.15.4 device does not have an ethertype, and requires specific
  // McpsDataIndication parameters.
  // For further study:  how to support these methods somehow, such as
  // inventing a fake ethertype and packet tag for McpsDataRequest
  NS_LOG_WARN ("Unsupported; use LrWpan MAC APIs instead");
}

void
Isa100NetDevice::SetPromiscReceiveCallback (PromiscReceiveCallback cb)
{
  // This method basically assumes an 802.3-compliant device, but a raw
  // 802.15.4 device does not have an ethertype, and requires specific
  // McpsDataIndication parameters.
  // For further study:  how to support these methods somehow, such as
  // inventing a fake ethertype and packet tag for McpsDataRequest
  NS_LOG_WARN ("Unsupported; use LrWpan MAC APIs instead");
}

bool
Isa100NetDevice::SupportsSendFrom (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return false;
}

} // namespace ns3
