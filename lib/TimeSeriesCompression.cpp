#include "TimeSeriesCompression.h"
#include <assert.h>
#include <math.h>

// TODO - Get Git to update the minor version on checkin
#define OSCILLIO_TIME_COMPRESS_MAJOR_VERISON 0
#define OSCILLIO_TIME_COMPRESS_MINOR_VERSION 1

namespace oscill {
	namespace io {

		struct 
		{
  			int64_t max_delta;
			int delta_size;
  			uint32_t pattern;
			int pattern_size;
		} static const timestamp_encoding_info[4] = 
		{
			{0x3F, 7, 2, 2},
        	{0xFF, 9, 6, 3},
        	{0x7FF, 12, 14, 4},
        	{0x7FFFFFFF, 32, 30, 5}
		};
		
		// Helper Function to determine max bit size of value
		int NumberOfBits(int64_t max, int64_t min)
		{
			int num_bits = 0;
			uint64_t zeroed_value = (uint64_t)((max - min) );

			// Common algorithm. Keep dividing by 2 until we get 0.  A value of 0, will still take one bit to represent 
			do
			{
				zeroed_value /= 2;
				num_bits++;
			} while (zeroed_value != 0);

			return num_bits;
		}

		bool ByteBuffer::IncrementByte()
		{
			if (m_current_data_index >= m_size)
			{
				return false;
			}
			else
			{
				m_current_data_index++;
				m_remaining_bits_in_byte = 8;
				return true;
			}
		}

