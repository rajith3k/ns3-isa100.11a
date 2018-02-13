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

#include "ns3/isa100-processor.h"
#include "ns3/isa100-battery.h"

#include "ns3/log.h"
#include "ns3/abort.h"
#include "ns3/double.h"

NS_LOG_COMPONENT_DEFINE ("Isa100Processor");

using namespace ns3;
using namespace std;


NS_OBJECT_ENSURE_REGISTERED (Isa100Processor);

TypeId
Isa100Processor::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::Isa100Processor")
    .SetParent<NetDevice> ()
    .AddConstructor<Isa100Processor> ()

    .AddAttribute ("ActiveCurrent",
    		"Amount of current consumed when active (A).",
				DoubleValue (0.0),
				MakeDoubleAccessor (&Isa100Processor::m_currentActive),
				MakeDoubleChecker<double> (0))
		.AddAttribute ("SleepCurrent",
				"Amount of current consumed when sleeping (A).",
				DoubleValue (0.0),
				MakeDoubleAccessor (&Isa100Processor::m_currentSleep),
				MakeDoubleChecker<double> (0))
	 .AddAttribute ("SupplyVoltage",
			 "Supply voltage (V).",
			 DoubleValue (0.0),
			 MakeDoubleAccessor (&Isa100Processor::m_supplyVoltage),
			 MakeDoubleChecker<double> (0))

  ;
  return tid;
}


Isa100Processor::Isa100Processor ()
{
  NS_LOG_FUNCTION (this);

  int nTypes = 2;
  string energyTypes[] = {
  		"ProcessorActive",
  		"ProcessorSleeping"
  };

  m_energyCategories.assign(energyTypes,energyTypes+nTypes);

  m_lastUpdateTime = Seconds(0.0);

/*  m_currentActive = 0;
  m_currentSleep = 0;
  m_supplyVoltage = 0;
*/

  m_state = PROCESSOR_SLEEP;
  m_current = 0;
  m_energyCategory = "ProcessorSleep";
}


Isa100Processor::~Isa100Processor ()
{
  NS_LOG_FUNCTION (this);
}


vector<string>&
Isa100Processor::GetEnergyCategories()
{
	return m_energyCategories;
}

void
Isa100Processor::SetBatteryCallback(BatteryDecrementCallback c)
{
	NS_LOG_FUNCTION(this);
	m_batteryDecrementCallback = c;
}

double
Isa100Processor::GetActiveCurrent()
{
	return m_currentActive;
}

double
Isa100Processor::GetSleepCurrent()
{
	return m_currentSleep;
}

void
Isa100Processor::SetState(Isa100ProcessorState state)
{
	if(state == m_state)
		return;

	NS_LOG_FUNCTION (this);

  Time duration = Simulator::Now () - m_lastUpdateTime;
  NS_ASSERT (duration.GetNanoSeconds () >= 0);

  double energyConsumed = m_current * duration.GetSeconds() * m_supplyVoltage * 1e6;
  if(!m_batteryDecrementCallback.IsNull())
  	m_batteryDecrementCallback(m_energyCategory,energyConsumed);

  NS_LOG_LOGIC(" Current state " << m_energyCategories[state] << ", consumed " << energyConsumed << " uJ in " << duration.GetMilliSeconds() << " ms");

	m_state = state;
	m_lastUpdateTime = Simulator::Now();

	switch(m_state)
	{
		case PROCESSOR_ACTIVE:
			m_current = m_currentActive;
			m_energyCategory = "ProcessorActive";
			break;

		case PROCESSOR_SLEEP:
			m_current = m_currentSleep;
			m_energyCategory = "ProcessorSleeping";
			break;
	}
}









