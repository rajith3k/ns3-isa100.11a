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

#include "ns3/isa100-sensor.h"
#include "ns3/isa100-battery.h"

#include "ns3/log.h"
#include "ns3/abort.h"
#include "ns3/double.h"

NS_LOG_COMPONENT_DEFINE ("Isa100Sensor");

using namespace ns3;
using namespace std;


NS_OBJECT_ENSURE_REGISTERED (Isa100Sensor);

TypeId
Isa100Sensor::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::Isa100Sensor")
    .SetParent<NetDevice> ()
    .AddConstructor<Isa100Sensor> ()

    .AddAttribute ("SensingTime","Time required to perform a sensing operation (s).",
    		TimeValue(Seconds(0.0)),
				MakeTimeAccessor(&Isa100Sensor::m_sensingTime),
				MakeTimeChecker())

	  .AddAttribute ("ActiveCurrent",
	  		"Amount of current consumed when the sensor is active (A).",
				DoubleValue (0.0),
				MakeDoubleAccessor (&Isa100Sensor::m_currentActive),
				MakeDoubleChecker<double> (0))

    .AddAttribute ("IdleCurrent",
    		"Amount of current consumed when the sensor is idle (A).",
				DoubleValue (0.0),
				MakeDoubleAccessor (&Isa100Sensor::m_currentIdle),
				MakeDoubleChecker<double> (0))

	  .AddAttribute ("SupplyVoltage",
	  		"Supply voltage (V).",
				DoubleValue (0.0),
				MakeDoubleAccessor (&Isa100Sensor::m_supplyVoltage),
				MakeDoubleChecker<double> (0))




  ;
  return tid;
}


Isa100Sensor::Isa100Sensor ()
{
  NS_LOG_FUNCTION (this);

  int nTypes = 2;
  string energyTypes[] = {
  		"SensorActive",
  		"SensorIdle"
  };

  m_energyCategories.assign(energyTypes,energyTypes+nTypes);

  m_lastUpdateTime = Seconds(0.0);
  m_sensingTime = Seconds(0.0);

  m_currentActive = 0;
  m_currentIdle = 0;
  m_supplyVoltage = 0;

  m_state = SENSOR_IDLE;
  m_current = m_currentIdle;
  m_energyCategory = "SensorIdle";
}


Isa100Sensor::~Isa100Sensor ()
{
  NS_LOG_FUNCTION (this);
}


vector<string>&
Isa100Sensor::GetEnergyCategories()
{
	return m_energyCategories;
}

void
Isa100Sensor::SetBatteryCallback(BatteryDecrementCallback c)
{
	NS_LOG_FUNCTION(this);
	m_batteryDecrementCallback = c;
}

void
Isa100Sensor::SetActiveCurrent(double current)
{
	m_currentActive = current;
}

void
Isa100Sensor::SetSupplyVoltage(double voltage)
{
	m_supplyVoltage = voltage;
}

void
Isa100Sensor::StartSensing()
{
	NS_ASSERT(m_state == SENSOR_IDLE);

	SetState(SENSOR_ACTIVE);

	Simulator::Schedule(m_sensingTime,&Isa100Sensor::EndSensing,this);
}

void
Isa100Sensor::EndSensing()
{
	SetState(SENSOR_IDLE);

	// For now, we just return zero samples.
	m_sensingCallback(0.0);
}




void
Isa100Sensor::SetState(Isa100SensorState state)
{
	if(state == m_state)
		return;

	NS_LOG_FUNCTION (this);

  Time duration = Simulator::Now () - m_lastUpdateTime;
  NS_ASSERT (duration.GetNanoSeconds () >= 0);

  double energyConsumed = m_current * duration.GetSeconds() * m_supplyVoltage * 1e6 ;
  if(!m_batteryDecrementCallback.IsNull())
  	m_batteryDecrementCallback(m_energyCategory,energyConsumed);

  NS_LOG_LOGIC(" State " << m_energyCategories[m_state] << " to "<< m_energyCategories[state] << ", after: " << duration.GetSeconds() << "s, consumed " << energyConsumed << " uJ");

  m_state = state;
  m_lastUpdateTime = Simulator::Now();

	switch(m_state)
	{
		case SENSOR_ACTIVE:
			m_current = m_currentActive;
			m_energyCategory = "SensorActive";
			break;

		case SENSOR_IDLE:
			m_current = m_currentIdle;
			m_energyCategory = "SensorIdle";
			break;
	}
}

void Isa100Sensor::SetSensingCallback(SensingCallback c)
{
	m_sensingCallback = c;
}







