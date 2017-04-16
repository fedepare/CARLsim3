/* * Copyright (c) 2014 Regents of the University of California. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. The names of its contributors may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * *********************************************************************************************** *
 * CARLsim
 * created by: 		(MDR) Micah Richert, (JN) Jayram M. Nageswaran
 * maintained by:	(MA) Mike Avery <averym@uci.edu>, (MB) Michael Beyeler <mbeyeler@uci.edu>,
 *					(KDC) Kristofor Carlson <kdcarlso@uci.edu>
 *
 * CARLsim available from http://socsci.uci.edu/~jkrichma/CARLsim/
 * Ver 2/21/2014
 */

#include <snn.h>

#if defined(WIN32) || defined(WIN64)
	#include <Windows.h>
#else
	#include <sys/stat.h>		// mkdir
#endif

#include <math.h> 		// fabs
#include <string.h> 	// std::string, memset
#include <stdlib.h> 	// abs, drand48
#include <algorithm> 	// std::min, std::max
#include <limits.h> 	// UINT_MAX

#include <connection_monitor.h>
#include <connection_monitor_core.h>
#include <spike_monitor.h>
#include <spike_monitor_core.h>
#include <group_monitor.h>
#include <group_monitor_core.h>

// \FIXME what are the following for? why were they all the way at the bottom of this file?

#define COMPACTION_ALIGNMENT_PRE  16
#define COMPACTION_ALIGNMENT_POST 0

#define SETPOST_INFO(name, nid, sid, val) name[cumulativePost[nid]+sid]=val;

#define SETPRE_INFO(name, nid, sid, val)  name[cumulativePre[nid]+sid]=val;



/// **************************************************************************************************************** ///
/// CONSTRUCTOR / DESTRUCTOR
/// **************************************************************************************************************** ///


// TODO: consider moving unsafe computations out of constructor
CpuSNN::CpuSNN(const std::string& name, simMode_t simMode, loggerMode_t loggerMode, int ithGPU, int randSeed)
					: networkName_(name), simMode_(simMode), loggerMode_(loggerMode), ithGPU_(ithGPU),
					  randSeed_(CpuSNN::setRandSeed(randSeed)) // all of these are const
{
	// move all unsafe operations out of constructor
	CpuSNNinit();
}

// destructor
CpuSNN::~CpuSNN() {
	if (!simulatorDeleted)
		deleteObjects();
}



/// ************************************************************************************************************ ///
/// PUBLIC METHODS: SETTING UP A SIMULATION
/// ************************************************************************************************************ ///

// make from each neuron in grpId1 to 'numPostSynapses' neurons in grpId2
short int CpuSNN::connect(int grpId1, int grpId2, const std::string& _type, float initWt, float maxWt, float prob,
						uint8_t minDelay, uint8_t maxDelay, float radX, float radY, float radZ,
						float _mulSynFast, float _mulSynSlow, bool synWtType) {
						//const std::string& wtType
	int retId=-1;
	assert(grpId1 < numGrp);
	assert(grpId2 < numGrp);
	assert(minDelay <= maxDelay);
	assert(!isPoissonGroup(grpId2));

    //* \deprecated Do these ramp thingies still work?
//    bool useRandWts = (wtType.find("random") != std::string::npos);
//    bool useRampDownWts = (wtType.find("ramp-down") != std::string::npos);
//    bool useRampUpWts = (wtType.find("ramp-up") != std::string::npos);
//    uint32_t connProp = SET_INITWTS_RANDOM(useRandWts)
//      | SET_CONN_PRESENT(1)
//      | SET_FIXED_PLASTIC(synWtType)
//      | SET_INITWTS_RAMPUP(useRampUpWts)
//      | SET_INITWTS_RAMPDOWN(useRampDownWts);
	uint32_t connProp = SET_CONN_PRESENT(1) | SET_FIXED_PLASTIC(synWtType);

//	Grid3D szPre = getGroupGrid3D(grpId1);
//	Grid3D szPost = getGroupGrid3D(grpId2);

	grpConnectInfo_t* newInfo = (grpConnectInfo_t*) calloc(1, sizeof(grpConnectInfo_t));
	newInfo->grpSrc   		  = grpId1;
	newInfo->grpDest  		  = grpId2;
	newInfo->initWt	  		  = initWt;
	newInfo->maxWt	  		  = maxWt;
	newInfo->maxDelay 		  = maxDelay;
	newInfo->minDelay 		  = minDelay;
//		newInfo->radX             = (radX<0) ? std::max(szPre.x,szPost.x) : radX; // <0 means full connectivity, so the
//		newInfo->radY             = (radY<0) ? std::max(szPre.y,szPost.y) : radY; // effective group size is Grid3D.x. Grab
//		newInfo->radZ             = (radZ<0) ? std::max(szPre.z,szPost.z) : radZ; // the larger of pre / post to connect all
	newInfo->radX             = radX;
	newInfo->radY             = radY;
	newInfo->radZ             = radZ;
	newInfo->mulSynFast       = _mulSynFast;
	newInfo->mulSynSlow       = _mulSynSlow;
	newInfo->connProp         = connProp;
	newInfo->p                = prob;
	newInfo->type             = CONN_UNKNOWN;
	newInfo->numPostSynapses  = 1;
	newInfo->ConnectionMonitorId = -1;

	newInfo->next 				= connectBegin; //linked list of connection..
	connectBegin 				= newInfo;

	if ( _type.find("random") != std::string::npos) {
		newInfo->type 	= CONN_RANDOM;
		newInfo->numPostSynapses	= (std::min)(grp_Info[grpId2].SizeN,((int) (prob*grp_Info[grpId2].SizeN +6.5*sqrt(prob*(1-prob)*grp_Info[grpId2].SizeN)+0.5))); // estimate the maximum number of connections we need.  This uses a binomial distribution at 6.5 stds.
		newInfo->numPreSynapses   = (std::min)(grp_Info[grpId1].SizeN,((int) (prob*grp_Info[grpId1].SizeN +6.5*sqrt(prob*(1-prob)*grp_Info[grpId1].SizeN)+0.5))); // estimate the maximum number of connections we need.  This uses a binomial distribution at 6.5 stds.
	}
	//so you're setting the size to be prob*Number of synapses in group info + some standard deviation ...
	else if ( _type.find("full-no-direct") != std::string::npos) {
		newInfo->type 	= CONN_FULL_NO_DIRECT;
		newInfo->numPostSynapses	= grp_Info[grpId2].SizeN-1;
		newInfo->numPreSynapses	= grp_Info[grpId1].SizeN-1;
	}
	else if ( _type.find("full") != std::string::npos) {
		newInfo->type 	= CONN_FULL;

		newInfo->numPostSynapses	= grp_Info[grpId2].SizeN;
		newInfo->numPreSynapses   = grp_Info[grpId1].SizeN;
	}
	else if ( _type.find("one-to-one") != std::string::npos) {
		newInfo->type 	= CONN_ONE_TO_ONE;
		newInfo->numPostSynapses	= 1;
		newInfo->numPreSynapses	= 1;
	} else if ( _type.find("gaussian") != std::string::npos) {
		newInfo->type   = CONN_GAUSSIAN;
		// the following is antiquated, just assume the worst case for now
		newInfo->numPostSynapses	= std::min(MAX_nPostSynapses, grp_Info[grpId2].SizeN);
		newInfo->numPreSynapses   = std::min(MAX_nPreSynapses, grp_Info[grpId1].SizeN);
	} else {
		KERNEL_ERROR("Invalid connection type (should be 'random', 'full', 'one-to-one', 'full-no-direct', or 'gaussian')");
		exitSimulation(-1);
	}

	if (newInfo->numPostSynapses > MAX_nPostSynapses) {
		KERNEL_ERROR("ConnID %d exceeded the maximum number of output synapses (%d), has %d.",
			newInfo->connId,
			MAX_nPostSynapses, newInfo->numPostSynapses);
		assert(newInfo->numPostSynapses <= MAX_nPostSynapses);
	}

	if (newInfo->numPreSynapses > MAX_nPreSynapses) {
		KERNEL_ERROR("ConnID %d exceeded the maximum number of input synapses (%d), has %d.",
			newInfo->connId,
			MAX_nPreSynapses, newInfo->numPreSynapses);
		assert(newInfo->numPreSynapses <= MAX_nPreSynapses);
	}

	// update the pre and post size...
	// Subtlety: each group has numPost/PreSynapses from multiple connections.
	// The newInfo->numPost/PreSynapses are just for this specific connection.
	// We are adding the synapses counted in this specific connection to the totals for both groups.
	grp_Info[grpId1].numPostSynapses 	+= newInfo->numPostSynapses;
	grp_Info[grpId2].numPreSynapses 	+= newInfo->numPreSynapses;

	KERNEL_DEBUG("grp_Info[%d, %s].numPostSynapses = %d, grp_Info[%d, %s].numPreSynapses = %d",
					grpId1,grp_Info2[grpId1].Name.c_str(),grp_Info[grpId1].numPostSynapses,grpId2,
					grp_Info2[grpId2].Name.c_str(),grp_Info[grpId2].numPreSynapses);

	newInfo->connId	= numConnections++;
	assert(numConnections <= MAX_nConnections);	// make sure we don't overflow connId

	retId = newInfo->connId;

	KERNEL_DEBUG("CONNECT SETUP: connId=%d, mulFast=%f, mulSlow=%f",newInfo->connId,newInfo->mulSynFast,
						newInfo->mulSynSlow);
	assert(retId != -1);
	return retId;
}

// make custom connections from grpId1 to grpId2
short int CpuSNN::connect(int grpId1, int grpId2, ConnectionGeneratorCore* conn, float _mulSynFast, float _mulSynSlow,
						bool synWtType, int maxM, int maxPreM) {
	int retId=-1;

	assert(grpId1 < numGrp);
	assert(grpId2 < numGrp);

	if (maxM == 0)
		maxM = grp_Info[grpId2].SizeN;

	if (maxPreM == 0)
		maxPreM = grp_Info[grpId1].SizeN;

	if (maxM > MAX_nPostSynapses) {
		KERNEL_ERROR("Connection from %s (%d) to %s (%d) exceeded the maximum number of output synapses (%d), "
							"has %d.", grp_Info2[grpId1].Name.c_str(),grpId1,grp_Info2[grpId2].Name.c_str(),
							grpId2,	MAX_nPostSynapses,maxM);
		assert(maxM <= MAX_nPostSynapses);
	}

	if (maxPreM > MAX_nPreSynapses) {
		KERNEL_ERROR("Connection from %s (%d) to %s (%d) exceeded the maximum number of input synapses (%d), "
							"has %d.\n", grp_Info2[grpId1].Name.c_str(), grpId1,grp_Info2[grpId2].Name.c_str(),
							grpId2, MAX_nPreSynapses,maxPreM);
		assert(maxPreM <= MAX_nPreSynapses);
	}

	grpConnectInfo_t* newInfo = (grpConnectInfo_t*) calloc(1, sizeof(grpConnectInfo_t));

	newInfo->grpSrc   = grpId1;
	newInfo->grpDest  = grpId2;
	newInfo->initWt	  = 1;
	newInfo->maxWt	  = 1;
	newInfo->maxDelay = MAX_SynapticDelay;
	newInfo->minDelay = 1;
	newInfo->mulSynFast = _mulSynFast;
	newInfo->mulSynSlow = _mulSynSlow;
	newInfo->connProp = SET_CONN_PRESENT(1) | SET_FIXED_PLASTIC(synWtType);
	newInfo->type	  = CONN_USER_DEFINED;
	newInfo->numPostSynapses	  	  = maxM;
	newInfo->numPreSynapses	  = maxPreM;
	newInfo->conn	= conn;
	newInfo->ConnectionMonitorId = -1;

	newInfo->next	= connectBegin;  // build a linked list
	connectBegin      = newInfo;

	// update the pre and post size...
	grp_Info[grpId1].numPostSynapses    += newInfo->numPostSynapses;
	grp_Info[grpId2].numPreSynapses += newInfo->numPreSynapses;

	KERNEL_DEBUG("grp_Info[%d, %s].numPostSynapses = %d, grp_Info[%d, %s].numPreSynapses = %d",
					grpId1,grp_Info2[grpId1].Name.c_str(),grp_Info[grpId1].numPostSynapses,grpId2,
					grp_Info2[grpId2].Name.c_str(),grp_Info[grpId2].numPreSynapses);

	newInfo->connId	= numConnections++;
	assert(numConnections <= MAX_nConnections);	// make sure we don't overflow connId

	retId = newInfo->connId;
	assert(retId != -1);
	return retId;
}

// make a compartmental connection between two groups
short int CpuSNN::connectCompartments(int grpIdLower, int grpIdUpper) {
	assert(grpIdLower >= 0 && grpIdLower < numGrp);
	assert(grpIdUpper >= 0 && grpIdUpper < numGrp);
	assert(grpIdLower != grpIdUpper);
	assert(!isPoissonGroup(grpIdLower));
	assert(!isPoissonGroup(grpIdUpper));

	// this flag must be set if any compartmental connections exist
	// note that grpId.withCompartments is not necessarily set just yet, this will be done in
	// CpuSNN::setCompartmentParameters
	sim_with_compartments = true;

	// add entry to linked list
	compConnectInfo_t* newInfo = (compConnectInfo_t*) calloc(1, sizeof(compConnectInfo_t));
	newInfo->grpSrc = grpIdLower;
	newInfo->grpDest = grpIdUpper;
	newInfo->connId = numCompartmentConnections++;
	newInfo->next = compConnectBegin;
	compConnectBegin = newInfo;

	return newInfo->connId;
}


// create group of Izhikevich neurons
// use int for nNeur to avoid arithmetic underflow
int CpuSNN::createGroup(const std::string& grpName, const Grid3D& grid, int neurType) {
	assert(grid.x*grid.y*grid.z>0);
	assert(neurType>=0);
	assert(numGrp < MAX_GRP_PER_SNN);

	if ( (!(neurType&TARGET_AMPA) && !(neurType&TARGET_NMDA) &&
		  !(neurType&TARGET_GABAa) && !(neurType&TARGET_GABAb)) || (neurType&POISSON_NEURON)) {
		KERNEL_ERROR("Invalid type using createGroup... Cannot create poisson generators here.");
		exitSimulation(1);
	}

	grp_Info[numGrp].withCompartments = 0;//All groups are non-compartmental by default

	// We don't store the Grid3D struct in grp_Info so we don't have to deal with allocating structs on the GPU
	grp_Info[numGrp].SizeN  			= grid.x * grid.y * grid.z; // number of neurons in the group
	grp_Info[numGrp].SizeX              = grid.x; // number of neurons in first dim of Grid3D
	grp_Info[numGrp].SizeY              = grid.y; // number of neurons in second dim of Grid3D
	grp_Info[numGrp].SizeZ              = grid.z; // number of neurons in third dim of Grid3D

	grp_Info[numGrp].Type   			= neurType;
	grp_Info[numGrp].WithSTP			= false;
	grp_Info[numGrp].WithSTDP			= false;
	grp_Info[numGrp].WithESTDPtype      = UNKNOWN_STDP;
	grp_Info[numGrp].WithISTDPtype		= UNKNOWN_STDP;
	grp_Info[numGrp].WithHomeostasis	= false;

	if ( (neurType&TARGET_GABAa) || (neurType&TARGET_GABAb)) {
		grp_Info[numGrp].MaxFiringRate 	= INHIBITORY_NEURON_MAX_FIRING_RATE;
	} else {
		grp_Info[numGrp].MaxFiringRate 	= EXCITATORY_NEURON_MAX_FIRING_RATE;
	}

	grp_Info2[numGrp].Name  			= grpName;
	grp_Info[numGrp].isSpikeGenerator	= false;
	grp_Info[numGrp].MaxDelay			= 1;

	grp_Info2[numGrp].Izh_a 			= -1; // \FIXME ???

	// init homeostasis params even though not used
	grp_Info2[numGrp].baseFiring        = 10.0f;
	grp_Info2[numGrp].baseFiringSD      = 0.0f;

	grp_Info2[numGrp].Name              = grpName;
	finishedPoissonGroup				= true;

	// update number of neuron counters
	if ( (neurType&TARGET_GABAa) || (neurType&TARGET_GABAb))
		numNInhReg += grid.N; // regular inhibitory neuron
	else
		numNExcReg += grid.N; // regular excitatory neuron
	numNReg += grid.N;
	numN += grid.N;

	numGrp++;
	return (numGrp-1);
}

// create spike generator group
// use int for nNeur to avoid arithmetic underflow
int CpuSNN::createSpikeGeneratorGroup(const std::string& grpName, const Grid3D& grid, int neurType) {
	assert(grid.x*grid.y*grid.z>0);
	assert(neurType>=0);
	grp_Info[numGrp].withCompartments = 0;//All groups are non-compartmental by default  FIXME:IS THIS NECESSARY?
	grp_Info[numGrp].SizeN   		= grid.x * grid.y * grid.z; // number of neurons in the group
	grp_Info[numGrp].SizeX          = grid.x; // number of neurons in first dim of Grid3D
	grp_Info[numGrp].SizeY          = grid.y; // number of neurons in second dim of Grid3D
	grp_Info[numGrp].SizeZ          = grid.z; // number of neurons in third dim of Grid3D
	grp_Info[numGrp].Type    		= neurType | POISSON_NEURON;
	grp_Info[numGrp].WithSTP		= false;
	grp_Info[numGrp].WithSTDP		= false;
	grp_Info[numGrp].WithESTDPtype  = UNKNOWN_STDP;
	grp_Info[numGrp].WithISTDPtype	= UNKNOWN_STDP;
	grp_Info[numGrp].WithHomeostasis	= false;
	grp_Info[numGrp].isSpikeGenerator	= true;		// these belong to the spike generator class...
	grp_Info2[numGrp].Name    		= grpName;
	grp_Info[numGrp].MaxFiringRate 	= POISSON_MAX_FIRING_RATE;

	grp_Info2[numGrp].Name          = grpName;

	if ( (neurType&TARGET_GABAa) || (neurType&TARGET_GABAb))
		numNInhPois += grid.N; // inh poisson group
	else
		numNExcPois += grid.N; // exc poisson group
	numNPois += grid.N;
	numN += grid.N;

	numGrp++;
	numSpikeGenGrps++;

	return (numGrp-1);
}

void CpuSNN::setCompartmentParameters(int grpId, float couplingUp, float couplingDown) {
	if (grpId == ALL) { // shortcut for all groups
		for (int grpId1 = 0; grpId1<numGrp; grpId1++) {
			setCompartmentParameters(grpId1, couplingUp, couplingDown);
		}
	} else {
		sim_with_compartments = true;
		grp_Info[grpId].withCompartments = true;
		grp_Info[grpId].compCouplingUp = couplingUp; 
		grp_Info[grpId].compCouplingDown = couplingDown;
		numComp += grp_Info[grpId].SizeN;
	}
}


// set conductance values for a simulation (custom values or disable conductances alltogether)
void CpuSNN::setConductances(bool isSet, int tdAMPA, int trNMDA, int tdNMDA, int tdGABAa,
int trGABAb, int tdGABAb) {
	if (isSet) {
		assert(tdAMPA>0); assert(tdNMDA>0); assert(tdGABAa>0); assert(tdGABAb>0);
		assert(trNMDA>=0); assert(trGABAb>=0); // 0 to disable rise times
		assert(trNMDA!=tdNMDA); assert(trGABAb!=tdGABAb); // singularity
	}

	// set conductances globally for all connections
	sim_with_conductances  |= isSet;
	dAMPA  = 1.0-1.0/tdAMPA;
	dNMDA  = 1.0-1.0/tdNMDA;
	dGABAa = 1.0-1.0/tdGABAa;
	dGABAb = 1.0-1.0/tdGABAb;

	if (trNMDA>0) {
		// use rise time for NMDA
		sim_with_NMDA_rise = true;
		rNMDA = 1.0-1.0/trNMDA;

		// compute max conductance under this model to scale it back to 1
		// otherwise the peak conductance will not be equal to the weight
		double tmax = (-tdNMDA*trNMDA*log(1.0*trNMDA/tdNMDA))/(tdNMDA-trNMDA); // t at which cond will be max
		sNMDA = 1.0/(exp(-tmax/tdNMDA)-exp(-tmax/trNMDA)); // scaling factor, 1 over max amplitude
		assert(!isinf(tmax) && !isnan(tmax) && tmax>=0);
		assert(!isinf(sNMDA) && !isnan(sNMDA) && sNMDA>0);
	}

	if (trGABAb>0) {
		// use rise time for GABAb
		sim_with_GABAb_rise = true;
		rGABAb = 1.0-1.0/trGABAb;

		// compute max conductance under this model to scale it back to 1
		// otherwise the peak conductance will not be equal to the weight
		double tmax = (-tdGABAb*trGABAb*log(1.0*trGABAb/tdGABAb))/(tdGABAb-trGABAb); // t at which cond will be max
		sGABAb = 1.0/(exp(-tmax/tdGABAb)-exp(-tmax/trGABAb)); // scaling factor, 1 over max amplitude
		assert(!isinf(tmax) && !isnan(tmax)); assert(!isinf(sGABAb) && !isnan(sGABAb) && sGABAb>0);
	}

	if (sim_with_conductances) {
		KERNEL_INFO("Running COBA mode:");
		KERNEL_INFO("  - AMPA decay time            = %5d ms", tdAMPA);
		KERNEL_INFO("  - NMDA rise time %s  = %5d ms", sim_with_NMDA_rise?"          ":"(disabled)", trNMDA);
		KERNEL_INFO("  - GABAa decay time           = %5d ms", tdGABAa);
		KERNEL_INFO("  - GABAb rise time %s = %5d ms", sim_with_GABAb_rise?"          ":"(disabled)",trGABAb);
		KERNEL_INFO("  - GABAb decay time           = %5d ms", tdGABAb);
	} else {
		KERNEL_INFO("Running CUBA mode (all synaptic conductances disabled)");
	}
}

// set homeostasis for group
void CpuSNN::setHomeostasis(int grpId, bool isSet, float homeoScale, float avgTimeScale) {
	if (grpId == ALL) { // shortcut for all groups
		for(int grpId1=0; grpId1<numGrp; grpId1++) {
			setHomeostasis(grpId1, isSet, homeoScale, avgTimeScale);
		}
	} else {
		// set conductances for a given group
		sim_with_homeostasis 			   |= isSet;
		grp_Info[grpId].WithHomeostasis    = isSet;
		grp_Info[grpId].homeostasisScale   = homeoScale;
		grp_Info[grpId].avgTimeScale       = avgTimeScale;
		grp_Info[grpId].avgTimeScaleInv    = 1.0f/avgTimeScale;
		grp_Info[grpId].avgTimeScale_decay = (avgTimeScale*1000.0f-1.0f)/(avgTimeScale*1000.0f);
		grp_Info[grpId].newUpdates 		= true; // \FIXME: what's this?

		KERNEL_INFO("Homeostasis parameters %s for %d (%s):\thomeoScale: %f, avgTimeScale: %f",
					isSet?"enabled":"disabled",grpId,grp_Info2[grpId].Name.c_str(),homeoScale,avgTimeScale);
	}
}

// set a homeostatic target firing rate (enforced through homeostatic synaptic scaling)
void CpuSNN::setHomeoBaseFiringRate(int grpId, float baseFiring, float baseFiringSD) {
	if (grpId == ALL) { // shortcut for all groups
		for(int grpId1=0; grpId1<numGrp; grpId1++) {
			setHomeoBaseFiringRate(grpId1, baseFiring, baseFiringSD);
		}
	} else {
		// set conductances for a given group
		assert(grp_Info[grpId].WithHomeostasis);

		grp_Info2[grpId].baseFiring 	= baseFiring;
		grp_Info2[grpId].baseFiringSD 	= baseFiringSD;
		grp_Info[grpId].newUpdates 	= true; //TODO: I have to see how this is handled.  -- KDC

		KERNEL_INFO("Homeostatic base firing rate set for %d (%s):\tbaseFiring: %3.3f, baseFiringStd: %3.3f",
							grpId,grp_Info2[grpId].Name.c_str(),baseFiring,baseFiringSD);
	}
}

void CpuSNN::setIntegrationMethod(integrationMethod_t method, int numStepsPerMs) {
	assert(numStepsPerMs >= 1 && numStepsPerMs <= 100);
	simIntegrationMethod_ = method;
	simNumStepsPerMs_ = numStepsPerMs;
	timeStep_ = 1.0f / simNumStepsPerMs_;
}

// set Izhikevich parameters for group
void CpuSNN::setNeuronParameters(int grpId, float izh_a, float izh_a_sd, float izh_b, float izh_b_sd,
								float izh_c, float izh_c_sd, float izh_d, float izh_d_sd)
{
	assert(grpId>=-1); assert(izh_a_sd>=0); assert(izh_b_sd>=0); assert(izh_c_sd>=0);
	assert(izh_d_sd>=0);

	if (grpId == ALL) { // shortcut for all groups
		for(int grpId1=0; grpId1<numGrp; grpId1++) {
			setNeuronParameters(grpId1, izh_a, izh_a_sd, izh_b, izh_b_sd, izh_c, izh_c_sd, izh_d, izh_d_sd);
		}
	} else {
		grp_Info2[grpId].Izh_a	  	=   izh_a;
		grp_Info2[grpId].Izh_a_sd  =   izh_a_sd;
		grp_Info2[grpId].Izh_b	  	=   izh_b;
		grp_Info2[grpId].Izh_b_sd  =   izh_b_sd;
		grp_Info2[grpId].Izh_c		=   izh_c;
		grp_Info2[grpId].Izh_c_sd	=   izh_c_sd;
		grp_Info2[grpId].Izh_d		=   izh_d;
		grp_Info2[grpId].Izh_d_sd	=   izh_d_sd;
		grp_Info[grpId].withParamModel_9 = 0;
	}
}

// set (9) Izhikevich parameters for group
void CpuSNN::setNeuronParameters(int grpId, float izh_C, float izh_C_sd, float izh_k, float izh_k_sd,
									float izh_vr, float izh_vr_sd, float izh_vt, float izh_vt_sd,
									float izh_a, float izh_a_sd, float izh_b, float izh_b_sd,
									float izh_vpeak, float izh_vpeak_sd, float izh_c, float izh_c_sd,
									float izh_d, float izh_d_sd)
{
	//Finish the assertment statements part.
	assert(grpId >= -1); assert(izh_C_sd >= 0); assert(izh_k_sd >= 0); assert(izh_vr_sd >= 0);
	assert(izh_vt_sd >= 0); assert(izh_a_sd >= 0); assert(izh_b_sd >= 0); assert(izh_vpeak_sd >= 0);
	assert(izh_c_sd >= 0); assert(izh_d_sd >= 0);

	if (grpId == ALL) { // shortcut for all groups
		for (int grpId1 = 0; grpId1<numGrp; grpId1++) {
			setNeuronParameters(grpId1,  izh_C,  izh_C_sd,  izh_k, izh_k_sd, izh_vr,  izh_vr_sd,  izh_vt,  izh_vt_sd,
									izh_a,  izh_a_sd,  izh_b,  izh_b_sd, izh_vpeak,  izh_vpeak_sd,  izh_c,  izh_c_sd,
									izh_d,  izh_d_sd);
		}
	}
	else {
		grp_Info2[grpId].Izh_C = izh_C;
		grp_Info2[grpId].Izh_C_sd = izh_C_sd;
		grp_Info2[grpId].Izh_k = izh_k;
		grp_Info2[grpId].Izh_k_sd = izh_k_sd;
		grp_Info2[grpId].Izh_vr = izh_vr;
		grp_Info2[grpId].Izh_vr_sd = izh_vr_sd;
		grp_Info2[grpId].Izh_vt = izh_vt;
		grp_Info2[grpId].Izh_vt_sd = izh_vt_sd;
		grp_Info2[grpId].Izh_a = izh_a;
		grp_Info2[grpId].Izh_a_sd = izh_a_sd;
		grp_Info2[grpId].Izh_b = izh_b;
		grp_Info2[grpId].Izh_b_sd = izh_b_sd;
		grp_Info2[grpId].Izh_vpeak = izh_vpeak;
		grp_Info2[grpId].Izh_vpeak_sd = izh_vpeak_sd;
		grp_Info2[grpId].Izh_c = izh_c;
		grp_Info2[grpId].Izh_c_sd = izh_c_sd;
		grp_Info2[grpId].Izh_d = izh_d;
		grp_Info2[grpId].Izh_d_sd = izh_d_sd;
		grp_Info[grpId].withParamModel_9 = 1;
	}
}

void CpuSNN::setNeuromodulator(int grpId, float baseDP, float tauDP, float base5HT, float tau5HT, float baseACh,
	float tauACh, float baseNE, float tauNE)
{
	grp_Info[grpId].baseDP	= baseDP;
	grp_Info[grpId].decayDP = 1.0 - (1.0 / tauDP);
	grp_Info[grpId].base5HT = base5HT;
	grp_Info[grpId].decay5HT = 1.0 - (1.0 / tau5HT);
	grp_Info[grpId].baseACh = baseACh;
	grp_Info[grpId].decayACh = 1.0 - (1.0 / tauACh);
	grp_Info[grpId].baseNE	= baseNE;
	grp_Info[grpId].decayNE = 1.0 - (1.0 / tauNE);
}

// set ESTDP params
void CpuSNN::setESTDP(int grpId, bool isSet, stdpType_t type, stdpCurve_t curve, float alphaPlus, float tauPlus, 
	float alphaMinus, float tauMinus, float gamma)
{
	assert(grpId>=-1);
	if (isSet) {
		assert(type!=UNKNOWN_STDP);
		assert(tauPlus>0.0f); assert(tauMinus>0.0f); assert(gamma>=0.0f);
	}

	if (grpId == ALL) { // shortcut for all groups
		for(int grpId1=0; grpId1<numGrp; grpId1++) {
			setESTDP(grpId1, isSet, type, curve, alphaPlus, tauPlus, alphaMinus, tauMinus, gamma);
		}
	} else {
		// set STDP for a given group
		// set params for STDP curve
		grp_Info[grpId].ALPHA_PLUS_EXC 		= alphaPlus;
		grp_Info[grpId].ALPHA_MINUS_EXC 	= alphaMinus;
		grp_Info[grpId].TAU_PLUS_INV_EXC 	= 1.0f/tauPlus;
		grp_Info[grpId].TAU_MINUS_INV_EXC	= 1.0f/tauMinus;
		grp_Info[grpId].GAMMA				= gamma;
		grp_Info[grpId].KAPPA				= (1 + exp(-gamma/tauPlus))/(1 - exp(-gamma/tauPlus));
		grp_Info[grpId].OMEGA				= alphaPlus * (1 - grp_Info[grpId].KAPPA);
		// set flags for STDP function
		grp_Info[grpId].WithESTDPtype	= type;
		grp_Info[grpId].WithESTDPcurve  = curve;
		grp_Info[grpId].WithESTDP		= isSet;
		grp_Info[grpId].WithSTDP		|= grp_Info[grpId].WithESTDP;
		sim_with_stdp					|= grp_Info[grpId].WithSTDP;

		KERNEL_INFO("E-STDP %s for %s(%d)", isSet?"enabled":"disabled", grp_Info2[grpId].Name.c_str(), grpId);
	}
}

