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


int main() {
	

	// ---------------- CONFIG STATE -------------------;

	VisualStimulus VS("inpGratingPlaid_gray_32x32x61.dat");
	int videoLength = VS.getStimulusLength();

	CARLsim sim("VisualStimulus Example",CPU_MODE,USER);
	int g0 = sim.createSpikeGeneratorGroup("VSinput", Grid3D(32,32,1), EXCITATORY_NEURON);

	int gOut = sim.createGroup("output", 1, EXCITATORY_NEURON);
	sim.setNeuronParameters(gOut, 0.02f, 0.2f, -65.0f, 8.0f);

	sim.connect(g0, gOut, "full", RangeWeight(0.5f), 1.0f);


	// ---------------- SETUP STATE -------------------

	sim.setupNetwork();

	// ---------------- RUN STATE -------------------

	for (int i=0; i<videoLength; i++) {
		PoissonRate * rates = VS.readFrame(50.0f); // grayscale value 255 will be mapped to 50 Hz
		sim.setSpikeRate(g0, rates); // for this to work, there must be 128x128=16384 neurons in g0
		sim.runNetwork(1,0); // run the network
	}

	
	return 0;
}
