/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2016 The University Of Calgary- FISHLAB
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
 * Author:   Michael Herrmann <mjherrma@ucalgary.ca>
 */

#include "zigbee-radio-energy-model-helper.h"
#include "ns3/basic-energy-source-helper.h"
#include "ns3/zigbee-phy.h"
#include "ns3/isa100-net-device.h"
#include "ns3/config.h"
#include "ns3/names.h"
#include "ns3/zigbee-tx-current-model.h"

namespace ns3 {

ZigbeeRadioEnergyModelHelper::ZigbeeRadioEnergyModelHelper ()
{
  m_radioEnergy.SetTypeId ("ns3::ZigbeeRadioEnergyModel");
  m_depletionCallback.Nullify ();
  m_rechargedCallback.Nullify ();
}

ZigbeeRadioEnergyModelHelper::~ZigbeeRadioEnergyModelHelper ()
{
}

void
ZigbeeRadioEnergyModelHelper::Set (std::string name, const AttributeValue &v)
{
  m_radioEnergy.Set (name, v);
}

void
ZigbeeRadioEnergyModelHelper::SetDepletionCallback (
  ZigbeeRadioEnergyModel::ZigbeeRadioEnergyDepletionCallback callback)
{
  m_depletionCallback = callback;
}

void
ZigbeeRadioEnergyModelHelper::SetRechargedCallback (
  ZigbeeRadioEnergyModel::ZigbeeRadioEnergyRechargedCallback callback)
{
  m_rechargedCallback = callback;
}

void
ZigbeeRadioEnergyModelHelper::SetTxCurrentModel (std::string name,
                                               std::string n0, const AttributeValue& v0,
                                               std::string n1, const AttributeValue& v1,
                                               std::string n2, const AttributeValue& v2,
                                               std::string n3, const AttributeValue& v3,
                                               std::string n4, const AttributeValue& v4,
                                               std::string n5, const AttributeValue& v5,
                                               std::string n6, const AttributeValue& v6,
                                               std::string n7, const AttributeValue& v7)
{
  ObjectFactory factory;
  factory.SetTypeId (name);
  factory.Set (n0, v0);
  factory.Set (n1, v1);
  factory.Set (n2, v2);
  factory.Set (n3, v3);
  factory.Set (n4, v4);
  factory.Set (n5, v5);
  factory.Set (n6, v6);
  factory.Set (n7, v7);
  m_txCurrentModel = factory;
}


/*
 * Private function starts here.
 */

Ptr<DeviceEnergyModel>
ZigbeeRadioEnergyModelHelper::DoInstall (Ptr<NetDevice> device,
                                       Ptr<EnergySource> source) const
{
  NS_ASSERT (device != NULL);
  NS_ASSERT (source != NULL);
  // check if device is Isa100NetDevice
  std::string deviceName = device->GetInstanceTypeId ().GetName ();
  if (deviceName.compare ("ns3::Isa100NetDevice") != 0)
    {
      NS_FATAL_ERROR ("NetDevice type is not Isa100NetDevice!");
    }
  Ptr<Node> node = device->GetNode ();
  Ptr<ZigbeeRadioEnergyModel> model = m_radioEnergy.Create ()->GetObject<ZigbeeRadioEnergyModel> ();
  NS_ASSERT (model != NULL);
  // set energy source pointer
  model->SetEnergySource (source);
  // set energy depletion callback
  // if none is specified, make a callback to ZigbeePhy::EnergyDepleted
  Ptr<Isa100NetDevice> isa100Device = DynamicCast<Isa100NetDevice> (device);
  Ptr<ZigbeePhy> zigbeePhy = isa100Device->GetPhy ();
  if (m_depletionCallback.IsNull ())
    {
      model->SetEnergyDepletionCallback (MakeCallback (&ZigbeePhy::EnergyDepleted, zigbeePhy));
    }
  else
    {
      model->SetEnergyDepletionCallback (m_depletionCallback);
    }
  // set energy recharged callback
  // if none is specified, make a callback to ZigbeePhy::EnergyReplenished
  if (m_rechargedCallback.IsNull ())
    {
      model->SetEnergyRechargedCallback (MakeCallback (&ZigbeePhy::EnergyReplenished, zigbeePhy));
    }
  else
    {
      model->SetEnergyRechargedCallback (m_rechargedCallback);
    }
  // add model to device model list in energy source
  source->AppendDeviceEnergyModel (model);
  // create and register energy model phy listener
  zigbeePhy->RegisterListener (model->GetPhyListener ());
  // add a current model if specified
  if (m_txCurrentModel.GetTypeId ().GetUid ())
    {
      Ptr<ZigbeeTxCurrentModel> txcurrent = m_txCurrentModel.Create<ZigbeeTxCurrentModel> ();
      model->SetTxCurrentModel (txcurrent);
    }
  return model;
}

} // namespace ns3
