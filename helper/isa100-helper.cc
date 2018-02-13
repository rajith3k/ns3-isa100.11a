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
 */


#include "ns3/isa100-helper.h"

#include "ns3/position-allocator.h"
#include "ns3/output-stream-wrapper.h"
#include "ns3/log.h"
#include "ns3/double.h"
#include "ns3/uinteger.h"
#include "ns3/integer.h"
#include "ns3/boolean.h"

#include "ns3/zigbee-trx-current-model.h"
#include "ns3/constant-position-mobility-model.h"
#include "ns3/single-model-spectrum-channel.h"
#include "ns3/isa100-application.h"
#include "ns3/isa100-routing.h"
#include "ns3/isa100-net-device.h"
#include "ns3/goldsmith-tdma-optimizer.h"
#include "ns3/minhop-tdma-optimizer.h"
#include "ns3/convex-integer-tdma-optimizer.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/packet.h"


NS_LOG_COMPONENT_DEFINE ("Isa100Helper");

namespace ns3 {

TypeId
Isa100Helper::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::Isa100Helper")
    .SetParent<Object> ()
    .AddConstructor<Isa100Helper> ()

		.AddTraceSource("NodeLocations",
				"Node locations.",
				MakeTraceSourceAccessor (&Isa100Helper::m_locationTrace),
				"ns3::TracedCallback::Location")

    .AddTraceSource("HopTrace",
		    "Number of hops for each node.",
        MakeTraceSourceAccessor (&Isa100Helper::m_hopTrace),
				"ns3::TracedCallback::Hops")

  ;
  return tid;
}



Isa100Helper::Isa100Helper(void)
{
  NS_LOG_FUNCTION (this);

  m_txPwrDbm = 0;
}

Isa100Helper::~Isa100Helper(void)
{
	if(m_txPwrDbm){
		for(int i=0; i < m_devices.GetN(); i++)
  			delete[] m_txPwrDbm[i];
		delete[] m_txPwrDbm;
	}
}


NetDeviceContainer Isa100Helper::Install (NodeContainer c, Ptr<SingleModelSpectrumChannel> channel, uint32_t sinkIndex)
{
  NS_LOG_FUNCTION (this);

	for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i){

		Ptr<Node> node = *i;
		Ptr<Isa100NetDevice> device = CreateObject<Isa100NetDevice>();

		SetDlAttributes(device);
		SetPhyAttributes (device->GetPhy ());
		device->GetPhy()->SetTrxCurrentAttributes(m_trxCurrentAttributes);

		uint8_t addrBuffer[2];
		addrBuffer[1] = 0xff & (node->GetId());
		addrBuffer[0] = 0xff & (node->GetId() >> 8);

		Mac16Address address;
		address.CopyFrom(addrBuffer);

		device->GetDl()->SetAttribute("Address",Mac16AddressValue(address));
		device->SetChannel(channel);
		device->SetNode(node);
		device->GetPhy()->SetDevice(device);

    // Add the device to the node and the devices list
		node->AddDevice (device);
		m_devices.Add (device);

	}
	return m_devices;
}



void Isa100Helper::SetSourceRoutingTable(uint32_t nodeInd, uint32_t numNodes,  std::string *routingTable)
{
	Ptr<NetDevice> baseDevice = m_devices.Get(nodeInd);
	Ptr<Isa100NetDevice> netDevice = baseDevice->GetObject<Isa100NetDevice>();

	if(!baseDevice || !netDevice)
		NS_FATAL_ERROR("Installing routing table on non-existent ISA100 net device.");

	Ptr<Isa100RoutingAlgorithm> routingAlgorithm = CreateObject<Isa100SourceRoutingAlgorithm>(numNodes,routingTable);

	netDevice->GetDl()->SetRoutingAlgorithm(routingAlgorithm);

	Mac16AddressValue address;
	netDevice->GetDl()->GetAttribute("Address",address);
	netDevice->GetDl()->GetRoutingAlgorithm()->SetAttribute("Address",address);

}

void Isa100Helper::InstallBattery(uint32_t nodeIndex, Ptr<Isa100Battery> battery)
{

  NS_LOG_FUNCTION (this);

	Ptr<NetDevice> baseDevice = m_devices.Get(nodeIndex);
	Ptr<Isa100NetDevice> devPtr = baseDevice->GetObject<Isa100NetDevice>();

	if(!devPtr || !devPtr->GetDl() || !devPtr->GetPhy())
		NS_FATAL_ERROR("Installing battery on a unconfigured net device or non-existent node.");

	battery->SetDevicePointer(devPtr);

  devPtr->GetPhy()->SetBatteryCallback( MakeCallback(&Isa100Battery::DecrementEnergy, battery) );
  battery->SetConsumptionCategories(devPtr->GetPhy()->GetEnergyCategories());

  if(devPtr->GetProcessor()){
  	devPtr->GetProcessor()->SetBatteryCallback( MakeCallback(&Isa100Battery::DecrementEnergy, battery) );
  	battery->SetConsumptionCategories(devPtr->GetProcessor()->GetEnergyCategories());
  }

  if(devPtr->GetSensor()){
  	devPtr->GetSensor()->SetBatteryCallback( MakeCallback(&Isa100Battery::DecrementEnergy, battery) );
  	battery->SetConsumptionCategories(devPtr->GetSensor()->GetEnergyCategories());
  }

  devPtr->SetBattery(battery);

}

