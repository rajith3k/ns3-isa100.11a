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


#ifndef ISA100_BATTERY_H
#define ISA100_BATTERY_H

#include <map>
#include <string>
#include <vector>
#include "ns3/event-id.h"
#include "ns3/traced-callback.h"
#include "ns3/net-device.h"
#include "ns3/output-stream-wrapper.h"



using namespace ns3;
using namespace std;


/** Callback to decrement battery energy
 *
 * @param category String indicating category name.
 * @param amount Amount of energy to decrement (uJ).
 */
typedef Callback< void, string, double > BatteryDecrementCallback;


/** Callback used when battery energy is depleted.
 *
 * @param addr Address of the depleted node.
 */
typedef Callback< void, Mac16Address > BatteryDepletionCallback;

/** Energy consumption trace callback.
 *
 * @param address Address of node.
 * @param category String indicating energy category.
 * @param amount Amount of energy decremented (uJ).
 * @param currEnergy Current amount of energy (uJ).
 * @param initEnergy Initial amount of energy (uJ).
 */
typedef TracedCallback<Mac16Address, std::string, double, double, double> BatteryEnergyTraceCallback;

class Isa100Battery : public Object
{
public:

  static TypeId GetTypeId (void);

  Isa100Battery();
  virtual ~Isa100Battery();

  /** Set the initial battery energy.
   *
   * @param initEnergy Initial energy.
   */
  void SetInitEnergy(double initEnergy);

  /** Define a category that identifies a type of energy consumption.
   *
   * @param category String identifying the energy consumption category.
   */
  void SetConsumptionCategories(vector<string> &categories);

  /** Decrement battery energy.
   *
   * @param category The energy consumption category.
   * @param amount Amount of energy to decrement in uJ.
   */
  void DecrementEnergy(string category, double amount);

  /** Get amount of energy in the battery.
   *
   */
  double GetEnergy() const;

  /** Get the initial amount of energy in the battery.
   *
   */
  double GetInitialEnergy() const;

  /** Set the net device pointer.
   *
   * @param device Pointer to the net device.
   */
  void SetDevicePointer(Ptr<NetDevice> device);

  /** Sets the battery depletion callback function.
   *
   * @param c Callback function.
   */
  void SetBatteryDepletionCallback(BatteryDepletionCallback c);

  /** Prints an energy consumption breakdown.
   *
   * @stream Stream for the output.
   */
  void PrintEnergySummary (Ptr<OutputStreamWrapper> stream);

  /** Resets energy consumption categories to zero.
   *
   */
  void ZeroConsumptionCategories();


private:

  double m_energy; /// Energy left in battery (uJ).
  double m_initEnergy; /// Initial energy.
  map<string,double> m_energyBreakdown; /// Breaks out battery energy consumption into different categories.

  Ptr<NetDevice> m_device; /// Pointer to the net device that contains the battery.

  BatteryEnergyTraceCallback m_energyConsumptionTrace;  /// Energy consumption trace.
  BatteryDepletionCallback m_depletionCallback; /// Depletion callback.


};


#endif
