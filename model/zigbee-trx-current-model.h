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

#ifndef ZIGBEE_TRX_CURRENT_MODEL_H
#define ZIGBEE_TRX_CURRENT_MODEL_H

#include "ns3/object.h"

namespace ns3 {

/**
 * \ingroup energy
 * 
 * \class ZigbeeTrxCurrentModel
 *
 * \brief The current consumption model of the Zigbee radio
 *
 * There are two current consuming components: the transceiver and processor.
 *
 * The processor has two currents:
 *   - processor sleep current
 *   - processor active current
 *
 * The transceiver has a current corresponding to each state
 *   - trx off current
 *   - rx on current
 *   - busy rx current
 *   - tx on current
 *   - busy tx current
 *   - sleep current
 *
 * This model assumes that the transmit current is a linear function
 * of the transmit power. This makes sense because P = I*V where power, P, is
 * directly proportional to voltage, V, and also to current, I.
 *
 * The transmit current is configured with two parameters: slope and offset. Start with
 * a data set, S, of transmit current vs. transmit power (often found on
 * transceiver datasheets). Create a line of best fit for S and the line's
 * slope and offset (or y-intercept) are the values needed to configure this model.
 *
 * The default values used for the tx current come from an Atmel Zigbee transceiver,
 * AT86RF233. The given Tx power and current relationship from the datasheet
 * are as follows (P_tx, I_tx):
 *       - (4 dBm  , 13.8 mA)
 *       - (0 dBm  , 11.8 mA)
 *       - (-17 dBm, 7.2 mA )
 *
 *  Current (A) vs. power (dBm) was plotted and the line of best fit results in:
 *       - slope  = 0.0003013 A/dBm
 *       - offset = 0.01224 dBm
 *
 *  The defaults for this model are from two devices:
 *  - (1) ZigBee Transceiver: AT86RF233
 *      -# TRX_OFF: 300uA
 *      -# RX_ON: 11.8mA
 *      -# TX_ON: 5.2mA
 *      -# BUSY_TX: 13.8mA (max)
 *      -# SLEEP: 20nA
 *  - (2) 32-bit Microcontroller: AT32UC3B
 *      -# Active: 18.5mA
 *      -# Sleep: 24.9uA
 */
class ZigbeeTrxCurrentModel : public Object
{
public:
  static TypeId GetTypeId (void);

  ZigbeeTrxCurrentModel ();
  virtual ~ZigbeeTrxCurrentModel ();

  /**
   * \param slope (A/dBm)
   *
   * Set the slope of the Tx Current(A) vs Tx Power(dBm) relationship.
   * This can be obtained from a linear regression of data points.
   */
  void SetTxCurrentPowerSlope (double slope);

  /**
   * \param offset (A)
   *
   * Set the offset of the Tx Current(A) vs Tx Power(dBm) relationship.
   * This can be obtained from a linear regression of data points.
   */
  void SetTxCurrentPowerOffset (double offset);

  /**
   * \return the Tx Current(A) vs Tx Power(dBm) slope.
   */
  double GetTxCurrentPowerSlope (void) const;

  /**
   * \return the Tx Current(A) vs Tx Power(dBm) offset.
   */
  double GetTxCurrentPowerOffset (void) const;

  /**
   * \brief Calculates and updates the tx current using the given tx power
   * \param txPowerDbm the nominal tx power in dBm
   */
  void UpdateTxCurrent (double txPowerDbm);

  // Setter & getters for different current consumptions.
  double GetProcessorSleepCurrentA (void) const;
  void SetProcessorSleepCurrentA (double sleepCurrentA);
  double GetProcessorActiveCurrentA (void) const;
  void SetProcessorActiveCurrentA (double activeCurrentA);
  double GetProcessorIdleCurrentA (void) const;
  void SetProcessorIdleCurrentA (double idleCurrentA);

  double GetTrxOffCurrentA (void) const;
  void SetTrxOffCurrentA (double trxOffCurrentA);
  double GetRxOnCurrentA (void) const;
  void SetRxOnCurrentA (double rxOnCurrentA);
  double GetBusyRxCurrentA (void) const;
  void SetBusyRxCurrentA (double rxOnCurrentA);
  double GetTxOnCurrentA (void) const;
  void SetTxOnCurrentA (double txOnCurrentA);
  double GetBusyTxCurrentA (void) const;
  double GetBusyTxCurrentA (double) const;
  void SetBusyTxCurrentA (double busyTxCurrentA);
  double GetSleepCurrentA (void) const;
  void SetSleepCurrentA (double sleepCurrentA);


private:
  // Current draw of microprocessor in different modes.
  double m_processorSleepCurrentA;
  double m_processorActiveCurrentA;
  double m_processorIdleCurrentA;

  // Current draw in different radio modes.
  double m_trxOffCurrentA;
  double m_rxOnCurrentA;
  double m_busyRxCurrentA;
  double m_txOnCurrentA;
  double m_busyTxCurrentA;
  double m_sleepCurrentA;

  // Tx current characteristics
  double m_slope;
  double m_offset;

};

} // namespace ns3

#endif /* ZIGBEE_TRX_CURRENT_MODEL_H */