// set ISTDP params
void CpuSNN::setISTDP(int grpId, bool isSet, stdpType_t type, stdpCurve_t curve, float ab1, float ab2, float tau1, float tau2) {
	assert(grpId>=-1);
	if (isSet) {
		assert(type!=UNKNOWN_STDP);
		assert(tau1>0); assert(tau2>0);
	}

	if (grpId==ALL) { // shortcut for all groups
		for(int grpId1=0; grpId1 < numGrp; grpId1++) {
			setISTDP(grpId1, isSet, type, curve, ab1, ab2, tau1, tau2);
		}
	} else {
		// set STDP for a given group
		// set params for STDP curve
		if (curve == EXP_CURVE) {
			grp_Info[grpId].ALPHA_PLUS_INB = ab1;
			grp_Info[grpId].ALPHA_MINUS_INB = ab2;
			grp_Info[grpId].TAU_PLUS_INV_INB = 1.0f / tau1;
			grp_Info[grpId].TAU_MINUS_INV_INB = 1.0f / tau2;
			grp_Info[grpId].BETA_LTP 		= 0.0f;
			grp_Info[grpId].BETA_LTD 		= 0.0f;
			grp_Info[grpId].LAMBDA			= 1.0f;
			grp_Info[grpId].DELTA			= 1.0f;
		} else {
			grp_Info[grpId].ALPHA_PLUS_INB = 0.0f;
			grp_Info[grpId].ALPHA_MINUS_INB = 0.0f;
			grp_Info[grpId].TAU_PLUS_INV_INB = 1.0f;
			grp_Info[grpId].TAU_MINUS_INV_INB = 1.0f;
			grp_Info[grpId].BETA_LTP 		= ab1;
			grp_Info[grpId].BETA_LTD 		= ab2;
			grp_Info[grpId].LAMBDA			= tau1;
			grp_Info[grpId].DELTA			= tau2;
		}
		// set flags for STDP function
		//FIXME: separate STDPType to ESTDPType and ISTDPType
		grp_Info[grpId].WithISTDPtype	= type;
		grp_Info[grpId].WithISTDPcurve = curve;
		grp_Info[grpId].WithISTDP		= isSet;
		grp_Info[grpId].WithSTDP		|= grp_Info[grpId].WithISTDP;
		sim_with_stdp					|= grp_Info[grpId].WithSTDP;

		KERNEL_INFO("I-STDP %s for %s(%d)", isSet?"enabled":"disabled", grp_Info2[grpId].Name.c_str(), grpId);
	}
}

// set STP params
void CpuSNN::setSTP(int grpId, bool isSet, float STP_U, float STP_tau_u, float STP_tau_x) {
	assert(grpId>=-1);
	if (isSet) {
		assert(STP_U>0 && STP_U<=1); assert(STP_tau_u>0); assert(STP_tau_x>0);
	}

	if (grpId == ALL) { // shortcut for all groups
		for(int grpId1=0; grpId1<numGrp; grpId1++) {
			setSTP(grpId1, isSet, STP_U, STP_tau_u, STP_tau_x);
		}
	} else {
		// set STDP for a given group
		sim_with_stp 				   |= isSet;
		grp_Info[grpId].WithSTP 		= isSet;
		grp_Info[grpId].STP_A 			= (STP_U>0.0f) ? 1.0/STP_U : 1.0f; // scaling factor
		grp_Info[grpId].STP_U 			= STP_U;
		grp_Info[grpId].STP_tau_u_inv	= 1.0f/STP_tau_u; // facilitatory
		grp_Info[grpId].STP_tau_x_inv	= 1.0f/STP_tau_x; // depressive
		grp_Info[grpId].newUpdates = true;

		KERNEL_INFO("STP %s for %d (%s):\tA: %1.4f, U: %1.4f, tau_u: %4.0f, tau_x: %4.0f", isSet?"enabled":"disabled",
					grpId, grp_Info2[grpId].Name.c_str(), grp_Info[grpId].STP_A, STP_U, STP_tau_u, STP_tau_x);
	}
}

void CpuSNN::setWeightAndWeightChangeUpdate(updateInterval_t wtANDwtChangeUpdateInterval, bool enableWtChangeDecay, float wtChangeDecay) {
	assert(wtChangeDecay > 0.0f && wtChangeDecay < 1.0f);

	switch (wtANDwtChangeUpdateInterval) {
		case INTERVAL_10MS:
			wtANDwtChangeUpdateInterval_ = 10;
			break;
		case INTERVAL_100MS:
			wtANDwtChangeUpdateInterval_ = 100;
			break;
		case INTERVAL_1000MS:
		default:
			wtANDwtChangeUpdateInterval_ = 1000;
			break;
	}

	if (enableWtChangeDecay) {
		// set up stdp factor according to update interval
		switch (wtANDwtChangeUpdateInterval) {
		case INTERVAL_10MS:
			stdpScaleFactor_ = 0.005f;
			break;
		case INTERVAL_100MS:
			stdpScaleFactor_ = 0.05f;
			break;
		case INTERVAL_1000MS:
		default:
			stdpScaleFactor_ = 0.5f;
			break;
		}
		// set up weight decay
		wtChangeDecay_ = wtChangeDecay;
	} else {
		stdpScaleFactor_ = 1.0f;
		wtChangeDecay_ = 0.0f;
	}

	KERNEL_INFO("Update weight and weight change every %d ms", wtANDwtChangeUpdateInterval_);
	KERNEL_INFO("Weight Change Decay is %s", enableWtChangeDecay? "enabled" : "disable");
	KERNEL_INFO("STDP scale factor = %1.3f, wtChangeDecay = %1.3f", stdpScaleFactor_, wtChangeDecay_);
}


/// ************************************************************************************************************ ///
/// PUBLIC METHODS: RUNNING A SIMULATION
/// ************************************************************************************************************ ///

int CpuSNN::runNetwork(int _nsec, int _nmsec, bool printRunSummary, bool copyState, bool shareWeights) {
	assert(_nmsec >= 0 && _nmsec < 1000);
	assert(_nsec  >= 0);
	int runDurationMs = _nsec*1000 + _nmsec;
	KERNEL_DEBUG("runNetwork: runDur=%dms, printRunSummary=%s, copyState=%s", runDurationMs, printRunSummary?"y":"n",
		copyState?"y":"n");

	// setupNetwork() must have already been called
	assert(doneReorganization);

	// don't bother printing if logger mode is SILENT
	printRunSummary = (loggerMode_==SILENT) ? false : printRunSummary;

	// first-time run: inform the user the simulation is running now
	if (simTime==0 && printRunSummary) {
		KERNEL_INFO("");
		if (simMode_==GPU_MODE) {
			KERNEL_INFO("******************** Running GPU Simulation on GPU %d ***************************",
			ithGPU_);
		} else {
			KERNEL_INFO("********************      Running CPU Simulation      ***************************");
		}
		KERNEL_INFO("");
	}

	// reset all spike counters
	if (simMode_==CPU_MODE) {
		resetSpikeCnt(ALL);
#ifndef __CPU_ONLY__
	} else {
		resetSpikeCnt_GPU(0,numGrp);
#endif
	}

	// store current start time for future reference
	simTimeRunStart = simTime;
	simTimeRunStop  = simTime+runDurationMs;
	assert(simTimeRunStop>=simTimeRunStart); // check for arithmetic underflow

	// ConnectionMonitor is a special case: we might want the first snapshot at t=0 in the binary
	// but updateTime() is false for simTime==0.
	// And we cannot put this code in ConnectionMonitorCore::init, because then the user would have no
	// way to call ConnectionMonitor::setUpdateTimeIntervalSec before...
	if (simTime==0 && numConnectionMonitor) {
		updateConnectionMonitor();
	}

	// set the Poisson generation time slice to be at the run duration up to PROPOGATED_BUFFER_SIZE ms.
	// \TODO: should it be PROPAGATED_BUFFER_SIZE-1 or PROPAGATED_BUFFER_SIZE ?
	setGrpTimeSlice(ALL, (std::max)(1,(std::min)(runDurationMs,PROPAGATED_BUFFER_SIZE-1)));

#ifndef __CPU_ONLY__
	CUDA_RESET_TIMER(timer);
	CUDA_START_TIMER(timer);
#endif

	// if nsec=0, simTimeMs=10, we need to run the simulator for 10 timeStep;
	// if nsec=1, simTimeMs=10, we need to run the simulator for 1*1000+10, time Step;
	for(int i=0; i<runDurationMs; i++) {
		if(simMode_ == CPU_MODE) {
			doSnnSim();
#ifndef __CPU_ONLY__
		} else {
			doGPUSim();
#endif
		}

		// update weight every updateInterval ms if plastic synapses present
		if (!sim_with_fixedwts && wtANDwtChangeUpdateInterval_ == ++wtANDwtChangeUpdateIntervalCnt_) {
			wtANDwtChangeUpdateIntervalCnt_ = 0; // reset counter
			if (!sim_in_testing) {
				// keep this if statement separate from the above, so that the counter is updated correctly
				if (simMode_ == CPU_MODE) {
                    if (!shareWeights) updateWeights();
                    else updateWeightsAndShare();
#ifndef __CPU_ONLY__
				} else{
					updateWeights_GPU();
#endif
				}
			}
		}

		// Note: updateTime() advance simTime, simTimeMs, and simTimeSec accordingly
		if (updateTime()) {
			// finished one sec of simulation...
			if (numSpikeMonitor) {
				updateSpikeMonitor();
			}
			if (numGroupMonitor) {
				updateGroupMonitor();
			}
			if (numConnectionMonitor) {
				updateConnectionMonitor();
			}

			if(simMode_ == CPU_MODE) {
				updateFiringTable();
#ifndef __CPU_ONLY__
			} else {
				updateFiringTable_GPU();
#endif
			}
		}

#ifndef __CPU_ONLY__
		if(simMode_ == GPU_MODE) {
			copyFiringStateFromGPU();
		}
#endif
	}

#ifndef __CPU_ONLY__
	// in GPU mode, copy info from device to host
	if (simMode_==GPU_MODE) {
		if(copyState) {
			copyNeuronState(&cpuNetPtrs, &cpu_gpuNetPtrs, cudaMemcpyDeviceToHost, false, ALL);

			if (sim_with_stp) {
				copySTPState(&cpuNetPtrs, &cpu_gpuNetPtrs, cudaMemcpyDeviceToHost, false);
			}
		}
	}
#endif

	// user can opt to display some runNetwork summary
	if (printRunSummary) {

		// if there are Monitors available and it's time to show the log, print status for each group
		if (numSpikeMonitor) {
			printStatusSpikeMonitor(ALL);
		}
		if (numConnectionMonitor) {
			printStatusConnectionMonitor(ALL);
		}
		if (numGroupMonitor) {
			printStatusGroupMonitor(ALL);
		}

		// record time of run summary print
		simTimeLastRunSummary = simTime;
	}

	// call updateSpike(Group)Monitor again to fetch all the left-over spikes and group status (neuromodulator)
	updateSpikeMonitor();
	updateGroupMonitor();

	// keep track of simulation time...
#ifndef __CPU_ONLY__
	CUDA_STOP_TIMER(timer);
	lastExecutionTime = CUDA_GET_TIMER_VALUE(timer);
	cumExecutionTime += lastExecutionTime;
#endif

	return 0;
}



/// ************************************************************************************************************ ///
/// PUBLIC METHODS: INTERACTING WITH A SIMULATION
/// ************************************************************************************************************ ///

// adds a bias to every weight in the connection
void CpuSNN::biasWeights(short int connId, float bias, bool updateWeightRange) {
	assert(connId>=0 && connId<numConnections);

	grpConnectInfo_t* connInfo = getConnectInfo(connId);

	// iterate over all postsynaptic neurons
	for (int i=grp_Info[connInfo->grpDest].StartN; i<=grp_Info[connInfo->grpDest].EndN; i++) {
		unsigned int cumIdx = cumulativePre[i];

		// iterate over all presynaptic neurons
		unsigned int pos_ij = cumIdx;
		for (int j=0; j<Npre[i]; pos_ij++, j++) {
			if (cumConnIdPre[pos_ij]==connId) {
				// apply bias to weight
				float weight = wt[pos_ij] + bias;

				// inform user of acton taken if weight is out of bounds
//				bool needToPrintDebug = (weight+bias>connInfo->maxWt || weight+bias<connInfo->minWt);
				bool needToPrintDebug = (weight>connInfo->maxWt || weight<0.0f);

				if (updateWeightRange) {
					// if this flag is set, we need to update minWt,maxWt accordingly
					// will be saving new maxSynWt and copying to GPU below
//					connInfo->minWt = fmin(connInfo->minWt, weight);
					connInfo->maxWt = fmax(connInfo->maxWt, weight);
					if (needToPrintDebug) {
						KERNEL_DEBUG("biasWeights(%d,%f,%s): updated weight ranges to [%f,%f]", connId, bias,
							(updateWeightRange?"true":"false"), 0.0f, connInfo->maxWt);
					}
				} else {
					// constrain weight to boundary values
					// compared to above, we swap minWt/maxWt logic
					weight = fmin(weight, connInfo->maxWt);
//					weight = fmax(weight, connInfo->minWt);
					weight = fmax(weight, 0.0f);
					if (needToPrintDebug) {
						KERNEL_DEBUG("biasWeights(%d,%f,%s): constrained weight %f to [%f,%f]", connId, bias,
							(updateWeightRange?"true":"false"), weight, 0.0f, connInfo->maxWt);
					}
				}

				// update datastructures
				wt[pos_ij] = weight;
				maxSynWt[pos_ij] = connInfo->maxWt; // it's easier to just update, even if it hasn't changed
			}
		}

#ifndef __CPU_ONLY__
		// update GPU datastructures in batches, grouped by post-neuron
		if (simMode_==GPU_MODE) {
			CUDA_CHECK_ERRORS( cudaMemcpy(&(cpu_gpuNetPtrs.wt[cumIdx]), &(wt[cumIdx]), sizeof(float)*Npre[i],
				cudaMemcpyHostToDevice) );

			if (cpu_gpuNetPtrs.maxSynWt!=NULL) {
				// only copy maxSynWt if datastructure actually exists on the GPU
				// (that logic should be done elsewhere though)
				CUDA_CHECK_ERRORS( cudaMemcpy(&(cpu_gpuNetPtrs.maxSynWt[cumIdx]), &(maxSynWt[cumIdx]),
					sizeof(float)*Npre[i], cudaMemcpyHostToDevice) );
			}
		}
#endif
	}
}

// deallocates dynamical structures and exits
void CpuSNN::exitSimulation(int val) {
	deleteObjects();
	exit(val);
}

// reads network state from file
void CpuSNN::loadSimulation(FILE* fid) {
	loadSimFID = fid;
}

// reset spike counter to zero
void CpuSNN::resetSpikeCounter(int grpId) {
	if (!sim_with_spikecounters)
		return;

	assert(grpId>=-1); assert(grpId<numGrp);

	if (grpId == ALL) { // shortcut for all groups
		for(int grpId1=0; grpId1<numGrp; grpId1 ++) {
			resetSpikeCounter(grpId1);
		}
	} else {
		// only update if SpikeMonRT is set for this group
		if (!grp_Info[grpId].withSpikeCounter)
			return;

		grp_Info[grpId].spkCntRecordDurHelper = 0;

		if (simMode_==CPU_MODE) {
			int bufPos = grp_Info[grpId].spkCntBufPos; // retrieve buf pos
			memset(spkCntBuf[bufPos],0,grp_Info[grpId].SizeN*sizeof(int)); // set all to 0
#ifndef __CPU_ONLY__
		} else {
			resetSpikeCounter_GPU(grpId);
#endif
		}
	}
}

// multiplies every weight with a scaling factor
void CpuSNN::scaleWeights(short int connId, float scale, bool updateWeightRange) {
	assert(connId>=0 && connId<numConnections);
	assert(scale>=0.0f);

	grpConnectInfo_t* connInfo = getConnectInfo(connId);

	// iterate over all postsynaptic neurons
	for (int i=grp_Info[connInfo->grpDest].StartN; i<=grp_Info[connInfo->grpDest].EndN; i++) {
		unsigned int cumIdx = cumulativePre[i];

		// iterate over all presynaptic neurons
		unsigned int pos_ij = cumIdx;
		for (int j=0; j<Npre[i]; pos_ij++, j++) {
			if (cumConnIdPre[pos_ij]==connId) {
				// apply bias to weight
				float weight = wt[pos_ij]*scale;

				// inform user of acton taken if weight is out of bounds
//				bool needToPrintDebug = (weight>connInfo->maxWt || weight<connInfo->minWt);
				bool needToPrintDebug = (weight>connInfo->maxWt || weight<0.0f);

				if (updateWeightRange) {
					// if this flag is set, we need to update minWt,maxWt accordingly
					// will be saving new maxSynWt and copying to GPU below
//					connInfo->minWt = fmin(connInfo->minWt, weight);
					connInfo->maxWt = fmax(connInfo->maxWt, weight);
					if (needToPrintDebug) {
						KERNEL_DEBUG("scaleWeights(%d,%f,%s): updated weight ranges to [%f,%f]", connId, scale,
							(updateWeightRange?"true":"false"), 0.0f, connInfo->maxWt);
					}
				} else {
					// constrain weight to boundary values
					// compared to above, we swap minWt/maxWt logic
					weight = fmin(weight, connInfo->maxWt);
//					weight = fmax(weight, connInfo->minWt);
					weight = fmax(weight, 0.0f);
					if (needToPrintDebug) {
						KERNEL_DEBUG("scaleWeights(%d,%f,%s): constrained weight %f to [%f,%f]", connId, scale,
							(updateWeightRange?"true":"false"), weight, 0.0f, connInfo->maxWt);
					}
				}

				// update datastructures
				wt[pos_ij] = weight;
				maxSynWt[pos_ij] = connInfo->maxWt; // it's easier to just update, even if it hasn't changed
			}
		}

#ifndef __CPU_ONLY__
		// update GPU datastructures in batches, grouped by post-neuron
		if (simMode_==GPU_MODE) {
			CUDA_CHECK_ERRORS( cudaMemcpy(&(cpu_gpuNetPtrs.wt[cumIdx]), &(wt[cumIdx]), sizeof(float)*Npre[i],
				cudaMemcpyHostToDevice) );

			if (cpu_gpuNetPtrs.maxSynWt!=NULL) {
				// only copy maxSynWt if datastructure actually exists on the GPU
				// (that logic should be done elsewhere though)
				CUDA_CHECK_ERRORS( cudaMemcpy(&(cpu_gpuNetPtrs.maxSynWt[cumIdx]), &(maxSynWt[cumIdx]),
					sizeof(float)*Npre[i], cudaMemcpyHostToDevice));
			}
		}
#endif
	}
}

GroupMonitor* CpuSNN::setGroupMonitor(int grpId, FILE* fid) {
	// check whether group already has a GroupMonitor
	if (grp_Info[grpId].GroupMonitorId >= 0) {
		KERNEL_ERROR("setGroupMonitor has already been called on Group %d (%s).",
			grpId, grp_Info2[grpId].Name.c_str());
		exitSimulation(1);
	}

	// create new GroupMonitorCore object in any case and initialize analysis components
	// grpMonObj destructor (see below) will deallocate it
	GroupMonitorCore* grpMonCoreObj = new GroupMonitorCore(this, numGroupMonitor, grpId);
	groupMonCoreList[numGroupMonitor] = grpMonCoreObj;

	// assign group status file ID if we selected to write to a file, else it's NULL
	// if file pointer exists, it has already been fopened
	// this will also write the header section of the group status file
	// grpMonCoreObj destructor will fclose it
	grpMonCoreObj->setGroupFileId(fid);

	// create a new GroupMonitor object for the user-interface
	// CpuSNN::deleteObjects will deallocate it
	GroupMonitor* grpMonObj = new GroupMonitor(grpMonCoreObj);
	groupMonList[numGroupMonitor] = grpMonObj;

	// also inform the group that it is being monitored...
	grp_Info[grpId].GroupMonitorId = numGroupMonitor;

    // not eating much memory anymore, got rid of all buffers
	cpuSnnSz.monitorInfoSize += sizeof(GroupMonitor*);
	cpuSnnSz.monitorInfoSize += sizeof(GroupMonitorCore*);

	numGroupMonitor++;
	KERNEL_INFO("GroupMonitor set for group %d (%s)",grpId,grp_Info2[grpId].Name.c_str());

	return grpMonObj;
}

ConnectionMonitor* CpuSNN::setConnectionMonitor(int grpIdPre, int grpIdPost, FILE* fid) {
	// find connection based on pre-post pair
	short int connId = getConnectId(grpIdPre,grpIdPost);
	if (connId<0) {
		KERNEL_ERROR("No connection found from group %d(%s) to group %d(%s)", grpIdPre, getGroupName(grpIdPre).c_str(),
			grpIdPost, getGroupName(grpIdPost).c_str());
		exitSimulation(1);
	}

	// check whether connection already has a connection monitor
	grpConnectInfo_t* connInfo = getConnectInfo(connId);
	if (connInfo->ConnectionMonitorId >= 0) {
		KERNEL_ERROR("setConnectionMonitor has already been called on Connection %d (MonitorId=%d)", connId, connInfo->ConnectionMonitorId);
		exitSimulation(1);
	}

	// inform the connection that it is being monitored...
	// this needs to be called before new ConnectionMonitorCore
	connInfo->ConnectionMonitorId = numConnectionMonitor;

	// create new ConnectionMonitorCore object in any case and initialize
	// connMonObj destructor (see below) will deallocate it
	ConnectionMonitorCore* connMonCoreObj = new ConnectionMonitorCore(this, numConnectionMonitor, connId,
		grpIdPre, grpIdPost);
	connMonCoreList[numConnectionMonitor] = connMonCoreObj;

	// assign conn file ID if we selected to write to a file, else it's NULL
	// if file pointer exists, it has already been fopened
	// this will also write the header section of the conn file
	// connMonCoreObj destructor will fclose it
	connMonCoreObj->setConnectFileId(fid);

	// create a new ConnectionMonitor object for the user-interface
	// CpuSNN::deleteObjects will deallocate it
	ConnectionMonitor* connMonObj = new ConnectionMonitor(connMonCoreObj);
	connMonList[numConnectionMonitor] = connMonObj;

	// now init core object (depends on several datastructures allocated above)
	connMonCoreObj->init();

    // not eating much memory anymore, got rid of all buffers
	cpuSnnSz.monitorInfoSize += sizeof(ConnectionMonitor*);
	cpuSnnSz.monitorInfoSize += sizeof(ConnectionMonitorCore*);

	numConnectionMonitor++;
	KERNEL_INFO("ConnectionMonitor %d set for Connection %d: %d(%s) => %d(%s)", connInfo->ConnectionMonitorId, connId, grpIdPre, getGroupName(grpIdPre).c_str(),
		grpIdPost, getGroupName(grpIdPost).c_str());

	return connMonObj;
}

void CpuSNN::setExternalCurrent(int grpId, const std::vector<float>& current) {
	assert(grpId>=0); assert(grpId<numGrp);
	assert(!isPoissonGroup(grpId));
	assert(current.size() == (unsigned int)getGroupNumNeurons(grpId));

	// // update flag for faster handling at run-time
	// if (count_if(current.begin(), current.end(), isGreaterThanZero)) {
	// 	grp_Info[grpId].WithCurrentInjection = true;
	// } else {
	// 	grp_Info[grpId].WithCurrentInjection = false;
	// }

	// store external current in array
	for (int i=grp_Info[grpId].StartN, j=0; i<=grp_Info[grpId].EndN; i++, j++) {
		extCurrent[i] = current[j];
	}

	// copy to GPU if necessary
	// don't allocate; allocation done in buildNetwork
#ifndef __CPU_ONLY__
	if (simMode_==GPU_MODE) {
		copyExternalCurrent(&cpu_gpuNetPtrs, &cpuNetPtrs, false, grpId);
	}
#endif
}

// sets up a spike generator
void CpuSNN::setSpikeGenerator(int grpId, SpikeGeneratorCore* spikeGen) {
	assert(!doneReorganization); // must be called before setupNetwork to work on GPU
	assert(spikeGen);
	assert (grp_Info[grpId].isSpikeGenerator);
	grp_Info[grpId].spikeGen = spikeGen;
}

// A Spike Counter keeps track of the number of spikes per neuron in a group.
void CpuSNN::setSpikeCounter(int grpId, int recordDur) {
	assert(grpId>=0); assert(grpId<numGrp);

	sim_with_spikecounters = true; // inform simulation
	grp_Info[grpId].withSpikeCounter = true; // inform the group
	grp_Info[grpId].spkCntRecordDur = (recordDur>0)?recordDur:-1; // set record duration, after which spike buf will be reset
	grp_Info[grpId].spkCntRecordDurHelper = 0; // counter to help make fast modulo
	grp_Info[grpId].spkCntBufPos = numSpkCnt; // inform group which pos it has in spike buf
	spkCntBuf[numSpkCnt] = new int[grp_Info[grpId].SizeN]; // create spike buf
	memset(spkCntBuf[numSpkCnt],0,(grp_Info[grpId].SizeN)*sizeof(int)); // set all to 0

	numSpkCnt++;

	KERNEL_INFO("SpikeCounter set for Group %d (%s): %d ms recording window", grpId, grp_Info2[grpId].Name.c_str(),
		recordDur);
}

// record spike information, return a SpikeInfo object
SpikeMonitor* CpuSNN::setSpikeMonitor(int grpId, FILE* fid) {
	// check whether group already has a SpikeMonitor
	if (grp_Info[grpId].SpikeMonitorId >= 0) {
		// in this case, return the current object and update fid
		SpikeMonitor* spkMonObj = getSpikeMonitor(grpId);

		// update spike file ID
		SpikeMonitorCore* spkMonCoreObj = getSpikeMonitorCore(grpId);
		spkMonCoreObj->setSpikeFileId(fid);

		KERNEL_INFO("SpikeMonitor updated for group %d (%s)",grpId,grp_Info2[grpId].Name.c_str());
		return spkMonObj;
	} else {
		// create new SpikeMonitorCore object in any case and initialize analysis components
		// spkMonObj destructor (see below) will deallocate it
		SpikeMonitorCore* spkMonCoreObj = new SpikeMonitorCore(this, numSpikeMonitor, grpId);
		spikeMonCoreList[numSpikeMonitor] = spkMonCoreObj;

		// assign spike file ID if we selected to write to a file, else it's NULL
		// if file pointer exists, it has already been fopened
		// this will also write the header section of the spike file
		// spkMonCoreObj destructor will fclose it
		spkMonCoreObj->setSpikeFileId(fid);

		// create a new SpikeMonitor object for the user-interface
		// CpuSNN::deleteObjects will deallocate it
		SpikeMonitor* spkMonObj = new SpikeMonitor(spkMonCoreObj);
		spikeMonList[numSpikeMonitor] = spkMonObj;

		// also inform the grp that it is being monitored...
		grp_Info[grpId].SpikeMonitorId	= numSpikeMonitor;

    	// not eating much memory anymore, got rid of all buffers
		cpuSnnSz.monitorInfoSize += sizeof(SpikeMonitor*);
		cpuSnnSz.monitorInfoSize += sizeof(SpikeMonitorCore*);

		numSpikeMonitor++;
		KERNEL_INFO("SpikeMonitor set for group %d (%s)",grpId,grp_Info2[grpId].Name.c_str());

		return spkMonObj;
	}
}

// assigns spike rate to group
void CpuSNN::setSpikeRate(int grpId, PoissonRate* ratePtr, int refPeriod) {
	assert(grpId>=0 && grpId<numGrp);
	assert(ratePtr);
	assert(grp_Info[grpId].isSpikeGenerator);
	assert(ratePtr->getNumNeurons()==grp_Info[grpId].SizeN);
	assert(refPeriod>=1);

	grp_Info[grpId].RatePtr = ratePtr;
	grp_Info[grpId].RefractPeriod   = refPeriod;
	spikeRateUpdated = true;
}

// sets the weight value of a specific synapse
void CpuSNN::setWeight(short int connId, int neurIdPre, int neurIdPost, float weight, bool updateWeightRange) {
	assert(connId>=0 && connId<getNumConnections());
	assert(weight>=0.0f);

	grpConnectInfo_t* connInfo = getConnectInfo(connId);
	assert(neurIdPre>=0  && neurIdPre<getGroupNumNeurons(connInfo->grpSrc));
	assert(neurIdPost>=0 && neurIdPost<getGroupNumNeurons(connInfo->grpDest));

	float maxWt = fabs(connInfo->maxWt);
	float minWt = 0.0f;

	// inform user of acton taken if weight is out of bounds
	bool needToPrintDebug = (weight>maxWt || weight<minWt);

	if (updateWeightRange) {
		// if this flag is set, we need to update minWt,maxWt accordingly
		// will be saving new maxSynWt and copying to GPU below
//		connInfo->minWt = fmin(connInfo->minWt, weight);
		maxWt = fmax(maxWt, weight);
		if (needToPrintDebug) {
			KERNEL_DEBUG("setWeight(%d,%d,%d,%f,%s): updated weight ranges to [%f,%f]", connId, neurIdPre, neurIdPost,
				weight, (updateWeightRange?"true":"false"), minWt, maxWt);
		}
	} else {
		// constrain weight to boundary values
		// compared to above, we swap minWt/maxWt logic
		weight = fmin(weight, maxWt);
		weight = fmax(weight, minWt);
		if (needToPrintDebug) {
			KERNEL_DEBUG("setWeight(%d,%d,%d,%f,%s): constrained weight %f to [%f,%f]", connId, neurIdPre, neurIdPost,
				weight, (updateWeightRange?"true":"false"), weight, minWt, maxWt);
		}
	}

	// find real ID of pre- and post-neuron
	int neurIdPreReal = grp_Info[connInfo->grpSrc].StartN+neurIdPre;
	int neurIdPostReal = grp_Info[connInfo->grpDest].StartN+neurIdPost;

	// iterate over all presynaptic synapses until right one is found
	bool synapseFound = false;
	int pos_ij = cumulativePre[neurIdPostReal];
	for (int j=0; j<Npre[neurIdPostReal]; pos_ij++, j++) {
		post_info_t* preId = &preSynapticIds[pos_ij];
//		int pre_nid = GET_CONN_NEURON_ID((*preId));
		if (GET_CONN_NEURON_ID((*preId))==(unsigned int)neurIdPreReal) {
			assert(cumConnIdPre[pos_ij]==connId); // make sure we've got the right connection ID

			wt[pos_ij] = isExcitatoryGroup(connInfo->grpSrc) ? weight : -1.0*weight;
			maxSynWt[pos_ij] = isExcitatoryGroup(connInfo->grpSrc) ? maxWt : -1.0*maxWt;

#ifndef __CPU_ONLY__
			if (simMode_==GPU_MODE) {
				// need to update datastructures on GPU
				CUDA_CHECK_ERRORS( cudaMemcpy(&(cpu_gpuNetPtrs.wt[pos_ij]), &(wt[pos_ij]), sizeof(float), cudaMemcpyHostToDevice));
				if (cpu_gpuNetPtrs.maxSynWt!=NULL) {
					// only copy maxSynWt if datastructure actually exists on the GPU
					// (that logic should be done elsewhere though)
					CUDA_CHECK_ERRORS( cudaMemcpy(&(cpu_gpuNetPtrs.maxSynWt[pos_ij]), &(maxSynWt[pos_ij]), sizeof(float), cudaMemcpyHostToDevice));
				}
			}
#endif

			// synapse found and updated: we're done!
			synapseFound = true;
			break;
		}
	}

	if (!synapseFound) {
		KERNEL_WARN("setWeight(%d,%d,%d,%f,%s): Synapse does not exist, not updated.", connId, neurIdPre, neurIdPost,
			weight, (updateWeightRange?"true":"false"));
	}
}


