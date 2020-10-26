/**********************
 * Frequent pattern compression
 **********************/

#ifndef __NVMAIN_FPC_H__
#define __NVMAIN_FPC_H__

#include "src/DataEncoder.h"

namespace NVM {

class FPC : public DataEncoder {
public:
	FPC();
	~FPC();

	void SetConfig(Config *conf, bool createChildren=true);

	ncycle_t Read(NVMainRequest *req);
	ncycle_t Write(NVMainRequest *req);

	void RegisterStats();

private:
	size_t pattern_hit_counts[8];
	size_t raw_total_write_size;
	size_t compressed_total_write_size;
	float compress_percentage;
};

}
#endif

