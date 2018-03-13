#pragma once
#include <stdint.h>
#include <memory>
#include <iterator>
#include <vector>

namespace oscill {
	namespace io {
		struct TimeSeriesValue
		{
			// Unix Time in Nanoseconds
			uint64_t time;
			// Value
			double value;
		};

		class ByteBuffer
		{
		
		typedef uint8_t byte_t;
		typedef std::vector<byte_t> bytes_t;


		public:
			ByteBuffer(void *data, size_t size) :
				m_size(size), m_current_data_index(0), m_num_bits_available(size * 8)
			{
				m_data = bytes_t(size);
				memcpy(m_data.data(), data, size);
			}
			ByteBuffer(size_t size) :
				m_size(size), m_current_data_index(0), m_num_bits_available(size * 8)
			{
				m_data = bytes_t(size);
			}
			virtual ~ByteBuffer() {}
			int64_t ByteCount() { return (uint64_t)(m_current_data_index + 1); }
			void *RawData() { return m_data.data(); }
			size_t Size() { return m_size; }
			void Reset() { m_current_data_index = 0; m_remaining_bits_in_byte = 8; }
		protected:
			// Make default, copy constructor, and assignment always private, to prevent problems
			ByteBuffer() {}
			ByteBuffer& operator = (const ByteBuffer& other){ return *this; }
			ByteBuffer(const ByteBuffer & other) {/* do nothing */ }

			bool IncrementByte();

			size_t m_current_data_index = 0;
			int64_t m_num_bits_available = 0;
			uint8_t m_remaining_bits_in_byte = 8;

			bytes_t m_data;
			const size_t m_size = 0;
		};

		class WriteByteBuffer : public ByteBuffer
		{
		public:
			WriteByteBuffer(void *data, size_t size) : ByteBuffer(data, size)
			{}
			WriteByteBuffer(size_t size) : ByteBuffer(size) {}
			bool WriteBits(uint64_t value, int num_bits);
			bool WriteBool(bool value)
			{
				return WriteBits((uint64_t)value, 1);
			}
		protected:
			// Make default, copy constructor, and assignment always private, to prevent problems
			WriteByteBuffer() {}
			WriteByteBuffer& operator = (const ByteBuffer& other) {return *this;}
			WriteByteBuffer(const ByteBuffer & other) {/* do nothing */ }
		};
		class ReadByteBuffer : public ByteBuffer
		{
		public:
			ReadByteBuffer(void *data, size_t size) : ByteBuffer(data, size) {}
			ReadByteBuffer(size_t size) : ByteBuffer(size) {}
			bool UnsafeReadBool()
			{
				uint8_t to_ret = 0;
				if (ReadNextBit(&to_ret))
				{
					if (to_ret != 0)
					{
						return true;
					}
					else
					{
						return false;
					}
				}
				else
				{
					return false;
				}
			}
			bool ReadNextBits(uint64_t *value, int num_bits);
			bool ReadNextBit(uint8_t *value)
			{
				return ReadNextBits((uint64_t *)value, 1);
			}
		protected:
			// Make default, copy constructor, and assignment always private, to prevent problems
			ReadByteBuffer() {}
			ReadByteBuffer& operator = (const ByteBuffer& other) {return *this;}
			ReadByteBuffer(const ByteBuffer & other) {/* do nothing */ }
		};

		class SingleTimeSeries 
		{
			public:
				SingleTimeSeries(const double precision, const int decimal_places, const int time_precision_nanoseconds_pow, const double min, const double max);
				virtual ~SingleTimeSeries() {}
			protected:
				double m_base_precision;
				int m_decimal_places;
				double m_min;
				double m_max;
				double m_precision;
				uint64_t m_time_precision_divisor;
				int m_time_precision_nanoseconds_pow;

				static constexpr uint32_t k_full_timestamp = 0x1F;
				static constexpr uint32_t k_timestamp_size = 64;
				static constexpr uint32_t k_default_delta = 1000;

				size_t m_bit_size;
				uint64_t m_previous_timestamp;
				uint64_t m_previous_delta;
				
			
				// Helper Function to determine max bit size of value
				int NumberOfBits(const double precision, const double min, const double max);
		};
		class SingleTimeSeriesWriteBuffer : public SingleTimeSeries, public WriteByteBuffer
		{
			
		public:
			// TODO - Get the decmial places based off the precision specified.
			SingleTimeSeriesWriteBuffer(const double precision, const int precision_decimal_places, const int time_precision_nanoseconds_pow, const double min, const double max, size_t size) :
				WriteByteBuffer(size), SingleTimeSeries(precision, precision_decimal_places, time_precision_nanoseconds_pow, min, max)
			{}
			virtual ~SingleTimeSeriesWriteBuffer() {}
			bool AddValue(TimeSeriesValue ts_value);
			bool AddValues(std::vector<TimeSeriesValue> values, size_t *values_added);
		protected:
			bool m_AddTimeStamp(uint64_t timestamp, bool first);
			bool m_AddValue(double value, bool first);

			bool m_first_value = true;
			uint64_t m_last_value = 0;
			friend class SingleTimeSeriesReadBuffer;
		};
		class SingleTimeSeriesReadBuffer : public SingleTimeSeries, public ReadByteBuffer
		{
		public:
			SingleTimeSeriesReadBuffer(const double precision, const int precision_decimal_places, const int time_precision_nanoseconds_pow, const double min, const double max, void *data, size_t size) :
				ReadByteBuffer(data, size), SingleTimeSeries(precision, precision_decimal_places, time_precision_nanoseconds_pow, min, max)
			{}
			SingleTimeSeriesReadBuffer(SingleTimeSeriesWriteBuffer& write_buffer) : ReadByteBuffer(write_buffer.RawData(), write_buffer.Size()), 
				SingleTimeSeries(write_buffer.m_base_precision, write_buffer.m_decimal_places, write_buffer.m_time_precision_nanoseconds_pow, write_buffer.m_min, write_buffer.m_max)
			{

			}
			virtual ~SingleTimeSeriesReadBuffer() {}
			bool ReadNext(TimeSeriesValue *ts_value);
			std::vector<TimeSeriesValue> ReadAll();
			void ReadAll(std::vector<TimeSeriesValue> *buffer);
		protected:
			bool m_ReadNextValue(double *value);
			bool m_ReadNextTime(uint64_t *time);
			double m_last_value;

		};

	}
}
