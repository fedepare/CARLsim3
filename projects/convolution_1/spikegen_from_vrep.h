#include <callback.h>
#include <string>
#include <vector>


class CARLsim;

class SpikeGeneratorFromVREP : public SpikeGenerator {
public:
	/*!
	 * \brief SpikeGeneratorFromFile constructor
	 *
	 * \param[in] fileName file name of spike file (must be created from SpikeMonitor)
	 */
	SpikeGeneratorFromVREP(std::string fileName);

	//! SpikeGeneratorFromFile destructor
	~SpikeGeneratorFromVREP();

	/*!
	 * \brief schedules the next spike time
	 *
	 * This function schedules the next spike time, given the currentTime and the lastScheduledSpikeTime. It implements
	 * the virtual function of the base class.
	 * \param[in] sim pointer to a CARLsim object
	 * \param[in] grpId current group ID for which to schedule spikes
	 * \param[in] nid current neuron ID for which to schedule spikes
	 * \param[in] currentTime current time (ms) at which spike scheduler is called
	 * \param[in] lastScheduledSpikeTime the last time (ms) at which a spike was scheduled for this nid, grpId
	 * \param[in] endOfTimeSlice the end of the current scheduling time slice (ms). A spike delivered at a time
	 *                           >= endOfTimeSlice will not be scheduled by CARLsim
	 * \returns the next spike time (ms)
	 */
	unsigned int nextSpikeTime(CARLsim* sim, int grpId, int nid, unsigned int currentTime, 
		unsigned int lastScheduledSpikeTime, unsigned int endOfTimeSlice);

private:
	void openFile();

	std::string fileName_;		//!< file name

	//! A 2D vector of spike times, first dim=neuron ID, second dim=spike times.
	//! This makes it easy to keep track of which spike needs to be scheduled next, by maintaining
	//! a vector of iterators.
	std::vector< std::vector<int> > spikes_;

	//! A vector of iterators to easily keep track of which spike to schedule next (per neuron)
	std::vector< std::vector<int>::iterator > spikesIt_;

	int nNeur_;                 //!< number of neurons in the group
};

#endif