// writes network state to file
// handling of file pointer should be handled externally: as far as this function is concerned, it is simply
// trying to write to file
void CpuSNN::saveSimulation(FILE* fid, bool saveSynapseInfo) {
	int tmpInt;
	float tmpFloat;

	// +++++ WRITE HEADER SECTION +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ //

	// write file signature
	tmpInt = 294338571; // some int used to identify saveSimulation files
	if (!fwrite(&tmpInt,sizeof(int),1,fid)) KERNEL_ERROR("saveSimulation fwrite error");

	// write version number
	tmpFloat = 0.2f;
	if (!fwrite(&tmpFloat,sizeof(int),1,fid)) KERNEL_ERROR("saveSimulation fwrite error");

	// write simulation time so far (in seconds)
	tmpFloat = ((float)simTimeSec) + ((float)simTimeMs)/1000.0f;
	if (!fwrite(&tmpFloat,sizeof(float),1,fid)) KERNEL_ERROR("saveSimulation fwrite error");

	// write execution time so far (in seconds)
	if(simMode_ == CPU_MODE) {
		stopCPUTiming();
		tmpFloat = cpuExecutionTime/1000.0f;
#ifndef __CPU_ONLY__
	} else {
		stopGPUTiming();
		tmpFloat = gpuExecutionTime/1000.0f;
#endif
	}
	if (!fwrite(&tmpFloat,sizeof(float),1,fid)) KERNEL_ERROR("saveSimulation fwrite error");

	// TODO: add more params of interest

	// write network info
	if (!fwrite(&numN,sizeof(int),1,fid)) KERNEL_ERROR("saveSimulation fwrite error");
	if (!fwrite(&preSynCnt,sizeof(int),1,fid)) KERNEL_ERROR("saveSimulation fwrite error");
	if (!fwrite(&postSynCnt,sizeof(int),1,fid)) KERNEL_ERROR("saveSimulation fwrite error");
	if (!fwrite(&numGrp,sizeof(int),1,fid)) KERNEL_ERROR("saveSimulation fwrite error");

	// write group info
	char name[100];
	for (int g=0;g<numGrp;g++) {
		if (!fwrite(&grp_Info[g].StartN,sizeof(int),1,fid)) KERNEL_ERROR("saveSimulation fwrite error");
		if (!fwrite(&grp_Info[g].EndN,sizeof(int),1,fid)) KERNEL_ERROR("saveSimulation fwrite error");

		if (!fwrite(&grp_Info[g].SizeX,sizeof(int),1,fid)) KERNEL_ERROR("saveSimulation fwrite error");
		if (!fwrite(&grp_Info[g].SizeY,sizeof(int),1,fid)) KERNEL_ERROR("saveSimulation fwrite error");
		if (!fwrite(&grp_Info[g].SizeZ,sizeof(int),1,fid)) KERNEL_ERROR("saveSimulation fwrite error");

		strncpy(name,grp_Info2[g].Name.c_str(),100);
		if (!fwrite(name,1,100,fid)) KERNEL_ERROR("saveSimulation fwrite error");
	}

#ifndef __CPU_ONLY__
	// +++++ Fetch WEIGHT DATA (GPU Mode only) ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ //
	if (simMode_ == GPU_MODE)
		copyWeightState(&cpuNetPtrs, &cpu_gpuNetPtrs, cudaMemcpyDeviceToHost, false);
#endif

	// +++++ WRITE SYNAPSE INFO +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ //

	// \FIXME: replace with faster version
	if (saveSynapseInfo) {
		for (int i=0;i<numN;i++) {
			unsigned int offset = cumulativePost[i];

			unsigned int count = 0;
			for (int t=0;t<maxDelay_;t++) {
				delay_info_t dPar = postDelayInfo[i*(maxDelay_+1)+t];

				for(int idx_d=dPar.delay_index_start; idx_d<(dPar.delay_index_start+dPar.delay_length); idx_d++)
					count++;
			}

			if (!fwrite(&count,sizeof(int),1,fid)) KERNEL_ERROR("saveSimulation fwrite error");

			for (int t=0;t<maxDelay_;t++) {
				delay_info_t dPar = postDelayInfo[i*(maxDelay_+1)+t];

				for(int idx_d=dPar.delay_index_start; idx_d<(dPar.delay_index_start+dPar.delay_length); idx_d++) {
					// get synaptic info...
					post_info_t post_info = postSynapticIds[offset + idx_d];

					// get neuron id
					//int p_i = (post_info&POST_SYN_NEURON_MASK);
					unsigned int p_i = GET_CONN_NEURON_ID(post_info);
					assert(p_i<(unsigned int)numN);

					// get syn id
					unsigned int s_i = GET_CONN_SYN_ID(post_info);
					//>>POST_SYN_NEURON_BITS)&POST_SYN_CONN_MASK;
					assert(s_i<(Npre[p_i]));

					// get the cumulative position for quick access...
					unsigned int pos_i = cumulativePre[p_i] + s_i;

					uint8_t delay = t+1;
					uint8_t plastic = s_i < Npre_plastic[p_i]; // plastic or fixed.

					if (!fwrite(&i,sizeof(int),1,fid)) KERNEL_ERROR("saveSimulation fwrite error");
					if (!fwrite(&p_i,sizeof(int),1,fid)) KERNEL_ERROR("saveSimulation fwrite error");
					if (!fwrite(&(wt[pos_i]),sizeof(float),1,fid)) KERNEL_ERROR("saveSimulation fwrite error");
					if (!fwrite(&(maxSynWt[pos_i]),sizeof(float),1,fid)) KERNEL_ERROR("saveSimulation fwrite error");
					if (!fwrite(&delay,sizeof(uint8_t),1,fid)) KERNEL_ERROR("saveSimulation fwrite error");
					if (!fwrite(&plastic,sizeof(uint8_t),1,fid)) KERNEL_ERROR("saveSimulation fwrite error");
					if (!fwrite(&(cumConnIdPre[pos_i]),sizeof(short int),1,fid)) KERNEL_ERROR("saveSimulation fwrite error");
				}
			}
		}
	}
}

// writes population weights from gIDpre to gIDpost to file fname in binary
void CpuSNN::writePopWeights(std::string fname, int grpIdPre, int grpIdPost) {
	assert(grpIdPre>=0); assert(grpIdPost>=0);

	float* weights;
	int matrixSize;
	FILE* fid;
//	int numPre, numPost;
	fid = fopen(fname.c_str(), "wb");
	assert(fid != NULL);

	if(!doneReorganization){
		KERNEL_ERROR("Simulation has not been run yet, cannot output weights.");
		exitSimulation(1);
	}

	post_info_t* preId;
	int pre_nid, pos_ij;

	//population sizes
//	numPre = grp_Info[grpIdPre].SizeN;
//	numPost = grp_Info[grpIdPost].SizeN;

	//first iteration gets the number of synaptic weights to place in our
	//weight matrix.
	matrixSize=0;
	//iterate over all neurons in the post group
	for (int i=grp_Info[grpIdPost].StartN; i<=grp_Info[grpIdPost].EndN; i++) {
		// for every post-neuron, find all pre
		pos_ij = cumulativePre[i]; // i-th neuron, j=0th synapse
		//iterate over all presynaptic synapses
		for(int j=0; j<Npre[i]; pos_ij++,j++) {
			preId = &preSynapticIds[pos_ij];
			pre_nid = GET_CONN_NEURON_ID((*preId)); // neuron id of pre
			if (pre_nid<grp_Info[grpIdPre].StartN || pre_nid>grp_Info[grpIdPre].EndN)
				continue; // connection does not belong to group grpIdPre
			matrixSize++;
		}
	}

	//now we have the correct size
	weights = new float[matrixSize];
	//second iteration assigns the weights
	int curr = 0; // iterator for return array
	//iterate over all neurons in the post group
	for (int i=grp_Info[grpIdPost].StartN; i<=grp_Info[grpIdPost].EndN; i++) {
		// for every post-neuron, find all pre
		pos_ij = cumulativePre[i]; // i-th neuron, j=0th synapse
		//do the GPU copy here.  Copy the current weights from GPU to CPU.
#ifndef __CPU_ONLY__
		if(simMode_==GPU_MODE){
			copyWeightsGPU(i,grpIdPre);
		}
#endif

		//iterate over all presynaptic synapses
		for(int j=0; j<Npre[i]; pos_ij++,j++) {
			preId = &preSynapticIds[pos_ij];
			pre_nid = GET_CONN_NEURON_ID((*preId)); // neuron id of pre
			if (pre_nid<grp_Info[grpIdPre].StartN || pre_nid>grp_Info[grpIdPre].EndN)
				continue; // connection does not belong to group grpIdPre
			weights[curr] = wt[pos_ij];
			curr++;
		}
	}

	fwrite(weights,sizeof(float),matrixSize,fid);
	fclose(fid);
	//Let my memory FREE!!!
	delete [] weights;
}


/// ************************************************************************************************************ ///
/// PUBLIC METHODS: PLOTTING / LOGGING
/// ************************************************************************************************************ ///

// set new file pointer for all files
// fp==NULL is code for don't change it
// can be called in all logger modes; however, the analogous interface function can only be called in CUSTOM
void CpuSNN::setLogsFp(FILE* fpInf, FILE* fpErr, FILE* fpDeb, FILE* fpLog) {
	if (fpInf!=NULL) {
		if (fpInf_!=NULL && fpInf_!=stdout && fpInf_!=stderr)
			fclose(fpInf_);
		fpInf_ = fpInf;
	}

	if (fpErr!=NULL) {
		if (fpErr_ != NULL && fpErr_!=stdout && fpErr_!=stderr)
			fclose(fpErr_);
		fpErr_ = fpErr;
	}

	if (fpDeb!=NULL) {
		if (fpDeb_!=NULL && fpDeb_!=stdout && fpDeb_!=stderr)
			fclose(fpDeb_);
		fpDeb_ = fpDeb;
	}

	if (fpLog!=NULL) {
		if (fpLog_!=NULL && fpLog_!=stdout && fpLog_!=stderr)
			fclose(fpLog_);
		fpLog_ = fpLog;
	}
}


/// **************************************************************************************************************** ///
/// GETTERS / SETTERS
/// **************************************************************************************************************** ///

// loop over linked list entries to find a connection with the right pre-post pair, O(N)
short int CpuSNN::getConnectId(int grpIdPre, int grpIdPost) {
	grpConnectInfo_t* connInfo = connectBegin;

	short int connId = -1;
	while (connInfo) {
		// check whether pre and post match
		if (connInfo->grpSrc == grpIdPre && connInfo->grpDest == grpIdPost) {
			connId = connInfo->connId;
			break;
		}

		// otherwise, keep looking
		connInfo = connInfo->next;
	}

	return connId;
}

//! used for parameter tuning functionality
grpConnectInfo_t* CpuSNN::getConnectInfo(short int connectId) {
	grpConnectInfo_t* nextConn = connectBegin;
	CHECK_CONNECTION_ID(connectId, numConnections);

	// clear all existing connection info...
	while (nextConn) {
		if (nextConn->connId == connectId) {
			nextConn->newUpdates = true;		// \FIXME: this is a Jay hack
			return nextConn;
		}
		nextConn = nextConn->next;
	}

	KERNEL_DEBUG("Total Connections = %d", numConnections);
	KERNEL_DEBUG("ConnectId (%d) cannot be recognized", connectId);
	return NULL;
}

std::vector<float> CpuSNN::getConductanceAMPA(int grpId) {
	assert(isSimulationWithCOBA());

#ifndef __CPU_ONLY__
	// need to copy data from GPU first
	if (getSimMode()==GPU_MODE) {
		copyConductanceAMPA(&cpuNetPtrs, &cpu_gpuNetPtrs, cudaMemcpyDeviceToHost, false, grpId);
	}
#endif

	std::vector<float> gAMPAvec;
	for (int i=grp_Info[grpId].StartN; i<=grp_Info[grpId].EndN; i++) {
		gAMPAvec.push_back(gAMPA[i]);
	}
	return gAMPAvec;
}

std::vector<float> CpuSNN::getConductanceNMDA(int grpId) {
	assert(isSimulationWithCOBA());

#ifndef __CPU_ONLY__
	// need to copy data from GPU first
	if (getSimMode()==GPU_MODE)
		copyConductanceNMDA(&cpuNetPtrs, &cpu_gpuNetPtrs, cudaMemcpyDeviceToHost, false, grpId);
#endif

	std::vector<float> gNMDAvec;
	if (isSimulationWithNMDARise()) {
		// need to construct conductance from rise and decay parts
		for (int i=grp_Info[grpId].StartN; i<=grp_Info[grpId].EndN; i++) {
			gNMDAvec.push_back(gNMDA_d[i]-gNMDA_r[i]);
		}
	} else {
		for (int i=grp_Info[grpId].StartN; i<=grp_Info[grpId].EndN; i++) {
			gNMDAvec.push_back(gNMDA[i]);
		}
	}
	return gNMDAvec;
}

std::vector<float> CpuSNN::getConductanceGABAa(int grpId) {
	assert(isSimulationWithCOBA());

#ifndef __CPU_ONLY__
	// need to copy data from GPU first
	if (getSimMode()==GPU_MODE) {
		copyConductanceGABAa(&cpuNetPtrs, &cpu_gpuNetPtrs, cudaMemcpyDeviceToHost, false, grpId);
	}
#endif

	std::vector<float> gGABAaVec;
	for (int i=grp_Info[grpId].StartN; i<=grp_Info[grpId].EndN; i++) {
		gGABAaVec.push_back(gGABAa[i]);
	}
	return gGABAaVec;
}

std::vector<float> CpuSNN::getConductanceGABAb(int grpId) {
	assert(isSimulationWithCOBA());

#ifndef __CPU_ONLY__
	// need to copy data from GPU first
	if (getSimMode()==GPU_MODE) {
		copyConductanceGABAb(&cpuNetPtrs, &cpu_gpuNetPtrs, cudaMemcpyDeviceToHost, false, grpId);
	}
#endif

	std::vector<float> gGABAbVec;
	if (isSimulationWithGABAbRise()) {
		// need to construct conductance from rise and decay parts
		for (int i=grp_Info[grpId].StartN; i<=grp_Info[grpId].EndN; i++) {
			gGABAbVec.push_back(gGABAb_d[i]-gGABAb_r[i]);
		}
	} else {
		for (int i=grp_Info[grpId].StartN; i<=grp_Info[grpId].EndN; i++) {
			gGABAbVec.push_back(gGABAb[i]);
		}
	}
	return gGABAbVec;
}

// returns RangeDelay struct of a connection
RangeDelay CpuSNN::getDelayRange(short int connId) {
	assert(connId>=0 && connId<numConnections);
	grpConnectInfo_t* connInfo = getConnectInfo(connId);
	return RangeDelay(connInfo->minDelay, connInfo->maxDelay);
}


// this is a user function
// \FIXME: fix this
uint8_t* CpuSNN::getDelays(int gIDpre, int gIDpost, int& Npre, int& Npost, uint8_t* delays) {
	Npre = grp_Info[gIDpre].SizeN;
	Npost = grp_Info[gIDpost].SizeN;

	if (delays == NULL) delays = new uint8_t[Npre*Npost];
	memset(delays,0,Npre*Npost);

	for (int i=grp_Info[gIDpre].StartN;i<grp_Info[gIDpre].EndN;i++) {
		unsigned int offset = cumulativePost[i];

		for (int t=0;t<maxDelay_;t++) {
			delay_info_t dPar = postDelayInfo[i*(maxDelay_+1)+t];

			for(int idx_d=dPar.delay_index_start; idx_d<(dPar.delay_index_start+dPar.delay_length); idx_d++) {
				// get synaptic info...
				post_info_t post_info = postSynapticIds[offset + idx_d];

				// get neuron id
				//int p_i = (post_info&POST_SYN_NEURON_MASK);
				int p_i = GET_CONN_NEURON_ID(post_info);
				assert(p_i<numN);

				if (p_i >= grp_Info[gIDpost].StartN && p_i <= grp_Info[gIDpost].EndN) {
					// get syn id
//					int s_i = GET_CONN_SYN_ID(post_info);

					// get the cumulative position for quick access...
//					unsigned int pos_i = cumulativePre[p_i] + s_i;

					delays[i+Npre*(p_i-grp_Info[gIDpost].StartN)] = t+1;
				}
			}
		}
	}
	return delays;
}

Grid3D CpuSNN::getGroupGrid3D(int grpId) {
	assert(grpId>=0 && grpId<numGrp);
	return Grid3D(grp_Info[grpId].SizeX, grp_Info[grpId].SizeY, grp_Info[grpId].SizeZ);
}

// find ID of group with name grpName
int CpuSNN::getGroupId(std::string grpName) {
	for (int grpId=0; grpId<numGrp; grpId++) {
		if (grp_Info2[grpId].Name.compare(grpName)==0)
			return grpId;
	}

	// group not found
	return -1;
}

group_info_t CpuSNN::getGroupInfo(int grpId) {
	assert(grpId>=-1 && grpId<numGrp);
	return grp_Info[grpId];
}

std::string CpuSNN::getGroupName(int grpId) {
	assert(grpId>=-1 && grpId<numGrp);

	if (grpId==ALL)
		return "ALL";

	return grp_Info2[grpId].Name;
}

GroupSTDPInfo_t CpuSNN::getGroupSTDPInfo(int grpId) {
	GroupSTDPInfo_t gInfo;

	gInfo.WithSTDP = grp_Info[grpId].WithSTDP;
	gInfo.WithESTDP = grp_Info[grpId].WithESTDP;
	gInfo.WithISTDP = grp_Info[grpId].WithISTDP;
	gInfo.WithESTDPtype = grp_Info[grpId].WithESTDPtype;
	gInfo.WithISTDPtype = grp_Info[grpId].WithISTDPtype;
	gInfo.WithESTDPcurve = grp_Info[grpId].WithESTDPcurve;
	gInfo.WithISTDPcurve = grp_Info[grpId].WithISTDPcurve;
	gInfo.ALPHA_MINUS_EXC = grp_Info[grpId].ALPHA_MINUS_EXC;
	gInfo.ALPHA_PLUS_EXC = grp_Info[grpId].ALPHA_PLUS_EXC;
	gInfo.TAU_MINUS_INV_EXC = grp_Info[grpId].TAU_MINUS_INV_EXC;
	gInfo.TAU_PLUS_INV_EXC = grp_Info[grpId].TAU_PLUS_INV_EXC;
	gInfo.ALPHA_MINUS_INB = grp_Info[grpId].ALPHA_MINUS_INB;
	gInfo.ALPHA_PLUS_INB = grp_Info[grpId].ALPHA_PLUS_INB;
	gInfo.TAU_MINUS_INV_INB = grp_Info[grpId].TAU_MINUS_INV_INB;
	gInfo.TAU_PLUS_INV_INB = grp_Info[grpId].TAU_PLUS_INV_INB;
	gInfo.GAMMA = grp_Info[grpId].GAMMA;
	gInfo.BETA_LTP = grp_Info[grpId].BETA_LTP;
	gInfo.BETA_LTD = grp_Info[grpId].BETA_LTD;
	gInfo.LAMBDA = grp_Info[grpId].LAMBDA;
	gInfo.DELTA = grp_Info[grpId].DELTA;

	return gInfo;
}

GroupNeuromodulatorInfo_t CpuSNN::getGroupNeuromodulatorInfo(int grpId) {
	GroupNeuromodulatorInfo_t gInfo;

	gInfo.baseDP = grp_Info[grpId].baseDP;
	gInfo.base5HT = grp_Info[grpId].base5HT;
	gInfo.baseACh = grp_Info[grpId].baseACh;
	gInfo.baseNE = grp_Info[grpId].baseNE;
	gInfo.decayDP = grp_Info[grpId].decayDP;
	gInfo.decay5HT = grp_Info[grpId].decay5HT;
	gInfo.decayACh = grp_Info[grpId].decayACh;
	gInfo.decayNE = grp_Info[grpId].decayNE;

	return gInfo;
}

Point3D CpuSNN::getNeuronLocation3D(int neurId) {
	assert(neurId>=0 && neurId<numN);
	int grpId = grpIds[neurId];
	assert(neurId>=grp_Info[grpId].StartN && neurId<=grp_Info[grpId].EndN);

	// adjust neurId for neuron ID of first neuron in the group
	neurId -= grp_Info[grpId].StartN;

	return getNeuronLocation3D(grpId, neurId);
}

Point3D CpuSNN::getNeuronLocation3D(int grpId, int relNeurId) {
	assert(grpId>=0 && grpId<numGrp);
	assert(relNeurId>=0 && relNeurId<getGroupNumNeurons(grpId));

	// coordinates are in x e[-SizeX/2,SizeX/2], y e[-SizeY/2,SizeY/2], z e[-SizeZ/2,SizeZ/2]
	// instead of x e[0,SizeX], etc.
	int intX = relNeurId % grp_Info[grpId].SizeX;
	int intY = (relNeurId/grp_Info[grpId].SizeX)%grp_Info[grpId].SizeY;
	int intZ = relNeurId/(grp_Info[grpId].SizeX*grp_Info[grpId].SizeY);

	// so subtract SizeX/2, etc. to get coordinates center around origin
	double coordX = 1.0*intX - (grp_Info[grpId].SizeX-1)/2.0;
	double coordY = 1.0*intY - (grp_Info[grpId].SizeY-1)/2.0;
	double coordZ = 1.0*intZ - (grp_Info[grpId].SizeZ-1)/2.0;
	return Point3D(coordX, coordY, coordZ);
}

// returns the number of synaptic connections associated with this connection.
int CpuSNN::getNumSynapticConnections(short int connectionId) {
//  grpConnectInfo_t* connInfo;
  grpConnectInfo_t* connIterator = connectBegin;
  while(connIterator){
    if(connIterator->connId == connectionId){
      //found the corresponding connection
      return connIterator->numberOfConnections;
    }
    //move to the next grpConnectInfo_t
    connIterator=connIterator->next;
  }
  //we didn't find the connection.
  KERNEL_ERROR("Connection ID was not found.  Quitting.");
  exitSimulation(1);
  return -1;
}

// return spike buffer, which contains #spikes per neuron in the group
int* CpuSNN::getSpikeCounter(int grpId) {
	assert(grpId>=0); assert(grpId<numGrp);

	if (!grp_Info[grpId].withSpikeCounter)
		return NULL;

	// determine whether spike counts are currently stored on CPU or GPU side
	bool retrieveSpikesFromGPU = simMode_==GPU_MODE;
	if (grp_Info[grpId].isSpikeGenerator) {
		// this flag should be set if group was created via CARLsim::createSpikeGeneratorGroup
		// could be SpikeGen callback or PoissonRate
		if (grp_Info[grpId].RatePtr != NULL) {
			// group is Poisson group
			// even though mean rates might be on either CPU or GPU (RatePtr->isOnGPU()), in GPU mode the
			// actual random numbers will always be generated on the GPU
//			retrieveSpikesFromGPU = simMode_==GPU_MODE;
		} else {
			// group is generator with callback, CPU only
			retrieveSpikesFromGPU = false;
		}
	}

	// retrieve spikes from either CPU or GPU
#ifndef __CPU_ONLY__
	if (retrieveSpikesFromGPU) {
		return getSpikeCounter_GPU(grpId);
	} else {
#endif
		int bufPos = grp_Info[grpId].spkCntBufPos; // retrieve buf pos
		return spkCntBuf[bufPos]; // return pointer to buffer
#ifndef __CPU_ONLY__
	}
#endif
}

// returns pointer to existing SpikeMonitor object, NULL else
SpikeMonitor* CpuSNN::getSpikeMonitor(int grpId) {
	assert(grpId>=0 && grpId<getNumGroups());
	if (grp_Info[grpId].SpikeMonitorId>=0) {
		return spikeMonList[(grp_Info[grpId].SpikeMonitorId)];
	} else {
		return NULL;
	}
}

SpikeMonitorCore* CpuSNN::getSpikeMonitorCore(int grpId) {
	assert(grpId>=0 && grpId<getNumGroups());
	if (grp_Info[grpId].SpikeMonitorId>=0) {
		return spikeMonCoreList[(grp_Info[grpId].SpikeMonitorId)];
	} else {
		return NULL;
	}
}

// returns RangeWeight struct of a connection
RangeWeight CpuSNN::getWeightRange(short int connId) {
	assert(connId>=0 && connId<numConnections);
	grpConnectInfo_t* connInfo = getConnectInfo(connId);
	return RangeWeight(0.0f, connInfo->initWt, connInfo->maxWt);
}


/// **************************************************************************************************************** ///
/// PRIVATE METHODS
/// **************************************************************************************************************** ///

// all unsafe operations of CpuSNN constructor
void CpuSNN::CpuSNNinit() {
	assert(ithGPU_>=0);

	// set logger mode (defines where to print all status, error, and debug messages)
	switch (loggerMode_) {
	case USER:
		fpInf_ = stdout;
		fpErr_ = stderr;
		#if defined(WIN32) || defined(WIN64)
			fpDeb_ = fopen("nul","w");
		#else
			fpDeb_ = fopen("/dev/null","w");
		#endif
		break;
	case DEVELOPER:
		fpInf_ = stdout;
		fpErr_ = stderr;
		fpDeb_ = stdout;
		break;
	case SHOWTIME:
		#if defined(WIN32) || defined(WIN64)
			fpInf_ = fopen("nul","w");
		#else
			fpInf_ = fopen("/dev/null","w");
		#endif
		fpErr_ = stderr;
		#if defined(WIN32) || defined(WIN64)
			fpDeb_ = fopen("nul","w");
		#else
			fpDeb_ = fopen("/dev/null","w");
		#endif
		break;
	case SILENT:
	case CUSTOM:
		#if defined(WIN32) || defined(WIN64)
			fpInf_ = fopen("nul","w");
			fpErr_ = fopen("nul","w");
			fpDeb_ = fopen("nul","w");
		#else
			fpInf_ = fopen("/dev/null","w");
			fpErr_ = fopen("/dev/null","w");
			fpDeb_ = fopen("/dev/null","w");
		#endif
	break;
	default:
		fpErr_ = stderr; // need to open file stream first
		KERNEL_ERROR("Unknown logger mode. Aborting simulation...");
		exit(1);
	}

	// try to open log file in results folder: create if not exists
	#if defined(WIN32) || defined(WIN64)
		CreateDirectory("results", NULL);
		fpLog_ = fopen("results/carlsim.log", "w");
	#else
		struct stat sb;
		int createDir = 1;
		if (stat("results", &sb) == -1 || !S_ISDIR(sb.st_mode)) {
			// results dir does not exist, try to create:
			createDir = mkdir("results", 0777);
		}

		if (createDir == -1) {
			// tried to create dir, but failed
			fprintf(stderr, "Could not create directory \"results/\", which is required to "
				"store simulation results. Aborting simulation...\n");
			exit(1);
		} else {
			// open log file
			fpLog_ = fopen("results/carlsim.log","w");

			if (createDir == 0) {
				// newly created dir: now that fpLog_/fpInf_ exist, inform user
				KERNEL_INFO("Created results directory \"results/\".");
			}
		}
	#endif
	if (fpLog_ == NULL) {
		fprintf(stderr, "Could not create the directory \"results/\" or the log file \"results/carlsim.log\""
			", which is required to store simulation results. Aborting simulation...\n");
		exit(1);
	}
	
	#ifdef __REGRESSION_TESTING__
	#if defined(WIN32) || defined(WIN64)
		fpInf_ = fopen("nul","w");
		fpErr_ = fopen("nul","w");
		fpDeb_ = fopen("nul","w");
	#else
		fpInf_ = fopen("/dev/null","w");
		fpErr_ = fopen("/dev/null","w");
		fpDeb_ = fopen("/dev/null","w");
	#endif
	#endif

	KERNEL_INFO("*********************************************************************************");
	KERNEL_INFO("********************      Welcome to CARLsim %d.%d      ***************************",
				MAJOR_VERSION,MINOR_VERSION);
	KERNEL_INFO("*********************************************************************************\n");

	KERNEL_INFO("***************************** Configuring Network ********************************");
	KERNEL_INFO("Starting CARLsim simulation \"%s\" in %s mode",networkName_.c_str(),
		loggerMode_string[loggerMode_]);
	KERNEL_INFO("Random number seed: %d",randSeed_);

	time_t rawtime;
	struct tm * timeinfo;
	time(&rawtime);
	timeinfo = localtime(&rawtime);
	KERNEL_DEBUG("Current local time and date: %s", asctime(timeinfo));

	// init random seed
	srand48(randSeed_);
	//getRand.seed(randSeed_*2);
	//getRandClosed.seed(randSeed_*3);

	finishedPoissonGroup  = false;
	connectBegin = NULL;
	compConnectBegin = NULL;

	simTimeRunStart     = 0;    simTimeRunStop      = 0;
	simTimeLastRunSummary = 0;
	simTimeMs	 		= 0;    simTimeSec          = 0;    simTime = 0;
	spikeCountAll1secHost	= 0;    secD1fireCntHost    = 0;    secD2fireCntHost  = 0;
	spikeCountAllHost 		= 0;    spikeCountD2Host    = 0;    spikeCountD1Host = 0;
	nPoissonSpikes 		= 0;

	numGrp   = 0;
	numConnections = 0;
	numCompartmentConnections = 0;
	numSpikeGenGrps  = 0;
	NgenFunc = 0;
	simulatorDeleted = false;

	allocatedN      = 0;
	allocatedComp   = 0;
	allocatedPre    = 0;
	allocatedPost   = 0;
	doneReorganization = false;
	memoryOptimized	   = false;

	cumExecutionTime = 0.0;
	cpuExecutionTime = 0.0;
	gpuExecutionTime = 0.0;

	spikeRateUpdated = false;
	numSpikeMonitor = 0;
	numGroupMonitor = 0;
	numConnectionMonitor = 0;
	numSpkCnt = 0;

	sim_with_compartments = false;
	sim_with_fixedwts = true; // default is true, will be set to false if there are any plastic synapses
	sim_with_conductances = false; // default is false
	sim_with_stdp = false;
	sim_with_modulated_stdp = false;
	sim_with_homeostasis = false;
	sim_with_stp = false;
	sim_in_testing = false;

	maxSpikesD2 = maxSpikesD1 = 0;
	loadSimFID = NULL;

	numN = 0;
	numNPois = 0;
	numNExcPois = 0;
	numNInhPois = 0;
	numNReg = 0;
	numComp = 0;
	numNExcReg = 0;
	numNInhReg = 0;

	numPostSynapses_ = 0;
	numPreSynapses_ = 0;
	maxDelay_ = 0;

	// conductance info struct for simulation
	sim_with_NMDA_rise = false;
	sim_with_GABAb_rise = false;
	dAMPA  = 1.0-1.0/5.0;		// some default decay and rise times
	rNMDA  = 1.0-1.0/10.0;
	dNMDA  = 1.0-1.0/150.0;
	sNMDA  = 1.0;
	dGABAa = 1.0-1.0/6.0;
	rGABAb = 1.0-1.0/100.0;
	dGABAb = 1.0-1.0/150.0;
	sGABAb = 1.0;

	// default integration method: Forward-Euler with 0.5ms integration step
	setIntegrationMethod(FORWARD_EULER, 2);

#ifndef __CPU_ONLY__
	// each CpuSNN object hold its own random number object
	gpuPoissonRand = NULL;
#endif
	// reset all pointers, don't deallocate (false)
	resetPointers(false);

	memset(&cpuSnnSz, 0, sizeof(cpuSnnSz));

	showGrpFiringInfo = true;

	// initialize propogated spike buffers.....
	pbuf = new PropagatedSpikeBuffer(0, PROPAGATED_BUFFER_SIZE);

	memset(&cpu_gpuNetPtrs, 0, sizeof(network_ptr_t));
	memset(&net_Info, 0, sizeof(network_info_t));
	cpu_gpuNetPtrs.allocated = false;

	memset(&cpuNetPtrs, 0, sizeof(network_ptr_t));
	cpuNetPtrs.allocated = false;

	for (int i=0; i < MAX_GRP_PER_SNN; i++) {
		grp_Info[i].Type = UNKNOWN_NEURON;
		grp_Info[i].MaxFiringRate = UNKNOWN_NEURON_MAX_FIRING_RATE;
		grp_Info[i].SpikeMonitorId = -1;
		grp_Info[i].GroupMonitorId = -1;
//		grp_Info[i].ConnectionMonitorId = -1;
		grp_Info[i].FiringCount1sec=0;
		grp_Info[i].numPostSynapses 		= 0;	// default value
		grp_Info[i].numPreSynapses 	= 0;	// default value
		grp_Info[i].WithSTP = false;
		grp_Info[i].WithSTDP = false;
		grp_Info[i].WithESTDP = false;
		grp_Info[i].WithISTDP = false;
		grp_Info[i].WithESTDPtype = UNKNOWN_STDP;
		grp_Info[i].WithISTDPtype = UNKNOWN_STDP;
		grp_Info[i].WithESTDPcurve = UNKNOWN_CURVE;
		grp_Info[i].WithISTDPcurve = UNKNOWN_CURVE;
		grp_Info[i].FixedInputWts = true; // Default is true. This value changed to false
		// if any incoming  connections are plastic
		grp_Info[i].isSpikeGenerator = false;
		grp_Info[i].RatePtr = NULL;

		grp_Info[i].homeoId = -1;
		grp_Info[i].avgTimeScale  = 10000.0;

		grp_Info[i].baseDP = 1.0f;
		grp_Info[i].base5HT = 1.0f;
		grp_Info[i].baseACh = 1.0f;
		grp_Info[i].baseNE = 1.0f;
		grp_Info[i].decayDP = 1 - (1.0f / 100);
		grp_Info[i].decay5HT = 1 - (1.0f / 100);
		grp_Info[i].decayACh = 1 - (1.0f / 100);
		grp_Info[i].decayNE = 1 - (1.0f / 100);

		grp_Info[i].spikeGen = NULL;

		grp_Info[i].numCompNeighbors = 0;
		memset(&grp_Info[i].compNeighbors, 0, sizeof(grp_Info[i].compNeighbors[0])*MAX_NUM_COMP_CONN);
		memset(&grp_Info[i].compCoupling, 0, sizeof(grp_Info[i].compCoupling[0])*MAX_NUM_COMP_CONN);

		grp_Info[i].withSpikeCounter = false;
		grp_Info[i].spkCntRecordDur = -1;
		grp_Info[i].spkCntRecordDurHelper = 0;
		grp_Info[i].spkCntBufPos = -1;

		grp_Info[i].StartN       = -1;
		grp_Info[i].EndN       	 = -1;

		grp_Info[i].CurrTimeSlice = 0;
		grp_Info[i].NewTimeSlice = 0;
		grp_Info[i].SliceUpdateTime = 0;

		grp_Info2[i].numPostConn = 0;
		grp_Info2[i].numPreConn  = 0;
		grp_Info2[i].maxPostConn = 0;
		grp_Info2[i].maxPreConn  = 0;
		grp_Info2[i].sumPostConn = 0;
		grp_Info2[i].sumPreConn  = 0;


	}

#ifndef __CPU_ONLY__
	CUDA_CREATE_TIMER(timer);
	CUDA_RESET_TIMER(timer);
#endif

	// default weight update parameter
	wtANDwtChangeUpdateInterval_ = 1000; // update weights every 1000 ms (default)
	wtANDwtChangeUpdateIntervalCnt_ = 0; // helper var to implement fast modulo
	stdpScaleFactor_ = 1.0f;
	wtChangeDecay_ = 0.0f;

#ifndef __CPU_ONLY__
	if (simMode_ == GPU_MODE) {
		configGPUDevice();
	}
#endif
}