		bool WriteByteBuffer::WriteBits(uint64_t value, int num_bits)
		{
			// Check to make sure we can write that many bits into the buffer
			if (num_bits > m_num_bits_available) return false;

			// If the last add took up a whole byte, make sure we can get the next one.
			// We do this so that the last add to the buffer doesn't return false
			if (m_remaining_bits_in_byte == 0)
			{
				if (!IncrementByte()) return false;
			}

			// If we don't need more space than what's left in the current byte, use what we need
			if (num_bits <= m_remaining_bits_in_byte)
			{
				m_data[m_current_data_index] += (uint8_t)(value << (m_remaining_bits_in_byte - num_bits));
				m_remaining_bits_in_byte -= num_bits;
			}
			else
			{
				uint32_t bits_to_write = num_bits;

				// Fill up remainder of the current byte
				if (m_remaining_bits_in_byte > 0)
				{

					m_data[m_current_data_index] += (uint8_t)(value >> (bits_to_write - m_remaining_bits_in_byte));
					bits_to_write -= m_remaining_bits_in_byte;

					// Get the next byte in the buffer, return if we can't or full
					if (!IncrementByte()) return false;
				}

				// While we have more than a byte to copy, keep copying whole bytes into the buffer
				while (bits_to_write >= 8)
				{
					m_data[m_current_data_index] = (uint8_t)(value >> (bits_to_write - 8)) & 0xFF;
					bits_to_write -= 8;
					if (!IncrementByte()) return false;
				}

				// If we ended up with not a mutiple of 8, copy the remainder to the current byte
				if (bits_to_write > 0)
				{
					m_data[m_current_data_index] = (uint8_t)(value & ((1 << bits_to_write) - 1));
					// Shift to the left so next bits will be inserted after
					m_data[m_current_data_index] = m_data[m_current_data_index] << (8 - bits_to_write);
					m_remaining_bits_in_byte = 8 - bits_to_write;
				}

			}
			m_num_bits_available -= num_bits;
			return true;
		}
		bool ReadByteBuffer::ReadNextBits(uint64_t *value, int num_bits)
		{
			uint64_t to_ret = 0;
			int bits_to_read = num_bits;
			*value = 0;

			// Check to make sure we can write that many bits into the buffer
			if (num_bits > m_num_bits_available) return false;

			// This is done on the first read to assure that we do at least one
			if (m_remaining_bits_in_byte == 0)
			{
				if (!IncrementByte()) return false;
			}
			if (num_bits <= m_remaining_bits_in_byte)
			{

				to_ret = m_data[m_current_data_index] >> (m_remaining_bits_in_byte - num_bits);
				m_remaining_bits_in_byte -= num_bits;
				m_data[m_current_data_index] = (m_data[m_current_data_index] << (8 - m_remaining_bits_in_byte));
				m_data[m_current_data_index] = m_data[m_current_data_index] >> (8 - m_remaining_bits_in_byte);

				if (m_remaining_bits_in_byte == 0)
				{
					if (!IncrementByte()) return false;
				}
			}
			else
			{
				// If there are remaining bits to read, read 'em
				if (m_remaining_bits_in_byte < 8)
				{
					to_ret = m_data[m_current_data_index];
					bits_to_read -= m_remaining_bits_in_byte;
					if (!IncrementByte()) return false;
				}

				// While we need to read whole bytes
				while (bits_to_read > 8)
				{
					to_ret = (to_ret << 8) | m_data[m_current_data_index];
					if (!IncrementByte()) return false;
					bits_to_read -= 8;
				}

				// Read the remainder of what we need
				if (bits_to_read != 0)
				{
					to_ret = to_ret << bits_to_read;
					to_ret = to_ret | (m_data[m_current_data_index] >> (m_remaining_bits_in_byte - bits_to_read));
					m_remaining_bits_in_byte -= bits_to_read;
					m_data[m_current_data_index] = (m_data[m_current_data_index] << (8 - m_remaining_bits_in_byte));
					m_data[m_current_data_index] = m_data[m_current_data_index] >> (8 - m_remaining_bits_in_byte);

					if (m_remaining_bits_in_byte == 0)
					{
						if (!IncrementByte()) return false;
					}
				}
			}
			*value = to_ret;
			m_num_bits_available -= num_bits;
			return true;
		}
		SingleTimeSeries::SingleTimeSeries(const int precision_decimal_places, const int time_precision_nanoseconds_pow, const double min, const double max)
			: m_bit_size(0), m_time_precision_nanoseconds_pow(time_precision_nanoseconds_pow), m_full_min(min), m_full_max(max)
		{
			m_decimal_places = precision_decimal_places;
			m_max = (int64_t)((max)* pow(10, m_decimal_places));
			m_min = (int64_t)((min)* pow(10, m_decimal_places));
			m_bit_size = NumberOfBits(m_max, m_min);
			
			// We want to divide the time amount so that we are only storing the bits that
			// are relevant.  Example is if I ony care about 10th of a second I should be dividing
			// the time values entered by 10^8 to get a much smaller value than if I cared about 
			// 10 nanoseconds in which case I would only divide by 10
			m_time_precision_divisor = (uint64_t)pow(10, m_time_precision_nanoseconds_pow);

		}
		bool SingleTimeSeriesWriteBuffer::AddValue(SingleTimeSeriesValue ts_value)
		{
			if (!m_AddTimeStamp(ts_value.time, m_first_value)) return false;
			if (!m_AddValue(ts_value.value, m_first_value)) return false;

			// If this is the first time writing, assume the value changed 
			if (m_first_value)
			{
				m_first_value = false;
			}
			
			return true;
		}
		bool SingleTimeSeriesWriteBuffer::m_AddTimeStamp(uint64_t timestamp, bool first)
		{
			// Slight variation on Facebook's Gorilla method.  Accounts
			// for timing precision so that we can more effeciently store
			// different ranges of time deltas ( seconds to nanoseconds )
			//
			// 0		= no change in delta of delts
			// 10		= change with a length of 7 
			// 110		= change with a length of 9
			// 1110		= change with a length of 12
			// 11110	= change with a length of 32
			// 11111	= full 64-bit timestamp.  Re-generate delta of deltas.  This could be either because the time varied too much, 
			//			  or because we are starting at the beginning of a file offset index

			// Return whether or not we succesfully added to value to the buffer

			// TODO - Should we add some sort of configuration to round up or down.  For now, always
			// round to the closest value

			// Convert the time to an appropriate value based upon our specified timestamp precision
			uint64_t timestamp_to_precision = 0;
			timestamp_to_precision = timestamp / m_time_precision_divisor;

			// Do our own rounding.  Rounding messes up at bounds of an uint64_t.  We also want to ensure that 
			// we are round up to the nearest second instead of truncating.  If we care about time stamp precision to 
			// the second, 1.999 is most likey 2 seconds with some jitter than it is 1 seconds with a lot of jitter.  In 
			// the future we could make this tolerance configurable.
			if (m_time_precision_nanoseconds_pow != 0)
			{
				uint64_t timestamp_to_more_precision = timestamp / (uint64_t)pow(10, m_time_precision_nanoseconds_pow - 1);
				if (timestamp_to_precision != 0)
				{
					uint64_t time_fraction = timestamp_to_more_precision % timestamp_to_precision;
					if (time_fraction >= 5)
					{
						timestamp_to_precision++;
					}
				}
				else
				{
					if (timestamp_to_more_precision >= 5)
					{
						timestamp_to_precision++;
					}
				}
			}

			if (first)
			{	
				// Write 11111, indicating we will write the full-ish timestamp 
				if (!WriteBits(k_full_timestamp, 5)) return false;

				// Write the full 64-bit timestamp
				if (!WriteBits(timestamp_to_precision, k_timestamp_size)) return false;

				m_previous_timestamp = timestamp_to_precision;
				m_previous_delta = k_default_delta;
				return true;
			}

			// Check to see if our delta changed
			int64_t delta = timestamp_to_precision - m_previous_timestamp;
			int64_t delta_of_delta = delta - m_previous_delta;
			

			// If the delta didn't change, just write 0 
			if ( delta_of_delta == 0)
			{
				if (!WriteBits(0, 1)) return false;
			}
			else
			{
				// Since we can never get 0 here ( which would imply no change), shifrt the abs value by
				// 1 to better fit in the bits we want to store
				delta_of_delta--;
				int64_t abs_delta_of_delta = std::abs(delta_of_delta);

				for ( int i = 0; i < 5; i++)
				{
					// Pretty big time change, just indicate and write out the whole value
					if ( i >= 4)
					{
						if (!WriteBits(k_full_timestamp, 5)) return false;
						// Write the full 64-bit timestamp
						if (!WriteBits(timestamp_to_precision, k_timestamp_size)) return false;
						break;
					}
					if (abs_delta_of_delta <= timestamp_encoding_info[i].max_delta)
					{
						if ( !WriteBits(timestamp_encoding_info[i].pattern, timestamp_encoding_info[i].pattern_size)) return false;
						
						// Write the sign bit
						if ( delta_of_delta < 1)
						{
							if (!WriteBits(1, 1)) return false;
						}
						else
						{
							if (!WriteBits(0, 1)) return false;
						}
						// Write the absolute value
						if ( !WriteBits(abs_delta_of_delta, timestamp_encoding_info[i].delta_size - 1)) return false;
						break;
					}
				}
				
			}

			// Set our values for the next add
			m_previous_timestamp = timestamp_to_precision;
			m_previous_delta = delta;
			return true;
		}
		bool SingleTimeSeriesWriteBuffer::m_AddValue(double value, bool first)
		{
			// If we are above the maximum, set it to the maximum.
			if (value > m_full_max)
			{
				value = m_full_max;
			}
			// If we are below the minimum, set it to the minimum. 
			if (value < m_full_min)
			{
				value = m_full_min;
			}
			// Get binary representation with the designated precision / precsion
			int64_t value_to_write = (int64_t)((value) * pow(10,m_decimal_places));
			value_to_write -= m_min;

			if (first)
			{
				m_last_value = value_to_write;

				// Write a single 1 bit indicating that the value changed
				if (!WriteBits(1, 1)) return false;
				return WriteBits(value_to_write, m_bit_size);
			}
		
			
			// If the value didn't change, write a 0 bit.  Otherwise write a 1 bit and the value
			if (m_last_value == value_to_write)
			{
				if (!WriteBits(0, 1)) return false;
			}
			else
			{
				/****** Placeholder to see if we need to do Facebook's compression algorithm ******/
				// Now do Facebook's Gorilla compression on the value
				// Convert current and last value to uint64_t for the XOR comparison
				//uint64_t* u_value = (uint64_t*)&value_to_write;
  				//uint64_t xored = *(uint64_t *)&m_last_value ^ *u_value;

				// If the section of changed bits is the same as the last values 
				// changed bits, just store the XORed value.
				/**********************************************************************************/

				m_last_value = value_to_write;
				// Write 1 bit signifiying that the value did change
				if (!WriteBits(1, 1)) return false;
				


				if (!WriteBits(value_to_write, m_bit_size)) return false;
			}
			return true;
		}
		bool SingleTimeSeriesWriteBuffer::AddValues(std::vector<SingleTimeSeriesValue> values, size_t *values_added)
		{
			if (!values_added)
			{
				return false;
			}

			*values_added = 0;
			for (auto &&value : values)
			{
				if (!AddValue(value))
				{
					return false;
				}
				else
				{
					*values_added += 1;
				}
			}
			return true;
		}

				
		MultipleTimeSeriesWriteBuffer::MultipleTimeSeriesWriteBuffer(const int time_precision_nanoseconds_pow, std::vector<ValueTypeDefinition> definitions,  size_t size) :
			WriteByteBuffer(size), m_last_data_type_id(0), m_first_time(true), m_definitions(definitions), m_time_precision_nanoseconds_pow(time_precision_nanoseconds_pow)
		{
			// We want to divide the time amount so that we are only storing the bits that
			// are relevant.  Example is if I ony care about 10th of a second I should be dividing
			// the time values entered by 10^8 to get a much smaller value than if I cared about 
			// 10 nanoseconds in which case I would only divide by 10
			m_time_precision_divisor = (uint64_t)pow(10, m_time_precision_nanoseconds_pow);
		}
		bool MultipleTimeSeriesWriteBuffer::mInit()
		{
			// Write out the version used, major first then minor ( each 4 bits )
			if (!WriteBits((uint64_t)OSCILLIO_TIME_COMPRESS_MAJOR_VERISON, 4)) return false;
			if (!WriteBits((uint64_t)OSCILLIO_TIME_COMPRESS_MINOR_VERSION, 4)) return false;
		
			// Write out our time precision 
			if (!WriteBits((uint64_t)m_time_precision_nanoseconds_pow, 8)) return false;

			// Write out the nature of the data in the fule ( periodic vs aperiodic )
			if (!WriteBits((uint64_t)0, 16)) return false;

			// See how many bits our label ID needs to be
			int label_bit_size = NumberOfBits(m_definitions.size() - 1, 0);

			// Write out the number of bits a label uses 
			if (!WriteBits((uint64_t)label_bit_size, 32)) return false;

			// Write out the number of data types
			if (!WriteBits((uint64_t)m_definitions.size(), 32)) return false;

			// Write the information for each value type out in the header
			for ( auto &&value_def : m_definitions)
			{
				ValueMetrics to_add;
				to_add.definition = value_def;
				to_add.id = m_last_data_type_id;
				to_add.precise_max = (int64_t)((to_add.definition.max)* pow(10, to_add.definition.precision_decimal_places));
				to_add.precise_min = (int64_t)((to_add.definition.min)* pow(10, to_add.definition.precision_decimal_places));
				to_add.m_bit_size = NumberOfBits(to_add.precise_max, to_add.precise_min);

				//TODO - Scrub the label of any newline characters. 
				//TODO - Support not just UTF_8

				// Write out the label terminated with a newline
				const char *current_label = value_def.label.c_str();
				for ( int i = 0; i < value_def.label.size(); i++)
				{
					// TODO - Use a standard string encoding such that we never have to rely
					// on cross systems having the same char size ( shoudl always be 8 )
					if (!WriteBits((uint64_t)current_label[i], sizeof(char))) return false;
				}
				if (!WriteBits((uint64_t)'\n', sizeof(char))) return false;

				// Pad out to 32-bits
				int mod_32 = (value_def.label.size()+1) % 4;
				if ( mod_32 != 0 )
				{
					if (!WriteBits((uint64_t)0, mod_32 * 8)) return false;
				}

				// Write out the definition into the buffer header
				if (!WriteBits((uint64_t)to_add.definition.precision_decimal_places, 32)) return false;
				if (!WriteBits((uint64_t)to_add.definition.max, 64)) return false;
				if (!WriteBits((uint64_t)to_add.definition.min, 64)) return false;
				
				// Update the next ID
				m_last_data_type_id++;

				m_metrics.push_back(to_add);
				m_label_to_metrics[to_add.definition.label] = to_add;
			}
			return true;
		}	
		bool MultipleTimeSeriesWriteBuffer::mAddTimeStamp(uint64_t timestamp, bool first)
		{
			// Slight variation on Facebook's Gorilla method.  Accounts
			// for timing precision so that we can more effeciently store
			// different ranges of time deltas ( seconds to nanoseconds )
			//
			// 0		= no change in delta of delts
			// 10		= change with a length of 7 
			// 110		= change with a length of 9
			// 1110		= change with a length of 12
			// 11110	= change with a length of 32
			// 11111	= full 64-bit timestamp.  Re-generate delta of deltas.  This could be either because the time varied too much, 
			//			  or because we are starting at the beginning of a file offset index

			// Return whether or not we succesfully added to value to the buffer

			// TODO - Should we add some sort of configuration to round up or down.  For now, always
			// round to the closest value

			// Convert the time to an appropriate value based upon our specified timestamp precision
			uint64_t timestamp_to_precision = 0;
			timestamp_to_precision = timestamp / m_time_precision_divisor;

			// Do our own rounding.  Rounding messes up at bounds of an uint64_t.  We also want to ensure that 
			// we are round up to the nearest second instead of truncating.  If we care about time stamp precision to 
			// the second, 1.999 is most likey 2 seconds with some jitter than it is 1 seconds with a lot of jitter.  In 
			// the future we could make this tolerance configurable.
			if (m_time_precision_nanoseconds_pow != 0)
			{
				uint64_t timestamp_to_more_precision = timestamp / (uint64_t)pow(10, m_time_precision_nanoseconds_pow - 1);
				if (timestamp_to_precision != 0)
				{
					uint64_t time_fraction = timestamp_to_more_precision % timestamp_to_precision;
					if (time_fraction >= 5)
					{
						timestamp_to_precision++;
					}
				}
				else
				{
					if (timestamp_to_more_precision >= 5)
					{
						timestamp_to_precision++;
					}
				}
			}

			if (first)
			{	
				// Write 11111, indicating we will write the full-ish timestamp 
				if (!WriteBits(k_full_timestamp, 5)) return false;

				// Write the full 64-bit timestamp
				if (!WriteBits(timestamp_to_precision, k_timestamp_size)) return false;

				m_previous_timestamp = timestamp_to_precision;
				m_previous_delta = k_default_delta;
				return true;
			}

			// Check to see if our delta changed
			int64_t delta = timestamp_to_precision - m_previous_timestamp;
			int64_t delta_of_delta = delta - m_previous_delta;
			

			// If the delta didn't change, just write 0 
			if ( delta_of_delta == 0)
			{
				if (!WriteBits(0, 1)) return false;
			}
			else
			{
				// Since we can never get 0 here ( which would imply no change), shifrt the abs value by
				// 1 to better fit in the bits we want to store
				delta_of_delta--;
				int64_t abs_delta_of_delta = std::abs(delta_of_delta);

				for ( int i = 0; i < 5; i++)
				{
					// Pretty big time change, just indicate and write out the whole value
					if ( i >= 4)
					{
						if (!WriteBits(k_full_timestamp, 5)) return false;
						// Write the full 64-bit timestamp
						if (!WriteBits(timestamp_to_precision, k_timestamp_size)) return false;
						break;
					}
					if (abs_delta_of_delta <= timestamp_encoding_info[i].max_delta)
					{
						if ( !WriteBits(timestamp_encoding_info[i].pattern, timestamp_encoding_info[i].pattern_size)) return false;
						
						// Write the sign bit
						if ( delta_of_delta < 1)
						{
							if (!WriteBits(1, 1)) return false;
						}
						else
						{
							if (!WriteBits(0, 1)) return false;
						}
						// Write the absolute value
						if ( !WriteBits(abs_delta_of_delta, timestamp_encoding_info[i].delta_size - 1)) return false;
						break;
					}
				}
				
			}

			// Set our values for the next add
			m_previous_timestamp = timestamp_to_precision;
			m_previous_delta = delta;
			return true;
		}
		bool MultipleTimeSeriesWriteBuffer::mAddValue(ValueMetrics metrics, double value)
		{
						// If we are above the maximum, set it to the maximum.
			if (value > metrics.definition.max)
			{
				value = metrics.definition.max;
			}
			// If we are below the minimum, set it to the minimum. 
			if (value < metrics.definition.min)
			{
				value = metrics.definition.min;
			}
			// Get binary representation with the designated precision / precsion
			int64_t value_to_write = (int64_t)((value) * pow(10,metrics.definition.precision_decimal_places));
			value_to_write -= metrics.precise_min;

			if (metrics.first_value)
			{
				metrics.m_last_value = value_to_write;

				// Write a single 1 bit indicating that the value changed
				if (!WriteBits(1, 1)) return false;
				metrics.first_value = false;
				return WriteBits(value_to_write, metrics.m_bit_size);
			}
		
			
			// If the value didn't change, write a 0 bit.  Otherwise write a 1 bit and the value
			if (metrics.m_last_value == value_to_write)
			{
				if (!WriteBits(0, 1)) return false;
			}
			else
			{
				/****** Placeholder to see if we need to do Facebook's compression algorithm ******/
				// Now do Facebook's Gorilla compression on the value
				// Convert current and last value to uint64_t for the XOR comparison
				//uint64_t* u_value = (uint64_t*)&value_to_write;
  				//uint64_t xored = *(uint64_t *)&m_last_value ^ *u_value;

				// If the section of changed bits is the same as the last values 
				// changed bits, just store the XORed value.
				/**********************************************************************************/

				metrics.m_last_value = value_to_write;
				// Write 1 bit signifiying that the value did change
				if (!WriteBits(1, 1)) return false;
				


				if (!WriteBits(value_to_write, metrics.m_bit_size)) return false;
			}
			return true;
			return true;
		}
		bool MultipleTimeSeriesWriteBuffer::AddValue(LabeledTimeSeriesValues ts_value)
		{
			// If this is periodic, check to make sure we have a full row
			if ( ts_value.labeled_values.size() != m_definitions.size())
			{
				return false;
			}

			if (m_first_time) 
			{
				if (!mInit()) return false;
			}

			if (!mAddTimeStamp(ts_value.time, m_first_time)) return false;

			// Write each value
			for (int i = 0; i < ts_value.labeled_values.size(); i++)
			{
				// Write the value
				if (!mAddValue(m_metrics[i], ts_value.labeled_values[i].second)) return false;
			}
			m_first_time = false;
			return true;
		}
		bool MultipleTimeSeriesWriteBuffer::AddValues(std::vector<LabeledTimeSeriesValues> values, size_t *values_added)
		{
			if (!values_added)
			{
				return false;
			}

			*values_added = 0;
			for (auto &&value : values)
			{
				if (!AddValue(value))
				{
					return false;
				}
				else
				{
					*values_added += 1;
				}
			}
			return true;
		}


