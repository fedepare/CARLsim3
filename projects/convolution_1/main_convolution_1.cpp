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

// class for the synapse connection between the input and the first convolutional layer
class InputC1 : public ConnectionGenerator {
public:

	InputC1(int idxVector[9][32*32*8]) {

		for (int rows = 0; rows < 32*32*8; rows++) {
			for (int cols = 0; cols < 9; cols++) {
				idxVector_[cols][rows] = idxVector[cols][rows];
			}
		}

	}

	// connection function, connect neuron i in scrGrp to neuron j in destGrp
	void connect(CARLsim* net, int srcGrp, int i, int destGrp, int j, float& weight, float& maxWt, float& delay, bool& connected) {

		// get the location of the pre- and postsynaptic neurons
		Point3D pre  = net->getNeuronLocation3D(srcGrp, i);  // .x, .y, .z
		Point3D post = net->getNeuronLocation3D(destGrp, j); // .x, .y, .z

		// generate the connection
		if (pre.x == post.x && pre.y == post.y) {
			connected = true;
			delay  = 1;
			weight = 1;
			maxWt  = 1;
		} else connected = false;
	}

private:
	int idxVector_[9][32*32*8];
};


int main() {

	// ---------------- CONFIG STATE -------------------

	// simulation
    CARLsim sim("Convolution Example", CPU_MODE, USER);

    // input parameters
    int sideInput = 32;
	int nNMInput  = 1;

	// convolutional layer parameters
	int sideOutput = 15;
	int nNMOutput  = 8;

    // input
    VisualStimulus stim("input/inpGratingPlaid_gray_32x32x61.dat");
    int g0 = sim.createSpikeGeneratorGroup("Input", Grid3D(sideInput,sideInput,nNMInput), EXCITATORY_NEURON);

    // convolutional layer
    int nNeurMaps = 8;
    int g1 = sim.createGroup("g1", Grid3D(sideOutput,sideOutput,nNMOutput), EXCITATORY_NEURON);
    sim.setNeuronParameters(g1, 0.02f, 0.2f, -65.0f, 8.0f);

    // generate connections (Input - C1)
	int aux;
	int pNcnt  = 0, pNcnt_ = 0;
	int cntCol = 0, cntRow = 0;
    int idxVector[9][sideOutput*sideOutput*nNMOutput] = {{ 0 }};
	int cntVector[sideOutput*sideOutput*nNMOutput]    = { 0 };

	for (int rows = 0; rows < sideInput; rows++) {
		for (int cols = 0; cols < sideInput; cols++) {

			if (cntCol < 3) {
				for (int nMaps = 0; nMaps < nNMOutput; nNMOutput++) {
					aux = pNcnt + nMaps*sideOutput*sideOutput;
					idxVector[cntVector[aux]][aux] = rows * sideInput + cols;
					cntVector[aux]++;
				}
				cntCol++;
			} else {
				cntCol = 0;
				pNcnt++;
			}
		}

		// update counters
		if (cntRow < 3) {
			cntRow++;
			pNcnt = pNcnt_;
		} else {
			cntRow = 0;
			pNcnt_ = pNcnt;
		}
	}

	// update the connection class
	InputC1(idxVector);

    // user-defined synaptic connections
    InputC1* UDconnIC1;
    sim.connect(g0, g1, UDconnIC1, SYN_PLASTIC);
    sim.setConductances(false);

    // ---------------- SETUP STATE -------------------
	sim.setupNetwork();
	sim.setSpikeMonitor(g0, "DEFAULT");
	sim.setSpikeMonitor(g1, "DEFAULT");

    // ---------------- RUN STATE -------------------
}