//! update (initialize) numN, numPostSynapses, numPreSynapses, maxDelay_, postSynCnt, preSynCnt
//! allocate space for voltage, recovery, Izh_a, Izh_b, Izh_c, Izh_d, current, gAMPA, gNMDA, gGABAa, gGABAb
//! lastSpikeTime, nSpikeCnt, intrinsicWeight, stpu, stpx, Npre, Npre_plastic, Npost, cumulativePost, cumulativePre
//! postSynapticIds, tmp_SynapticDely, postDelayInfo, wt, maxSynWt, preSynapticIds, timeTableD2, timeTableD1
void CpuSNN::buildNetworkInit() {
	// \FIXME: need to figure out STP buffer for delays > 1
	if (sim_with_stp && maxDelay_>1) {
		KERNEL_ERROR("STP with delays > 1 ms is currently not supported.");
		exitSimulation(1);
	}

	voltage	   = new float[numNReg];
	nextVoltage = new float[numNReg]; // voltage buffer for previous time step
	recovery   = new float[numNReg];
	Izh_C = new float[numNReg];
	Izh_k = new float[numNReg];
	Izh_vr = new float[numNReg];
	Izh_vt = new float[numNReg];
	Izh_a = new float[numNReg];
	Izh_b = new float[numNReg];
	Izh_vpeak = new float[numNReg];
	Izh_c = new float[numNReg];
	Izh_d = new float[numNReg];
	current	   = new float[numNReg];
	extCurrent = new float[numNReg];
	memset(extCurrent, 0, sizeof(extCurrent[0])*numNReg);

	// keeps track of all neurons that spiked at current time step
	curSpike = new bool[numNReg];
	memset(curSpike, 0, sizeof(curSpike[0])*numNReg);

	cpuSnnSz.neuronInfoSize += (sizeof(float)*numNReg*8);

	if (sim_with_conductances) {
		gAMPA  = new float[numNReg];
		gGABAa = new float[numNReg];
		cpuSnnSz.neuronInfoSize += sizeof(float)*numNReg*2;

		if (sim_with_NMDA_rise) {
			// If NMDA rise time is enabled, we'll have to compute NMDA conductance in two steps (using an exponential
			// for the rise time and one for the decay time)
			gNMDA_r = new float[numNReg];
			gNMDA_d = new float[numNReg];
			cpuSnnSz.neuronInfoSize += sizeof(float)*numNReg*2;
		} else {
			gNMDA = new float[numNReg];
			cpuSnnSz.neuronInfoSize += sizeof(float)*numNReg;
		}

		if (sim_with_GABAb_rise) {
			gGABAb_r = new float[numNReg];
			gGABAb_d = new float[numNReg];
			cpuSnnSz.neuronInfoSize += sizeof(float)*numNReg*2;
		} else {
			gGABAb = new float[numNReg];
			cpuSnnSz.neuronInfoSize += sizeof(float)*numNReg;
		}
	}

	grpDA = new float[numGrp];
	grp5HT = new float[numGrp];
	grpACh = new float[numGrp];
	grpNE = new float[numGrp];

	// init neuromodulators and their assistive buffers
	for (int i = 0; i < numGrp; i++) {
		grpDABuffer[i] = new float[1000]; // 1 second DA buffer
		grp5HTBuffer[i] = new float[1000];
		grpAChBuffer[i] = new float[1000];
		grpNEBuffer[i] = new float[1000];
	}

	resetCurrent();
	resetConductances();

	lastSpikeTime	= new uint32_t[numN];
	cpuSnnSz.neuronInfoSize += sizeof(int) * numN;
	memset(lastSpikeTime, 0, sizeof(lastSpikeTime[0]) * numN);

	nSpikeCnt  = new int[numN];
	KERNEL_INFO("allocated nSpikeCnt");

	//! homeostasis variables
	if (sim_with_homeostasis) {
		avgFiring  = new float[numN];
		baseFiring = new float[numN];
	}

	#ifdef NEURON_NOISE
	intrinsicWeight  = new float[numN];
	memset(intrinsicWeight,0,sizeof(float)*numN);
	cpuSnnSz.neuronInfoSize += (sizeof(int)*numN*2+sizeof(bool)*numN);
	#endif

	// STP can be applied to spike generators, too -> numN
	if (sim_with_stp) {
		// \TODO: The size of these data structures could be reduced to the max synaptic delay of all
		// connections with STP. That number might not be the same as maxDelay_.
		stpu = new float[numN*(maxDelay_+1)];
		stpx = new float[numN*(maxDelay_+1)];
		memset(stpu, 0, sizeof(float)*numN*(maxDelay_+1)); // memset works for 0.0
		for (int i=0; i < numN*(maxDelay_+1); i++)
			stpx[i] = 1.0f; // but memset doesn't work for 1.0
		cpuSnnSz.synapticInfoSize += (2*sizeof(float)*numN*(maxDelay_+1));
	}

	Npre 		   = new unsigned short[numN];
	Npre_plastic   = new unsigned short[numN];
	Npost 		   = new unsigned short[numN];
	cumulativePost = new unsigned int[numN];
	cumulativePre  = new unsigned int[numN];
	cpuSnnSz.networkInfoSize += (int)(sizeof(int) * numN * 3.5);

	postSynCnt = 0;
	preSynCnt  = 0;
	for(int g=0; g<numGrp; g++) {
		// check for INT overflow: postSynCnt is O(numNeurons*numSynapses), must be able to fit within u int limit
		assert(postSynCnt < UINT_MAX - (grp_Info[g].SizeN * grp_Info[g].numPostSynapses));
		assert(preSynCnt < UINT_MAX - (grp_Info[g].SizeN * grp_Info[g].numPreSynapses));
		postSynCnt += (grp_Info[g].SizeN * grp_Info[g].numPostSynapses);
		preSynCnt  += (grp_Info[g].SizeN * grp_Info[g].numPreSynapses);
	}
	assert(postSynCnt/numN <= (unsigned int)numPostSynapses_); // divide by numN to prevent INT overflow
	postSynapticIds		= new post_info_t[postSynCnt+100];
	tmp_SynapticDelay	= new uint8_t[postSynCnt+100];	//!< Temporary array to store the delays of each connection
	postDelayInfo		= new delay_info_t[numN*(maxDelay_+1)];	//!< Possible delay values are 0....maxDelay_ (inclusive of maxDelay_)
	cpuSnnSz.networkInfoSize += ((sizeof(post_info_t)+sizeof(uint8_t))*postSynCnt+100)+(sizeof(delay_info_t)*numN*(maxDelay_+1));
	assert(preSynCnt/numN <= (unsigned int)numPreSynapses_); // divide by numN to prevent INT overflow

	wt  			= new float[preSynCnt+100];
	maxSynWt     	= new float[preSynCnt+100];

	mulSynFast 		= new float[MAX_nConnections];
	mulSynSlow 		= new float[MAX_nConnections];
	cumConnIdPre	= new short int[preSynCnt+100];

	//! Temporary array to hold pre-syn connections. will be deleted later if necessary
	preSynapticIds	= new post_info_t[preSynCnt + 100];
	// size due to weights and maximum weights
	cpuSnnSz.synapticInfoSize += ((sizeof(int) + 2 * sizeof(float) + sizeof(post_info_t)) * (preSynCnt + 100));

	timeTableD2  = new unsigned int[1000 + maxDelay_ + 1];
	timeTableD1  = new unsigned int[1000 + maxDelay_ + 1];
	resetTimingTable();
	cpuSnnSz.spikingInfoSize += sizeof(int) * 2 * (1000 + maxDelay_ + 1);

	// poisson Firing Rate
	cpuSnnSz.neuronInfoSize += (sizeof(int) * numNPois);
}


int CpuSNN::addSpikeToTable(int nid, int g) {
	int spikeBufferFull = 0;
	lastSpikeTime[nid] = simTime;
	nSpikeCnt[nid]++;
	if (sim_with_homeostasis)
		avgFiring[nid] += 1000/(grp_Info[g].avgTimeScale*1000);

#ifndef __CPU_ONLY__
	if (simMode_ == GPU_MODE) {
		assert(grp_Info[g].isSpikeGenerator == true);
		setSpikeGenBit_GPU(nid, g);
		return 0;
	}
#endif

	if (grp_Info[g].WithSTP) {
		// update the spike-dependent part of du/dt and dx/dt
		// we need to retrieve the STP values from the right buffer position (right before vs. right after the spike)
		int ind_plus = STP_BUF_POS(nid,simTime); // index of right after the spike, such as in u^+
	    int ind_minus = STP_BUF_POS(nid,(simTime-1)); // index of right before the spike, such as in u^-

		// du/dt = -u/tau_F + U * (1-u^-) * \delta(t-t_{spk})
		stpu[ind_plus] += grp_Info[g].STP_U*(1.0-stpu[ind_minus]);

		// dx/dt = (1-x)/tau_D - u^+ * x^- * \delta(t-t_{spk})
		stpx[ind_plus] -= stpu[ind_plus]*stpx[ind_minus];
	}

	if (grp_Info[g].MaxDelay == 1) {
		assert(nid < numN);
		firingTableD1[secD1fireCntHost] = nid;
		secD1fireCntHost++;
		grp_Info[g].FiringCount1sec++;
		if (secD1fireCntHost >= maxSpikesD1) {
			spikeBufferFull = 2;
			secD1fireCntHost = maxSpikesD1-1;
		}
	} else {
		assert(nid < numN);
		firingTableD2[secD2fireCntHost] = nid;
		grp_Info[g].FiringCount1sec++;
		secD2fireCntHost++;
		if (secD2fireCntHost >= maxSpikesD2) {
			spikeBufferFull = 1;
			secD2fireCntHost = maxSpikesD2-1;
		}
	}
	return spikeBufferFull;
}


void CpuSNN::buildGroup(int grpId) {
	assert(grp_Info[grpId].StartN == -1);
	grp_Info[grpId].StartN = allocatedN;
	grp_Info[grpId].EndN   = allocatedN + grp_Info[grpId].SizeN - 1;

	KERNEL_DEBUG("Allocation for %d(%s), St=%d, End=%d",
				grpId, grp_Info2[grpId].Name.c_str(), grp_Info[grpId].StartN, grp_Info[grpId].EndN);

	resetNeuromodulator(grpId);

	allocatedN = allocatedN + grp_Info[grpId].SizeN;
	assert(allocatedN <= (unsigned int)numN);
	assert(allocatedComp <= allocatedN);

	for(int i=grp_Info[grpId].StartN; i <= grp_Info[grpId].EndN; i++) {
		resetNeuron(i, grpId);
		Npre_plastic[i]	= 0;
		Npre[i]		  	= 0;
		Npost[i]	  	= 0;
		cumulativePost[i] = allocatedPost;
		cumulativePre[i]  = allocatedPre;
		allocatedPost    += grp_Info[grpId].numPostSynapses;
		allocatedPre     += grp_Info[grpId].numPreSynapses;
	}

	assert(allocatedPost <= postSynCnt);
	assert(allocatedPre  <= preSynCnt);
}

//! build the network based on the current setting (e.g., group, connection)
/*!
 * \sa createGroup(), connect()
 */
void CpuSNN::buildNetwork() {
	// find the maximum values for number of pre- and post-synaptic neurons
	findMaxNumSynapses(&numPostSynapses_, &numPreSynapses_);

	// update (initialize) maxSpikesD1, maxSpikesD2 and allocate space for firingTableD1 and firingTableD2
	maxDelay_ = updateSpikeTables();

	// make sure number of neurons and max delay are within bounds
	assert(maxDelay_ <= MAX_SynapticDelay); 
	assert(numN <= 1000000);
	assert((numN > 0) && (numN == numNExcReg + numNInhReg + numNPois));

	// display the evaluated network and delay length....
	KERNEL_INFO("\n");
	KERNEL_INFO("***************************** Setting up Network **********************************");
	KERNEL_INFO("numN = %d, numPostSynapses = %d, numPreSynapses = %d, maxDelay = %d", numN, numPostSynapses_,
					numPreSynapses_, maxDelay_);

	if (numPostSynapses_ > MAX_nPostSynapses) {
		for (int g=0;g<numGrp;g++) {
			if (grp_Info[g].numPostSynapses>MAX_nPostSynapses)
				KERNEL_ERROR("Grp: %s(%d) has too many output synapses (%d), max %d.",grp_Info2[g].Name.c_str(),g,
							grp_Info[g].numPostSynapses,MAX_nPostSynapses);
		}
		assert(numPostSynapses_ <= MAX_nPostSynapses);
	}
	if (numPreSynapses_ > MAX_nPreSynapses) {
		for (int g=0;g<numGrp;g++) {
			if (grp_Info[g].numPreSynapses>MAX_nPreSynapses)
				KERNEL_ERROR("Grp: %s(%d) has too many input synapses (%d), max %d.",grp_Info2[g].Name.c_str(),g,
 							grp_Info[g].numPreSynapses,MAX_nPreSynapses);
		}
		assert(numPreSynapses_ <= MAX_nPreSynapses);
	}

	// initialize all the parameters....
	//! update (initialize) numN, numPostSynapses, numPreSynapses, maxDelay_, postSynCnt, preSynCnt
	//! allocate space for voltage, recovery, Izh_a, Izh_b, Izh_c, Izh_d, current, gAMPA, gNMDA, gGABAa, gGABAb
	//! lastSpikeTime, nSpikeCnt, intrinsicWeight, stpu, stpx, Npre, Npre_plastic, Npost, cumulativePost, cumulativePre
	//! postSynapticIds, tmp_SynapticDely, postDelayInfo, wt, maxSynWt, preSynapticIds, timeTableD2, timeTableD1, grpDA, grp5HT, grpACh, grpNE
	buildNetworkInit();

	// we build network in the order...
	/////    !!!!!!! IMPORTANT : NEURON ORGANIZATION/ARRANGEMENT MAP !!!!!!!!!!
	////     <--- Excitatory --> | <-------- Inhibitory REGION ----------> | <-- Excitatory -->
	///      Excitatory-Regular  | Inhibitory-Regular | Inhibitory-Poisson | Excitatory-Poisson
	int allocatedGrp = 0;
	for(int order = 0; order < 4; order++) {
		for(int g = 0; g < numGrp; g++) {
			if (IS_EXCITATORY_TYPE(grp_Info[g].Type) && (grp_Info[g].Type & POISSON_NEURON) && order == 3) {
				buildPoissonGroup(g);
				allocatedGrp++;
			} else if (IS_INHIBITORY_TYPE(grp_Info[g].Type) &&  (grp_Info[g].Type & POISSON_NEURON) && order == 2) {
				buildPoissonGroup(g);
				allocatedGrp++;
			} else if (IS_EXCITATORY_TYPE(grp_Info[g].Type) && !(grp_Info[g].Type & POISSON_NEURON) && order == 0) {
				buildGroup(g);
				allocatedGrp++;
			} else if (IS_INHIBITORY_TYPE(grp_Info[g].Type) && !(grp_Info[g].Type & POISSON_NEURON) && order == 1) {
				buildGroup(g);
				allocatedGrp++;
			}
		}
	}
	assert(allocatedGrp == numGrp);

	// print group overview
	for (int g=0;g<numGrp;g++) {
		printGroupInfo(g);
	}


	grpIds = new short int[numN];
	for (int nid=0; nid<numN; nid++) {
		grpIds[nid] = -1;
		for (int g=0; g<numGrp; g++) {
			if (nid>=grp_Info[g].StartN && nid<=grp_Info[g].EndN) {
				grpIds[nid] = (short int)g;
//				printf("grpIds[%d] = %d\n",nid,g);
				break;
			}
		}
		assert(grpIds[nid]!=-1);
	}


	grpConnectInfo_t* newInfo = connectBegin;
	compConnectInfo_t* newInfo2 = compConnectBegin;

	if (loadSimFID != NULL) {
		int loadError;
		// we the user specified loadSimulation the synaptic weights will be restored here...
		KERNEL_DEBUG("Start to load simulation");
		loadError = loadSimulation_internal(true); // read the plastic synapses first
		KERNEL_DEBUG("loadSimulation_internal() error number:%d", loadError);
		loadError = loadSimulation_internal(false); // read the fixed synapses second
		KERNEL_DEBUG("loadSimulation_internal() error number:%d", loadError);
		for(int con = 0; con < 2; con++) {
			newInfo = connectBegin;
			while(newInfo) {
				bool synWtType = GET_FIXED_PLASTIC(newInfo->connProp);
				if (synWtType == SYN_PLASTIC) {
					// given group has plastic connection, and we need to apply STDP rule...
					grp_Info[newInfo->grpDest].FixedInputWts = false;
				}

				// store scaling factors for synaptic currents in connection-centric array
				mulSynFast[newInfo->connId] = newInfo->mulSynFast;
				mulSynSlow[newInfo->connId] = newInfo->mulSynSlow;

				if( ((con == 0) && (synWtType == SYN_PLASTIC)) || ((con == 1) && (synWtType == SYN_FIXED))) {
					printConnectionInfo(newInfo->connId);
				}
				newInfo = newInfo->next;
			}
		}
	} else {

		// build all the compartmental connections first
		while (newInfo2) {
			int grpLower = newInfo2->grpSrc;
			int grpUpper = newInfo2->grpDest;

			int i = grp_Info[grpLower].numCompNeighbors;
			if (i >= MAX_NUM_COMP_CONN) {
				KERNEL_ERROR("Group %s(%d) exceeds max number of allowed compartmental connections (%d).",
					grp_Info2[grpLower].Name.c_str(), grpLower, (int)MAX_NUM_COMP_CONN);
				exitSimulation(1);
			}
			grp_Info[grpLower].compNeighbors[i] = grpUpper;
			grp_Info[grpLower].compCoupling[i] = grp_Info[grpUpper].compCouplingDown; // get down-coupling from upper neighbor
			grp_Info[grpLower].numCompNeighbors++;

			int j = grp_Info[grpUpper].numCompNeighbors;
			if (j >= MAX_NUM_COMP_CONN) {
				KERNEL_ERROR("Group %s(%d) exceeds max number of allowed compartmental connections (%d).",
					grp_Info2[grpUpper].Name.c_str(), grpUpper, (int)MAX_NUM_COMP_CONN);
				exitSimulation(1);
			}
			grp_Info[grpUpper].compNeighbors[j] = grpLower;
			grp_Info[grpUpper].compCoupling[j] = grp_Info[grpLower].compCouplingUp; // get up-coupling from lower neighbor
			grp_Info[grpUpper].numCompNeighbors++;
			
			newInfo2 = newInfo2->next;
		}

		// build all the connections here...
		// we run over the linked list two times...
		// first time, we make all plastic connections...
		// second time, we make all fixed connections...
		// this ensures that all the initial pre and post-synaptic
		// connections are of fixed type and later if of plastic type
		for(int con = 0; con < 2; con++) {
			newInfo = connectBegin;
			while(newInfo) {
				bool synWtType = GET_FIXED_PLASTIC(newInfo->connProp);
				if (synWtType == SYN_PLASTIC) {
					// given group has plastic connection, and we need to apply STDP rule...
					grp_Info[newInfo->grpDest].FixedInputWts = false;
				}

				// store scaling factors for synaptic currents in connection-centric array
				mulSynFast[newInfo->connId] = newInfo->mulSynFast;
				mulSynSlow[newInfo->connId] = newInfo->mulSynSlow;


				if( ((con == 0) && (synWtType == SYN_PLASTIC)) || ((con == 1) && (synWtType == SYN_FIXED))) {
					switch(newInfo->type) {
						case CONN_RANDOM:
							connectRandom(newInfo);
							break;
						case CONN_FULL:
							connectFull(newInfo);
							break;
						case CONN_FULL_NO_DIRECT:
							connectFull(newInfo);
							break;
						case CONN_ONE_TO_ONE:
							connectOneToOne(newInfo);
							break;
						case CONN_GAUSSIAN:
							connectGaussian(newInfo);
							break;
						case CONN_USER_DEFINED:
							connectUserDefined(newInfo);
							break;
						default:
							KERNEL_ERROR("Invalid connection type( should be 'random', 'full', 'full-no-direct', or 'one-to-one')");
							exitSimulation(-1);
					}

					printConnectionInfo(newInfo->connId);
				}
				newInfo = newInfo->next;
			}
		}
	}
}

void CpuSNN::buildPoissonGroup(int grpId) {
	assert(grp_Info[grpId].StartN == -1);
	grp_Info[grpId].StartN 	= allocatedN;
	grp_Info[grpId].EndN   	= allocatedN + grp_Info[grpId].SizeN - 1;

	KERNEL_DEBUG("Allocation for %d(%s), St=%d, End=%d",
				grpId, grp_Info2[grpId].Name.c_str(), grp_Info[grpId].StartN, grp_Info[grpId].EndN);

	allocatedN = allocatedN + grp_Info[grpId].SizeN;
	assert(allocatedN <= (unsigned int)numN);

	for(int i=grp_Info[grpId].StartN; i <= grp_Info[grpId].EndN; i++) {
		resetPoissonNeuron(i, grpId);
		Npre_plastic[i]	  = 0;
		Npre[i]		  	  = 0;
		Npost[i]	      = 0;
		cumulativePost[i] = allocatedPost;
		cumulativePre[i]  = allocatedPre;
		allocatedPost    += grp_Info[grpId].numPostSynapses;
		allocatedPre     += grp_Info[grpId].numPreSynapses;
	}
	assert(allocatedPost <= postSynCnt);
	assert(allocatedPre  <= preSynCnt);
}

/*!
 * \brief check whether Spike Counters need to be reset
 *
 * A Spike Counter keeps track of all spikes per neuron for a certain time period (recordDur)
 * After this period of time, the spike buffers need to be reset. The trick is to reset it in the very next
 * millisecond, before continuing. For example, if recordDur=1000ms, we want to reset it right before we start
 * executing the 1001st millisecond, so that at t=1000ms the user is able to access non-zero data.
 */
void CpuSNN::checkSpikeCounterRecordDur() {
	for (int g=0; g<numGrp; g++) {
		// skip groups w/o spkMonRT or non-real record durations
		if (!grp_Info[g].withSpikeCounter || grp_Info[g].spkCntRecordDur<=0)
			continue;

		// skip if simTime doesn't need udpating
		// we want to update in spkCntRecordDur + 1, because this function is called rigth at the beginning
		// of each millisecond
		if ( (simTime % ++grp_Info[g].spkCntRecordDurHelper) != 1)
			continue;

 		if (simMode_==CPU_MODE) {
			resetSpikeCounter(g);
#ifndef __CPU_ONLY__
		} else {
			resetSpikeCounter_GPU(g);
#endif
		}
	}
}

// We parallelly cleanup the postSynapticIds array to minimize any other wastage in that array by compacting the store
// Appropriate alignment specified by ALIGN_COMPACTION macro is used to ensure some level of alignment (if necessary)
void CpuSNN::compactConnections() {
	unsigned int* tmp_cumulativePost = new unsigned int[numN];
	unsigned int* tmp_cumulativePre  = new unsigned int[numN];
	unsigned int lastCnt_pre         = 0;
	unsigned int lastCnt_post        = 0;

	tmp_cumulativePost[0]   = 0;
	tmp_cumulativePre[0]    = 0;

	for(int i=1; i < numN; i++) {
		lastCnt_post = tmp_cumulativePost[i-1]+Npost[i-1]; //position of last pointer
		lastCnt_pre  = tmp_cumulativePre[i-1]+Npre[i-1]; //position of last pointer
		#if COMPACTION_ALIGNMENT_POST
			lastCnt_post= lastCnt_post + COMPACTION_ALIGNMENT_POST-lastCnt_post%COMPACTION_ALIGNMENT_POST;
			lastCnt_pre = lastCnt_pre  + COMPACTION_ALIGNMENT_PRE- lastCnt_pre%COMPACTION_ALIGNMENT_PRE;
		#endif
		tmp_cumulativePost[i] = lastCnt_post;
		tmp_cumulativePre[i]  = lastCnt_pre;
		assert(tmp_cumulativePost[i] <= cumulativePost[i]);
		assert(tmp_cumulativePre[i]  <= cumulativePre[i]);
	}

	// compress the post_synaptic array according to the new values of the tmp_cumulative counts....
	unsigned int tmp_postSynCnt = tmp_cumulativePost[numN-1]+Npost[numN-1];
	unsigned int tmp_preSynCnt  = tmp_cumulativePre[numN-1]+Npre[numN-1];
	assert(tmp_postSynCnt <= allocatedPost);
	assert(tmp_preSynCnt  <= allocatedPre);
	assert(tmp_postSynCnt <= postSynCnt);
	assert(tmp_preSynCnt  <= preSynCnt);
	KERNEL_DEBUG("******************");
	KERNEL_DEBUG("CompactConnection: ");
	KERNEL_DEBUG("******************");
	KERNEL_DEBUG("old_postCnt = %d, new_postCnt = %d", postSynCnt, tmp_postSynCnt);
	KERNEL_DEBUG("old_preCnt = %d,  new_postCnt = %d", preSynCnt,  tmp_preSynCnt);

	// new buffer with required size + 100 bytes of additional space just to provide limited overflow
	post_info_t* tmp_postSynapticIds   = new post_info_t[tmp_postSynCnt+100];

	// new buffer with required size + 100 bytes of additional space just to provide limited overflow
	post_info_t* tmp_preSynapticIds	= new post_info_t[tmp_preSynCnt+100];
	float* tmp_wt	    	  		= new float[tmp_preSynCnt+100];
	float* tmp_maxSynWt   	  		= new float[tmp_preSynCnt+100];
	short int *tmp_cumConnIdPre 		= new short int[tmp_preSynCnt+100];
	float *tmp_mulSynFast 			= new float[numConnections];
	float *tmp_mulSynSlow  			= new float[numConnections];

	// compact synaptic information
	for(int i=0; i<numN; i++) {
		assert(tmp_cumulativePost[i] <= cumulativePost[i]);
		assert(tmp_cumulativePre[i]  <= cumulativePre[i]);
		for( int j=0; j<Npost[i]; j++) {
			unsigned int tmpPos = tmp_cumulativePost[i]+j;
			unsigned int oldPos = cumulativePost[i]+j;
			tmp_postSynapticIds[tmpPos] = postSynapticIds[oldPos];
			tmp_SynapticDelay[tmpPos]   = tmp_SynapticDelay[oldPos];
		}
		for( int j=0; j<Npre[i]; j++) {
			unsigned int tmpPos =  tmp_cumulativePre[i]+j;
			unsigned int oldPos =  cumulativePre[i]+j;
			tmp_preSynapticIds[tmpPos]  = preSynapticIds[oldPos];
			tmp_maxSynWt[tmpPos] 	    = maxSynWt[oldPos];
			tmp_wt[tmpPos]              = wt[oldPos];
			tmp_cumConnIdPre[tmpPos]	= cumConnIdPre[oldPos];
		}
	}

	// delete old buffer space
	delete[] postSynapticIds;
	postSynapticIds = tmp_postSynapticIds;
	cpuSnnSz.networkInfoSize -= (sizeof(post_info_t)*postSynCnt);
	cpuSnnSz.networkInfoSize += (sizeof(post_info_t)*(tmp_postSynCnt+100));

	delete[] cumulativePost;
	cumulativePost  = tmp_cumulativePost;

	delete[] cumulativePre;
	cumulativePre   = tmp_cumulativePre;

	delete[] maxSynWt;
	maxSynWt = tmp_maxSynWt;
	cpuSnnSz.synapticInfoSize -= (sizeof(float)*preSynCnt);
	cpuSnnSz.synapticInfoSize += (sizeof(float)*(tmp_preSynCnt+100));

	delete[] wt;
	wt = tmp_wt;
	cpuSnnSz.synapticInfoSize -= (sizeof(float)*preSynCnt);
	cpuSnnSz.synapticInfoSize += (sizeof(float)*(tmp_preSynCnt+100));

	delete[] cumConnIdPre;
	cumConnIdPre = tmp_cumConnIdPre;
	cpuSnnSz.synapticInfoSize -= (sizeof(short int)*preSynCnt);
	cpuSnnSz.synapticInfoSize += (sizeof(short int)*(tmp_preSynCnt+100));

	// compact connection-centric information
	for (int i=0; i<numConnections; i++) {
		tmp_mulSynFast[i] = mulSynFast[i];
		tmp_mulSynSlow[i] = mulSynSlow[i];
	}
	delete[] mulSynFast;
	delete[] mulSynSlow;
	mulSynFast = tmp_mulSynFast;
	mulSynSlow = tmp_mulSynSlow;
	cpuSnnSz.networkInfoSize -= (2*sizeof(uint8_t)*preSynCnt);
	cpuSnnSz.networkInfoSize += (2*sizeof(uint8_t)*(tmp_preSynCnt+100));


	delete[] preSynapticIds;
	preSynapticIds  = tmp_preSynapticIds;
	cpuSnnSz.synapticInfoSize -= (sizeof(post_info_t)*preSynCnt);
	cpuSnnSz.synapticInfoSize += (sizeof(post_info_t)*(tmp_preSynCnt+100));

	preSynCnt	= tmp_preSynCnt;
	postSynCnt	= tmp_postSynCnt;
}