		bool MultipleTimeSeriesReadBuffer::ReadNext(LabeledTimeSeriesValues *ts_value)
		{
			if ( m_first_time )
			{
				if ( !mInit()) return false;
			}
			m_first_time = false;

			if (!m_ReadNextTime(&ts_value->time)) return false;
			if (!m_ReadNextValue(&ts_value->labeled_values)) return false;

			return true;
		}
		bool MultipleTimeSeriesReadBuffer::m_ReadNextValue(std::vector<labeled_value> *value)
		{
			if (!value) return false;

			uint64_t bit_value = 0;

			// We are making the assumption here that the file is not corrupt and that 
			// we will be reading in N values where N is the number of data types specified
			// in the read in header.  We could add in a value to specify the length, but that
			// would add size to what we are trying to compress
			for ( int i = 0; i < m_metrics.size(); i++)
			{
				(*value)[i].first = m_metrics[i].definition.label;
				if ( m_metrics[i].first_value )
				{
					// TODO - See if we can't keep track of any debugging infomration here
					m_metrics[i].first_value = false;
				}

				// Read one bit to let us know if the value changed or not
				if (!ReadNextBits(&bit_value, 1)) return false;
				
				if ( bit_value != 0)
				{
					if (!ReadNextBits(&bit_value, m_metrics[i].m_bit_size)) return false;
					
					m_metrics[i].m_last_value = ((((int64_t)bit_value + m_metrics[i].precise_min)) / pow(10,m_metrics[i].definition.precision_decimal_places));
				}
				(*value)[i].second = m_metrics[i].m_last_value;
			}

			return true;
		}
		bool MultipleTimeSeriesReadBuffer::m_ReadNextTime(uint64_t *time)
		{
			uint64_t bit_value = 0;

			// Read number of ones to determine the size of the delta of the 
			// time delta
			int num_ones = 0;
		
			if (!ReadNextBits(&bit_value, 1)) return false;

			// If the next bit is 0, then 
			if ( bit_value == 0)
			{
				m_time_metrics.previous_timestamp = m_time_metrics.previous_timestamp + m_time_metrics.previous_delta;
			}
			else
			{
				num_ones = 1;
				while (num_ones < 5) {
					if (!ReadNextBits(&bit_value, 1)) return false;
					if (bit_value == 0) {
						break;
					}
					num_ones++;
				}
			}

			// Get the bits to read and the 
			if ( num_ones != 0)
			{
				// If the pattern is 11111, read the full timestamp and set the 
				// delta to our default value
				if ( num_ones == 5)
				{
					uint64_t time_bits = 0;
					if (!ReadNextBits(&time_bits, m_time_metrics.k_timestamp_size)) return false;
					m_time_metrics.previous_timestamp = *(int64_t *)&time_bits;
					*time = m_time_metrics.previous_timestamp;
					m_time_metrics.previous_delta = m_time_metrics.k_default_delta;
					*time = *time * m_time_metrics.time_precision_divisor;
					return true;
				}

				// Read the number of bits based off of the pattern
				bool sign_bit = false;
				// Read the sign bit
				if (!ReadNextBits(&bit_value, 1)) return false;
				if (bit_value == 1)
					sign_bit = true;


				int index = num_ones - 1;
				if (!ReadNextBits(&bit_value, timestamp_encoding_info[index].delta_size - 1)) return false;
				// [0,255] becomes [-128,127]
				int64_t encoded_delta_of_delta = (int64_t)bit_value;// -((int64_t)1 << (timestamp_encoding_info[index].delta_size - 1));
				
				if (sign_bit)
				{
					encoded_delta_of_delta = encoded_delta_of_delta * -1;
				}

				// [-128,127] becomes [-128,128] without the middle zero ( remembmer when we added it?)			
				encoded_delta_of_delta++;
				m_time_metrics.previous_delta = m_time_metrics.previous_delta + encoded_delta_of_delta;
				m_time_metrics.previous_timestamp = m_time_metrics.previous_timestamp + m_time_metrics.previous_delta;
			}

			// Convert to the full time stamp using the specified time resolution
			*time = m_time_metrics.previous_timestamp * m_time_metrics.time_precision_divisor;

			return true;
		}
		bool MultipleTimeSeriesReadBuffer::mInit()
		{
			// Placeholder for whenever we read
			uint64_t bits_read = 0;

			// Read the major and minor version, validate that they are as expected
			if (!ReadNextBits(&bits_read, 4)) return false;
			if ( bits_read != OSCILLIO_TIME_COMPRESS_MAJOR_VERISON) return false;

			if (!ReadNextBits(&bits_read, 4)) return false;
			if ( bits_read != OSCILLIO_TIME_COMPRESS_MINOR_VERSION) return false;

			// Read in our time precision
			if (!ReadNextBits(&bits_read, 8)) return false;
			m_time_metrics.time_precision_nanoseconds_pow = (int)bits_read;
			
			// Read in whether or not we 
			if (!ReadNextBits(&bits_read, 16)) return false;

			// Read in the size of our data type label
			if (!ReadNextBits(&bits_read, 32)) return false;
			m_data_type_label_size = (int)bits_read;

			// Read in the number of data types
			if (!ReadNextBits(&bits_read, 32)) return false;
			m_metrics = std::vector<ValueMetrics>(bits_read);

			for ( int i = 0; i < m_metrics.size(); i++)
			{
				ValueMetrics to_add;
				to_add.m_bit_size = to_add.m_last_value = to_add.precise_max = to_add.precise_min = 0;
				to_add.first_value = true;
				
				// Give the next char read a default value that's not '\n';
				char next_read = 'A';

				// Read in the label.  This will be terminated by a newline.
				while ( next_read != '\n')
				{
					// Read in the next character of the label
					if (!ReadNextBits(&bits_read, sizeof(char))) return false;
					next_read = (char)bits_read;
					to_add.definition.label.push_back(next_read);
				}

				// Read in the remaining padding to 32-bit boundary
				// Pad out to 32-bits
				int mod_32 = to_add.definition.label.size() % 4;
				if ( mod_32 != 0 )
				{
					if (!ReadNextBits(&bits_read, mod_32 * 8)) return false;
					if ( bits_read != 0)
					{
						// TODO - Report an error
						return false;
					}
				}

				// The ID will always be the index of the 
				to_add.id = m_last_data_type_id;
				m_last_data_type_id++;

				// Read in the metadata about this data value
				if (!ReadNextBits(&bits_read, 32)) return false;
				to_add.definition.precision_decimal_places = (int)bits_read;

				if (!ReadNextBits(&bits_read, 64)) return false;
				to_add.definition.max = (double)bits_read;

				if (!ReadNextBits(&bits_read, 64)) return false;
				to_add.definition.min = (double)bits_read;

				// Compute the internally used information from the read metadata
				to_add.precise_max = (int64_t)((to_add.definition.max)* pow(10, to_add.definition.precision_decimal_places));
				to_add.precise_min = (int64_t)((to_add.definition.min)* pow(10, to_add.definition.precision_decimal_places));
				to_add.m_bit_size = NumberOfBits(to_add.precise_max, to_add.precise_min);
				m_metrics[i] = to_add;
			}
			return true;
		}

		
		bool SingleTimeSeriesReadBuffer::m_ReadNextTime(uint64_t *time)
		{
			uint64_t bit_value = 0;

			// Read number of ones to determine the size of the delta of the 
			// time delta
			int num_ones = 0;
		
			if (!ReadNextBits(&bit_value, 1)) return false;

			// If the next bit is 0, then 
			if ( bit_value == 0)
			{
				m_previous_timestamp = m_previous_timestamp + m_previous_delta;
			}
			else
			{
				num_ones = 1;
				while (num_ones < 5) {
					if (!ReadNextBits(&bit_value, 1)) return false;
					if (bit_value == 0) {
						break;
					}
					num_ones++;
				}
			}

			// Get the bits to read and the 
			if ( num_ones != 0)
			{
				// If the pattern is 11111, read the full timestamp and set the 
				// delta to our default value
				if ( num_ones == 5)
				{
					uint64_t time_bits = 0;
					if (!ReadNextBits(&time_bits, k_timestamp_size)) return false;
					m_previous_timestamp = *(int64_t *)&time_bits;
					*time = m_previous_timestamp;
					m_previous_delta = k_default_delta;
					*time = *time * m_time_precision_divisor;
					return true;
				}

				// Read the number of bits based off of the pattern
				bool sign_bit = false;
				// Read the sign bit
				if (!ReadNextBits(&bit_value, 1)) return false;
				if (bit_value == 1)
					sign_bit = true;


				int index = num_ones - 1;
				if (!ReadNextBits(&bit_value, timestamp_encoding_info[index].delta_size - 1)) return false;
				// [0,255] becomes [-128,127]
				int64_t encoded_delta_of_delta = (int64_t)bit_value;// -((int64_t)1 << (timestamp_encoding_info[index].delta_size - 1));
				
				if (sign_bit)
				{
					encoded_delta_of_delta = encoded_delta_of_delta * -1;
				}

				// [-128,127] becomes [-128,128] without the middle zero ( remembmer when we added it?)			
				encoded_delta_of_delta++;
				m_previous_delta = m_previous_delta + encoded_delta_of_delta;
				m_previous_timestamp = m_previous_timestamp + m_previous_delta;
			}

			// Convert to the full time stamp using the specified time resolution
			*time = m_previous_timestamp * m_time_precision_divisor;

			return true;
		}
		bool SingleTimeSeriesReadBuffer::m_ReadNextValue(double *value)
		{
			if (!value) return false;

			uint64_t bit_value = 0;

			// Read one bit to let us know if the value changed or not
			if (!ReadNextBits(&bit_value, 1)) return false;

			if (bit_value == 0)
			{
				*value = m_last_value;
			}
			else
			{
				if (!ReadNextBits(&bit_value, m_bit_size)) return false;
				
				m_last_value = ((((int64_t)bit_value + m_min)) / pow(10,m_decimal_places));
				*value = m_last_value;
			}
			return true;
			
		}
		bool SingleTimeSeriesReadBuffer::ReadNext(SingleTimeSeriesValue *ts_value)
		{
			if (!m_ReadNextTime(&ts_value->time)) return false;
			if (!m_ReadNextValue(&ts_value->value)) return false;
			return true;
		}
		std::vector<SingleTimeSeriesValue> SingleTimeSeriesReadBuffer::ReadAll()
		{
			std::vector<SingleTimeSeriesValue> to_ret;
			SingleTimeSeriesValue to_add;
			while (ReadNext(&to_add))
			{
				to_ret.push_back(to_add);
			}
			return to_ret;
		}
		void SingleTimeSeriesReadBuffer::ReadAll(std::vector<SingleTimeSeriesValue> *buffer)
		{
			SingleTimeSeriesValue to_add;
			while (ReadNext(&to_add))
			{
				buffer->push_back(to_add);
			}
		}
	}
}