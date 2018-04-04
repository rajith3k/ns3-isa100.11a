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

#include "ns3/core-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/isa100-11a-module.h"

#include "ns3/trace-helper.h"

#include <iomanip>


// ************************************************** DEFINES *************************************************
// Defines for simulation
#define SIM_DURATION_S 1e9                  // Duration of simulation in (s) (really long so energy runs out)

// Defines for channel
#define PATH_LOSS_EXP 2.91                  // Path loss exponent from jp measurements
//#define SHADOWING_STD_DEV_DB 0.0           // Shadowing standard deviation from jp measurements (dB)
#define SHADOWING_STD_DEV_DB 4.58           // Shadowing standard deviation from jp measurements (dB)

// Topology
#define SENSOR_DENSITY 0.0093  // Nodes/m^2
#define MIN_NODE_SPACING 3.0               // Node spacing is at least this distance (m)
#define FIELD_SIZE_X 60.0  // Field size in the x direction.
#define FIELD_SIZE_Y 90.0  // Field size in the y direction.


// Defines for node applications
#define SENSOR_SAMPLE_DURATION_S  0.10     // Duration of a sensor sample (s)
#define SENSOR_SAMPLE_POWER_W     0.027    // Power required for performing a sensor sample (W)
#define PACKET_DATA_BYTES         40       // Size of Packet's data payload (bytes)
#define PACKET_OVERHEAD_BYTES 29 // Number of overhead bytes in a packet
#define SENSOR_SAMPLE_PERIOD 2.0 // Sample period (s)
#define TX_EARLIEST_S 2.212e-3  // Transmit dead time at the start of each timeslot (ms)

// DL layer defines
#define SINK_ADDR "00:00"                  // Data sink address

// Phy layer defines
#define DEFAULT_INITIAL_ENERGY_J 3.0       // Originally: 5J Default initial energy each sensor node has available (J)


#define RX_SENSITIVITY -101.0 // Receiver sensitivity (dBm)


// Macros
#define CREATE_STREAM_FILENAME(fnstr){  \
	ss.str( std::string() ); \
	ss.clear(); \
	ss << filePrefix << fnstr; \
	filename = ss.str(); \
	}

using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE ("RandomNetworkTdma");

// Global variables for simulation termination.
double networkLifetime;
Time terminateCheckPeriod;
int terminateSim = 0;

// ************************ CALLBACK FUNCTIONS ******************************

void BatteryDepletionCallbackEvent(Mac16Address addr)
{
	if(!terminateSim){
		networkLifetime = (Simulator::Now()).GetSeconds();
		NS_LOG_UNCOND(" Node " << addr << " out of energy at " << networkLifetime);
		terminateSim = 1;
	}

//	Simulator::Stop();
}



static void TerminateSimulation()
{
	if(terminateSim){
		NS_LOG_UNCOND(" Simulation terminated!");
		Simulator::Stop();
	}
	else
	  Simulator::Schedule(terminateCheckPeriod,&TerminateSimulation);
}

static void PrintDropPacket ( Ptr<OutputStreamWrapper> stream, Mac16Address addr, Ptr<const Packet> p, std::string message)
{
  *stream->GetStream() << Simulator::Now ().GetNanoSeconds () << "," << addr << ",#" << message << "#," << *p << std::endl;
}

vector<int> reportTxNum;
vector<int> reportRxNum;
vector<Time> reportTxTime;
vector<Time> reportTotalDelay;


static void LogReportTx(Ptr<OutputStreamWrapper> stream, Mac16Address addr )
{
	uint8_t buff[2];
	addr.CopyTo(buff);
	int nodeInd = ( (uint)buff[0] << 8 ) + (uint)buff[1];

	reportTxNum[nodeInd]++;
	reportTxTime[nodeInd] = Simulator::Now();

//	*stream->GetStream() << "Tx: "<< Simulator::Now().GetMilliSeconds() << ", " << addr << std::endl;
}

static void LogReportRx(Ptr<OutputStreamWrapper> stream, Mac16Address addr )
{
	uint8_t buff[2];
	addr.CopyTo(buff);
	int nodeInd = ( (uint)buff[0] << 8 ) + (uint)buff[1];

	reportRxNum[nodeInd]++;
	reportTotalDelay[nodeInd] += Simulator::Now() - reportTxTime[nodeInd];

//	*stream->GetStream() << "Rx: "<< Simulator::Now().GetMilliSeconds() << ", " << addr << std::endl;
}

