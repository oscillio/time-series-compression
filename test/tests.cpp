#include <vector>
#include "../lib/TimeSeriesCompression.h"
#include <iostream>
#include <assert.h>
#include <random>

#define BUFFER_SIZE 65535

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
	int value_precision_decimal_places;
	int time_precision_log;
	double minimum; 
	double maximum;
};

int main()
{
	std::vector<uint8_t> test_buffer(1024);
	
	// 1000000 time stamp accuracy, 0.1f value accuracy
	std::vector<TestTimeValues> test_values
	{
		{
			// Vector of expected values
			{
				{
					// Set
					{ 1000000, 10.673 },
					// Expected
					{ 1000000, 10.0 }
				},
				{
					// Set
					{ 500000, 239.788 },
					// Expected
					{ 500000, 230.0 }
				}
			},
		// Value precision decimal places
		-1,
		// Time Precision ^10
		5,
		// Minimum
		-1000.0f,
		// Maximum
		1000.0f
		},
		{
			// Vector of expected values
			{
				{
					// Set
					{ 1422568543702900123, 10.673 },
					// Expected
					{ 1422568543702900000, 10.673 }
				},
				{
					// Set
					{ 1422568543752950000, 230319.788 },
					// Expected
					{ 1422568543753000000, 230319.788 }
				},
				{
					// Set
					{ 1422568543752950001, -68710.714987991407 },
					// Expected
					{ 1422568543753000000, -68710.714 }
				}
			},
		// Value precision decimal places
		3,
		// Time Precision ^10
		5,
		// Minimum
		-250000.0f,
		// Maximum
		250000.0f
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
	
	// Push some randomness into the test vectors
	

	std::random_device rd;
	std::mt19937_64 gen(rd());

	/* This is where you define the number generator for unsigned long long: */
	std::uniform_int_distribution<unsigned long long> dis;
	std::uniform_int_distribution<int> dis2(0, 1000000);

	for (int i = 0; i < 100; i++)
	{
		TestTimeValues random_to_add;
		random_to_add.maximum = dis2(gen);
		random_to_add.minimum = random_to_add.maximum - dis2(gen);
		random_to_add.time_precision_log = 0;
		random_to_add.value_precision_decimal_places = 3;
		std::uniform_real_distribution<double> fis(random_to_add.minimum - 100, random_to_add.maximum + 100);

		for (int j = 0; j < 100; j++)
		{
			TestTimeValue test_to_add;
			test_to_add.expected.time = test_to_add.set.time = dis(gen);
			test_to_add.set.value = test_to_add.expected.value = fis(gen);
			if (test_to_add.set.value > random_to_add.maximum)
			{
				test_to_add.expected.value = random_to_add.maximum;
			}
			if (test_to_add.set.value < random_to_add.minimum)
			{
				test_to_add.expected.value = random_to_add.minimum;
			}
			if (test_to_add.expected.value < 0)
			{
				int x = 5;
			}
			int64_t temp = (int64_t)(test_to_add.expected.value * pow(10, random_to_add.value_precision_decimal_places));
			test_to_add.expected.value = temp / pow(10, random_to_add.value_precision_decimal_places);
			random_to_add.values.push_back(test_to_add);
		}
		test_values.push_back(random_to_add);
	}

	oscill::io::WriteByteBuffer test_write_buff(test_buffer.data(), test_buffer.size());
	

	int counter = 0;
	uint64_t val;
	test_write_buff.WriteBits(123, 18);


	oscill::io::ReadByteBuffer test_oscill_read_buff(test_write_buff.RawData(), (18/8)+1);
	test_oscill_read_buff.ReadNextBits(&val, 18);
	assert(val == 123);
	assert(test_oscill_read_buff.ReadNextBits(&val, 18) == false);
	// Test the corner cases of the time series buffer

	// Add a whole bunch of random times and values into the time series buffer 
	for (auto&& test : test_values)
	{

		oscill::io::SingleTimeSeriesWriteBuffer test_oscillio_write_buff(test.value_precision_decimal_places, test.time_precision_log, test.minimum, test.maximum, BUFFER_SIZE);

		for (auto&& value : test.values)
		{
			assert(test_oscillio_write_buff.AddValue(value.set) != false);
		}
	
		oscill::io::SingleTimeSeriesReadBuffer test_oscillio_read_buff(test_oscillio_write_buff);

		for (auto&& value : test.values)
		{
			oscill::io::TimeSeriesValue to_read;
			assert(test_oscillio_read_buff.ReadNext(&to_read) != false);
			if (to_read.time != value.expected.time)
			{
				assert(to_read.time == value.expected.time);
			}
			if (to_read.value != value.expected.value)
			{
				assert(to_read.value == value.expected.value);
			}
			
		}
		
	}

	

	return 0;
}