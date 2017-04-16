/*
 * Copyright (c) 2016 Regents of the University of California. All rights reserved.
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
 */

// include CARLsim user interface
#include <carlsim.h>
#include <cstring>
#include <ctime>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <iterator>

// input parameters
#define SIDE_INPUT 128
#define NM_INPUT 1

// convolutional layer parameters
#define SIDE_OUTPUT 32
#define NM_OUTPUT 8

// receptive field
#define SIDE_RF 4
#define SIZE_RF 16

using namespace std;


// class for the synapse connection between the input and the first convolutional layer
class vrepSpikes : public SpikeGenerator {
public:

	// constructor
	vrepSpikes(void) {

		printf("fede\n");

		string line;
		ifstream file("DVS_dataset.txt");

		if (!file) {
	        printf("There was an error opening the file.\n");
	    }

	    vector<int> Row;

	    int cnt = 0;
	    while (getline(file, line)) {
	    	istringstream iss(line);
	 		int a, b, c, d;
	 		if(!(iss >> a >> b >> c >> d)) {
	 			printf("Error: parsing line could not be completed.\n");
			}
			Row.push_back(a); // pixel X
			Row.push_back(b); // pixel Y
			Row.push_back(c); // polarity
			Row.push_back(d); // time (ms)
			DVSdataset.push_back(Row);
			printf("fede\n");
			Row.clear();
    	}
	}

	// destructor
	~vrepSpikes() {};

	unsigned int nextSpikeTime(CARLsim* sim, int grpId, int nid, unsigned int currentTime, unsigned int lastScheduledSpikeTime) {
        return (lastScheduledSpikeTime + 100);
	}

private:
	std::vector< vector<int> > DVSdataset;
};

// class for the synapse connection between the input and the first convolutional layer
class InputC1 : public ConnectionGenerator {
public:

	// initalize connection array
	InputC1(void) {

		// generate connections (Input - C1)
		int aux;
		int pNcnt  = 0, pNcnt_ = 0;
		int cntCol = 0, cntRow = 0;
		int cntVector[SIDE_OUTPUT*SIDE_OUTPUT*NM_OUTPUT] = { 0 };

		for (int rows = 0; rows < SIDE_INPUT; rows++) {
			for (int cols = 0; cols < SIDE_INPUT; cols++) {

					for (int nMaps = 0; nMaps < NM_OUTPUT; nMaps++) {
						aux = pNcnt + nMaps*SIDE_OUTPUT*SIDE_OUTPUT;
						idxVector[cntVector[aux]][aux] = rows * SIDE_INPUT + cols;
						cntVector[aux]++;
					}

					cntCol++;

					if (cntCol == SIDE_RF) {
						cntCol = 0;
						pNcnt++;
					}
			}

			cntRow++;

			// update counters
			if (cntRow == SIDE_RF) {
				cntCol = 0;
				cntRow = 0;
				pNcnt_ = pNcnt;
			} else {
				cntCol = 0;
				pNcnt = pNcnt_;
			}
		}

		// generate uniformly distributed weights for the neural maps
		srand(time(0));
		for (int nMaps = 0; nMaps < NM_OUTPUT; nMaps++) {
			weightsC1[nMaps] = rand()/float(RAND_MAX);
		}
	}

	// connection function, connect neuron i in scrGrp to neuron j in destGrp
	void connect(CARLsim* net, int srcGrp, int i, int destGrp, int j, float& weight, float& maxWt, float& delay, bool& connected) {

		// check connections
		bool exists = false;
		for (int row = 0; row < SIZE_RF; row++) {
			if (idxVector[row][j] == i) {
				exists = true;
				break;
		 	}
		}

		// generate connections
		if (exists) {
			connected = true;
			delay  = 1;
			weight = weightsC1[j%(SIDE_OUTPUT*SIDE_OUTPUT)];
			maxWt  = 1;
		} connected = false;
	}

private:
	float weightsC1[NM_OUTPUT];
	int idxVector[SIZE_RF][SIDE_OUTPUT*SIDE_OUTPUT*NM_OUTPUT];
};


// class for the synapse connection between the input and the first convolutional layer
class latInhC1 : public ConnectionGenerator {
public:

	// initalize connection array
	latInhC1(void) {

		int aux;
		int cnt;
		int normIdx;

		// generate inhibitory connections (C1inh - C1exc)
		for (int nIdx = 0; nIdx < SIDE_OUTPUT*SIDE_OUTPUT*NM_OUTPUT; nIdx++) {

			cnt = 0;
			aux = nIdx % (SIDE_OUTPUT*SIDE_OUTPUT);
			normIdx = nIdx - aux * SIDE_OUTPUT * SIDE_OUTPUT;

			for (int nMaps = 0; nMaps < NM_OUTPUT; nMaps++) {
				if (aux != nMaps) {
					idxVector[cnt][nIdx] = normIdx + nMaps * SIDE_OUTPUT * SIDE_OUTPUT;
					cnt++;
				}
			}
		}
	}

	// // connection function, connect neuron i in scrGrp to neuron j in destGrp
	// void connect(CARLsim* net, int srcGrp, int i, int destGrp, int j, float& weight, float& maxWt, float& delay, bool& connected) {

	// 	// check connections
	// 	bool exists = false;
	// 	for (int row = 0; row < SIZE_RF; row++) {
	// 		if (idxVector[row][j] == i) {
	// 			exists = true;
	// 			break;
	// 	 	}
	// 	}

	// 	// generate connections
	// 	if (exists) {
	// 		connected = true;
	// 		delay  = 1;
	// 		weight = weightsC1[j%(SIDE_OUTPUT*SIDE_OUTPUT)];
	// 		maxWt  = 1;
	// 	} connected = false;
	// }

private:
	int idxVector[NM_OUTPUT - 1][SIDE_OUTPUT*SIDE_OUTPUT*NM_OUTPUT];
};


int main() {

	// ---------------- READ INPUTS FROM V-REP SIMULATOR -------------------

	

	// ---------------- CONFIG STATE -------------------

	// simulation
    CARLsim sim("Convolution Example", CPU_MODE, USER);

    // input
    VisualStimulus stim("input/inpGratingPlaid_gray_32x32x61.dat");
    int g0 = sim.createSpikeGeneratorGroup("Input", Grid3D(SIDE_INPUT, SIDE_INPUT, NM_INPUT), EXCITATORY_NEURON);

    // convolutional layer
    int g1exc = sim.createGroup("Conv 1 Exc", Grid3D(SIDE_OUTPUT, SIDE_OUTPUT, NM_OUTPUT), EXCITATORY_NEURON);
    sim.setNeuronParameters(g1exc, 0.02f, 0.2f, -65.0f, 8.0f);

    int g1inh = sim.createGroup("Conv 1 Inh", Grid3D(SIDE_OUTPUT, SIDE_OUTPUT, NM_OUTPUT), INHIBITORY_NEURON);
    sim.setNeuronParameters(g1exc, 0.02f, 0.2f, -65.0f, 8.0f);

    // connection Input-C1
    InputC1* connIC1exc;
    connIC1exc = new InputC1();    
    sim.connect(g0, g1exc, connIC1exc, SYN_PLASTIC);

    InputC1* connIC1inh;
    connIC1inh = new InputC1();    
    sim.connect(g0, g1inh, connIC1inh, SYN_PLASTIC);

    sim.setConductances(false);

    // when are the weights updated
    sim.setWeightAndWeightChangeUpdate(INTERVAL_10MS, true, 0.9f);

    // ---------------- SETUP STATE -------------------

    // ---------------- RUN STATE -------------------
}