// make 'C' full connections from grpSrc to grpDest
void CpuSNN::connectFull(grpConnectInfo_t* info) {
	int grpSrc = info->grpSrc;
	int grpDest = info->grpDest;
	bool noDirect = (info->type == CONN_FULL_NO_DIRECT);

	// rebuild struct for easier handling
	RadiusRF radius(info->radX, info->radY, info->radZ);

	for(int i = grp_Info[grpSrc].StartN; i <= grp_Info[grpSrc].EndN; i++)  {
		Point3D loc_i = getNeuronLocation3D(i); // 3D coordinates of i
		for(int j = grp_Info[grpDest].StartN; j <= grp_Info[grpDest].EndN; j++) { // j: the temp neuron id
			// if flag is set, don't connect direct connections
			if((noDirect) && (i - grp_Info[grpSrc].StartN) == (j - grp_Info[grpDest].StartN))
				continue;

			// check whether pre-neuron location is in RF of post-neuron
			Point3D loc_j = getNeuronLocation3D(j); // 3D coordinates of j
			if (!isPoint3DinRF(radius, loc_i, loc_j))
				continue;

			//uint8_t dVal = info->minDelay + (int)(0.5 + (drand48() * (info->maxDelay - info->minDelay)));
			uint8_t dVal = info->minDelay + rand() % (info->maxDelay - info->minDelay + 1);
			assert((dVal >= info->minDelay) && (dVal <= info->maxDelay));
			float synWt = getWeights(info->connProp, info->initWt, info->maxWt, i, grpSrc);

			setConnection(grpSrc, grpDest, i, j, synWt, info->maxWt, dVal, info->connProp, info->connId);
			info->numberOfConnections++;
		}
	}

	grp_Info2[grpSrc].sumPostConn += info->numberOfConnections;
	grp_Info2[grpDest].sumPreConn += info->numberOfConnections;
}

void CpuSNN::connectGaussian(grpConnectInfo_t* info) {
	// rebuild struct for easier handling
	// adjust with sqrt(2) in order to make the Gaussian kernel depend on 2*sigma^2
	RadiusRF radius(info->radX, info->radY, info->radZ);

	// in case pre and post have different Grid3D sizes: scale pre to the grid size of post
	int grpSrc = info->grpSrc;
	int grpDest = info->grpDest;
	Grid3D grid_i = getGroupGrid3D(grpSrc);
	Grid3D grid_j = getGroupGrid3D(grpDest);
	Point3D scalePre = Point3D(grid_j.x, grid_j.y, grid_j.z) / Point3D(grid_i.x, grid_i.y, grid_i.z);

	for(int i = grp_Info[grpSrc].StartN; i <= grp_Info[grpSrc].EndN; i++)  {
		Point3D loc_i = getNeuronLocation3D(i)*scalePre; // i: adjusted 3D coordinates

		for(int j = grp_Info[grpDest].StartN; j <= grp_Info[grpDest].EndN; j++) { // j: the temp neuron id
			// check whether pre-neuron location is in RF of post-neuron
			Point3D loc_j = getNeuronLocation3D(j); // 3D coordinates of j

			// make sure point is in RF
			double rfDist = getRFDist3D(radius,loc_i,loc_j);
			if (rfDist < 0.0 || rfDist > 1.0)
				continue;

			// if rfDist is valid, it returns a number between 0 and 1
			// we want these numbers to fit to Gaussian weigths, so that rfDist=0 corresponds to max Gaussian weight
			// and rfDist=1 corresponds to 0.1 times max Gaussian weight
			// so we're looking at gauss = exp(-a*rfDist), where a such that exp(-a)=0.1
			// solving for a, we find that a = 2.3026
			double gauss = exp(-2.3026*rfDist);
			if (gauss < 0.1)
				continue;

			if (drand48() < info->p) {
				uint8_t dVal = info->minDelay + rand() % (info->maxDelay - info->minDelay + 1);
				assert((dVal >= info->minDelay) && (dVal <= info->maxDelay));
				float synWt = gauss * info->initWt; // scale weight according to gauss distance
				setConnection(grpSrc, grpDest, i, j, synWt, info->maxWt, dVal, info->connProp, info->connId);
				info->numberOfConnections++;
			}
		}
	}

	grp_Info2[grpSrc].sumPostConn += info->numberOfConnections;
	grp_Info2[grpDest].sumPreConn += info->numberOfConnections;
}

void CpuSNN::connectOneToOne (grpConnectInfo_t* info) {
	int grpSrc = info->grpSrc;
	int grpDest = info->grpDest;
	assert( grp_Info[grpDest].SizeN == grp_Info[grpSrc].SizeN );

	// NOTE: RadiusRF does not make a difference here: ignore
	for(int nid=grp_Info[grpSrc].StartN,j=grp_Info[grpDest].StartN; nid<=grp_Info[grpSrc].EndN; nid++, j++)  {
		uint8_t dVal = info->minDelay + rand() % (info->maxDelay - info->minDelay + 1);
		assert((dVal >= info->minDelay) && (dVal <= info->maxDelay));
		float synWt = getWeights(info->connProp, info->initWt, info->maxWt, nid, grpSrc);
		setConnection(grpSrc, grpDest, nid, j, synWt, info->maxWt, dVal, info->connProp, info->connId);
		info->numberOfConnections++;
	}

	grp_Info2[grpSrc].sumPostConn += info->numberOfConnections;
	grp_Info2[grpDest].sumPreConn += info->numberOfConnections;
}

// make 'C' random connections from grpSrc to grpDest
void CpuSNN::connectRandom (grpConnectInfo_t* info) {
	int grpSrc = info->grpSrc;
	int grpDest = info->grpDest;

	// rebuild struct for easier handling
	RadiusRF radius(info->radX, info->radY, info->radZ);

	for(int pre_nid=grp_Info[grpSrc].StartN; pre_nid<=grp_Info[grpSrc].EndN; pre_nid++) {
		Point3D loc_pre = getNeuronLocation3D(pre_nid); // 3D coordinates of i
		for(int post_nid=grp_Info[grpDest].StartN; post_nid<=grp_Info[grpDest].EndN; post_nid++) {
			// check whether pre-neuron location is in RF of post-neuron
			Point3D loc_post = getNeuronLocation3D(post_nid); // 3D coordinates of j
			if (!isPoint3DinRF(radius, loc_pre, loc_post))
				continue;

			if (drand48() < info->p) {
				//uint8_t dVal = info->minDelay + (int)(0.5+(drand48()*(info->maxDelay-info->minDelay)));
				uint8_t dVal = info->minDelay + rand() % (info->maxDelay - info->minDelay + 1);
				assert((dVal >= info->minDelay) && (dVal <= info->maxDelay));
				float synWt = getWeights(info->connProp, info->initWt, info->maxWt, pre_nid, grpSrc);
				setConnection(grpSrc, grpDest, pre_nid, post_nid, synWt, info->maxWt, dVal, info->connProp, info->connId);
				info->numberOfConnections++;
			}
		}
	}

	grp_Info2[grpSrc].sumPostConn += info->numberOfConnections;
	grp_Info2[grpDest].sumPreConn += info->numberOfConnections;
}

// user-defined functions called here...
// This is where we define our user-defined call-back function.  -- KDC
void CpuSNN::connectUserDefined (grpConnectInfo_t* info) {
	int grpSrc = info->grpSrc;
	int grpDest = info->grpDest;
	info->maxDelay = 0;
	for(int nid=grp_Info[grpSrc].StartN; nid<=grp_Info[grpSrc].EndN; nid++) {
		for(int nid2=grp_Info[grpDest].StartN; nid2 <= grp_Info[grpDest].EndN; nid2++) {
			int srcId  = nid  - grp_Info[grpSrc].StartN;
			int destId = nid2 - grp_Info[grpDest].StartN;
			float weight, maxWt, delay;
			bool connected;

			info->conn->connect(this, grpSrc, srcId, grpDest, destId, weight, maxWt, delay, connected);
			if(connected)  {
				if (GET_FIXED_PLASTIC(info->connProp) == SYN_FIXED)
					maxWt = weight;

				info->maxWt = maxWt;

				assert(delay >= 1);
				assert(delay <= MAX_SynapticDelay);
				assert(abs(weight) <= abs(maxWt));

				// adjust the sign of the weight based on inh/exc connection
				weight = isExcitatoryGroup(grpSrc) ? fabs(weight) : -1.0*fabs(weight);
				maxWt  = isExcitatoryGroup(grpSrc) ? fabs(maxWt)  : -1.0*fabs(maxWt);

				setConnection(grpSrc, grpDest, nid, nid2, weight, maxWt, delay, info->connProp, info->connId);
				info->numberOfConnections++;
				if(delay > info->maxDelay) {
					info->maxDelay = delay;
				}
			}
		}
	}

	grp_Info2[grpSrc].sumPostConn += info->numberOfConnections;
	grp_Info2[grpDest].sumPreConn += info->numberOfConnections;
}

void CpuSNN::printSimSummary() {
	// stop the timers and update spikeCount* class members
	float executionTimeMs = getActualExecutionTimeMs();

	KERNEL_INFO("\n");
	KERNEL_INFO("********************      %s Simulation Summary      ***************************",
		simMode_ == GPU_MODE ? "GPU" : "CPU");

	KERNEL_INFO("Network Parameters: \tnumNeurons = %d (numNExcReg:numNInhReg = %2.1f:%2.1f)", 
		numN, 100.0*numNExcReg/numN, 100.0*numNInhReg/numN);
	KERNEL_INFO("\t\t\tnumSynapses = %d", postSynCnt);
	KERNEL_INFO("\t\t\tmaxDelay = %d", maxDelay_);
	KERNEL_INFO("Simulation Mode:\t%s",sim_with_conductances?"COBA":"CUBA");
	KERNEL_INFO("Random Seed:\t\t%d", randSeed_);
	KERNEL_INFO("Timing:\t\t\tModel Simulation Time = %lld sec", (unsigned long long)simTimeSec);
	KERNEL_INFO("\t\t\tActual Execution Time = %4.2f sec", executionTimeMs/1000.0);
	KERNEL_INFO("Average Firing Rate:\t2+ms delay = %3.3f Hz", spikeCountD2Host/(1.0*simTimeSec*numNExcReg));
	KERNEL_INFO("\t\t\t1ms delay = %3.3f Hz", spikeCountD1Host/(1.0*simTimeSec*numNInhReg));
	KERNEL_INFO("\t\t\tOverall = %3.3f Hz", spikeCountAllHost/(1.0*simTimeSec*numN));
	KERNEL_INFO("Overall Firing Count:\t2+ms delay = %d", spikeCountD2Host);
	KERNEL_INFO("\t\t\t1ms delay = %d", spikeCountD1Host);
	KERNEL_INFO("\t\t\tTotal = %d", spikeCountAllHost);
	KERNEL_INFO("*********************************************************************************\n");
}

// stop CPU/GPU timer and retrieve actual execution time for printSimSummary
float CpuSNN::getActualExecutionTimeMs() {
	float etime;
	if (simMode_ == CPU_MODE) {
		stopCPUTiming();
		etime = cpuExecutionTime;
#ifndef __CPU_ONLY__
	} else {
		etime = getActualExecutionTimeMs_GPU();
#endif
	}

	return etime;
}

// delete all objects (CPU and GPU side)
void CpuSNN::deleteObjects() {
	if (simulatorDeleted)
		return;

	printSimSummary();

	// fclose file streams, unless in custom mode
	if (loggerMode_ != CUSTOM) {
		// don't fclose if it's stdout or stderr, otherwise they're gonna stay closed for the rest of the process
		if (fpInf_!=NULL && fpInf_!=stdout && fpInf_!=stderr)
			fclose(fpInf_);
		if (fpErr_!=NULL && fpErr_!=stdout && fpErr_!=stderr)
			fclose(fpErr_);
		if (fpDeb_!=NULL && fpDeb_!=stdout && fpDeb_!=stderr)
			fclose(fpDeb_);
		if (fpLog_!=NULL && fpLog_!=stdout && fpLog_!=stderr)
			fclose(fpLog_);
	}

	resetPointers(true); // deallocate pointers

#ifndef __CPU_ONLY__
	// do the same as above, but for snn_gpu.cu
	deleteObjects_GPU();
#endif

	simulatorDeleted = true;
}



// This method loops through all spikes that are generated by neurons with a delay of 1ms
// and delivers the spikes to the appropriate post-synaptic neuron
void CpuSNN::doD1CurrentUpdate() {
	int k     = secD1fireCntHost-1;
	int k_end = timeTableD1[simTimeMs+maxDelay_];

	while((k>=k_end) && (k>=0)) {

		int neuron_id      = firingTableD1[k];
		assert(neuron_id<numN);

		delay_info_t dPar = postDelayInfo[neuron_id*(maxDelay_+1)];

		unsigned int  offset = cumulativePost[neuron_id];

		for(int idx_d = dPar.delay_index_start;
			idx_d < (dPar.delay_index_start + dPar.delay_length);
			idx_d = idx_d+1) {
				generatePostSpike( neuron_id, idx_d, offset, 0);
		}
		k=k-1;
	}
}

// This method loops through all spikes that are generated by neurons with a delay of 2+ms
// and delivers the spikes to the appropriate post-synaptic neuron
void CpuSNN::doD2CurrentUpdate() {
	int k = secD2fireCntHost-1;
	int k_end = timeTableD2[simTimeMs+1];
	int t_pos = simTimeMs;

	while((k>=k_end)&& (k >=0)) {

		// get the neuron id from the index k
		int i  = firingTableD2[k];

		// find the time of firing from the timeTable using index k
		while (!(((unsigned int)k >= timeTableD2[t_pos+maxDelay_]) 
			&& ((unsigned int)k < timeTableD2[t_pos+maxDelay_+1]))) {
			t_pos = t_pos - 1;
			assert((t_pos+maxDelay_-1)>=0);
		}

		// \TODO: Instead of using the complex timeTable, can neuronFiringTime value...???
		// Calculate the time difference between time of firing of neuron and the current time...
		int tD = simTimeMs - t_pos;

		assert((tD<maxDelay_)&&(tD>=0));
		assert(i<numN);

		delay_info_t dPar = postDelayInfo[i*(maxDelay_+1)+tD];

		unsigned int offset = cumulativePost[i];

		// for each delay variables
		for(int idx_d = dPar.delay_index_start;
			idx_d < (dPar.delay_index_start + dPar.delay_length);
			idx_d = idx_d+1) {
			generatePostSpike( i, idx_d, offset, tD);
		}

		k=k-1;
	}
}

void CpuSNN::doSnnSim() {
	// for all Spike Counters, reset their spike counts to zero if simTime % recordDur == 0
	if (sim_with_spikecounters) {
		checkSpikeCounterRecordDur();
	}

	// decay STP vars and conductances
	globalStateDecay();

	updateSpikeGenerators();

	//generate all the scheduled spikes from the spikeBuffer..
	generateSpikes();

	// find the neurons that has fired..
	findFiring();

	timeTableD2[simTimeMs+maxDelay_+1] = secD2fireCntHost;
	timeTableD1[simTimeMs+maxDelay_+1] = secD1fireCntHost;

	doD2CurrentUpdate();
	doD1CurrentUpdate();

	globalStateUpdate();

	return;
}

void CpuSNN::globalStateDecay() {
	int spikeBufferFull = 0;

	// having outer loop is grpId produces slightly more code (every flag needs its own neurId inner loop)
	// but avoids having to check the condition for every neuron in the network (= faster)

	// decay the STP variables before adding new spikes.
	for (int grpId=0; (grpId < numGrp) & !spikeBufferFull; grpId++) {
		// decay homeostasis avg firing
		if (grp_Info[grpId].WithHomeostasis) {
			for(int i=grp_Info[grpId].StartN; i<=grp_Info[grpId].EndN; i++) {
				avgFiring[i] *= grp_Info[grpId].avgTimeScale_decay;
			}
		}

		// decay the STP variables before adding new spikes.
		if (grp_Info[grpId].WithSTP) {
			for(int i=grp_Info[grpId].StartN; i<=grp_Info[grpId].EndN; i++) {
				int ind_plus  = STP_BUF_POS(i,simTime);
				int ind_minus = STP_BUF_POS(i,(simTime-1));
				stpu[ind_plus] = stpu[ind_minus]*(1.0-grp_Info[grpId].STP_tau_u_inv);
				stpx[ind_plus] = stpx[ind_minus] + (1.0-stpx[ind_minus])*grp_Info[grpId].STP_tau_x_inv;
			}
		}

		if (grp_Info[grpId].Type&POISSON_NEURON)
			continue;

		// decay dopamine concentration
		if ((grp_Info[grpId].WithESTDPtype == DA_MOD || grp_Info[grpId].WithISTDP == DA_MOD) && 
			cpuNetPtrs.grpDA[grpId] > grp_Info[grpId].baseDP)
		{
			cpuNetPtrs.grpDA[grpId] *= grp_Info[grpId].decayDP;
		}

		// decay conductances
		if (sim_with_conductances) {
			for(int i=grp_Info[grpId].StartN; i<=grp_Info[grpId].EndN; i++) {
				gAMPA[i]  *= dAMPA;
				gGABAa[i] *= dGABAa;

				if (sim_with_NMDA_rise) {
					gNMDA_r[i] *= rNMDA;	// rise
					gNMDA_d[i] *= dNMDA;	// decay
				} else {
					gNMDA[i]   *= dNMDA;	// instantaneous rise
				}

				if (sim_with_GABAb_rise) {
					gGABAb_r[i] *= rGABAb;	// rise
					gGABAb_d[i] *= dGABAb;	// decay
				} else {
					gGABAb[i] *= dGABAb;	// instantaneous rise
				}
			}
		}
	} // end grpId loop

	// In CUBA mode, reset current to 0 each time step
	if (!sim_with_conductances) {
		resetCurrent();
	}
}

void CpuSNN::findFiring() {
	int spikeBufferFull = 0;

	for(int g=0; (g < numGrp) & !spikeBufferFull; g++) {
		// given group of neurons belong to the poisson group....
		if (grp_Info[g].Type&POISSON_NEURON)
			continue;

		// his flag is set if with_stdp is set and also grpType is set to have GROUP_SYN_FIXED
		for(int i=grp_Info[g].StartN; i <= grp_Info[g].EndN; i++) {
			assert(i < numNReg);

			// 9-param model can set vpeak, but it's hardcoded in 4-param model
			//float vpeak = (grp_Info[g].withParamModel_9) ? Izh_vpeak[i] : 30.0f;
			//if (voltage[i] >= vpeak) {
			if (curSpike[i]) {
				curSpike[i] = false;

				// if flag hasSpkMonRT is set, we want to keep track of how many spikes per neuron in the group
				if (grp_Info[g].withSpikeCounter) {// put the condition for runNetwork
					int bufPos = grp_Info[g].spkCntBufPos; // retrieve buf pos
					int bufNeur = i-grp_Info[g].StartN;
					spkCntBuf[bufPos][bufNeur]++;
				}
				spikeBufferFull = addSpikeToTable(i, g);

				if (spikeBufferFull)
					break;

				// STDP calculation: the post-synaptic neuron fires after the arrival of a pre-synaptic spike
				if (!sim_in_testing && grp_Info[g].WithSTDP) {
					unsigned int pos_ij = cumulativePre[i]; // the index of pre-synaptic neuron
					for(int j=0; j < Npre_plastic[i]; pos_ij++, j++) {
						int stdp_tDiff = (simTime-synSpikeTime[pos_ij]);
						assert(!((stdp_tDiff < 0) && (synSpikeTime[pos_ij] != MAX_SIMULATION_TIME)));

						if (stdp_tDiff > 0) {
							// check this is an excitatory or inhibitory synapse
							if (grp_Info[g].WithESTDP && maxSynWt[pos_ij] >= 0) { // excitatory synapse
								// Handle E-STDP curve
								switch (grp_Info[g].WithESTDPcurve) {
								case EXP_CURVE: // exponential curve
									if (stdp_tDiff * grp_Info[g].TAU_PLUS_INV_EXC < 25)
										wtChange[pos_ij] += STDP(stdp_tDiff, grp_Info[g].ALPHA_PLUS_EXC, grp_Info[g].TAU_PLUS_INV_EXC);
									break;
								case TIMING_BASED_CURVE: // sc curve
									if (stdp_tDiff * grp_Info[g].TAU_PLUS_INV_EXC < 25) {
										if (stdp_tDiff <= grp_Info[g].GAMMA)
											wtChange[pos_ij] += grp_Info[g].OMEGA + grp_Info[g].KAPPA * STDP(stdp_tDiff, grp_Info[g].ALPHA_PLUS_EXC, grp_Info[g].TAU_PLUS_INV_EXC);
										else // stdp_tDiff > GAMMA
											wtChange[pos_ij] -= STDP(stdp_tDiff, grp_Info[g].ALPHA_PLUS_EXC, grp_Info[g].TAU_PLUS_INV_EXC);
									}
									break;
								default:
									KERNEL_ERROR("Invalid E-STDP curve!");
									break;
								}
							} else if (grp_Info[g].WithISTDP && maxSynWt[pos_ij] < 0) { // inhibitory synapse
								// Handle I-STDP curve
								switch (grp_Info[g].WithISTDPcurve) {
								case EXP_CURVE: // exponential curve
									if (stdp_tDiff * grp_Info[g].TAU_PLUS_INV_INB < 25) { // LTP of inhibitory synapse, which decreases synapse weight
										wtChange[pos_ij] -= STDP(stdp_tDiff, grp_Info[g].ALPHA_PLUS_INB, grp_Info[g].TAU_PLUS_INV_INB);
									}
									break;
								case PULSE_CURVE: // pulse curve
									if (stdp_tDiff <= grp_Info[g].LAMBDA) { // LTP of inhibitory synapse, which decreases synapse weight
										wtChange[pos_ij] -= grp_Info[g].BETA_LTP;
										//printf("I-STDP LTP\n");
									} else if (stdp_tDiff <= grp_Info[g].DELTA) { // LTD of inhibitory syanpse, which increase sysnapse weight
										wtChange[pos_ij] -= grp_Info[g].BETA_LTD;
										//printf("I-STDP LTD\n");
									} else { /*do nothing*/}
									break;
								default:
									KERNEL_ERROR("Invalid I-STDP curve!");
									break;
								}
							}
						}
					}
				}
				spikeCountAll1secHost++;
			}
		}
	}
}

int CpuSNN::findGrpId(int nid) {
	KERNEL_WARN("Using findGrpId is deprecated, use array grpIds[] instead...");
	for(int g=0; g < numGrp; g++) {
		if(nid >=grp_Info[g].StartN && (nid <=grp_Info[g].EndN)) {
			return g;
		}
	}
	KERNEL_ERROR("findGrp(): cannot find the group for neuron %d", nid);
	exitSimulation(1);
	return -1;
}

void CpuSNN::findMaxNumSynapses(int* numPostSynapses, int* numPreSynapses) {
	*numPostSynapses = 0;
	*numPreSynapses = 0;

	//  scan all the groups and find the required information
	for (int g=0; g<numGrp; g++) {
		// find the values for maximum postsynaptic length
		// and maximum pre-synaptic length
		if (grp_Info[g].numPostSynapses >= *numPostSynapses)
			*numPostSynapses = grp_Info[g].numPostSynapses;
		if (grp_Info[g].numPreSynapses >= *numPreSynapses)
			*numPreSynapses = grp_Info[g].numPreSynapses;
	}
}

void CpuSNN::generatePostSpike(unsigned int pre_i, unsigned int idx_d, unsigned int offset, unsigned int tD) {
	// get synaptic info...
	post_info_t post_info = postSynapticIds[offset + idx_d];

	// get post-neuron id
	unsigned int post_i = GET_CONN_NEURON_ID(post_info);
	assert(post_i<(unsigned int)numN);

	// get syn id
	int s_i = GET_CONN_SYN_ID(post_info);
	assert(s_i<(Npre[post_i]));

	// get the cumulative position for quick access
	unsigned int pos_i = cumulativePre[post_i] + s_i;
	assert(post_i < (unsigned int)numNReg); // \FIXME is this assert supposed to be for pos_i?

	// get group id of pre- / post-neuron
	short int post_grpId = grpIds[post_i];
	short int pre_grpId = grpIds[pre_i];

	unsigned int pre_type = grp_Info[pre_grpId].Type;

	// get connect info from the cumulative synapse index for mulSynFast/mulSynSlow (requires less memory than storing
	// mulSynFast/Slow per synapse or storing a pointer to grpConnectInfo_s)
	// mulSynFast will be applied to fast currents (either AMPA or GABAa)
	// mulSynSlow will be applied to slow currents (either NMDA or GABAb)
	short int mulIndex = cumConnIdPre[pos_i];
	assert(mulIndex>=0 && mulIndex<numConnections);


	// for each presynaptic spike, postsynaptic (synaptic) current is going to increase by some amplitude (change)
	// generally speaking, this amplitude is the weight; but it can be modulated by STP
	float change = wt[pos_i];

	if (grp_Info[pre_grpId].WithSTP) {
		// if pre-group has STP enabled, we need to modulate the weight
		// NOTE: Order is important! (Tsodyks & Markram, 1998; Mongillo, Barak, & Tsodyks, 2008)
		// use u^+ (value right after spike-update) but x^- (value right before spike-update)

		// dI/dt = -I/tau_S + A * u^+ * x^- * \delta(t-t_{spk})
		// I noticed that for connect(.., RangeDelay(1), ..) tD will be 0
		int ind_minus = STP_BUF_POS(pre_i,(simTime-tD-1));
		int ind_plus  = STP_BUF_POS(pre_i,(simTime-tD));

		change *= grp_Info[pre_grpId].STP_A*stpu[ind_plus]*stpx[ind_minus];

//		fprintf(stderr,"%d: %d[%d], numN=%d, td=%d, maxDelay_=%d, ind-=%d, ind+=%d, stpu=[%f,%f], stpx=[%f,%f], change=%f, wt=%f\n",
//			simTime, pre_grpId, pre_i,
//					numN, tD, maxDelay_, ind_minus, ind_plus,
//					stpu[ind_minus], stpu[ind_plus], stpx[ind_minus], stpx[ind_plus], change, wt[pos_i]);
	}

	// update currents
	// NOTE: it's faster to += 0.0 rather than checking for zero and not updating
	if (sim_with_conductances) {
		if (pre_type & TARGET_AMPA) // if post_i expresses AMPAR
			gAMPA [post_i] += change*mulSynFast[mulIndex]; // scale by some factor
		if (pre_type & TARGET_NMDA) {
			if (sim_with_NMDA_rise) {
				gNMDA_r[post_i] += change*sNMDA*mulSynSlow[mulIndex];
				gNMDA_d[post_i] += change*sNMDA*mulSynSlow[mulIndex];
			} else {
				gNMDA [post_i] += change*mulSynSlow[mulIndex];
			}
		}
		if (pre_type & TARGET_GABAa)
			gGABAa[post_i] -= change*mulSynFast[mulIndex]; // wt should be negative for GABAa and GABAb
		if (pre_type & TARGET_GABAb) {
			if (sim_with_GABAb_rise) {
				gGABAb_r[post_i] -= change*sGABAb*mulSynSlow[mulIndex];
				gGABAb_d[post_i] -= change*sGABAb*mulSynSlow[mulIndex];
			} else {
				gGABAb[post_i] -= change*mulSynSlow[mulIndex];
			}
		}
	} else {
		current[post_i] += change;
	}

	synSpikeTime[pos_i] = simTime;

	// Got one spike from dopaminergic neuron, increase dopamine concentration in the target area
	if (pre_type & TARGET_DA) {
		cpuNetPtrs.grpDA[post_grpId] += 0.04;
	}

	// STDP calculation: the post-synaptic neuron fires before the arrival of a pre-synaptic spike
	if (!sim_in_testing && grp_Info[post_grpId].WithSTDP) {
		int stdp_tDiff = (simTime-lastSpikeTime[post_i]);

		if (stdp_tDiff >= 0) {
			if (grp_Info[post_grpId].WithISTDP && ((pre_type & TARGET_GABAa) || (pre_type & TARGET_GABAb))) { // inhibitory syanpse
				// Handle I-STDP curve
				switch (grp_Info[post_grpId].WithISTDPcurve) {
				case EXP_CURVE: // exponential curve
					if ((stdp_tDiff*grp_Info[post_grpId].TAU_MINUS_INV_INB)<25) { // LTD of inhibitory syanpse, which increase synapse weight
						wtChange[pos_i] -= STDP(stdp_tDiff, grp_Info[post_grpId].ALPHA_MINUS_INB, grp_Info[post_grpId].TAU_MINUS_INV_INB);
					}
					break;
				case PULSE_CURVE: // pulse curve
					if (stdp_tDiff <= grp_Info[post_grpId].LAMBDA) { // LTP of inhibitory synapse, which decreases synapse weight
						wtChange[pos_i] -= grp_Info[post_grpId].BETA_LTP;
					} else if (stdp_tDiff <= grp_Info[post_grpId].DELTA) { // LTD of inhibitory syanpse, which increase synapse weight
						wtChange[pos_i] -= grp_Info[post_grpId].BETA_LTD;
					} else { /*do nothing*/ }
					break;
				default:
					KERNEL_ERROR("Invalid I-STDP curve");
					break;
				}
			} else if (grp_Info[post_grpId].WithESTDP && ((pre_type & TARGET_AMPA) || (pre_type & TARGET_NMDA))) { // excitatory synapse
				// Handle E-STDP curve
				switch (grp_Info[post_grpId].WithESTDPcurve) {
				case EXP_CURVE: // exponential curve
				case TIMING_BASED_CURVE: // sc curve
					if (stdp_tDiff * grp_Info[post_grpId].TAU_MINUS_INV_EXC < 25)
						wtChange[pos_i] += STDP(stdp_tDiff, grp_Info[post_grpId].ALPHA_MINUS_EXC, grp_Info[post_grpId].TAU_MINUS_INV_EXC);
					break;
				default:
					KERNEL_ERROR("Invalid E-STDP curve");
					break;
				}
			} else { /*do nothing*/ }
		}
		assert(!((stdp_tDiff < 0) && (lastSpikeTime[post_i] != MAX_SIMULATION_TIME)));
	}
}

void CpuSNN::generateSpikes() {
	PropagatedSpikeBuffer::const_iterator srg_iter;
	PropagatedSpikeBuffer::const_iterator srg_iter_end = pbuf->endSpikeTargetGroups();

	for( srg_iter = pbuf->beginSpikeTargetGroups(); srg_iter != srg_iter_end; ++srg_iter )  {
		// Get the target neurons for the given groupId
		int nid	 = srg_iter->stg;
		//delaystep_t del = srg_iter->delay;
		//generate a spike to all the target neurons from source neuron nid with a delay of del
		short int g = grpIds[nid];

		addSpikeToTable (nid, g);
		spikeCountAll1secHost++;
		nPoissonSpikes++;
	}

	// advance the time step to the next phase...
	pbuf->nextTimeStep();
}

