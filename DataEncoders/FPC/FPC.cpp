#include "DataEncoders/FPC/FPC.h"

using namespace NVM;

FPC::FPC()
{
	for (int i = 0; i < 8; i++)
		this->pattern_hit_counts[i] = 0;
	raw_total_write_size = 0;
	compressed_total_write_size = 0;
	compress_percentage = 100.0;
}

FPC::~FPC()
{

}

void FPC::SetConfig(Config *conf, bool)
{
	Params *params = new Params();
	params->SetParams(conf);
	SetParams(params);
}

void FPC::RegisterStats()
{
	AddStat(pattern_hit_counts[0]);
	AddStat(pattern_hit_counts[1]);
	AddStat(pattern_hit_counts[2]);
	AddStat(pattern_hit_counts[3]);
	AddStat(pattern_hit_counts[4]);
	AddStat(pattern_hit_counts[5]);
	AddStat(pattern_hit_counts[6]);
	AddStat(pattern_hit_counts[7]);
	AddStat(raw_total_write_size);
	AddStat(compressed_total_write_size);
	AddStat(compress_percentage);
}

ncycle_t FPC::Read( NVMainRequest* /*request*/ )
{
	/* TODO
	 */

	return 0;
}

ncycle_t FPC::Write(NVMainRequest *req)
{
	/* TODO
	 * x compression stats
	 * - actual data encoding
	 *   x calculate encoded data
	 *   - write encoded data
	 *   - addressable data encoding
	 * - write prefix / tag somewhere
	 * - cycle count
	 */
	NVMDataBlock &new_data = req->data;
	//NVMDataBlock &existing_data = req->oldData;
	const auto new_data_range = new_data.GetSize();

	//NVMDataBlock compressed_data;
	//// HACK: size of compressed data \leq raw new data
	//compressed_data.rawData = new uint8_t[new_data_range];
	//compressed_data.isValid = true;
	//// NOTE: member .size to be assigned later, after compression

	typedef int32_t word_t;	// back to 32-bit days :)
	typedef int16_t halfword_t;
	typedef int8_t byte_t;
	//word_t *pnew = (word_t*)(new_data.rawData);
	//byte_t *pcom = compressed_data.rawData;

	size_t offset_new = 0;	// in words
	size_t compressed_size = 0;	// in bytes
	while (offset_new < new_data_range) {
		word_t word = ((word_t*)new_data.rawData)[offset_new];

		halfword_t high = (halfword_t)(word >> 16), low = (halfword_t)(word & 0xffff);
		halfword_t hs = high >> 8, ls = low >> 8;
		bool hp = hs == -1 || hs == 0, lp = ls == -1 || ls == 0;
		byte_t *pbyte = (byte_t*)(&word);

		/* case 000 - zero run */
		if (word == 0) {
			pattern_hit_counts[0]++;
			// get zero run travel length (max 8)
			int run_length = 1;	// HACK: run_length == 0 already checked, start from 1
			while (run_length < 8 && offset_new + run_length < new_data_range) {
				if (new_data.rawData[offset_new + run_length] == 0) {
					run_length++;
				}
				else {
					break;
				}
			}

			word = (word_t)(run_length - 1);

			offset_new += run_length;
			compressed_size += 1;	// upper(3 bits)
		}

		/* case 001 - 4-bit sign-extended */
		else if (word >> 3 == -1 || word >> 3 == 0) {
			pattern_hit_counts[1]++;

			word = (word_t)(word & 0xf);

			offset_new += 1;
			compressed_size += 1;	// upper(4 bits)
		}

		/* case 010 - one byte sign-extended */
		else if (word >> 7 == -1 || word >> 7 == 0) {
			pattern_hit_counts[2]++;

			word = (word_t)(word & 0xff);

			offset_new += 1;
			compressed_size += 1;	// 8 bits
		}

		/* case 011 - halfword sign-extended */
		else if (word >> 15 == -1 || word >> 15 == 0) {
			pattern_hit_counts[3]++;

			word = (word_t)(word & 0xffff);

			offset_new += 1;
			compressed_size += 2;	// 16 bits
		}

		/* case 100 - halfword padded with a zero halfword */
		else if (word << 16 == 0) {
			pattern_hit_counts[4]++;

			word = (word_t)(word >> 16);

			offset_new += 1;
			compressed_size += 2;	// 16 bits
		}

		/* case 101 - two halfwords, each a byte sign-extended */
		else if (hp && lp) {
			pattern_hit_counts[5]++;

			word = (word_t)(((halfword_t)high << 8) | (low & 0xff));

			offset_new += 1;
			compressed_size += 2;	// 16 bits
		}

		/* case 110 - word consisting of repeated bytes */
		else if (pbyte[0] == pbyte[1] && pbyte[0] == pbyte[2]
				&& pbyte[0] == pbyte[3]) {
			pattern_hit_counts[6]++;

			word = (word_t)(word & 0xff);

			offset_new += 1;
			compressed_size += 1;	// 8 bits
		}

		/* case 111 - uncompressed word */
		else {
			pattern_hit_counts[7]++;

			//word = (word_t)word;

			offset_new += 1;
			compressed_size += 4;	// uncompressed 32 bits
		}
	}

	//compressed_data.size = compressed_size;

	raw_total_write_size += new_data.GetSize();
	compressed_total_write_size += compressed_size;
	compress_percentage = (float)compressed_total_write_size / raw_total_write_size * 100.0;

	return 0;
}

