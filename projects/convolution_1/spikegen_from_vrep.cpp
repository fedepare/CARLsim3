#include <spikegen_from_vrep.h>

#include <carlsim.h>
#include <stdio.h>				// fopen, fread, fclose
#include <string.h>				// std::string
#include <assert.h>				// assert
#include <cstring>
#include <ctime>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <vector>
#include <sstream>
#include <iterator>

SpikeGeneratorFromVREP::SpikeGeneratorFromVREP(std::string fileName) {
	fileName_ = fileName;
	nNeur_ = 16384;

	// move unsafe operations out of constructor
	openFile();
}

SpikeGeneratorFromVREP::~SpikeGeneratorFromVREP() {}

void SpikeGeneratorFromVREP::openFile() {

	string line;
	ifstream file(fileName_);

	if (!file) {
        printf("There was an error opening the file.\n");
	}

	spikes_.clear();
	for (int i=0; i<nNeur_; i++) {
		spikes_.push_back(std::vector<int>());
	}

	int NeurId;
    while (getline(file, line)) {
    	istringstream iss(line);
 		int x, y, p, t;
 		if(!(iss >> x >> y >> p >> t)) {
 			printf("Error: parsing line could not be completed.\n");
		}

		NeurId = (127 - y) * 128 + x;
		spikes_[NeurId].push_back(t);
	}

	// reset all iterators
	spikesIt_.clear();
	for (int i=0; i<nNeur_; i++) {
		spikesIt_.push_back(spikes_[i].begin());
	}	
}

unsigned int SpikeGeneratorFromVREP::nextSpikeTime(CARLsim* sim, int grpId, int nid, unsigned int currentTime, 
	unsigned int lastScheduledSpikeTime, unsigned int endOfTimeSlice) {
	assert(nNeur_>0);
	assert(nid < nNeur_);

	if (spikesIt_[nid] != spikes_[nid].end()) {
		// if there are spikes left in the vector ...

		if (*(spikesIt_[nid]) < endOfTimeSlice) {
			// ... and if the next spike time is in the current scheduling time slice:
			// return the next spike time and update iterator
			return (unsigned int)(*(spikesIt_[nid]++));
		}
	}

	// if the next spike time is not a valid number, return a large positive number instead
	// this will signal CARLsim to break the nextSpikeTime loop
	return -1; // large positive number
}