void CpuSNN::generateSpikesFromFuncPtr(int grpId) {
	// \FIXME this function is a mess
	bool done;
	SpikeGeneratorCore* spikeGen = grp_Info[grpId].spikeGen;
	int timeSlice = grp_Info[grpId].CurrTimeSlice;
	unsigned int currTime = simTime;
	int spikeCnt = 0;
	for(int i = grp_Info[grpId].StartN; i <= grp_Info[grpId].EndN; i++) {
		// start the time from the last time it spiked, that way we can ensure that the refractory period is maintained
		unsigned int nextTime = lastSpikeTime[i];
		if (nextTime == MAX_SIMULATION_TIME)
			nextTime = 0;

		// the end of the valid time window is either the length of the scheduling time slice from now (because that
		// is the max of the allowed propagated buffer size) or simply the end of the simulation
		unsigned int endOfTimeWindow = (std::min)(currTime+timeSlice,simTimeRunStop);

		done = false;
		while (!done) {
			// generate the next spike time (nextSchedTime) from the nextSpikeTime callback
			unsigned int nextSchedTime = spikeGen->nextSpikeTime(this, grpId, i - grp_Info[grpId].StartN, currTime, 
				nextTime, endOfTimeWindow);

			// the generated spike time is valid only if:
			// - it has not been scheduled before (nextSchedTime > nextTime)
			//    - but careful: we would drop spikes at t=0, because we cannot initialize nextTime to -1...
			// - it is within the scheduling time slice (nextSchedTime < endOfTimeWindow)
			// - it is not in the past (nextSchedTime >= currTime)
			if ((nextSchedTime==0 || nextSchedTime>nextTime) && (nextSchedTime<endOfTimeWindow)
				&& (nextSchedTime>=currTime)) {
//				fprintf(stderr,"%u: spike scheduled for %d at %u\n",currTime, i-grp_Info[grpId].StartN,nextSchedTime);
				// scheduled spike...
				// \TODO CPU mode does not check whether the same AER event has been scheduled before (bug #212)
				// check how GPU mode does it, then do the same here.
				nextTime = nextSchedTime;
				pbuf->scheduleSpikeTargetGroup(i, nextTime - currTime);
				spikeCnt++;

				// update number of spikes if SpikeCounter set
				if (grp_Info[grpId].withSpikeCounter) {
					int bufPos = grp_Info[grpId].spkCntBufPos; // retrieve buf pos
					int bufNeur = i-grp_Info[grpId].StartN;
					spkCntBuf[bufPos][bufNeur]++;
				}
			} else {
				done = true;
			}
		}
	}
}

void CpuSNN::generateSpikesFromRate(int grpId) {
	bool done;
	PoissonRate* rate = grp_Info[grpId].RatePtr;
	float refPeriod = grp_Info[grpId].RefractPeriod;
	int timeSlice   = grp_Info[grpId].CurrTimeSlice;
	unsigned int currTime = simTime;
	int spikeCnt = 0;

	if (rate == NULL)
		return;

	if (rate->isOnGPU()) {
		KERNEL_ERROR("Specifying rates on the GPU but using the CPU SNN is not supported.");
		exitSimulation(1);
	}

	const int nNeur = rate->getNumNeurons();
	if (nNeur != grp_Info[grpId].SizeN) {
		KERNEL_ERROR("Length of PoissonRate array (%d) did not match number of neurons (%d) for group %d(%s).",
			nNeur, grp_Info[grpId].SizeN, grpId, getGroupName(grpId).c_str());
		exitSimulation(1);
	}

	for (int neurId=0; neurId<nNeur; neurId++) {
		float frate = rate->getRate(neurId);

		// start the time from the last time it spiked, that way we can ensure that the refractory period is maintained
		unsigned int nextTime = lastSpikeTime[grp_Info[grpId].StartN + neurId];
		if (nextTime == MAX_SIMULATION_TIME)
			nextTime = 0;

		done = false;
		while (!done && frate>0) {
			nextTime = poissonSpike(nextTime, frate/1000.0, refPeriod);
			// found a valid timeSlice
			if (nextTime < (currTime+timeSlice)) {
				if (nextTime >= currTime) {
//					int nid = grp_Info[grpId].StartN+cnt;
					pbuf->scheduleSpikeTargetGroup(grp_Info[grpId].StartN + neurId, nextTime-currTime);
					spikeCnt++;

					// update number of spikes if SpikeCounter set
					if (grp_Info[grpId].withSpikeCounter) {
						int bufPos = grp_Info[grpId].spkCntBufPos; // retrieve buf pos
						spkCntBuf[bufPos][neurId]++;
					}
				}
			}
			else {
				done=true;
			}
		}
	}
}

inline int CpuSNN::getPoissNeuronPos(int nid) {
	int nPos = nid-numNReg;
	assert(nid >= numNReg);
	assert(nid < numN);
	assert((nid-numNReg) < numNPois);
	return nPos;
}

//We need pass the neuron id (nid) and the grpId just for the case when we want to
//ramp up/down the weights.  In that case we need to set the weights of each synapse
//depending on their nid (their position with respect to one another). -- KDC
float CpuSNN::getWeights(int connProp, float initWt, float maxWt, unsigned int nid, int grpId) {
	float actWts;
	// \FIXME: are these ramping thingies still supported?
	bool setRandomWeights   = GET_INITWTS_RANDOM(connProp);
	bool setRampDownWeights = GET_INITWTS_RAMPDOWN(connProp);
	bool setRampUpWeights   = GET_INITWTS_RAMPUP(connProp);

	if (setRandomWeights)
		actWts = initWt * drand48();
	else if (setRampUpWeights)
		actWts = (initWt + ((nid - grp_Info[grpId].StartN) * (maxWt - initWt) / grp_Info[grpId].SizeN));
	else if (setRampDownWeights)
		actWts = (maxWt - ((nid - grp_Info[grpId].StartN) * (maxWt - initWt) / grp_Info[grpId].SizeN));
	else
		actWts = initWt;

	return actWts;
}

// single integration step for voltage equation of 4-param Izhikevich
inline
float dvdtIzhikevich4(float volt, float recov, float totalCurrent, float timeStep=1.0f) {
	return ( ((0.04f * volt + 5.0f) * volt + 140.0f - recov + totalCurrent) * timeStep );
}

// single integration step for recovery equation of 4-param Izhikevich
inline
float dudtIzhikevich4(float volt, float recov, float izhA, float izhB, float timeStep=1.0f) {
	return ( izhA * (izhB * volt - recov) * timeStep );
}

// single integration step for voltage equation of 9-param Izhikevich
inline
float dvdtIzhikevich9(float volt, float recov, float invCapac, float izhK, float voltRest,
	float voltInst,float totalCurrent, float timeStep=1.0f)
{
	return ( (izhK * (volt - voltRest) * (volt - voltInst) - recov + totalCurrent) * invCapac * timeStep );
}

// single integration step for recovery equation of 9-param Izhikevich
inline
float dudtIzhikevich9(float volt, float recov, float voltRest, float izhA, float izhB, float timeStep=1.0f) {
	return ( izhA * (izhB * (volt - voltRest) - recov) * timeStep );
}

float CpuSNN::getCompCurrent(int grpId, int neurId, float const0, float const1) {
	float compCurrent = 0.0f;
	for (int k=0; k<grp_Info[grpId].numCompNeighbors; k++) {
		// compartment connections are always one-to-one, which means that the i-th neuron in grpId connects
		// to the i-th neuron in grpIdOther
		int grpIdOther = grp_Info[grpId].compNeighbors[k];
		int neurIdOther = neurId - grp_Info[grpId].StartN + grp_Info[grpIdOther].StartN;
		compCurrent += grp_Info[grpId].compCoupling[k] * ((voltage[neurIdOther] + const1)
			- (voltage[neurId] + const0));
	}

	return compCurrent;
}

void  CpuSNN::globalStateUpdate() {
	// We use the current values of voltage and recovery to compute the values for the next (future) time step
	// these results are stored in nextVoltage, and are not applied to the voltage array until the end of the
	// integration step.
	// We do it this way because compartmental currents depend on neighboring neuron's voltages.
	// We don't need a nextRecovery buffer because every neuron depends only on its own recovery value.
	for (int j=1; j<=simNumStepsPerMs_; j++) {
		for(int g=0; g<numGrp; g++) {
			if (grp_Info[g].Type & POISSON_NEURON) {
				continue;
			}

			// update group dopamine
			cpuNetPtrs.grpDABuffer[g][simTimeMs] = cpuNetPtrs.grpDA[g];

			for (int i=grp_Info[g].StartN; i<=grp_Info[g].EndN; i++) {
				// pre-load izhikevich variables to avoid unnecessary memory accesses + unclutter the code.
				float k = Izh_k[i];
				float vr = Izh_vr[i];
				float vt = Izh_vt[i];
				float inverse_C = 1.0f / Izh_C[i];
				float a = Izh_a[i];
				float b = Izh_b[i];

				// sum up total current = synaptic + external + compartmental
				float totalCurrent = extCurrent[i];
				if (sim_with_conductances) { // COBA model
					float tmp_gNMDA = sim_with_NMDA_rise ? gNMDA_d[i]-gNMDA_r[i] : gNMDA[i];
					float tmp_gGABAb = sim_with_GABAb_rise ? gGABAb_d[i]-gGABAb_r[i] : gGABAb[i];
					float tmp_iNMDA = (voltage[i] + 80.0f) * (voltage[i] + 80.0f) / 60.0f / 60.0f;

					totalCurrent += -(gAMPA[i] * (voltage[i] - 0.0f) +
						tmp_gNMDA * tmp_iNMDA / (1.0f + tmp_iNMDA) * (voltage[i] - 0.0f) +
						gGABAa[i] * (voltage[i] + 70.0f) +
						tmp_gGABAb * (voltage[i] + 90.0f));
				} else { // CUBA model
					totalCurrent += current[i];
				}
				if (grp_Info[g].withCompartments) {
					totalCurrent += getCompCurrent(g, i);
				}

				switch (simIntegrationMethod_) {
				case FORWARD_EULER:
					if (!grp_Info[g].withParamModel_9) {
						// 4-param Izhikevich
						nextVoltage[i] = voltage[i] + dvdtIzhikevich4(voltage[i], recovery[i], totalCurrent, timeStep_);
						if (nextVoltage[i] > 30.0f) {
							nextVoltage[i] = 30.0f;
							curSpike[i] = true;
							nextVoltage[i] = Izh_c[i];
							recovery[i] += Izh_d[i];
						}
					} else {
						// 9-param Izhikevich
						nextVoltage[i] = voltage[i] + dvdtIzhikevich9(voltage[i], recovery[i], inverse_C, k, vr, vt, 
							totalCurrent, timeStep_);
						if (nextVoltage[i] > Izh_vpeak[i]) {
							nextVoltage[i] = Izh_vpeak[i];
							curSpike[i] = true;
							nextVoltage[i] = Izh_c[i];
							recovery[i] += Izh_d[i];
						}
					}
					if (nextVoltage[i] < -90.0f) {
						nextVoltage[i] = -90.0f;
					}
					#if defined(WIN32) || defined(WIN64)
						assert(!_isnan(nextVoltage[i]));
						assert(_finite(nextVoltage[i]));
					#else
						assert(!isnan(nextVoltage[i]));
						assert(!isinf(nextVoltage[i]));
					#endif

					// To maintain consistency with Izhikevich' original Matlab code, recovery is based on nextVoltage.
					if (!grp_Info[g].withParamModel_9) {
						recovery[i] += dudtIzhikevich4(nextVoltage[i], recovery[i], a, b, timeStep_);
					} else {
						recovery[i] += dudtIzhikevich9(nextVoltage[i], recovery[i], vr, a, b, timeStep_);
					}

					break;
				case RUNGE_KUTTA4:
					// TODO for Stas
					if (!grp_Info[g].withParamModel_9) {
						// 4-param Izhikevich
						float k1 = dvdtIzhikevich4(voltage[i], recovery[i], totalCurrent, timeStep_);
						float l1 = dudtIzhikevich4(voltage[i], recovery[i], a, b, timeStep_);

						float k2 = dvdtIzhikevich4(voltage[i] + k1/2.0f, recovery[i] + l1/2.0f, totalCurrent, 
							timeStep_);
						float l2 = dudtIzhikevich4(voltage[i] + k1/2.0f, recovery[i] + l1/2.0f, a, b, timeStep_);

						float k3 = dvdtIzhikevich4(voltage[i] + k2/2.0f, recovery[i] + l2/2.0f, totalCurrent, 
							timeStep_);
						float l3 = dudtIzhikevich4(voltage[i] + k2/2.0f, recovery[i] + l2/2.0f, a, b, timeStep_);

						float k4 = dvdtIzhikevich4(voltage[i] + k3, recovery[i] + l3, totalCurrent, timeStep_);
						float l4 = dudtIzhikevich4(voltage[i] + k3, recovery[i] + l3, a, b, timeStep_);

						nextVoltage[i] = voltage[i] + (1.0f / 6.0f) * (k1 + 2.0f * k2 + 2.0f * k3 + k4);
						if (nextVoltage[i] > 30.0f) {
							nextVoltage[i] = 30.0f;
							curSpike[i] = true;
							nextVoltage[i] = Izh_c[i];
							recovery[i] += Izh_d[i];
						}
						if (nextVoltage[i] < -90.0f) {
							nextVoltage[i] = -90.0f;
						}
						#if defined(WIN32) || defined(WIN64)
						assert(!_isnan(nextVoltage[i]));
						assert(_finite(nextVoltage[i]));
						#else
						assert(!isnan(nextVoltage[i]));
						assert(!isinf(nextVoltage[i]));
						#endif

						recovery[i] += (1.0f / 6.0f) * (l1 + 2.0f * l2 + 2.0f * l3 + l4);
					} else {
						// 9-param Izhikevich

						float k1 = dvdtIzhikevich9(voltage[i], recovery[i], inverse_C, k, vr, vt, totalCurrent, 
							timeStep_);
						float l1 = dudtIzhikevich9(voltage[i], recovery[i], vr, a, b, timeStep_);

						float k2 = dvdtIzhikevich9(voltage[i] + k1/2.0f, recovery[i] + l1/2.0f, inverse_C, k, vr, vt, 
							totalCurrent, timeStep_);
						float l2 = dudtIzhikevich9(voltage[i] + k1/2.0f, recovery[i] + l1/2.0f, vr, a, b, timeStep_);

						float k3 = dvdtIzhikevich9(voltage[i] + k2/2.0f, recovery[i] + l2/2.0f, inverse_C, k, vr, vt,
							totalCurrent, timeStep_);
						float l3 = dudtIzhikevich9(voltage[i] + k2/2.0f, recovery[i] + l2/2.0f, vr, a, b, timeStep_);

						float k4 = dvdtIzhikevich9(voltage[i] + k3, recovery[i] + l3, inverse_C, k, vr, vt, 
							totalCurrent, timeStep_);
						float l4 = dudtIzhikevich9(voltage[i] + k3, recovery[i] + l3, vr, a, b, timeStep_);

						nextVoltage[i] = voltage[i] + (1.0f / 6.0f) * (k1 + 2.0f * k2 + 2.0f * k3 + k4);

						if (nextVoltage[i] > Izh_vpeak[i]) {
							nextVoltage[i] = Izh_vpeak[i];
							curSpike[i] = true;
							nextVoltage[i] = Izh_c[i];
							recovery[i] += Izh_d[i];
						}

						if (nextVoltage[i] < -90.0f) {
							nextVoltage[i] = -90.0f;
						}
						#if defined(WIN32) || defined(WIN64)
						assert(!_isnan(nextVoltage[i]));
						assert(_finite(nextVoltage[i]));
						#else
						assert(!isnan(nextVoltage[i]));
						assert(!isinf(nextVoltage[i]));
						#endif

						recovery[i] += (1.0f / 6.0f) * (l1 + 2.0f * l2 + 2.0f * l3 + l4);
					}
					break;
				case UNKNOWN_INTEGRATION:
				default:
					KERNEL_ERROR("Unknown integration method.");
					exitSimulation(1);
				}
			}  // end StartN...EndN
		}  // end numGrp

		// Only after we are done computing nextVoltage for all neurons do we copy the new values to the voltage array.
		// This is crucial for GPU (asynchronous kernel launch) and in the future for a multi-threaded CARLsim version.
		memcpy(voltage, nextVoltage, sizeof(float)*numNReg);
	}  // end simNumStepsPerMs_ loop
}

// initialize all the synaptic weights to appropriate values..
// total size of the synaptic connection is 'length' ...
void CpuSNN::initSynapticWeights() {
	// Initialize the network wtChange, wt, synaptic firing time
	wtChange         = new float[preSynCnt];
	synSpikeTime     = new uint32_t[preSynCnt];
	cpuSnnSz.synapticInfoSize = sizeof(float)*(preSynCnt*2);

	resetSynapticConnections(false);
}

// checks whether a connection ID contains plastic synapses O(#connections)
bool CpuSNN::isConnectionPlastic(short int connId) {
	assert(connId!=ALL);
	assert(connId<numConnections);

	// search linked list for right connection ID
	grpConnectInfo_t* connInfo = connectBegin;
	bool isPlastic = false;
	while (connInfo) {
		if (connId == connInfo->connId) {
			// get syn wt type from connection property
			isPlastic = GET_FIXED_PLASTIC(connInfo->connProp);
			break;
		}

		connInfo = connInfo->next;
	}

	return isPlastic;
}

// returns whether group has homeostasis enabled
bool CpuSNN::isGroupWithHomeostasis(int grpId) {
	assert(grpId>=0 && grpId<getNumGroups());
	return (grp_Info[grpId].WithHomeostasis);
}

// performs various verification checkups before building the network
void CpuSNN::verifyNetwork() {
	// make sure simulation mode is valid
	if (simMode_ == UNKNOWN_SIM) {
		KERNEL_ERROR("Simulation mode cannot be UNKNOWN_SIM");
		exitSimulation(1);
	}

	// make sure integration method is valid
	if (simIntegrationMethod_ == UNKNOWN_INTEGRATION) {
		KERNEL_ERROR("Integration method cannot be UNKNOWN_INTEGRATION");
		exitSimulation(1);
	}

	// make sure number of neuron parameters have been accumulated correctly
	// NOTE: this used to be updateParameters
	verifyNumNeurons();

	// make sure compartment config is valid
	verifyCompartments();

	// make sure STDP post-group has some incoming plastic connections
	verifySTDP();

	// make sure every group with homeostasis also has STDP
	verifyHomeostasis();
}

// checks whether STDP is set on a post-group with incoming plastic connections
void CpuSNN::verifySTDP() {
	for (int grpId=0; grpId<getNumGroups(); grpId++) {
		if (grp_Info[grpId].WithSTDP) {
			// for each post-group, check if any of the incoming connections are plastic
			grpConnectInfo_t* connInfo = connectBegin;
			bool isAnyPlastic = false;
			while (connInfo) {
				if (connInfo->grpDest == grpId) {
					// get syn wt type from connection property
					isAnyPlastic |= GET_FIXED_PLASTIC(connInfo->connProp);
					if (isAnyPlastic) {
						// at least one plastic connection found: break while
						break;
					}
				}
				connInfo = connInfo->next;
			}
			if (!isAnyPlastic) {
				KERNEL_ERROR("If STDP on group %d (%s) is set, group must have some incoming plastic connections.",
					grpId, grp_Info2[grpId].Name.c_str());
				exitSimulation(1);
			}
		}
	}
}

// checks whether every group with Homeostasis also has STDP
void CpuSNN::verifyHomeostasis() {
	for (int grpId=0; grpId<getNumGroups(); grpId++) {
		if (grp_Info[grpId].WithHomeostasis) {
			if (!grp_Info[grpId].WithSTDP) {
				KERNEL_ERROR("If homeostasis is enabled on group %d (%s), then STDP must be enabled, too.",
					grpId, grp_Info2[grpId].Name.c_str());
				exitSimulation(1);
			}
		}
	}
}

// checks whether the numN* class members are consistent and complete
void CpuSNN::verifyNumNeurons() {
	int nExcPois = 0;
	int nInhPois = 0;
	int nExcReg = 0;
	int nInhReg = 0;

	//  scan all the groups and find the required information
	//  about the group (numN, numPostSynapses, numPreSynapses and others).
	for(int g=0; g<numGrp; g++)  {
		if (grp_Info[g].Type==UNKNOWN_NEURON) {
			KERNEL_ERROR("Unknown group for %d (%s)", g, grp_Info2[g].Name.c_str());
			exitSimulation(1);
		}

		if (IS_INHIBITORY_TYPE(grp_Info[g].Type) && !(grp_Info[g].Type & POISSON_NEURON))
			nInhReg += grp_Info[g].SizeN;
		else if (IS_EXCITATORY_TYPE(grp_Info[g].Type) && !(grp_Info[g].Type & POISSON_NEURON))
			nExcReg += grp_Info[g].SizeN;
		else if (IS_EXCITATORY_TYPE(grp_Info[g].Type) &&  (grp_Info[g].Type & POISSON_NEURON))
			nExcPois += grp_Info[g].SizeN;
		else if (IS_INHIBITORY_TYPE(grp_Info[g].Type) &&  (grp_Info[g].Type & POISSON_NEURON))
			nInhPois += grp_Info[g].SizeN;
	}

	// check the newly gathered information with class members
	if (numN != nExcReg+nInhReg+nExcPois+nInhPois) {
		KERNEL_ERROR("nExcReg+nInhReg+nExcPois+nInhPois=%d does not add up to numN=%d",
			nExcReg+nInhReg+nExcPois+nInhPois, numN);
		exitSimulation(1);
	}
	if (numNReg != nExcReg+nInhReg) {
		KERNEL_ERROR("nExcReg+nInhReg=%d does not add up to numNReg=%d", nExcReg+nInhReg, numNReg);
		exitSimulation(1);
	}
	if (numNPois != nExcPois+nInhPois) {
		KERNEL_ERROR("nExcPois+nInhPois=%d does not add up to numNPois=%d", nExcPois+nInhPois, numNPois);
		exitSimulation(1);
	}
//	printf("numN=%d == %d\n",numN,nExcReg+nInhReg+nExcPois+nInhPois);
//	printf("numNReg=%d == %d\n",numNReg, nExcReg+nInhReg);
//	printf("numNPois=%d == %d\n",numNPois, nExcPois+nInhPois);
}

void CpuSNN::verifyCompartments() {
	assert((sim_with_compartments && compConnectBegin!=NULL) || (!sim_with_compartments && compConnectBegin==NULL));

	compConnectInfo_t* newInfo = compConnectBegin;
	while (newInfo) {
		int grpLower = newInfo->grpSrc;
		int grpUpper = newInfo->grpDest;

		// make sure groups are compartmentally enabled
		if (!grp_Info[grpLower].withCompartments) {
			KERNEL_ERROR("Group %s(%d) is not compartmentally enabled, cannot be part of a compartmental connection.",
				grp_Info2[grpLower].Name.c_str(), grpLower);
			exitSimulation(1);
		}
		if (!grp_Info[grpUpper].withCompartments) {
			KERNEL_ERROR("Group %s(%d) is not compartmentally enabled, cannot be part of a compartmental connection.",
				grp_Info2[grpUpper].Name.c_str(), grpUpper);
			exitSimulation(1);
		}

		newInfo = newInfo->next;
	}
}

// \FIXME: not sure where this should go... maybe create some helper file?
bool CpuSNN::isPoint3DinRF(const RadiusRF& radius, const Point3D& pre, const Point3D& post) {
	// Note: RadiusRF rad is assumed to be the fanning in to the post neuron. So if the radius is 10 pixels, it means
	// that if you look at the post neuron, it will receive input from neurons that code for locations no more than
	// 10 pixels away. (The opposite is called a response/stimulus field.)

	double rfDist = getRFDist3D(radius, pre, post);
	return (rfDist >= 0.0 && rfDist <= 1.0);
}

double CpuSNN::getRFDist3D(const RadiusRF& radius, const Point3D& pre, const Point3D& post) {
	// Note: RadiusRF rad is assumed to be the fanning in to the post neuron. So if the radius is 10 pixels, it means
	// that if you look at the post neuron, it will receive input from neurons that code for locations no more than
	// 10 pixels away.

	// ready output argument
	// CpuSNN::isPoint3DinRF() will return true (connected) if rfDist e[0.0, 1.0]
	double rfDist = -1.0;

	// pre and post are connected in a generic 3D ellipsoid RF if x^2/a^2 + y^2/b^2 + z^2/c^2 <= 1.0, where
	// x = pre.x-post.x, y = pre.y-post.y, z = pre.z-post.z
	// x < 0 means:  connect if y and z satisfy some constraints, but ignore x
	// x == 0 means: connect if y and z satisfy some constraints, and enforce pre.x == post.x
	if ((radius.radX==0 && pre.x!=post.x) || (radius.radY==0 && pre.y!=post.y) || (radius.radZ==0 && pre.z!=post.z)) {
		rfDist = -1.0;
	} else {
		// 3D ellipsoid: x^2/a^2 + y^2/b^2 + z^2/c^2 <= 1.0
		double xTerm = (radius.radX<=0) ? 0.0 : pow(pre.x-post.x,2)/pow(radius.radX,2);
		double yTerm = (radius.radY<=0) ? 0.0 : pow(pre.y-post.y,2)/pow(radius.radY,2);
		double zTerm = (radius.radZ<=0) ? 0.0 : pow(pre.z-post.z,2)/pow(radius.radZ,2);
		rfDist = xTerm + yTerm + zTerm;
	}

	return rfDist;
}

// creates the CPU net pointers
// don't forget to cudaFree the device pointers if you make cpu_gpuNetPtrs
void CpuSNN::makePtrInfo() {
	cpuNetPtrs.voltage			= voltage;
	cpuNetPtrs.nextVoltage      = nextVoltage;
	cpuNetPtrs.recovery			= recovery;
	cpuNetPtrs.current			= current;
	cpuNetPtrs.extCurrent       = extCurrent;
	cpuNetPtrs.curSpike         = curSpike;
	cpuNetPtrs.Npre				= Npre;
	cpuNetPtrs.Npost			= Npost;
	cpuNetPtrs.cumulativePost 	= cumulativePost;
	cpuNetPtrs.cumulativePre  	= cumulativePre;
	cpuNetPtrs.synSpikeTime		= synSpikeTime;
	cpuNetPtrs.wt				= wt;
	cpuNetPtrs.wtChange			= wtChange;
	cpuNetPtrs.cumConnIdPre 	= cumConnIdPre;
	cpuNetPtrs.nSpikeCnt		= nSpikeCnt;
	cpuNetPtrs.firingTableD2 	= firingTableD2;
	cpuNetPtrs.firingTableD1 	= firingTableD1;
	cpuNetPtrs.grpIds 			= grpIds;

	// homeostasis variables
	cpuNetPtrs.avgFiring    	= avgFiring;
	cpuNetPtrs.baseFiring   	= baseFiring;

	cpuNetPtrs.gAMPA        	= gAMPA;
	cpuNetPtrs.gGABAa       	= gGABAa;
	if (sim_with_NMDA_rise) {
		cpuNetPtrs.gNMDA 		= NULL;
		cpuNetPtrs.gNMDA_r		= gNMDA_r;
		cpuNetPtrs.gNMDA_d		= gNMDA_d;
	} else {
		cpuNetPtrs.gNMDA		= gNMDA;
		cpuNetPtrs.gNMDA_r 		= NULL;
		cpuNetPtrs.gNMDA_d 		= NULL;
	}
	if (sim_with_GABAb_rise) {
		cpuNetPtrs.gGABAb		= NULL;
		cpuNetPtrs.gGABAb_r		= gGABAb_r;
		cpuNetPtrs.gGABAb_d		= gGABAb_d;
	} else {
		cpuNetPtrs.gGABAb		= gGABAb;
		cpuNetPtrs.gGABAb_r 	= NULL;
		cpuNetPtrs.gGABAb_d 	= NULL;
	}
	cpuNetPtrs.grpDA			= grpDA;
	cpuNetPtrs.grp5HT			= grp5HT;
	cpuNetPtrs.grpACh			= grpACh;
	cpuNetPtrs.grpNE			= grpNE;
	for (int i = 0; i < numGrp; i++) {
		cpuNetPtrs.grpDABuffer[i]	= grpDABuffer[i];
		cpuNetPtrs.grp5HTBuffer[i]	= grp5HTBuffer[i];
		cpuNetPtrs.grpAChBuffer[i]	= grpAChBuffer[i];
		cpuNetPtrs.grpNEBuffer[i]	= grpNEBuffer[i];
	}
	cpuNetPtrs.allocated    	= true;
	cpuNetPtrs.memType      	= CPU_MODE;
	cpuNetPtrs.stpu 			= stpu;
	cpuNetPtrs.stpx				= stpx;
}

// will be used in generateSpikesFromRate
// The time between each pair of consecutive events has an exponential distribution with parameter \lambda and
// each of these ISI values is assumed to be independent of other ISI values.
// What follows a Poisson distribution is the actual number of spikes sent during a certain interval.
unsigned int CpuSNN::poissonSpike(unsigned int currTime, float frate, int refractPeriod) {
	// refractory period must be 1 or greater, 0 means could have multiple spikes specified at the same time.
	assert(refractPeriod>0);
	assert(frate>=0.0f);

	bool done = false;
	unsigned int nextTime = 0;
	while (!done) {
		// A Poisson process will always generate inter-spike-interval (ISI) values from an exponential distribution.
		float randVal = drand48();
		unsigned int tmpVal  = -log(randVal)/frate;

		// add new ISI to current time
		// this might be faster than keeping currTime fixed until drand48() returns a large enough value for the ISI
		nextTime = currTime + tmpVal;

		// reject new firing time if ISI is smaller than refractory period
		if ((nextTime - currTime) >= (unsigned) refractPeriod)
			done = true;
	}

	assert(nextTime != 0);
	return nextTime;
}