static void LogHops(Ptr<OutputStreamWrapper> stream, vector<int> hops)
{
	double avgHops = 0;
	for(int iHop=0; iHop < hops.size(); iHop++)
		avgHops += hops[iHop];

	*stream->GetStream() << "AvgHops," << avgHops/hops.size() << std::endl;
}

static void PrintLocations(Ptr<OutputStreamWrapper> stream, int node, double x, double y, double z)
{
	float distToSink = sqrt((FIELD_SIZE_X/2-x)*(FIELD_SIZE_X/2-x) + y*y);
	*stream->GetStream() << "Node " << node << ": (" << x << "," << y << ") " << distToSink << "m from sink." << std::endl;
}


// ************************************************ MAIN BEGIN ************************************************
int main (int argc, char *argv[])
{
//	  LogComponentEnable("FishPropagationLossModel",LOG_LEVEL_LOGIC);
//		LogComponentEnable("Isa100Dl",LOG_LEVEL_LOGIC);
//	  LogComponentEnable("Isa100HelperScheduling",LOG_LEVEL_LOGIC);
//	  LogComponentEnable("MinHopTdmaOptimizer",LOG_LEVEL_LOGIC);
//	  LogComponentEnable("ConvexIntTdmaOptimizer",LOG_LEVEL_LOGIC);
//	  LogComponentEnable("TdmaOptimizerBase",LOG_LEVEL_LOGIC);


//	  LogComponentEnable("ZigbeePhy",LOG_LEVEL_LOGIC);
//	  LogComponentEnable("Isa100Battery",LOG_LEVEL_LOGIC);
//	  LogComponentEnable("Isa100Routing",LOG_LEVEL_LOGIC);


	/*  LogComponentEnable("ZigbeePhy",LOG_LEVEL_LOGIC);
	  LogComponentEnable("Isa100Processor",LOG_LEVEL_LOGIC);
	*/


	// Command Line Arguments
  uint32_t seed = 1002;
  std::string optString;
  unsigned int numSensorNodes=0;

  int iter = -1;

  CommandLine cmd;
  cmd.AddValue("rndSeed", "Seed for random number generation.", seed);
  cmd.AddValue("iter", "Iteration number.", iter);
  cmd.AddValue("nnodes", "Number of sensor nodes.",numSensorNodes);
//  cmd.AddValue("optType", "0 = min hop, 1 = Goldsmith, 2 = Convex Int", optimizerType);
  cmd.AddValue("optType","Optimization type: MinHop10ms, MinHopPckt, Goldsmith10ms, GoldsmithPckt, ConvInt10ms, ConvIntPckt",optString);

  cmd.Parse (argc, argv);

  uint16_t optimizerType;
  bool multiplePacketsPerSlot = false;
  Time slotDuration;
  unsigned int numSlotsPerFrame;


  if(optString == "MinHop10ms"){
  	optimizerType = TDMA_MIN_HOP;
  	slotDuration = MilliSeconds(10);
  }
  else if(optString == "MinHopPckt"){
  	optimizerType = TDMA_MIN_HOP;
  	slotDuration = Seconds((double)(PACKET_OVERHEAD_BYTES+PACKET_DATA_BYTES) * 8 / 250e3 + TX_EARLIEST_S);
  }
  else if(optString == "Goldsmith10ms"){
  	optimizerType = TDMA_GOLDSMITH;
  	slotDuration = MilliSeconds(10);
  }
  else if(optString == "GoldsmithPckt"){
  	optimizerType = TDMA_GOLDSMITH;
  	slotDuration = Seconds((double)(PACKET_OVERHEAD_BYTES+PACKET_DATA_BYTES) * 8 / 250e3 + TX_EARLIEST_S);
  }
  else if(optString == "ConvInt10ms"){
  	optimizerType = TDMA_CONVEX_INT;
  	slotDuration = MilliSeconds(10);
  }
  else if(optString == "ConvIntPckt"){
  	optimizerType = TDMA_CONVEX_INT;
  	slotDuration = Seconds((double)(PACKET_OVERHEAD_BYTES+PACKET_DATA_BYTES) * 8 / 250e3 + TX_EARLIEST_S);
  }
  else
  	NS_FATAL_ERROR("Command line optimization string incorrect.");

  numSlotsPerFrame = ceil(SENSOR_SAMPLE_PERIOD / slotDuration.GetSeconds());

  NS_LOG_UNCOND("Optimization: " << optString << ", Iter: " << iter);
  NS_LOG_UNCOND("Slot Duration: " << slotDuration.GetSeconds() << "s, Slots Per Superframe: " << numSlotsPerFrame);



  terminateCheckPeriod = Seconds(numSlotsPerFrame*slotDuration.GetSeconds());
  Simulator::Schedule(terminateCheckPeriod/2,&TerminateSimulation);

  NS_LOG_UNCOND("Sample update period " << terminateCheckPeriod.GetSeconds() << " s");

  NS_ASSERT(numSensorNodes > 0);
  NS_ASSERT(iter >= 0);

  AsciiTraceHelper asciiTraceHelper;
  std::string filename;
  std::stringstream ss;

  std::string filePath = "/home/rajith/NS-3 Rajith/Results/";
	ss.str( std::string() );
	ss.clear();
	ss << filePath << "N" << numSensorNodes << "_" << optString << "_";
  std::string filePrefix = ss.str();

  uint16_t numNodes = 1 + numSensorNodes;
  double fieldSizeY = ( (double)numSensorNodes / SENSOR_DENSITY ) / FIELD_SIZE_X;


  // routing debug
//  numNodes = 6;


  reportTxNum.assign(numNodes,0);
  reportRxNum.assign(numNodes,0);
  reportTxTime.assign(numNodes,Seconds(0.0));
  reportTotalDelay.assign(numNodes,Seconds(0.0));



  NS_LOG_UNCOND("Number of Nodes: " << numNodes);


  if (!numSensorNodes){
    NS_FATAL_ERROR("Number of transmit nodes cannot be zero!");
  }

  // Cannot simulate more than 256 nodes
  NS_ASSERT_MSG(numNodes <= 256, "Simulation can only support upto 256 nodes total. Num Nodes = " << numNodes);

	// Change the random number seed to alter the random number sequence used by the simulator.
  RngSeedManager::SetSeed (seed);
  NS_LOG_UNCOND("Seed: " << seed);

  // ********************************************* HELPER ************************************************

  Ptr<Isa100Helper> isaHelper = CreateObject<Isa100Helper>();


  // ******************************************* DL & PHY ATTRIBUTES ********************************************

  // These are ISA100 frame size parameters.  Look in the ISA100 standard for more information.
  isaHelper->SetDlAttribute("SuperFramePeriod",UintegerValue(numSlotsPerFrame));
  isaHelper->SetDlAttribute("SuperFrameSlotDuration",TimeValue(slotDuration));

  // Dl attributes
  isaHelper->SetDlAttribute("MaxTxPowerDbm", IntegerValue(4));
  isaHelper->SetDlAttribute("MinTxPowerDbm", IntegerValue(-17));
  isaHelper->SetDlAttribute("DlSleepEnabled", BooleanValue(true));

  // Phy attributes
  isaHelper->SetPhyAttribute ("SupplyVoltage", DoubleValue (3.0));
  isaHelper->SetPhyAttribute ("SensitivityDbm", DoubleValue (RX_SENSITIVITY));

  // Phy power consumption attributes
  isaHelper->SetTrxCurrentAttribute ("TrxOffCurrentA", DoubleValue (0.0003));
  isaHelper->SetTrxCurrentAttribute ("RxOnCurrentA", DoubleValue (0.0118));
  isaHelper->SetTrxCurrentAttribute ("SleepCurrentA", DoubleValue (0.0000002));

  isaHelper->SetTrxCurrentAttribute ("BusyRxCurrentA", DoubleValue (0.0118));
  isaHelper->SetTrxCurrentAttribute ("TxOnCurrentA", DoubleValue (0.0052));
  isaHelper->SetTrxCurrentAttribute ("Slope", DoubleValue (0.0003013));
  isaHelper->SetTrxCurrentAttribute ("Offset", DoubleValue (0.01224));


  // ********************************************* CHANNEL MODEL ************************************************

  NS_LOG_UNCOND("Constructing the channel model...");
  Ptr<SingleModelSpectrumChannel> channel = CreateObject<SingleModelSpectrumChannel> ();
  Ptr<FishLogDistanceLossModel> propLossModel = CreateObject<FishLogDistanceLossModel> ();
  Ptr<ConstantSpeedPropagationDelayModel> propDelayModel = CreateObject<ConstantSpeedPropagationDelayModel> ();

  propLossModel->SetAttribute("PathLossExponent",DoubleValue(PATH_LOSS_EXP));
  propLossModel->SetAttribute("ShadowingStdDev",DoubleValue(SHADOWING_STD_DEV_DB));
  channel->AddPropagationLossModel (propLossModel);
  channel->SetPropagationDelayModel(propDelayModel);

  // Channel hopping, channels 11-26 are available in the hopping pattern (802.15.4 channel page 0, OQPSK)
  // Nodes will operate on channel 11.
  uint8_t hoppingPattern[] = { 11 };


  // ********************************************* FILE STREAMS ************************************************
  Ptr<OutputStreamWrapper> scheduleStream = asciiTraceHelper.CreateFileStream ("/dev/null",std::ios::out);

  CREATE_STREAM_FILENAME("energies.txt");
  Ptr<OutputStreamWrapper> energyStream = asciiTraceHelper.CreateFileStream (filename,std::ios::app);

  CREATE_STREAM_FILENAME("drops.txt");
  Ptr<OutputStreamWrapper> packetDropStream = asciiTraceHelper.CreateFileStream (filename,std::ios::app);

  CREATE_STREAM_FILENAME("reports.txt");
  Ptr<OutputStreamWrapper> reportStream = asciiTraceHelper.CreateFileStream (filename,std::ios::app);

  //CREATE_STREAM_FILENAME("locations.txt");     
  //Ptr<OutputStreamWrapper> locationStream = asciiTraceHelper.CreateFileStream (filename,std::ios::app); 

  //CREATE_STREAM_FILENAME("schedule.txt");     
  //Ptr<OutputStreamWrapper> scheduleStream = asciiTraceHelper.CreateFileStream (filename,std::ios::app); 


	*(energyStream->GetStream()) << "Iter," << iter << ",--------------\n";
	*(packetDropStream->GetStream()) << "Iter," << iter << ",--------------\n";
	*(reportStream->GetStream()) << "Iter," << iter << ",--------------\n";
	//*(locationStream->GetStream()) << "#" << iter << "#\n"; 
	//*(scheduleStream->GetStream()) << "#" << iter << "#\n";


	*(reportStream->GetStream()) << "Seed," << seed << "\n";

	isaHelper->TraceConnectWithoutContext ("HopTrace", MakeBoundCallback (&LogHops, reportStream));


	// ********************************************** NODE LOCATIONS **********************************************
	NS_LOG_UNCOND(" Creating network...");

//  isaHelper->TraceConnectWithoutContext ("NodeLocations", MakeBoundCallback (&PrintLocations, locationStream));


	Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();

	ns3::Vector sinkLoc(FIELD_SIZE_X/2,0.0,0.0);

	isaHelper->GenerateLocationsFixedNumNodes(positionAlloc,numNodes,FIELD_SIZE_X,fieldSizeY,MIN_NODE_SPACING,sinkLoc);

	*(reportStream->GetStream()) << "FieldArea," << FIELD_SIZE_X*fieldSizeY << "\n";
	*(reportStream->GetStream()) << "FieldRatio," << fieldSizeY/FIELD_SIZE_Y << "\n";

	// routing debug

/*
  	double scale = 100;
  positionAlloc->Add(ns3::Vector(0,0,0));
  positionAlloc->Add(ns3::Vector(-scale/2,scale,0));
  positionAlloc->Add(ns3::Vector(0,scale,0));
  positionAlloc->Add(ns3::Vector(scale/2,scale,0));
  positionAlloc->Add(ns3::Vector(scale/4,2*scale,0));
  positionAlloc->Add(ns3::Vector(scale/4,3*scale,0));
*/

	propLossModel->GenerateNewShadowingValues(positionAlloc,numNodes,SHADOWING_STD_DEV_DB);


	// ********************************************* CREATE NODES **********************************************

  // Nodes 'container' class.
  NodeContainer nc;
  nc.Create(numNodes);

	NetDeviceContainer devContainer;
	devContainer = isaHelper->Install(nc, channel, 0);

	isaHelper->SetDeviceConstantPosition(devContainer,positionAlloc);


	for (int16_t i = 1; i < numNodes; i++)
	{
		Ptr<Isa100Processor> processor = CreateObject<Isa100Processor>();

		processor->SetAttribute("ActiveCurrent", DoubleValue(0.0078));
		processor->SetAttribute("SleepCurrent", DoubleValue(0.0000026));
		processor->SetAttribute("SupplyVoltage", DoubleValue(3.0));

		isaHelper->InstallProcessor(i,processor);

		Ptr<Isa100Sensor> sensor = CreateObject<Isa100Sensor>();

		sensor->SetAttribute("ActiveCurrent", DoubleValue(SENSOR_SAMPLE_POWER_W/3.0));
		sensor->SetAttribute("IdleCurrent", DoubleValue(0.0));
		sensor->SetAttribute("SupplyVoltage", DoubleValue(3.0));
		sensor->SetAttribute("SensingTime", TimeValue( Seconds(SENSOR_SAMPLE_DURATION_S) ) );

		isaHelper->InstallSensor(i,sensor);

		Ptr<Isa100Battery> battery = CreateObject<Isa100Battery>();

		battery->SetInitEnergy(DEFAULT_INITIAL_ENERGY_J*1e6);
		battery->SetBatteryDepletionCallback(MakeCallback(&BatteryDepletionCallbackEvent));

		isaHelper->InstallBattery(i,battery);
	}

	// Sink application
	Ptr<Isa100BackboneNodeApplication> sinkNodeApp = CreateObject<Isa100BackboneNodeApplication>();

	sinkNodeApp->SetAttribute("SrcAddress",Mac16AddressValue(SINK_ADDR));
	sinkNodeApp->SetAttribute("StartTime",TimeValue(Seconds(0.0)));
  sinkNodeApp->TraceConnectWithoutContext ("ReportRx", MakeBoundCallback (&LogReportRx, reportStream));



	// Install application
	isaHelper->InstallApplication(nc,0,sinkNodeApp);

	// Create the sensor node applications
	Mac16AddressValue address;
	Ptr<Isa100NetDevice> netDevice;
	for (int16_t i = 1; i < numNodes; i++)
	{
		Ptr<Isa100FieldNodeApplication> sensorNodeApp = CreateObject<Isa100FieldNodeApplication>();

		// Sensor application attributes
		netDevice = devContainer.Get(i)->GetObject<Isa100NetDevice>();
		netDevice->GetDl()->GetAttribute("Address",address);
		sensorNodeApp->SetAttribute("SrcAddress",address);
		sensorNodeApp->SetAttribute("DestAddress",Mac16AddressValue(SINK_ADDR));
		sensorNodeApp->SetAttribute("PacketSize",UintegerValue(PACKET_DATA_BYTES));
		sensorNodeApp->SetAttribute("StartTime",TimeValue(Seconds(0.0)));
	  sensorNodeApp->TraceConnectWithoutContext ("ReportTx", MakeBoundCallback (&LogReportTx, reportStream));

		// Hook the application and sensor toegether
		sensorNodeApp->SetSensor(netDevice->GetSensor());
		sensorNodeApp->SetProcessor(netDevice->GetProcessor());
		netDevice->GetSensor()->SetSensingCallback(MakeCallback (&Isa100FieldNodeApplication::SensorSampleCallback, sensorNodeApp));

		// Install application
		isaHelper->InstallApplication(nc,i,sensorNodeApp);
	}
        
	// Traces
  Ptr<NetDevice> baseDevice;
  for (uint16_t i = 0; i < numNodes; i++){
    baseDevice = devContainer.Get(i);
    netDevice = baseDevice->GetObject<Isa100NetDevice>();

    netDevice->GetPhy()->TraceConnectWithoutContext ("InfoDropTrace", MakeBoundCallback (&PrintDropPacket, packetDropStream));
    netDevice->GetDl()->TraceConnectWithoutContext ("InfoDropTrace", MakeBoundCallback (&PrintDropPacket, packetDropStream));
  }


  // ******************************************** TDMA OPTIMIZATION *********************************************
  NS_LOG_UNCOND(" Beginning TDMA lifetime optimization...");

  // Optimizer Attributes
  isaHelper->SetTdmaOptAttribute("MultiplePacketsPerSlot", BooleanValue(multiplePacketsPerSlot));
  isaHelper->SetTdmaOptAttribute("NumBytesPkt", UintegerValue (PACKET_DATA_BYTES + PACKET_OVERHEAD_BYTES)); // 29 bytes is the isa100 header size
  isaHelper->SetTdmaOptAttribute("NumPktsNode", UintegerValue (1));
  isaHelper->SetTdmaOptAttribute("SensitivityDbm", DoubleValue (RX_SENSITIVITY));

  int nNodes = nc.GetN (); //Rajith 0331    
  NodeContainer ncNew; //Rajith 0331

  for (int i = 0; i < nNodes; ++i)  //Rajith 0331
  {     //Rajith 0331
        if(i!=4) ncNew.Add(nc.Get(i));  //Rajith 0331      
  }     //Rajith 0331

  // Call the helper
  clock_t begin = clock();
  SchedulingResult schedResult = isaHelper->CreateOptimizedTdmaSchedule(ncNew,propLossModel,hoppingPattern,1,(OptimizerSelect)optimizerType,scheduleStream);
  clock_t end = clock();   

  if(schedResult != SCHEDULE_FOUND){
    *(reportStream->GetStream()) << "Failure," << schedResult << "\n";

    energyStream->GetStream()->flush();
    packetDropStream->GetStream()->flush();
    scheduleStream->GetStream()->flush();
    reportStream->GetStream()->flush();

    return 0;
  }


  double optTime = ((double)end-(double)begin)/CLOCKS_PER_SEC;
  NS_LOG_UNCOND("  Optimization Time: " << optTime << " s");
  *(reportStream->GetStream()) << "Optimization," << optTime << "\n";


  // ********************************************** RUN SIMULATION **********************************************
  Simulator::Stop (Seconds (SIM_DURATION_S));       
  NS_LOG_UNCOND (" Simulation is running ....");
  Simulator::Run ();


  // ************************************************* SIMULATION COMPLETE **************************************************

  int totReportTx = 0, totReportRx = 0;
  Time totDelay = Seconds(0.0);
  bool starvedNode = false;

  for (int16_t i = 1; i < numNodes; i++){
  	baseDevice = devContainer.Get(i);
  	netDevice = baseDevice->GetObject<Isa100NetDevice>();

  	netDevice->GetBattery()->PrintEnergySummary(energyStream);

  	totReportTx += reportTxNum[i];
  	totReportRx += reportRxNum[i];
  	totDelay += reportTotalDelay[i];

  	if(reportRxNum[i] == 0){
  		starvedNode = true;
  		NS_LOG_UNCOND("*Starved Node*: " << i);
  	}


 // 	NS_LOG_UNCOND(" Node " << i << ", Tx: " << reportTxNum[i] << " Rx: " << reportRxNum[i]);

  }

  if(starvedNode)
    *(reportStream->GetStream()) << "Failure," << STARVED_NODE << "\n";
  else{
    *(reportStream->GetStream()) << "Lifetime," << networkLifetime << "\n";
  	*(reportStream->GetStream()) << "TotalTx," << totReportTx << "\n";
  	*(reportStream->GetStream()) << "TotalRx," << totReportRx << "\n";
  	*(reportStream->GetStream()) << "DropPct," << (1.0-(double)totReportRx/totReportTx) << "\n";
  	*(reportStream->GetStream()) << "AvgDelay," << totDelay.GetSeconds()/totReportRx << "\n";
  }

  // Flush streams
  energyStream->GetStream()->flush();
  packetDropStream->GetStream()->flush();
  scheduleStream->GetStream()->flush();
  reportStream->GetStream()->flush();
 // locationStream->GetStream()->flush();

	return 0;
}

