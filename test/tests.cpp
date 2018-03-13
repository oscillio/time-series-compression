#include <vector>
#include "../lib/TimeSeriesCompression.h"
#include <iostream>
#include <assert.h>


#define TEST_FIRST_VALUE 10.673
#define EXPECTED_FIRST_VALUE 10.6
#define TEST_SECOND_VALUE 10.6
#define EXPECTED_SECOND_VALUE 10.6
#define TEST_THIRD_VALUE 10.7
#define EXPECTED_THIRD_VALUE 10.7
#define TEST_FOURTH_VALUE 90.8123
#define EXPECTED_FOURTH_VALUE 90.8
#define TEST_FIFTH_VALUE 90
#define EXPECTED_FIFTH_VALUE 90

using std::cout;
using std::cin;
using std::endl;


struct TestTimeValue
{
	oscill::io::TimeSeriesValue set;
	oscill::io::TimeSeriesValue expected;
};

int main()
{
	std::vector<uint8_t> test_buffer(1024);
	
	std::vector<TestTimeValue> test_values{ 
		{
			// Set
			{ 1422568543702900001, 10.673},
			// Expected
			{ 1422568543700000000, 10.6}
		},
		{
			// Set
			{1422568543752900002, 10.6},
			// Expected
			{1422568543750000000, 10.6}
		}
	};

	std::vector<double> test_size{ 10.123, 10.124, 10.125, 10.126, 10.127, 10.128, 10.129, 10.23, 10.31 };

	oscill::io::WriteByteBuffer test_oscill_write_buff(test_buffer.data(), test_buffer.size());
	

	int counter = 0;
	uint64_t val;
	test_oscill_write_buff.WriteBits(123, 18);


	oscill::io::ReadByteBuffer test_oscill_read_buff(test_oscill_write_buff.RawData(), (18/8)+1);
	test_oscill_read_buff.ReadNextBits(&val, 18);
	assert(val == 123);
	assert(test_oscill_read_buff.ReadNextBits(&val, 18) == false);
	// Test the corner cases of the time series buffer

	// Add a whole bunch of random times and values into the time series buffer 

	oscill::io::SingleTimeSeriesWriteBuffer test_oscillio_write_buff(0.1f, 1, 2, 0.0f, 100.0f, 1024);

	for (auto&& value : test_values)
	{
		assert(test_oscillio_write_buff.AddValue(value.set) != false);
	}

	oscill::io::SingleTimeSeriesReadBuffer test_oscillio_read_buff(test_oscillio_write_buff);

	for (auto&& value : test_values)
	{
		oscill::io::TimeSeriesValue to_read;
		assert(test_oscillio_read_buff.ReadNext(&to_read) != false);
		assert(to_read.time == value.expected.time);
		assert(to_read.value == value.expected.value);
	}

	for (int i = 0; i < 5; i++)
	{
		double to_read = 0.0;
		//assert(test_oscillio_read_buff.ReadNextValue(&to_read) == true);
		//assert(to_read == expected_values[i]);
	}

	//oscill::io::SingleTimeSeriesWriteBuffer test_oscillio_write_buff2(0.1f, 1, 0.0f, 100.0f, 1024);

	for (int i = 0; i < test_size.size(); i++)
	{
		//assert(test_oscillio_write_buff2.AddValue(test_size[i]) != false);
	}

	//oscill::io::SingleTimeSeriesReadBuffer test_oscillio_read_buff2(test_oscillio_write_buff2);

	for (int i = 0; i < test_size.size(); i++)
	{
		double to_read = 0.0;
	//	assert(test_oscillio_read_buff2.ReadNextValue(&to_read) == true);
		//cout << to_read << std::endl;
	}

	//cout << "Normal " << test_size.size() * 64 << " bits" << std::endl;
	//cout << "Optimized " << test_oscillio_read_buff2.ByteCount() * 8 << " bits" << std::endl;

	return 0;
}