int CpuSNN::loadSimulation_internal(bool onlyPlastic) {
	// TSC: so that we can restore the file position later...
	// MB: not sure why though...
	int64_t file_position = ftell(loadSimFID);
	
	int tmpInt;
	float tmpFloat;

	bool readErr = false; // keep track of reading errors
	size_t result;


	// ------- read header ----------------

	fseek(loadSimFID, 0, SEEK_SET);

	// read file signature
	result = fread(&tmpInt, sizeof(int), 1, loadSimFID);
	readErr |= (result!=1);
	if (tmpInt != 294338571) {
		KERNEL_ERROR("loadSimulation: Unknown file signature. This does not seem to be a "
			"simulation file created with CARLsim::saveSimulation.");
		exitSimulation(-1);
	}

	// read file version number
	result = fread(&tmpFloat, sizeof(float), 1, loadSimFID);
	readErr |= (result!=1);
	if (tmpFloat > 0.2f) {
		KERNEL_ERROR("loadSimulation: Unsupported version number (%f)",tmpFloat);
		exitSimulation(-1);
	}

	// read simulation time
	result = fread(&tmpFloat, sizeof(float), 1, loadSimFID);
	readErr |= (result!=1);

	// read execution time
	result = fread(&tmpFloat, sizeof(float), 1, loadSimFID);
	readErr |= (result!=1);

	// read number of neurons
	result = fread(&tmpInt, sizeof(int), 1, loadSimFID);
	readErr |= (result!=1);
	if (tmpInt != numN) {
		KERNEL_ERROR("loadSimulation: Number of neurons in file (%d) and simulation (%d) don't match.",
			tmpInt, numN);
		exitSimulation(-1);
	}

	// read number of pre-synapses
	result = fread(&tmpInt, sizeof(int), 1, loadSimFID);
	readErr |= (result!=1);
	if (preSynCnt < (unsigned int)tmpInt) {
		KERNEL_ERROR("loadSimulation: preSynCnt in file (%d) should not be larger than preSynCnt in the config state (%d).",
			tmpInt, preSynCnt);
		exitSimulation(-1);
	}

	// read number of post-synapses
	result = fread(&tmpInt, sizeof(int), 1, loadSimFID);
	readErr |= (result!=1);
	if (postSynCnt < (unsigned int)tmpInt) {
		KERNEL_ERROR("loadSimulation: postSynCnt in file (%d) and not be larger than preSysnCnt in the config state (%d).",
			tmpInt, postSynCnt);
		exitSimulation(-1);
	}

	// read number of groups
	result = fread(&tmpInt, sizeof(int), 1, loadSimFID);
	readErr |= (result!=1);
	if (tmpInt != numGrp) {
		KERNEL_ERROR("loadSimulation: Number of groups in file (%d) and simulation (%d) don't match.",
			tmpInt, numGrp);
		exitSimulation(-1);
	}

	// throw reading error instead of proceeding
	if (readErr) {
		fprintf(stderr,"loadSimulation: Error while reading file header");
		exitSimulation(-1);
	}


	// ------- read group information ----------------

	for (int g=0; g<numGrp; g++) {
		// read StartN
		result = fread(&tmpInt, sizeof(int), 1, loadSimFID);
		readErr |= (result!=1);
		if (tmpInt != grp_Info[g].StartN) {
			KERNEL_ERROR("loadSimulation: StartN in file (%d) and grpInfo (%d) for group %d don't match.",
				tmpInt, grp_Info[g].StartN, g);
			exitSimulation(-1);
		}

		// read EndN
		result = fread(&tmpInt, sizeof(int), 1, loadSimFID);
		readErr |= (result!=1);
		if (tmpInt != grp_Info[g].EndN) {
			KERNEL_ERROR("loadSimulation: EndN in file (%d) and grpInfo (%d) for group %d don't match.",
				tmpInt, grp_Info[g].EndN, g);
			exitSimulation(-1);
		}

		// read SizeX
		result = fread(&tmpInt, sizeof(int), 1, loadSimFID);
		readErr |= (result!=1);

		// read SizeY
		result = fread(&tmpInt, sizeof(int), 1, loadSimFID);
		readErr |= (result!=1);

		// read SizeZ
		result = fread(&tmpInt, sizeof(int), 1, loadSimFID);
		readErr |= (result!=1);

		// read group name
		char name[100];
		result = fread(name, sizeof(char), 100, loadSimFID);
		readErr |= (result!=100);
		if (strcmp(name,grp_Info2[g].Name.c_str()) != 0) {
			KERNEL_ERROR("loadSimulation: Group names in file (%s) and grpInfo (%s) don't match.", name,
				grp_Info2[g].Name.c_str());
			exitSimulation(-1);
		}
	}

	if (readErr) {
		KERNEL_ERROR("loadSimulation: Error while reading group info");
		exitSimulation(-1);
	}


	// ------- read synapse information ----------------

	for (int i=0; i<numN; i++) {
		int nrSynapses = 0;

		// read number of synapses
		result = fread(&nrSynapses, sizeof(int), 1, loadSimFID);
		readErr |= (result!=1);

		for (int j=0; j<nrSynapses; j++) {
			unsigned int nIDpre;
			unsigned int nIDpost;
			float weight, maxWeight;
			uint8_t delay;
			uint8_t plastic;
			short int connId;

			// read nIDpre
			result = fread(&nIDpre, sizeof(int), 1, loadSimFID);
			readErr |= (result!=1);
			if (nIDpre != (unsigned int)i) {
				KERNEL_ERROR("loadSimulation: nIDpre in file (%u) and simulation (%u) don't match.", nIDpre, i);
				exitSimulation(-1);
			}

			// read nIDpost
			result = fread(&nIDpost, sizeof(int), 1, loadSimFID);
			readErr |= (result!=1);
			if (nIDpost >= (unsigned int)numN) {
				KERNEL_ERROR("loadSimulation: nIDpre in file (%u) is larger than in simulation (%u).", nIDpost, numN);
				exitSimulation(-1);
			}

			// read weight
			result = fread(&weight, sizeof(float), 1, loadSimFID);
			readErr |= (result!=1);

			short int gIDpre = grpIds[nIDpre];
			if ((IS_INHIBITORY_TYPE(grp_Info[gIDpre].Type) && (weight>0))
					|| (!IS_INHIBITORY_TYPE(grp_Info[gIDpre].Type) && (weight<0))) {
				KERNEL_ERROR("loadSimulation: Sign of weight value (%s) does not match neuron type (%s)",
					((weight>=0.0f)?"plus":"minus"), 
					(IS_INHIBITORY_TYPE(grp_Info[gIDpre].Type)?"inhibitory":"excitatory"));
				exitSimulation(-1);
			}

			// read max weight
			result = fread(&maxWeight, sizeof(float), 1, loadSimFID);
			readErr |= (result!=1);
			if ((IS_INHIBITORY_TYPE(grp_Info[gIDpre].Type) && (maxWeight>=0))
					|| (!IS_INHIBITORY_TYPE(grp_Info[gIDpre].Type) && (maxWeight<=0))) {
				KERNEL_ERROR("loadSimulation: Sign of maxWeight value (%s) does not match neuron type (%s)",
					((maxWeight>=0.0f)?"plus":"minus"), 
					(IS_INHIBITORY_TYPE(grp_Info[gIDpre].Type)?"inhibitory":"excitatory"));
				exitSimulation(-1);
			}

			// read delay
			result = fread(&delay, sizeof(uint8_t), 1, loadSimFID);
			readErr |= (result!=1);
			if (delay > MAX_SynapticDelay) {
				KERNEL_ERROR("loadSimulation: delay in file (%d) is larger than MAX_SynapticDelay (%d)",
					(int)delay, (int)MAX_SynapticDelay);
				exitSimulation(-1);
			}

			assert(!isnan(weight));
			// read plastic/fixed
			result = fread(&plastic, sizeof(uint8_t), 1, loadSimFID);
			readErr |= (result!=1);

			// read connection ID
			result = fread(&connId, sizeof(short int), 1, loadSimFID);
			readErr |= (result!=1);

			if ((plastic && onlyPlastic) || (!plastic && !onlyPlastic)) {
				int gIDpost = grpIds[nIDpost];
				int connProp = SET_FIXED_PLASTIC(plastic?SYN_PLASTIC:SYN_FIXED);

				setConnection(gIDpre, gIDpost, nIDpre, nIDpost, weight, maxWeight, delay, connProp, connId);
				grp_Info2[gIDpre].sumPostConn++;
				grp_Info2[gIDpost].sumPreConn++;

				if (delay > grp_Info[gIDpre].MaxDelay)
					grp_Info[gIDpre].MaxDelay = delay;
			}
		}
	}

	fseek(loadSimFID, file_position, SEEK_SET);

	return 0;
}


// The post synaptic connections are sorted based on delay here so that we can reduce storage requirement
// and generation of spike at the post-synaptic side.
// We also create the delay_info array has the delay_start and delay_length parameter
void CpuSNN::reorganizeDelay() {
	int tdMax = maxDelay_ > 1 ? maxDelay_ : 1;
	for (int grpId=0; grpId < numGrp; grpId++) {
		for (int nid=grp_Info[grpId].StartN; nid <= grp_Info[grpId].EndN; nid++) {
			unsigned int jPos=0;					// this points to the top of the delay queue
			unsigned int cumN=cumulativePost[nid];	// cumulativePost[] is unsigned int
			unsigned int cumDelayStart=0; 			// Npost[] is unsigned short

			// in a network without connections, where maxDelay_==0, we still need to enter the loop in order
			// to set the appropriate postDelayInfo entries to zero
			// otherwise the simulation might segfault because delay_length and delay_index_start are not
			// correctly initialized
			for (int td = 0; td < tdMax; td++) {
				unsigned int j=jPos;				// start searching from top of the queue until the end
				unsigned int cnt=0;					// store the number of nodes with a delay of td;
				while (j < Npost[nid]) {
					// found a node j with delay=td and we put
					// the delay value = 1 at array location td=0;
					if (td == (tmp_SynapticDelay[cumN+j]-1)) {
						assert(jPos<Npost[nid]);
						swapConnections(nid, j, jPos);

						jPos++;
						cnt++;
					}
					j++;
				}

				// update the delay_length and start values...
				postDelayInfo[nid*(maxDelay_+1)+td].delay_length	     = cnt;
				postDelayInfo[nid*(maxDelay_+1)+td].delay_index_start  = cumDelayStart;
				cumDelayStart += cnt;

				assert(cumDelayStart <= Npost[nid]);
			}

			// total cumulative delay should be equal to number of post-synaptic connections at the end of the loop
			assert(cumDelayStart == Npost[nid]);
			for (unsigned int j=1; j < Npost[nid]; j++) {
				unsigned int cumN=cumulativePost[nid]; // cumulativePost[] is unsigned int
				if (tmp_SynapticDelay[cumN+j] < tmp_SynapticDelay[cumN+j-1]) {
	  				KERNEL_ERROR("Post-synaptic delays not sorted correctly... id=%d, delay[%d]=%d, delay[%d]=%d",
						nid, j, tmp_SynapticDelay[cumN+j], j-1, tmp_SynapticDelay[cumN+j-1]);
					assert( tmp_SynapticDelay[cumN+j] >= tmp_SynapticDelay[cumN+j-1]);
				}
			}
		}
	}
}

// after all the initalization. Its time to create the synaptic weights, weight change and also
// time of firing these are the mostly costly arrays so dense packing is essential to minimize wastage of space
void CpuSNN::reorganizeNetwork(bool removeTempMemory) {
	//Double check...sometimes by mistake we might call reorganize network again...
	if(doneReorganization)
		return;

	KERNEL_DEBUG("Beginning reorganization of network....");

	// perform various consistency checks:
	// - numNeurons vs. sum of all neurons
	// - STDP set on a post-group with incoming plastic connections
	// - etc.
	verifyNetwork();

	// time to build the complete network with relevant parameters..
	buildNetwork();

	//..minimize any other wastage in that array by compacting the store
	compactConnections();

	// The post synaptic connections are sorted based on delay here
	reorganizeDelay();

	// Print the statistics again but dump the results to a file
	printMemoryInfo(fpDeb_);

	// initialize the synaptic weights accordingly..
	initSynapticWeights();

	updateSpikeGeneratorsInit();

	//ensure that we dont do all the above optimizations again
	doneReorganization = true;

	// reset all spike cnt
	resetSpikeCnt(ALL);

	printTuningLog(fpDeb_);

	makePtrInfo();

	KERNEL_INFO("");
	KERNEL_INFO("*****************      Initializing %s Simulation      *************************",
		simMode_==GPU_MODE?"GPU":"CPU");

	if(removeTempMemory) {
		memoryOptimized = true;
		delete[] tmp_SynapticDelay;
		tmp_SynapticDelay = NULL;
	}
}


void CpuSNN::resetConductances() {
	if (sim_with_conductances) {
		memset(gAMPA, 0, sizeof(float)*numNReg);
		if (sim_with_NMDA_rise) {
			memset(gNMDA_r, 0, sizeof(float)*numNReg);
			memset(gNMDA_d, 0, sizeof(float)*numNReg);
		} else {
			memset(gNMDA, 0, sizeof(float)*numNReg);
		}
		memset(gGABAa, 0, sizeof(float)*numNReg);
		if (sim_with_GABAb_rise) {
			memset(gGABAb_r, 0, sizeof(float)*numNReg);
			memset(gGABAb_d, 0, sizeof(float)*numNReg);
		} else {
			memset(gGABAb, 0, sizeof(float)*numNReg);
		}
	}
}

void CpuSNN::resetCPUTiming() {
	prevCpuExecutionTime = cumExecutionTime;
	cpuExecutionTime     = 0.0;
}

void CpuSNN::resetCurrent() {
	assert(current != NULL);
	memset(current, 0, sizeof(float) * numNReg);
}

void CpuSNN::resetFiringInformation() {
	// Reset firing tables and time tables to default values..

	// reset Various Times..
	spikeCountAllHost	  = 0;
	spikeCountAll1secHost = 0;
	spikeCountD2Host = 0;
	spikeCountD1Host = 0;
	secD1fireCntHost  = 0;
	secD2fireCntHost  = 0;

	for(int i=0; i < numGrp; i++) {
		grp_Info[i].FiringCount1sec = 0;
	}

	// reset various times...
	simTimeMs  = 0;
	simTimeSec = 0;
	simTime    = 0;

	// reset the propogation Buffer.
	resetPropogationBuffer();
	// reset Timing  Table..
	resetTimingTable();
}

#ifndef __CPU_ONLY__
void CpuSNN::resetGPUTiming() {
	prevGpuExecutionTime = cumExecutionTime;
	gpuExecutionTime     = 0.0;
}
#endif

void CpuSNN::resetGroups() {
	for(int g=0; (g < numGrp); g++) {
		// reset spike generator group...
		if (grp_Info[g].isSpikeGenerator) {
			grp_Info[g].CurrTimeSlice = grp_Info[g].NewTimeSlice;
			grp_Info[g].SliceUpdateTime  = 0;
			for(int nid=grp_Info[g].StartN; nid <= grp_Info[g].EndN; nid++)
				resetPoissonNeuron(nid, g);
		}
		// reset regular neuron group...
		else {
			for(int nid=grp_Info[g].StartN; nid <= grp_Info[g].EndN; nid++)
				resetNeuron(nid, g);
		}
	}

	// reset the currents for each neuron
	resetCurrent();

	// reset the conductances...
	resetConductances();
}

void CpuSNN::resetNeuromodulator(int grpId) {
	grpDA[grpId] = grp_Info[grpId].baseDP;
	grp5HT[grpId] = grp_Info[grpId].base5HT;
	grpACh[grpId] = grp_Info[grpId].baseACh;
	grpNE[grpId] = grp_Info[grpId].baseNE;
}

void CpuSNN::resetNeuron(unsigned int neurId, int grpId) {
	assert(neurId < (unsigned int)numNReg);
    if (grp_Info2[grpId].Izh_a == -1) {
		KERNEL_ERROR("setNeuronParameters must be called for group %s (%d)",grp_Info2[grpId].Name.c_str(),grpId);
		exitSimulation(1);
	}

	Izh_C[neurId] = grp_Info2[grpId].Izh_C + grp_Info2[grpId].Izh_C_sd*(float)drand48();
	Izh_k[neurId] = grp_Info2[grpId].Izh_k + grp_Info2[grpId].Izh_k_sd*(float)drand48();
	Izh_vr[neurId] = grp_Info2[grpId].Izh_vr + grp_Info2[grpId].Izh_vr_sd*(float)drand48();
	Izh_vt[neurId] = grp_Info2[grpId].Izh_vt + grp_Info2[grpId].Izh_vt_sd*(float)drand48();
	Izh_a[neurId] = grp_Info2[grpId].Izh_a + grp_Info2[grpId].Izh_a_sd*(float)drand48();
	Izh_b[neurId] = grp_Info2[grpId].Izh_b + grp_Info2[grpId].Izh_b_sd*(float)drand48();
	Izh_vpeak[neurId] = grp_Info2[grpId].Izh_vpeak + grp_Info2[grpId].Izh_vpeak_sd*(float)drand48();
	Izh_c[neurId] = grp_Info2[grpId].Izh_c + grp_Info2[grpId].Izh_c_sd*(float)drand48();
	Izh_d[neurId] = grp_Info2[grpId].Izh_d + grp_Info2[grpId].Izh_d_sd*(float)drand48();

	// initialize membrane potential to reset potential
	float vreset = grp_Info[grpId].withParamModel_9 ? Izh_vr[neurId] : Izh_c[neurId];
	voltage[neurId] = nextVoltage[neurId] = vreset;

	// recovery is initialized to 0 in 9-param model
	recovery[neurId] = grp_Info[grpId].withParamModel_9 ? 0.0f : Izh_b[neurId]*voltage[neurId];

 	if (grp_Info[grpId].WithHomeostasis) {
		// set the baseFiring with some standard deviation.
		if (drand48()>0.5)   {
			baseFiring[neurId] = grp_Info2[grpId].baseFiring + grp_Info2[grpId].baseFiringSD*-log(drand48());
		} else  {
			baseFiring[neurId] = grp_Info2[grpId].baseFiring - grp_Info2[grpId].baseFiringSD*-log(drand48());
			if(baseFiring[neurId] < 0.1) baseFiring[neurId] = 0.1;
		}

		if (grp_Info2[grpId].baseFiring != 0.0) {
			avgFiring[neurId]  = baseFiring[neurId];
		} else {
			baseFiring[neurId] = 0.0;
			avgFiring[neurId]  = 0;
		}
	}

	lastSpikeTime[neurId]  = MAX_SIMULATION_TIME;

	if(grp_Info[grpId].WithSTP) {
		for (int j=0; j<=maxDelay_; j++) { // is of size maxDelay_+1
			int ind = STP_BUF_POS(neurId,j);
			stpu[ind] = 0.0f;
			stpx[ind] = 1.0f;
		}
	}
}

void CpuSNN::resetPointers(bool deallocate) {
	// order is important! monitor objects might point to CpuSNN or CARLsim,
	// need to deallocate them first


	// -------------- DEALLOCATE MONITOR OBJECTS ---------------------- //

	// delete all SpikeMonitor objects
	// don't kill SpikeMonitorCore objects, they will get killed automatically
	for (unsigned int i=0; i<numSpikeMonitor; i++) {
		if (spikeMonList[i]!=NULL && deallocate) delete spikeMonList[i];
		spikeMonList[i]=NULL;
	}

	// delete all GroupMonitor objects
	// don't kill GroupMonitorCore objects, they will get killed automatically
	for (unsigned int i=0; i<numGroupMonitor; i++) {
		if (groupMonList[i]!=NULL && deallocate) delete groupMonList[i];
		groupMonList[i]=NULL;
	}

	// delete all ConnectionMonitor objects
	// don't kill ConnectionMonitorCore objects, they will get killed automatically
	for (int i=0; i<numConnectionMonitor; i++) {
		if (connMonList[i]!=NULL && deallocate) delete connMonList[i];
		connMonList[i]=NULL;
	}

	// delete all Spike Counters
	for (int i=0; i<numSpkCnt; i++) {
		if (spkCntBuf[i]!=NULL && deallocate)
			delete[] spkCntBuf[i];
		spkCntBuf[i]=NULL;
	}

	if (pbuf!=NULL && deallocate) delete pbuf;
	if (spikeGenBits!=NULL && deallocate) delete[] spikeGenBits;
	pbuf=NULL; spikeGenBits=NULL;

	// clear all existing connection info
	if (deallocate) {
		while (connectBegin) {
			grpConnectInfo_t* nextConn = connectBegin->next;
			if (connectBegin!=NULL) {
				free(connectBegin);
				connectBegin = nextConn;
			}
		}
	}
	connectBegin=NULL;

	if (sim_with_compartments && deallocate) {
		while (compConnectBegin) {
			compConnectInfo_t* nextConn = compConnectBegin->next;
			if (compConnectBegin!=NULL) {
				free(compConnectBegin);
				compConnectBegin = nextConn;
			}
		}
	}
	compConnectBegin = NULL;

	// clear data (i.e., concentration of neuromodulator) of groups
	if (grpDA != NULL && deallocate) delete [] grpDA;
	if (grp5HT != NULL && deallocate) delete [] grp5HT;
	if (grpACh != NULL && deallocate) delete [] grpACh;
	if (grpNE != NULL && deallocate) delete [] grpNE;
	grpDA = NULL;
	grp5HT = NULL;
	grpACh = NULL;
	grpNE = NULL;

	// clear assistive data buffer for group monitor
	if (deallocate) {
		for (int i = 0; i < numGrp; i++) {
			if (grpDABuffer[i] != NULL) delete [] grpDABuffer[i];
			if (grp5HTBuffer[i] != NULL) delete [] grp5HTBuffer[i];
			if (grpAChBuffer[i] != NULL) delete [] grpAChBuffer[i];
			if (grpNEBuffer[i] != NULL) delete [] grpNEBuffer[i];
			grpDABuffer[i] = NULL;
			grp5HTBuffer[i] = NULL;
			grpAChBuffer[i] = NULL;
			grpNEBuffer[i] = NULL;
		}
	} else {
		memset(grpDABuffer, 0, sizeof(float*) * MAX_GRP_PER_SNN);
		memset(grp5HTBuffer, 0, sizeof(float*) * MAX_GRP_PER_SNN);
		memset(grpAChBuffer, 0, sizeof(float*) * MAX_GRP_PER_SNN);
		memset(grpNEBuffer, 0, sizeof(float*) * MAX_GRP_PER_SNN);
	}


	// -------------- DEALLOCATE CORE OBJECTS ---------------------- //

	if (voltage!=NULL && deallocate) delete[] voltage;
	if (nextVoltage!=NULL && deallocate) delete[] nextVoltage;
	if (recovery!=NULL && deallocate) delete[] recovery;
	if (current!=NULL && deallocate) delete[] current;
	if (extCurrent!=NULL && deallocate) delete[] extCurrent;
	if (curSpike!=NULL && deallocate) delete[] curSpike;
	voltage=NULL; nextVoltage=NULL; recovery=NULL; current=NULL; extCurrent=NULL; curSpike = NULL;

	if (Izh_C != NULL && deallocate) delete[] Izh_C;
	if (Izh_k != NULL && deallocate) delete[] Izh_k;
	if (Izh_vr != NULL && deallocate) delete[] Izh_vr;
	if (Izh_vt != NULL && deallocate) delete[] Izh_vt;
	if (Izh_a!=NULL && deallocate) delete[] Izh_a;
	if (Izh_b!=NULL && deallocate) delete[] Izh_b;
	if (Izh_vpeak != NULL && deallocate) delete[] Izh_vpeak;
	if (Izh_c!=NULL && deallocate) delete[] Izh_c;
	if (Izh_d!=NULL && deallocate) delete[] Izh_d;
	Izh_C = NULL; Izh_k = NULL; Izh_vr = NULL; Izh_vt = NULL; Izh_a = NULL; Izh_b = NULL; Izh_vpeak = NULL;
	Izh_c = NULL; Izh_d = NULL;

	if (Npre!=NULL && deallocate) delete[] Npre;
	if (Npre_plastic!=NULL && deallocate) delete[] Npre_plastic;
	if (Npost!=NULL && deallocate) delete[] Npost;
	Npre=NULL; Npre_plastic=NULL; Npost=NULL;

	if (cumulativePre!=NULL && deallocate) delete[] cumulativePre;
	if (cumulativePost!=NULL && deallocate) delete[] cumulativePost;
	cumulativePre=NULL; cumulativePost=NULL;

	if (gAMPA!=NULL && deallocate) delete[] gAMPA;
	if (gNMDA!=NULL && deallocate) delete[] gNMDA;
	if (gNMDA_r!=NULL && deallocate) delete[] gNMDA_r;
	if (gNMDA_d!=NULL && deallocate) delete[] gNMDA_d;
	if (gGABAa!=NULL && deallocate) delete[] gGABAa;
	if (gGABAb!=NULL && deallocate) delete[] gGABAb;
	if (gGABAb_r!=NULL && deallocate) delete[] gGABAb_r;
	if (gGABAb_d!=NULL && deallocate) delete[] gGABAb_d;
	gAMPA=NULL; gNMDA=NULL; gNMDA_r=NULL; gNMDA_d=NULL; gGABAa=NULL; gGABAb=NULL; gGABAb_r=NULL; gGABAb_d=NULL;

	if (stpu!=NULL && deallocate) delete[] stpu;
	if (stpx!=NULL && deallocate) delete[] stpx;
	stpu=NULL; stpx=NULL;

	if (avgFiring!=NULL && deallocate) delete[] avgFiring;
	if (baseFiring!=NULL && deallocate) delete[] baseFiring;
	avgFiring=NULL; baseFiring=NULL;

	if (lastSpikeTime!=NULL && deallocate) delete[] lastSpikeTime;
	if (synSpikeTime !=NULL && deallocate) delete[] synSpikeTime;
	if (nSpikeCnt!=NULL && deallocate) delete[] nSpikeCnt;
	lastSpikeTime=NULL; synSpikeTime=NULL; nSpikeCnt=NULL;

	if (postDelayInfo!=NULL && deallocate) delete[] postDelayInfo;
	if (preSynapticIds!=NULL && deallocate) delete[] preSynapticIds;
	if (postSynapticIds!=NULL && deallocate) delete[] postSynapticIds;
	postDelayInfo=NULL; preSynapticIds=NULL; postSynapticIds=NULL;

	if (wt!=NULL && deallocate) delete[] wt;
	if (maxSynWt!=NULL && deallocate) delete[] maxSynWt;
	if (wtChange !=NULL && deallocate) delete[] wtChange;
	wt=NULL; maxSynWt=NULL; wtChange=NULL;

	if (mulSynFast!=NULL && deallocate) delete[] mulSynFast;
	if (mulSynSlow!=NULL && deallocate) delete[] mulSynSlow;
	if (cumConnIdPre!=NULL && deallocate) delete[] cumConnIdPre;
	mulSynFast=NULL; mulSynSlow=NULL; cumConnIdPre=NULL;

	if (grpIds!=NULL && deallocate) delete[] grpIds;
	grpIds=NULL;

	if (firingTableD2!=NULL && deallocate) delete[] firingTableD2;
	if (firingTableD1!=NULL && deallocate) delete[] firingTableD1;
	if (timeTableD2!=NULL && deallocate) delete[] timeTableD2;
	if (timeTableD1!=NULL && deallocate) delete[] timeTableD1;
	firingTableD2=NULL; firingTableD1=NULL; timeTableD2=NULL; timeTableD1=NULL;

#ifndef __CPU_ONLY__
	// clear poisson generator
	if (gpuPoissonRand != NULL) delete gpuPoissonRand;
	gpuPoissonRand = NULL;
#endif
}


void CpuSNN::resetPoissonNeuron(unsigned int nid, int grpId) {
	assert(nid < (unsigned int)numN);
	lastSpikeTime[nid]  = MAX_SIMULATION_TIME;
	if (grp_Info[grpId].WithHomeostasis)
		avgFiring[nid]      = 0.0;

	if(grp_Info[grpId].WithSTP) {
		for (int j=0; j<=maxDelay_; j++) { // is of size maxDelay_+1
			int ind = STP_BUF_POS(nid,j);
			stpu[ind] = 0.0f;
			stpx[ind] = 1.0f;
		}
	}
}

void CpuSNN::resetPropogationBuffer() {
	pbuf->reset(0, 1023);
}

// resets nSpikeCnt[]
void CpuSNN::resetSpikeCnt(int grpId) {
	int startGrp, endGrp;

	if (!doneReorganization)
		return;

	if (grpId == -1) {
		startGrp = 0;
		endGrp = numGrp;
	} else {
		 startGrp = grpId;
		 endGrp = grpId;
	}

	for (int g = startGrp; g<endGrp; g++) {
		int startN = grp_Info[g].StartN;
		int endN   = grp_Info[g].EndN;
		for (int i=startN; i<=endN; i++)
			nSpikeCnt[i] = 0;
	}
}

//Reset wt, wtChange, pre-firing time values to default values, rewritten to
//integrate changes between JMN and MDR -- KDC
//if changeWeights is false, we should keep the values of the weights as they currently
//are but we should be able to change them to plastic or fixed synapses. -- KDC
void CpuSNN::resetSynapticConnections(bool changeWeights) {
	int j;
	// Reset wt,wtChange,pre-firingtime values to default values...
	for(int destGrp=0; destGrp < numGrp; destGrp++) {
		const char* updateStr = (grp_Info[destGrp].newUpdates == true)?"(**)":"";
		KERNEL_DEBUG("Grp: %d:%s s=%d e=%d %s", destGrp, grp_Info2[destGrp].Name.c_str(), grp_Info[destGrp].StartN,
					grp_Info[destGrp].EndN,  updateStr);
		KERNEL_DEBUG("Grp: %d:%s s=%d e=%d  %s",  destGrp, grp_Info2[destGrp].Name.c_str(), grp_Info[destGrp].StartN,
					grp_Info[destGrp].EndN, updateStr);

		for(int nid=grp_Info[destGrp].StartN; nid <= grp_Info[destGrp].EndN; nid++) {
			unsigned int offset = cumulativePre[nid];
			for (j=0;j<Npre[nid]; j++) {
				wtChange[offset+j] = 0.0;						// synaptic derivatives is reset
				synSpikeTime[offset+j] = MAX_SIMULATION_TIME;	// some large negative value..
			}
			post_info_t *preIdPtr = &preSynapticIds[cumulativePre[nid]];
			float* synWtPtr       = &wt[cumulativePre[nid]];
			float* maxWtPtr       = &maxSynWt[cumulativePre[nid]];
			int prevPreGrp  = -1;

			for (j=0; j < Npre[nid]; j++,preIdPtr++, synWtPtr++, maxWtPtr++) {
				int preId    = GET_CONN_NEURON_ID((*preIdPtr));
				assert(preId < numN);
				int srcGrp = grpIds[preId];
				grpConnectInfo_t* connInfo;
				grpConnectInfo_t* connIterator = connectBegin;
				while(connIterator) {
					if(connIterator->grpSrc == srcGrp && connIterator->grpDest == destGrp) {
						//we found the corresponding connection
						connInfo=connIterator;
						break;
					}
					//move to the next grpConnectInfo_t
					connIterator=connIterator->next;
				}
				assert(connInfo != NULL);
				int connProp   = connInfo->connProp;
				bool   synWtType = GET_FIXED_PLASTIC(connProp);
				// print debug information...
				if( prevPreGrp != srcGrp) {
					if(nid==grp_Info[destGrp].StartN) {
						const char* updateStr = (connInfo->newUpdates==true)? "(**)":"";
						KERNEL_DEBUG("\t%d (%s) start=%d, type=%s maxWts = %f %s", srcGrp,
										grp_Info2[srcGrp].Name.c_str(), j, (j<Npre_plastic[nid]?"P":"F"),
										connInfo->maxWt, updateStr);
					}
					prevPreGrp = srcGrp;
				}

				if(!changeWeights)
					continue;

				// if connection was plastic or if the connection weights were updated we need to reset the weights
				// TODO: How to account for user-defined connection reset
				if ((synWtType == SYN_PLASTIC) || connInfo->newUpdates) {
					*synWtPtr = getWeights(connInfo->connProp, connInfo->initWt, connInfo->maxWt, nid, srcGrp);
					*maxWtPtr = connInfo->maxWt;
				}
			}
		}
		grp_Info[destGrp].newUpdates = false;
	}

	grpConnectInfo_t* connInfo = connectBegin;
	// clear all existing connection info...
	while (connInfo) {
		connInfo->newUpdates = false;
		connInfo = connInfo->next;
	}
}

void CpuSNN::resetTimingTable() {
		memset(timeTableD2, 0, sizeof(int) * (1000 + maxDelay_ + 1));
		memset(timeTableD1, 0, sizeof(int) * (1000 + maxDelay_ + 1));
}


//! nid=neuron id, sid=synapse id, grpId=group id.
inline post_info_t CpuSNN::SET_CONN_ID(int nid, int sid, int grpId) {
	if (sid > CONN_SYN_MASK) {
		KERNEL_ERROR("Error: Syn Id (%d) exceeds maximum limit (%d) for neuron %d (group %d)", sid, CONN_SYN_MASK, nid,
			grpId);
		exitSimulation(1);
	}
	post_info_t p;
	p.postId = (((sid)<<CONN_SYN_NEURON_BITS)+((nid)&CONN_SYN_NEURON_MASK));
	p.grpId  = grpId;
	return p;
}

