#include "TimeSeriesCompression.h"
#include <assert.h>
#include <math.h>

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
        	{0x7FFFFFFF, 32, 15, 4}
		};
		

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
		SingleTimeSeries::SingleTimeSeries(const double precision, const int decimal_places, const int time_precision_decimal_places, const double min, const double max)
			: m_precision(precision), m_min(min), m_max(max), m_bit_size(0)
		{
			m_base_precision = precision;
			m_precision = round(precision * (10 * decimal_places));
			m_decimal_places = decimal_places;
			m_bit_size = NumberOfBits(precision, min, max);
			

			// The lowest precision we can handle is nanoseconds
			assert(time_precision_decimal_places <= 9);

			// We want to divide the time amount so that we are only storing the bits that
			// are relevant.  Example is if I ony care about 10th of a second I should be dividing
			// the time values entered by 10^8 to get a much smaller value than if I cared about 
			// 10 nanoseconds in which case I would only divide by 10
			m_time_precision_divisor = 1000000000 / pow(10, time_precision_decimal_places);
			m_time_decimal_places = time_precision_decimal_places;
		}
		int SingleTimeSeries::NumberOfBits(const double precision, const double min, const double max)
		{
			int num_bits = 0;
			uint64_t zeroed_value = (uint64_t)((max - min) / precision);

			// Common algorithm. Keep dividing by 2 until we get 0.  A value of 0, will still take one bit to represent 
			do
			{
				zeroed_value /= 2;
				num_bits++;
			} while (zeroed_value != 0);

			return num_bits;
		}
		bool SingleTimeSeriesWriteBuffer::AddValue(TimeSeriesValue ts_value)
		{
			// Slight variation on the Gorilla method.  Accounts
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
			bool to_ret = true;

		
			if (!m_AddTimeStamp(ts_value.time, m_first_value)) return false;
			if (!m_AddValue(ts_value.value, m_first_value)) return false;

			// If this is the first time writing, assume the value changed 
			if (m_first_value)
			{
				m_first_value = false;
			}
			
			return true;
		}
		bool SingleTimeSeriesWriteBuffer::m_AddTimeStamp(int64_t timestamp, bool first)
		{
			// TODO - Should we add some sort of configuration to round up or down.  For now, always
			// round to the closest value

			// Convert the time to an appropriate value based upon our specified timestamp precision
			uint64_t timestamp_to_precision = 0;
			if ( m_time_precision_divisor != 9 )
			{
				timestamp_to_precision = round(timestamp / (double)(m_time_precision_divisor));
			}
			else
			{
				timestamp_to_precision = timestamp;
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
			int64_t delta = timestamp - m_previous_timestamp;
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
			// TODO - Print out an error message or have some means to denote we exceeded maximum
			if (value > m_max)
			{
				value = m_max;
			}


			// Get binary representation with the designated precision / precsion
			uint64_t value_to_write = (uint64_t)(((value - m_min) * (10 * m_decimal_places)) / m_precision);

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
				m_last_value = value_to_write;
				if (!WriteBits(1, 1)) return false;
				if (!WriteBits(value_to_write, m_bit_size)) return false;
			}
			return true;
		}
		bool SingleTimeSeriesWriteBuffer::AddValues(std::vector<TimeSeriesValue> values, size_t *values_added)
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
				*time = m_previous_timestamp + m_previous_delta;
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
					if (!ReadNextBits(time, k_timestamp_size)) return false;
					m_previous_timestamp = *time;
					m_previous_delta = k_default_delta;
					*time = *time * m_time_precision_divisor;
					return true;
				}

				// Read the number of bits based off of the pattern
				int index = num_ones - 1;
				if (!ReadNextBits(&bit_value, timestamp_encoding_info[index].delta_size)) return false;
				// [0,255] becomes [-128,127]
				int64_t encoded_delta = (int64_t)bit_value - ((int64_t)1 << (timestamp_encoding_info[index].delta_size - 1));
				
				// [-128,127] becomes [-128,128] without the middle zero ( remembmer when we added it?)			
				if ( encoded_delta >= 0)
				{
					encoded_delta++;
				}
				*time = m_previous_timestamp + encoded_delta;
			}

			// Convert to the full time stamp using the specified time resolution
			*time = *time * m_time_precision_divisor;

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
				m_last_value = (bit_value * m_precision) / (10 * m_decimal_places) + m_min;
				*value = m_last_value;
			}
			return true;
			
		}
		bool SingleTimeSeriesReadBuffer::ReadNext(TimeSeriesValue *ts_value)
		{
			if (!m_ReadNextTime(&ts_value->time)) return false;
			if (!m_ReadNextValue(&ts_value->value)) return false;
			return true;
		}
		std::vector<TimeSeriesValue> SingleTimeSeriesReadBuffer::ReadAll()
		{
			std::vector<TimeSeriesValue> to_ret;
			TimeSeriesValue to_add;
			while (ReadNext(&to_add))
			{
				to_ret.push_back(to_add);
			}
			return to_ret;
		}
		void SingleTimeSeriesReadBuffer::ReadAll(std::vector<TimeSeriesValue> *buffer)
		{
			TimeSeriesValue to_add;
			while (ReadNext(&to_add))
			{
				buffer->push_back(to_add);
			}
		}
	}
}