void Isa100Helper::InstallProcessor(uint32_t nodeIndex, Ptr<Isa100Processor> processor)
{
  NS_LOG_FUNCTION (this);

	Ptr<NetDevice> baseDevice = m_devices.Get(nodeIndex);
	Ptr<Isa100NetDevice> devPtr = baseDevice->GetObject<Isa100NetDevice>();

	if(!devPtr || !devPtr->GetDl() || !devPtr->GetPhy())
		NS_FATAL_ERROR("Installing processor on a unconfigured net device or non-existent node.");

	devPtr->GetDl()->SetProcessor(processor);

  if(devPtr->GetBattery()){
  	processor->SetBatteryCallback( MakeCallback(&Isa100Battery::DecrementEnergy, devPtr->GetBattery()) );
  	devPtr->GetBattery()->SetConsumptionCategories(processor->GetEnergyCategories());
  }

  devPtr->SetProcessor(processor);

}

void Isa100Helper::InstallSensor(uint32_t nodeIndex, Ptr<Isa100Sensor> sensor)
{
  NS_LOG_FUNCTION (this);

	Ptr<NetDevice> baseDevice = m_devices.Get(nodeIndex);
	Ptr<Isa100NetDevice> devPtr = baseDevice->GetObject<Isa100NetDevice>();

	if(!devPtr || !devPtr->GetDl() || !devPtr->GetPhy())
		NS_FATAL_ERROR("Installing processor on a unconfigured net device or non-existent node.");

  if(devPtr->GetBattery()){
  	sensor->SetBatteryCallback( MakeCallback(&Isa100Battery::DecrementEnergy, devPtr->GetBattery()) );
  	devPtr->GetBattery()->SetConsumptionCategories(sensor->GetEnergyCategories());
  }

  devPtr->SetSensor(sensor);

}



void Isa100Helper::InstallApplication(NodeContainer c, uint32_t nodeIndex, Ptr<Isa100Application> app)
{
  NS_LOG_FUNCTION (this);

	Ptr<NetDevice> baseDevice = m_devices.Get(nodeIndex);
	Ptr<Isa100NetDevice> devPtr = baseDevice->GetObject<Isa100NetDevice>();

	if(!devPtr)
		NS_FATAL_ERROR("Installing ISA100 application on non-existent node.");

	// Callbacks for Application to Dl communication
	app->SetDlDataRequestCallback(MakeCallback(&Isa100Dl::DlDataRequest,devPtr->GetDl()));

  // Callbacks for DL to Application layer communication
  devPtr->GetDl()->SetDlDataIndicationCallback (MakeCallback (&Isa100Application::DlDataIndication, app));
  devPtr->GetDl()->SetDlDataConfirmCallback (MakeCallback (&Isa100Application::DlDataConfirm, app));


	Ptr<Node> node = c.Get(nodeIndex);

	if(!node)
		NS_FATAL_ERROR("Installing ISA100 application on non-existent node.");

	app->SetNode(node);
	node->AddApplication(app);


}





void Isa100Helper::SetDlAttribute(std::string n, const AttributeValue &v)
{
  NS_LOG_FUNCTION (this);

	// Necessary?  If it's empty (ie. brand new) shouldn't it be cleared already?
	if (m_dlAttributes.empty())
		m_dlAttributes.clear();

	m_dlAttributes.insert ( std::pair< std::string, Ptr<AttributeValue> > (n,v.Copy()) );
}

void Isa100Helper::SetDlAttributes(Ptr<Isa100NetDevice> device)
{
  NS_LOG_FUNCTION (this);

  // Not all DL attributes have default values that will allow the simulation to operate.
  if(!m_dlAttributes.size())
  		NS_FATAL_ERROR("Installed ISA100 net device before configuring its attributes.");

	std::map<std::string,Ptr<AttributeValue> >::iterator it;
	for (it=m_dlAttributes.begin(); it!=m_dlAttributes.end(); ++it)
	{
		std::string name = it->first;
		if(!name.empty())
		{
			device->GetDl()->SetAttribute(it->first , *it->second);
		}
	}
}

void Isa100Helper::SetPhyAttribute(std::string n, const AttributeValue &v)
{
  NS_LOG_FUNCTION (this);

  m_phyAttributes.insert ( std::pair< std::string, Ptr<AttributeValue> > (n,v.Copy()) );
}

void Isa100Helper::SetPhyAttributes(Ptr<ZigbeePhy> phy)
{
  NS_LOG_FUNCTION (this);

  std::map<std::string,Ptr<AttributeValue> >::iterator it;
  for (it=m_phyAttributes.begin(); it!=m_phyAttributes.end(); ++it)
  {
    std::string name = it->first;
    if(!name.empty())
    {
      phy->SetAttribute(it->first , *it->second);
    }
  }
}




void Isa100Helper::SetTrxCurrentAttribute(std::string n, const AttributeValue &v)
{
  NS_LOG_FUNCTION (this);

  m_trxCurrentAttributes.insert (std::pair< std::string, Ptr<AttributeValue> > (n,v.Copy ()));
}




}