//! set one specific connection from neuron id 'src' to neuron id 'dest'
inline void CpuSNN::setConnection(int srcGrp,  int destGrp,  unsigned int src, unsigned int dest, float synWt,
	float maxWt, uint8_t dVal, int connProp, short int connId)
{
	assert(dest<=CONN_SYN_NEURON_MASK);			// total number of neurons is less than 1 million within a GPU
	assert((dVal >=1) && (dVal <= maxDelay_));

	// adjust sign of weight based on pre-group (negative if pre is inhibitory)
	synWt = isExcitatoryGroup(srcGrp) ? fabs(synWt) : -1.0*fabs(synWt);
	maxWt = isExcitatoryGroup(srcGrp) ? fabs(maxWt) : -1.0*fabs(maxWt);

	// we have exceeded the number of possible connection for one neuron
	if(Npost[src] >= grp_Info[srcGrp].numPostSynapses)	{
		KERNEL_ERROR("setConnection(%d (Grp=%s), %d (Grp=%s), %f, %d)", src, grp_Info2[srcGrp].Name.c_str(),
					dest, grp_Info2[destGrp].Name.c_str(), synWt, dVal);
		KERNEL_ERROR("Large number of postsynaptic connections established (%d), max for this group %d.", Npost[src], grp_Info[srcGrp].numPostSynapses);
		exitSimulation(1);
	}

	if(Npre[dest] >= grp_Info[destGrp].numPreSynapses) {
		KERNEL_ERROR("setConnection(%d (Grp=%s), %d (Grp=%s), %f, %d)", src, grp_Info2[srcGrp].Name.c_str(),
					dest, grp_Info2[destGrp].Name.c_str(), synWt, dVal);
		KERNEL_ERROR("Large number of presynaptic connections established (%d), max for this group %d.", Npre[dest], grp_Info[destGrp].numPreSynapses);
		exitSimulation(1);
	}

	int p = Npost[src];

	assert(Npost[src] >= 0);
	assert(Npre[dest] >= 0);
	assert((src*numPostSynapses_+p)/numN < (unsigned int)numPostSynapses_); // divide by numN to prevent INT overflow

	unsigned int post_pos = cumulativePost[src] + Npost[src];
	unsigned int pre_pos  = cumulativePre[dest] + Npre[dest];

	assert(post_pos < postSynCnt);
	assert(pre_pos  < preSynCnt);

	//generate a new postSynapticIds id for the current connection
	postSynapticIds[post_pos]   = SET_CONN_ID(dest, Npre[dest], destGrp);
	tmp_SynapticDelay[post_pos] = dVal;

	preSynapticIds[pre_pos] = SET_CONN_ID(src, Npost[src], srcGrp);
	wt[pre_pos] 	  = synWt;
	maxSynWt[pre_pos] = maxWt;
	cumConnIdPre[pre_pos] = connId;

	bool synWtType = GET_FIXED_PLASTIC(connProp);

	if (synWtType == SYN_PLASTIC) {
		sim_with_fixedwts = false; // if network has any plastic synapses at all, this will be set to true
		Npre_plastic[dest]++;
		// homeostasis
		if (grp_Info[destGrp].WithHomeostasis && grp_Info[destGrp].homeoId ==-1)
			grp_Info[destGrp].homeoId = dest; // this neuron info will be printed
	}

	Npre[dest] += 1;
	Npost[src] += 1;

	grp_Info2[srcGrp].numPostConn++;
	grp_Info2[destGrp].numPreConn++;

	if (Npost[src] > grp_Info2[srcGrp].maxPostConn)
		grp_Info2[srcGrp].maxPostConn = Npost[src];
	if (Npre[dest] > grp_Info2[destGrp].maxPreConn)
	grp_Info2[destGrp].maxPreConn = Npre[src];
}

void CpuSNN::setGrpTimeSlice(int grpId, int timeSlice) {
	if (grpId == ALL) {
		for(int g=0; (g < numGrp); g++) {
			if (grp_Info[g].isSpikeGenerator)
				setGrpTimeSlice(g, timeSlice);
		}
	} else {
		assert((timeSlice > 0 ) && (timeSlice <  PROPAGATED_BUFFER_SIZE));
		// the group should be poisson spike generator group
		grp_Info[grpId].NewTimeSlice = timeSlice;
		grp_Info[grpId].CurrTimeSlice = timeSlice;
	}
}

// method to set const member randSeed_
int CpuSNN::setRandSeed(int seed) {
	if (seed<0)
		return time(NULL);
	else if(seed==0)
		return 123;
	else
		return seed;
}

// reorganize the network and do the necessary allocation
// of all variable for carrying out the simulation..
// this code is run only one time during network initialization
void CpuSNN::setupNetwork(bool removeTempMem) {
	if(!doneReorganization)
		reorganizeNetwork(removeTempMem);

#ifndef __CPU_ONLY__
	if((simMode_ == GPU_MODE) && (cpu_gpuNetPtrs.allocated == false))
		allocateSNN_GPU();
#endif
}

#ifndef __CPU_ONLY__
void CpuSNN::startGPUTiming() {
	prevGpuExecutionTime = cumExecutionTime;
}
void CpuSNN::stopGPUTiming() {
	gpuExecutionTime += (cumExecutionTime - prevGpuExecutionTime);
	prevGpuExecutionTime = cumExecutionTime;
}
#endif

void CpuSNN::startCPUTiming() {
	prevCpuExecutionTime = cumExecutionTime;
}
void CpuSNN::stopCPUTiming() {
	cpuExecutionTime += (cumExecutionTime - prevCpuExecutionTime);
	prevCpuExecutionTime = cumExecutionTime;
}

// enters testing phase
// in testing, no weight changes can be made, allowing you to evaluate learned weights, etc.
void CpuSNN::startTesting(bool shallUpdateWeights) {
	// because this can be called at any point in time, if we're off the 1-second grid, we want to make
	// sure to apply the accumulated weight changes to the weight matrix
	// but we don't reset the wt update interval counter
	if (shallUpdateWeights && !sim_in_testing) {
		// careful: need to temporarily adjust stdpScaleFactor to make this right
		if (wtANDwtChangeUpdateIntervalCnt_) {
			float storeScaleSTDP = stdpScaleFactor_;
			stdpScaleFactor_ = 1.0f/wtANDwtChangeUpdateIntervalCnt_;

			if (simMode_ == CPU_MODE) {
				updateWeights();
#ifndef __CPU_ONLY__
			} else{
				updateWeights_GPU();
#endif
			}
			stdpScaleFactor_ = storeScaleSTDP;
		}
	}

	sim_in_testing = true;
	net_Info.sim_in_testing = true;

#ifndef __CPU_ONLY__
	if (simMode_ == GPU_MODE) {
		// copy new network info struct to GPU (|TODO copy only a single boolean)
		copyNetworkInfo();
	}
#endif
}

// exits testing phase
void CpuSNN::stopTesting() {
	sim_in_testing = false;
	net_Info.sim_in_testing = false;

#ifndef __CPU_ONLY__
	if (simMode_ == GPU_MODE) {
		// copy new network_info struct to GPU (|TODO copy only a single boolean)
		copyNetworkInfo();
	}
#endif
}


void CpuSNN::swapConnections(int nid, int oldPos, int newPos) {
	unsigned int cumN=cumulativePost[nid];

	// Put the node oldPos to the top of the delay queue
	post_info_t tmp = postSynapticIds[cumN+oldPos];
	postSynapticIds[cumN+oldPos]= postSynapticIds[cumN+newPos];
	postSynapticIds[cumN+newPos]= tmp;

	// Ensure that you have shifted the delay accordingly....
	uint8_t tmp_delay = tmp_SynapticDelay[cumN+oldPos];
	tmp_SynapticDelay[cumN+oldPos] = tmp_SynapticDelay[cumN+newPos];
	tmp_SynapticDelay[cumN+newPos] = tmp_delay;

	// update the pre-information for the postsynaptic neuron at the position oldPos.
	post_info_t  postInfo = postSynapticIds[cumN+oldPos];
	int  post_nid = GET_CONN_NEURON_ID(postInfo);
	int  post_sid = GET_CONN_SYN_ID(postInfo);

	post_info_t* preId    = &preSynapticIds[cumulativePre[post_nid]+post_sid];
	int  pre_nid  = GET_CONN_NEURON_ID((*preId));
	int  pre_sid  = GET_CONN_SYN_ID((*preId));
	int  pre_gid  = GET_CONN_GRP_ID((*preId));
	assert (pre_nid == nid);
	assert (pre_sid == newPos);
	*preId = SET_CONN_ID( pre_nid, oldPos, pre_gid);

	// update the pre-information for the postsynaptic neuron at the position newPos
	postInfo = postSynapticIds[cumN+newPos];
	post_nid = GET_CONN_NEURON_ID(postInfo);
	post_sid = GET_CONN_SYN_ID(postInfo);

	preId    = &preSynapticIds[cumulativePre[post_nid]+post_sid];
	pre_nid  = GET_CONN_NEURON_ID((*preId));
	pre_sid  = GET_CONN_SYN_ID((*preId));
	pre_gid  = GET_CONN_GRP_ID((*preId));
	assert (pre_nid == nid);
	assert (pre_sid == oldPos);
	*preId = SET_CONN_ID( pre_nid, newPos, pre_gid);
}

void CpuSNN::updateConnectionMonitor(short int connId) {
	for (int monId=0; monId<numConnectionMonitor; monId++) {
		if (connId==ALL || connMonCoreList[monId]->getConnectId()==connId) {
			int timeInterval = connMonCoreList[monId]->getUpdateTimeIntervalSec();
			if (timeInterval==1 || (timeInterval>1 && (getSimTime()%timeInterval)==0)) {
				// this ConnectionMonitor wants periodic recording
				connMonCoreList[monId]->writeConnectFileSnapshot(simTime,
					getWeightMatrix2D(connMonCoreList[monId]->getConnectId()));
			}
		}
	}
}


std::vector< std::vector<float> > CpuSNN::getWeightMatrix2D(short int connId) {
	assert(connId!=ALL);
	grpConnectInfo_t* connInfo = connectBegin;
	std::vector< std::vector<float> > wtConnId;

	// loop over all connections and find the ones with Connection Monitors
	while (connInfo) {
		if (connInfo->connId==connId) {
			int grpIdPre = connInfo->grpSrc;
			int grpIdPost = connInfo->grpDest;

			// init weight matrix with right dimensions
			for (int i=0; i<grp_Info[grpIdPre].SizeN; i++) {
				std::vector<float> wtSlice;
				for (int j=0; j<grp_Info[grpIdPost].SizeN; j++) {
					wtSlice.push_back(NAN);
				}
				wtConnId.push_back(wtSlice);
			}

#ifndef __CPU_ONLY__
			// copy the weights for a given post-group from device
			// \TODO: check if the weights for this grpIdPost have already been copied
			// \TODO: even better, but tricky because of ordering, make copyWeightState connection-based
			if (simMode_==GPU_MODE) {
				copyWeightState(&cpuNetPtrs, &cpu_gpuNetPtrs, cudaMemcpyDeviceToHost, false, grpIdPost);
			}
#endif

			for (int postId=grp_Info[grpIdPost].StartN; postId<=grp_Info[grpIdPost].EndN; postId++) {
				unsigned int pos_ij = cumulativePre[postId];
				for (int i=0; i<Npre[postId]; i++, pos_ij++) {
					// skip synapses that belong to a different connection ID
					if (cumConnIdPre[pos_ij]!=connInfo->connId)
						continue;

					// find pre-neuron ID and update ConnectionMonitor container
					int preId = GET_CONN_NEURON_ID(preSynapticIds[pos_ij]);
					wtConnId[preId-getGroupStartNeuronId(grpIdPre)][postId-getGroupStartNeuronId(grpIdPost)] =
						fabs(wt[pos_ij]);
				}
			}
			break;
		}
		connInfo = connInfo->next;
	}

	return wtConnId;
}

void CpuSNN::updateGroupMonitor(int grpId) {
	// don't continue if no group monitors in the network
	if (!numGroupMonitor)
		return;

	if (grpId == ALL) {
		for (int g = 0; g < numGrp; g++)
			updateGroupMonitor(g);
	} else {
		// update group monitor of a specific group

		// find index in group monitor arrays
		int monitorId = grp_Info[grpId].GroupMonitorId;

		// don't continue if no group monitor enabled for this group
		if (monitorId < 0)
			return;

		// find last update time for this group
		GroupMonitorCore* grpMonObj = groupMonCoreList[monitorId];
		int lastUpdate = grpMonObj->getLastUpdated();

		// don't continue if time interval is zero (nothing to update)
		if (getSimTime() - lastUpdate <=0)
			return;

		if (getSimTime() - lastUpdate > 1000)
			KERNEL_ERROR("updateGroupMonitor(grpId=%d) must be called at least once every second",grpId);

#ifndef __CPU_ONLY__
		if (simMode_ == GPU_MODE) {
			// copy the group status (neuromodulators) from the GPU to the CPU..
			copyGroupState(&cpuNetPtrs, &cpu_gpuNetPtrs, cudaMemcpyDeviceToHost, false);
		}
#endif

		// find the time interval in which to update group status
		// usually, we call updateGroupMonitor once every second, so the time interval is [0,1000)
		// however, updateGroupMonitor can be called at any time t \in [0,1000)... so we can have the cases
		// [0,t), [t,1000), and even [t1, t2)
		int numMsMin = lastUpdate%1000; // lower bound is given by last time we called update
		int numMsMax = getSimTimeMs(); // upper bound is given by current time
		if (numMsMax == 0)
			numMsMax = 1000; // special case: full second
		assert(numMsMin < numMsMax);

		// current time is last completed second in milliseconds (plus t to be added below)
		// special case is after each completed second where !getSimTimeMs(): here we look 1s back
		int currentTimeSec = getSimTimeSec();
		if (!getSimTimeMs())
			currentTimeSec--;

		// save current time as last update time
		grpMonObj->setLastUpdated(getSimTime());

		// prepare fast access
		FILE* grpFileId = groupMonCoreList[monitorId]->getGroupFileId();
		bool writeGroupToFile = grpFileId != NULL;
		bool writeGroupToArray = grpMonObj->isRecording();
		float data;

		// Read one peice of data at a time from the buffer and put the data to an appopriate monitor buffer. Later the user
		// may need need to dump these group status data to an output file
		for(int t = numMsMin; t < numMsMax; t++) {
			// fetch group status data, support dopamine concentration currently
			data = grpDABuffer[grpId][t];

			// current time is last completed second plus whatever is leftover in t
			int time = currentTimeSec*1000 + t;

			if (writeGroupToFile) {
				// TODO: write to group status file
			}

			if (writeGroupToArray) {
				grpMonObj->pushData(time, data);
			}
		}

		if (grpFileId!=NULL) // flush group status file
			fflush(grpFileId);
	}
}

void CpuSNN::updateSpikesFromGrp(int grpId) {
	assert(grp_Info[grpId].isSpikeGenerator==true);

//	bool done;
	//static FILE* _fp = fopen("spikes.txt", "w");
	unsigned int currTime = simTime;

	int timeSlice = grp_Info[grpId].CurrTimeSlice;
	grp_Info[grpId].SliceUpdateTime  = simTime;

	// we dont generate any poisson spike if during the
	// current call we might exceed the maximum 32 bit integer value
	if (((uint64_t) currTime + timeSlice) >= MAX_SIMULATION_TIME)
		return;

	if (grp_Info[grpId].spikeGen) {
		generateSpikesFromFuncPtr(grpId);
	} else {
		// current mode is GPU, and GPU would take care of poisson generators
		// and other information about refractor period etc. So no need to continue further...
#if !TESTING_CPU_GPU_POISSON
    if(simMode_ == GPU_MODE)
      return;
#endif

		generateSpikesFromRate(grpId);
	}
}

void CpuSNN::updateSpikeGenerators() {
	for(int g=0; g<numGrp; g++) {
		if (grp_Info[g].isSpikeGenerator) {
			// This evaluation is done to check if its time to get new set of spikes..
			// check whether simTime has advance more than the current time slice, in which case we need to schedule
			// spikes for the next time slice
			// we always have to run this the first millisecond of a new runNetwork call; that is,
			// when simTime==simTimeRunStart
			if(((simTime-grp_Info[g].SliceUpdateTime) >= (unsigned) grp_Info[g].CurrTimeSlice || simTime == simTimeRunStart)) {
				updateSpikesFromGrp(g);
			}
		}
	}
}

void CpuSNN::updateSpikeGeneratorsInit() {
	unsigned int cnt=0;
	for(int g=0; (g < numGrp); g++) {
		if (grp_Info[g].isSpikeGenerator) {
			// This is done only during initialization
			grp_Info[g].CurrTimeSlice = grp_Info[g].NewTimeSlice;

			// we only need NgenFunc for spike generator callbacks that need to transfer their spikes to the GPU
			if (grp_Info[g].spikeGen) {
				grp_Info[g].Noffset = NgenFunc;
				NgenFunc += grp_Info[g].SizeN;
			}
			//Note: updateSpikeFromGrp() will be called first time in updateSpikeGenerators()
			//updateSpikesFromGrp(g);
			cnt++;
			assert(cnt <= numSpikeGenGrps);
		}
	}

	// spikeGenBits can be set only once..
	assert(spikeGenBits == NULL);

	if (NgenFunc) {
		spikeGenBits = new uint32_t[NgenFunc/32+1];
		cpuNetPtrs.spikeGenBits = spikeGenBits;
		// increase the total memory size used by the routine...
		cpuSnnSz.addInfoSize += sizeof(spikeGenBits[0])*(NgenFunc/32+1);
	}
}

//! update CpuSNN::maxSpikesD1, CpuSNN::maxSpikesD2 and allocate sapce for CpuSNN::firingTableD1 and CpuSNN::firingTableD2
/*!
 * \return maximum delay in groups
 */
int CpuSNN::updateSpikeTables() {
	int curD = 0;
	int grpSrc;
	// find the maximum delay in the given network
	// and also the maximum delay for each group.
	grpConnectInfo_t* newInfo = connectBegin;
	while(newInfo) {
		grpSrc = newInfo->grpSrc;
		if (newInfo->maxDelay > curD)
			curD = newInfo->maxDelay;

		// check if the current connection's delay meaning grp1's delay
		// is greater than the MaxDelay for grp1. We find the maximum
		// delay for the grp1 by this scheme.
		if (newInfo->maxDelay > grp_Info[grpSrc].MaxDelay)
		 	grp_Info[grpSrc].MaxDelay = newInfo->maxDelay;

		newInfo = newInfo->next;
	}

	for(int g = 0; g < numGrp; g++) {
		if (grp_Info[g].MaxDelay == 1)
			maxSpikesD1 += (grp_Info[g].SizeN * grp_Info[g].MaxFiringRate);
		else
			maxSpikesD2 += (grp_Info[g].SizeN * grp_Info[g].MaxFiringRate);
	}

	if ((maxSpikesD1 + maxSpikesD2) < (unsigned int) (numNExcReg + numNInhReg + numNPois)
		 * UNKNOWN_NEURON_MAX_FIRING_RATE) {
		KERNEL_ERROR("Insufficient amount of buffer allocated...");
		exitSimulation(1);
	}

	firingTableD2 = new unsigned int[maxSpikesD2];
	firingTableD1 = new unsigned int[maxSpikesD1];
	cpuSnnSz.spikingInfoSize += sizeof(int) * ((maxSpikesD2 + maxSpikesD1) + 2* (1000 + maxDelay_ + 1));

	return curD;
}

// This function is called every second by simulator...
// This function updates the firingTable by removing older firing values...
void CpuSNN::updateFiringTable() {
	// Read the neuron ids that fired in the last maxDelay_ seconds
	// and put it to the beginning of the firing table...
	for(unsigned int p=timeTableD2[999],k=0;p<timeTableD2[999+maxDelay_+1];p++,k++) {
		firingTableD2[k]=firingTableD2[p];
	}

	for(int i=0; i < maxDelay_; i++) {
		timeTableD2[i+1] = timeTableD2[1000+i+1]-timeTableD2[1000];
	}

	timeTableD1[maxDelay_] = 0;

	/* the code of weight update has been moved to CpuSNN::updateWeights() */

	spikeCountAllHost	+= spikeCountAll1secHost;
	spikeCountD2Host += (secD2fireCntHost-timeTableD2[maxDelay_]);
	spikeCountD1Host += secD1fireCntHost;

	secD1fireCntHost  = 0;
	spikeCountAll1secHost = 0;
	secD2fireCntHost = timeTableD2[maxDelay_];

	for (int i=0; i < numGrp; i++) {
		grp_Info[i].FiringCount1sec=0;
	}
}

// updates simTime, returns true when new second started
bool CpuSNN::updateTime() {
	bool finishedOneSec = false;

	// done one second worth of simulation
	// update relevant parameters...now
	if(++simTimeMs == 1000) {
		simTimeMs = 0;
		simTimeSec++;
		finishedOneSec = true;
	}

	simTime++;
	if(simTime >= MAX_SIMULATION_TIME){
        // reached the maximum limit of the simulation time using 32 bit value...
        KERNEL_WARN("Maximum Simulation Time Reached...Resetting simulation time");
	}

	return finishedOneSec;
}


void CpuSNN::updateSpikeMonitor(int grpId) {
	// don't continue if no spike monitors in the network
	if (!numSpikeMonitor)
		return;

	if (grpId==ALL) {
		for (int g=0; g<numGrp; g++)
			updateSpikeMonitor(g);
	} else {
		// update spike monitor of a specific group

		// find index in spike monitor arrays
		int monitorId = grp_Info[grpId].SpikeMonitorId;

		// don't continue if no spike monitor enabled for this group
		if (monitorId<0)
			return;

		// find last update time for this group
		SpikeMonitorCore* spkMonObj = spikeMonCoreList[monitorId];
		int64_t lastUpdate = spkMonObj->getLastUpdated();

		// don't continue if time interval is zero (nothing to update)
		if ( ((int64_t)getSimTime()) - lastUpdate <=0)
			return;

		if ( ((int64_t)getSimTime()) - lastUpdate > 1000)
			KERNEL_ERROR("updateSpikeMonitor(grpId=%d) must be called at least once every second",grpId);

        // AER buffer max size warning here.
        // Because of C++ short-circuit evaluation, the last condition should not be evaluated
        // if the previous conditions are false.
        if (spkMonObj->getAccumTime() > LONG_SPIKE_MON_DURATION \
                && this->getGroupNumNeurons(grpId) > LARGE_SPIKE_MON_GRP_SIZE \
                && spkMonObj->isBufferBig()){
            // change this warning message to correct message
            KERNEL_WARN("updateSpikeMonitor(grpId=%d) is becoming very large. (>%lu MB)",grpId,(int64_t) MAX_SPIKE_MON_BUFFER_SIZE/1024 );// make this better
            KERNEL_WARN("Reduce the cumulative recording time (currently %lu minutes) or the group size (currently %d) to avoid this.",spkMonObj->getAccumTime()/(1000*60),this->getGroupNumNeurons(grpId));
		}
#ifndef __CPU_ONLY__
		if (simMode_ == GPU_MODE) {
			// copy the neuron firing information from the GPU to the CPU..
			copyFiringInfo_GPU();
		}
#endif

		// find the time interval in which to update spikes
		// usually, we call updateSpikeMonitor once every second, so the time interval is [0,1000)
		// however, updateSpikeMonitor can be called at any time t \in [0,1000)... so we can have the cases
		// [0,t), [t,1000), and even [t1, t2)
		int numMsMin = lastUpdate%1000; // lower bound is given by last time we called update
		int numMsMax = getSimTimeMs(); // upper bound is given by current time
		if (numMsMax==0)
			numMsMax = 1000; // special case: full second
		assert(numMsMin<numMsMax);

		// current time is last completed second in milliseconds (plus t to be added below)
		// special case is after each completed second where !getSimTimeMs(): here we look 1s back
		int currentTimeSec = getSimTimeSec();
		if (!getSimTimeMs())
			currentTimeSec--;

		// save current time as last update time
		spkMonObj->setLastUpdated( (int64_t)getSimTime() );

		// prepare fast access
		FILE* spkFileId = spikeMonCoreList[monitorId]->getSpikeFileId();
		bool writeSpikesToFile = spkFileId!=NULL;
		bool writeSpikesToArray = spkMonObj->getMode()==AER && spkMonObj->isRecording();

		// Read one spike at a time from the buffer and put the spikes to an appopriate monitor buffer. Later the user
		// may need need to dump these spikes to an output file
		for (int k=0; k < 2; k++) {
			unsigned int* timeTablePtr = (k==0)?timeTableD2:timeTableD1;
			unsigned int* fireTablePtr = (k==0)?firingTableD2:firingTableD1;
			for(int t=numMsMin; t<numMsMax; t++) {
				for(unsigned int i=timeTablePtr[t+maxDelay_]; i<timeTablePtr[t+maxDelay_+1];i++) {
					// retrieve the neuron id
					int nid   = fireTablePtr[i];
					if (simMode_ == GPU_MODE)
						nid = GET_FIRING_TABLE_NID(nid);
					assert(nid < numN);

					// make sure neuron belongs to currently relevant group
					int this_grpId = grpIds[nid];
					if (this_grpId != grpId)
						continue;

					// adjust nid to be 0-indexed for each group
					// this way, if a group has 10 neurons, their IDs in the spike file and spike monitor will be
					// indexed from 0..9, no matter what their real nid is
					nid -= grp_Info[grpId].StartN;
					assert(nid>=0);

					// current time is last completed second plus whatever is leftover in t
					int time = currentTimeSec*1000 + t;

					if (writeSpikesToFile) {
						int cnt;
						cnt = fwrite(&time, sizeof(int), 1, spkFileId); assert(cnt==1);
						cnt = fwrite(&nid,  sizeof(int), 1, spkFileId); assert(cnt==1);
					}

					if (writeSpikesToArray) {
						spkMonObj->pushAER(time,nid);
					}
				}
			}
		}

		if (spkFileId!=NULL) // flush spike file
			fflush(spkFileId);
	}
}

// This function updates the synaptic weights from its derivatives..
void CpuSNN::updateWeights() {
	// at this point we have already checked for sim_in_testing and sim_with_fixedwts
	assert(sim_in_testing==false);
	assert(sim_with_fixedwts==false);

	// update synaptic weights here for all the neurons..
	for(int g = 0; g < numGrp; g++) {
		// no changable weights so continue without changing..
		if(grp_Info[g].FixedInputWts || !(grp_Info[g].WithSTDP))
			continue;

		for(int i = grp_Info[g].StartN; i <= grp_Info[g].EndN; i++) {
			assert(i < numNReg);
			unsigned int offset = cumulativePre[i];
			float diff_firing = 0.0;
			float homeostasisScale = 1.0;

			if(grp_Info[g].WithHomeostasis) {
				assert(baseFiring[i]>0);
				diff_firing = 1-avgFiring[i]/baseFiring[i];
				homeostasisScale = grp_Info[g].homeostasisScale;
			}

			if (i==grp_Info[g].StartN)
				KERNEL_DEBUG("Weights, Change at %lu (diff_firing: %f)", simTimeSec, diff_firing);

			for(int j = 0; j < Npre_plastic[i]; j++) {
				//	if (i==grp_Info[g].StartN)
				//		KERNEL_DEBUG("%1.2f %1.2f \t", wt[offset+j]*10, wtChange[offset+j]*10);
				float effectiveWtChange = stdpScaleFactor_ * wtChange[offset + j];
//				if (wtChange[offset+j])
//					printf("connId=%d, wtChange[%d]=%f\n",cumConnIdPre[offset+j],offset+j,wtChange[offset+j]);

				// homeostatic weight update
				// FIXME: check WithESTDPtype and WithISTDPtype first and then do weight change update
				switch (grp_Info[g].WithESTDPtype) {
				case STANDARD:
					if (grp_Info[g].WithHomeostasis) {
						wt[offset+j] += (diff_firing*wt[offset+j]*homeostasisScale + wtChange[offset+j])*baseFiring[i]/grp_Info[g].avgTimeScale/(1+fabs(diff_firing)*50);
					} else {
						// just STDP weight update
						wt[offset+j] += effectiveWtChange;
					}
					break;
				case DA_MOD:
					if (grp_Info[g].WithHomeostasis) {
						effectiveWtChange = cpuNetPtrs.grpDA[g] * effectiveWtChange;
						wt[offset+j] += (diff_firing*wt[offset+j]*homeostasisScale + effectiveWtChange)*baseFiring[i]/grp_Info[g].avgTimeScale/(1+fabs(diff_firing)*50);
					} else {
						wt[offset+j] += cpuNetPtrs.grpDA[g] * effectiveWtChange;
					}
					break;
				case UNKNOWN_STDP:
				default:
					// we shouldn't even be in here if !WithSTDP
					break;
				}

				switch (grp_Info[g].WithISTDPtype) {
				case STANDARD:
					if (grp_Info[g].WithHomeostasis) {
						wt[offset+j] += (diff_firing*wt[offset+j]*homeostasisScale + wtChange[offset+j])*baseFiring[i]/grp_Info[g].avgTimeScale/(1+fabs(diff_firing)*50);
					} else {
						// just STDP weight update
						wt[offset+j] += effectiveWtChange;
					}
					break;
				case DA_MOD:
					if (grp_Info[g].WithHomeostasis) {
						effectiveWtChange = cpuNetPtrs.grpDA[g] * effectiveWtChange;
						wt[offset+j] += (diff_firing*wt[offset+j]*homeostasisScale + effectiveWtChange)*baseFiring[i]/grp_Info[g].avgTimeScale/(1+fabs(diff_firing)*50);
					} else {
						wt[offset+j] += cpuNetPtrs.grpDA[g] * effectiveWtChange;
					}
					break;
				case UNKNOWN_STDP:
				default:
					// we shouldn't even be in here if !WithSTDP
					break;
				}

				// It is users' choice to decay weight change or not
				// see setWeightAndWeightChangeUpdate()
				wtChange[offset+j] *= wtChangeDecay_;

				// if this is an excitatory or inhibitory synapse
				if (maxSynWt[offset + j] >= 0) {
					if (wt[offset + j] >= maxSynWt[offset + j])
						wt[offset + j] = maxSynWt[offset + j];
					if (wt[offset + j] < 0)
						wt[offset + j] = 0.0;
				} else {
					if (wt[offset + j] <= maxSynWt[offset + j])
						wt[offset + j] = maxSynWt[offset + j];
					if (wt[offset+j] > 0)
						wt[offset+j] = 0.0;
				}
			}
		}
	}
}

// This function updates the synaptic weights from its derivatives for all the neurons in the neural map
void CpuSNN::updateWeightsAndShare() {
	// at this point we have already checked for sim_in_testing and sim_with_fixedwts
	assert(sim_in_testing==false);
	assert(sim_with_fixedwts==false);

	// update synaptic weights here for all the neurons..
	for(int g = 0; g < numGrp; g++) {
		// no changable weights so continue without changing..
		if(grp_Info[g].FixedInputWts || !(grp_Info[g].WithSTDP))
			continue;

		// check the neuron in this neural group that is changing and share this weight update
        int NeurMap = 0;
        unsigned int offset;
        float posWeightAbs = 0;
        float negWeightAbs = 0;
        int initPost = grp_Info[g].StartN;
        int endPost  = grp_Info[g].EndN;
        int sizeNeurMap = grp_Info[g].SizeX * grp_Info[g].SizeY;
        for(int i = grp_Info[g].StartN; i <= grp_Info[g].EndN-1; i++) {
            offset = cumulativePre[i];

            if (NeurMap == i/sizeNeurMap) {
                for(int j = 0; j < Npre_plastic[i]; j++) {
                    if (wtChange[offset + j] > posWeightAbs) {
                        posWeightAbs = wtChange[offset + j];
                    } else if ((wtChange[offset + j] < negWeightAbs)) {
                        negWeightAbs = wtChange[offset + j];
                    }
                }
            }

            if (NeurMap != (i+1)/sizeNeurMap) {

                // perform weight update for this neural maps
                float effectiveWtChange;
                if (abs(posWeightAbs) > abs(negWeightAbs)) {
                    effectiveWtChange = stdpScaleFactor_ * posWeightAbs;
                } else {
                    effectiveWtChange = stdpScaleFactor_ * negWeightAbs;
                }

                // set the limits for the loop over the postsynaptic neurons in this map
                endPost = i+1;

                for(int pN = initPost; pN < endPost; pN++) {

                    offset = cumulativePre[pN];

                    for(int j = 0; j < Npre_plastic[pN]; j++) {

                        // weight update
                        switch (grp_Info[g].WithESTDPtype) {
                            case STANDARD:
                                wt[offset+j] += effectiveWtChange;
                                break;
                            case UNKNOWN_STDP:
                            default:
                                // we shouldn't even be in here if !WithSTDP
                                break;
                        }

                        switch (grp_Info[g].WithISTDPtype) {
                            case STANDARD:
                                wt[offset+j] += effectiveWtChange;
                                break;
                            case UNKNOWN_STDP:
                            default:
                                // we shouldn't even be in here if !WithSTDP
                                break;
                        }

                        // if this is an excitatory or inhibitory synapse
                        if (maxSynWt[offset + j] >= 0) {
                            if (wt[offset + j] >= maxSynWt[offset + j])
                                wt[offset + j] = maxSynWt[offset + j];
                            if (wt[offset + j] < 0)
                                wt[offset + j] = 0.0;
                        } else {
                            if (wt[offset + j] <= maxSynWt[offset + j])
                                wt[offset + j] = maxSynWt[offset + j];
                            if (wt[offset+j] > 0)
                                wt[offset+j] = 0.0;
                        }
                    }
                }

                // initialize the variable and update counters
                NeurMap++;
                initPost = i+1;
                posWeightAbs = 0;
                negWeightAbs = 0;
            }
		}
	}
}