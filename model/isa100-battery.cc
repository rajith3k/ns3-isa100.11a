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
 *
*/

#include "ns3/isa100-battery.h"

#include "ns3/log.h"
#include "ns3/abort.h"
#include "ns3/double.h"
#include <map>

#include "ns3/mac16-address.h"
#include "ns3/simulator.h"

NS_LOG_COMPONENT_DEFINE ("Isa100Battery");

using namespace ns3;
using namespace std;

NS_OBJECT_ENSURE_REGISTERED (Isa100Battery);

TypeId
Isa100Battery::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::Isa100Battery")
    .SetParent<Object> ()
    .AddConstructor<Isa100Battery> ()

    .AddAttribute ("Energy","Amount of energy in the battery (uJ).",
            DoubleValue(0),
            MakeDoubleAccessor(&Isa100Battery::m_energy),
            MakeDoubleChecker<double>())

		.AddTraceSource("EnergyConsumption",
				" Trace tracking energy consumed by category.",
				MakeTraceSourceAccessor (&Isa100Battery::m_energyConsumptionTrace),
				"ns3::TracedCallback::Energy")

  ;
  return tid;
}


Isa100Battery::Isa100Battery ()
{
  NS_LOG_FUNCTION (this);

  m_energy = 0;
  m_device = 0;
}

Isa100Battery::~Isa100Battery ()
{
  NS_LOG_FUNCTION (this);
}


void Isa100Battery::SetInitEnergy(double initEnergy)
{
	m_initEnergy = initEnergy;
	m_energy = m_initEnergy;
}

void Isa100Battery::ZeroConsumptionCategories()
{
  map<string,double>::iterator categoryIter;
  for(categoryIter = m_energyBreakdown.begin(); categoryIter != m_energyBreakdown.end(); categoryIter++)
  	categoryIter->second = 0;
}

void Isa100Battery::SetConsumptionCategories(vector<string> &categories)
{
	for(int n=0; n < categories.size(); n++){
		m_energyBreakdown[categories[n]] = 0;
	}
}

void Isa100Battery::SetDevicePointer(Ptr<NetDevice> device)
{
	m_device = device;
}

void Isa100Battery::SetBatteryDepletionCallback(BatteryDepletionCallback c)
{
	m_depletionCallback = c;
}

double Isa100Battery::GetEnergy() const
{
	return m_energy;
}

double Isa100Battery::GetInitialEnergy() const
{
	return m_initEnergy;
}

void Isa100Battery::DecrementEnergy(string category, double amount)
{
  // Do not update if simulation has finished
  if (Simulator::IsFinished ())
  {
    return;
  }

	m_energyBreakdown[category] += amount;
	m_energy -= amount;

	NS_ASSERT(m_device != 0);
  Mac16Address addr = Mac16Address::ConvertFrom(m_device->GetAddress());

  m_energyConsumptionTrace(addr, category, amount, m_energy, m_initEnergy);

  NS_LOG_LOGIC(Simulator::Now().GetSeconds() << "s: Node " << addr << " has consumed " << amount << "uJ in category " << category << " (Total Battery: " << m_energy << ")");

	if(m_energy <= 0){
		m_energy = 0;
	  m_energyConsumptionTrace(addr, "DEPLETION", amount, m_energy, m_initEnergy);

		if(!m_depletionCallback.IsNull()){
			m_depletionCallback(addr);
		}
	}
}

void Isa100Battery::PrintEnergySummary (Ptr<OutputStreamWrapper> stream)
{
  int64_t timenow = Simulator::Now().GetNanoSeconds();

	NS_ASSERT(m_device != 0);
  Mac16Address addr = Mac16Address::ConvertFrom(m_device->GetAddress());

  *stream->GetStream()
	        << timenow << "," << addr << ",Total," << m_energy << std::endl;

  map<string,double>::iterator categoryIter;
  for(categoryIter = m_energyBreakdown.begin(); categoryIter != m_energyBreakdown.end(); categoryIter++){
  	*stream->GetStream()
  	        << timenow << "," << addr << "," << categoryIter->first << "," << categoryIter->second << std::endl;
  }

}









