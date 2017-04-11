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

	// connection function, connect neuron i in scrGrp to neuron j in destGrp
	void connect(CARLsim* net, int srcGrp, int i, int destGrp, int j, float& weight, float& maxWt, float& delay,
		bool& connected) {

		connected = true;
		delay = 1;
		weight = i*1;
		maxWt = 1;
	}
};

int main() {

    // config some network
    CARLsim sim("example", CPU_MODE, USER);
    int g0 = sim.createGroup("g0", 10, EXCITATORY_NEURON);
    int g1 = sim.createGroup("g1", 10, EXCITATORY_NEURON);

    InputC1* UDconnIC1;
    sim.connect(g0, g1, UDconnIC1, SYN_PLASTIC);
}