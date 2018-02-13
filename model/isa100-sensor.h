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
 * Author: Geoff Messier <gmessier@ucalgary.ca>
 *
 */


#ifndef ISA100_SENSOR_H
#define ISA100_SENSOR_H

#include <vector>
#include <string>
#include "ns3/simulator.h"
#include "ns3/event-id.h"
#include "ns3/traced-callback.h"
#include "ns3/net-device.h"
#include "ns3/isa100-battery.h"



using namespace ns3;
using namespace std;

/** Enum for indexing processor states.
 */
typedef enum{
	SENSOR_ACTIVE,
	SENSOR_IDLE
} Isa100SensorState;

/** Callback used when sensing operation completes.
 *
 * @param data Sampled sensor data.
 */
typedef Callback< void, double > SensingCallback;

class Isa100Sensor : public Object
{
public:

  static TypeId GetTypeId (void);

  Isa100Sensor();

  virtual ~Isa100Sensor ();

  /** Get the energy consumption categories for the processor.
   *
   * @return Vector of string names of the categories.
   */
  vector<string>& GetEnergyCategories();

  /** Set the callback function to decrement battery energy.
   *
   * @param c Callback function.
   */
  void SetBatteryCallback(BatteryDecrementCallback c);

  /** Set active current.
   *
   * @param current Current (A).
   */
  void SetActiveCurrent(double current);

  /** Set sleep current.
   *
   * @param current Current (A).
   */
  void SetSleepCurrent(double current);

  /** Set supply voltage.
   *
   * @param voltage Voltage value (V).
   */
  void SetSupplyVoltage(double voltage);


  /** Set sensing callback.
   *
   * @param c Callback function.
   */
  void SetSensingCallback(SensingCallback c);

  /** Start the sensing operation.
   *
   */
  void StartSensing();


private:

  /** Set sensor state.
   *
   * @param state New state value.
   */
  void SetState(Isa100SensorState state);

  /** Called at the end of the sensing operation.
   *
   */
  void EndSensing();

  vector<string> m_energyCategories; /// Energy consumption categories.

  BatteryDecrementCallback m_batteryDecrementCallback;  /// Callback function used to decrement battery energy.

	Isa100SensorState m_state; /// Sensor state.

  double m_current; /// Current current (A).
  double m_currentActive; /// Active current (A).
  double m_currentIdle; /// Idle current (A).
  double m_supplyVoltage; /// Supply voltage (V).

  SensingCallback m_sensingCallback;  /// Function called when the sensing operation completes.

  Time m_sensingTime; /// Time required to perform the sensing operation.
  Time m_lastUpdateTime; /// Last time the energy consumption of the processor was updated.
  string m_energyCategory; /// Current energy consumption category string.
};

#endif
