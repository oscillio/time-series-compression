#include <vector>
#include "../lib/TimeSeriesCompression.h"
#include <iostream>
#include <assert.h>



using std::cout;
using std::cin;
using std::endl;



struct TestTimeValue
{
	oscill::io::TimeSeriesValue set;
	oscill::io::TimeSeriesValue expected;
};

struct TestTimeValues
{
	std::vector<TestTimeValue> values;
	double value_precision;
	int value_precision_decimal_places;
	int time_precision_log;
	double minimum; 
	double maximum;
};

int main()
{
	std::vector<uint8_t> test_buffer(1024);
	
	// 1000000 time stamp accuracy, 0.1f value accuracy
	std::vector<TestTimeValues> test_values{ 
		{
			// Vector of expected values
			{
				{
					// Set
					{ 1422568543702900123, 10.673 },
					// Expected
					{ 1422568543702900000, 10.6 }
				},
				{
					// Set
					{ 1422568543752950000, 10.6 },
					// Expected
					{ 1422568543753000000, 10.6 }
				}
			},
		// Value precision
		0.1f,
		// Value precision decimal places
		1,
		// Time Precision ^10
		5,
		// Minimum
		0.0f,
		// Maximum
		100.0f
		},
		{
			// Vector of expected values
			{
				{
					// Set
					{ 100, 10.673 },
					// Expected
					{ 100, 10.6 }
				},
				{
					// Set
					{ 50, 10.6 },
					// Expected
					{ 100, 10.6 }
				},
				{
					// Set
					{ 40, 10.6 },
					// Expected
					{ 0, 10.6 }
				},
				{
					// Set
					{ 300, 10.6 },
						// Expected
					{ 300, 10.6 }
				},
				{
					// Set
					{ 300, 10.6 },
					// Expected
					{ 300, 10.6 }
				},
				{
					// Set
					{ 400, 10.6 },
					// Expected
					{ 400, 10.6 }
				},
				{
					// Set
					{ 800, 10.6 },
					// Expected
					{ 800, 10.6 }
				},
				{
					// Set
					{ 800012, 10.6 },
					// Expected
					{ 800000, 10.6 }
				},
				{
					// Set
					{ 1422568543752950000, 10.6 },
					// Expected
					{ 1422568543752950000, 10.6 }
				}
			},
		// Value precision
		0.1f,
		// Value precision decimal places
		1,
		// Time Precision ^10
		2,
		// Minimum
		0.0f,
		// Maximum
		100.0f
		}

	};





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
	for (auto&& test : test_values)
	{

		oscill::io::SingleTimeSeriesWriteBuffer test_oscillio_write_buff(test.value_precision, test.value_precision_decimal_places, test.time_precision_log, test.minimum, test.maximum, 1024);

		for (auto&& value : test.values)
		{
			assert(test_oscillio_write_buff.AddValue(value.set) != false);
		}

		oscill::io::SingleTimeSeriesReadBuffer test_oscillio_read_buff(test_oscillio_write_buff);

		for (auto&& value : test.values)
		{
			oscill::io::TimeSeriesValue to_read;
			assert(test_oscillio_read_buff.ReadNext(&to_read) != false);
			assert(to_read.time == value.expected.time);
			assert(to_read.value == value.expected.value);
		}
	}

	//cout << "Normal " << test_size.size() * 64 << " bits" << std::endl;
	//cout << "Optimized " << test_oscillio_read_buff2.ByteCount() * 8 << " bits" << std::endl;

	return 0;
}