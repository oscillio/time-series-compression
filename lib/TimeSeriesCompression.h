#pragma once
#include <stdint.h>
#include <memory>
#include <iterator>
#include <vector>
#include <string>
#include <unordered_map>

namespace oscill {
	namespace io {
		// Information about a value.  Will be written into the files header
		// so that it can be read out correctly
		struct ValueTypeDefinition
		{
			// Can be empty string if it doesn't matter.
			std::string label;
			int precision_decimal_places;
			double min;
			double max;
		};

		// Information about a value's antecedent so that we can determine the correct value
		// to return at any given point in time
		struct ValueMetrics
		{
			struct ValueTypeDefinition definition;
			bool first_value = false;
			int id;
			int64_t precise_min;
			int64_t precise_max;
			uint64_t m_last_value;
			size_t m_bit_size;
		};

		// Metrics about the current time value
		struct TimeMetrics
		{
			int time_precision_nanoseconds_pow;
			uint64_t time_precision_divisor;
			static constexpr uint32_t k_full_timestamp = 0x1F;
			static constexpr uint32_t k_timestamp_size = 64;
			static constexpr uint32_t k_default_delta = 10;
			uint64_t previous_timestamp;
			uint64_t previous_delta;
		};


		struct SingleTimeSeriesValue
		{
			// Unix Time in Nanoseconds
			uint64_t time;
			// Value
			double value;
		};
		typedef std::pair< std::string, double> labeled_value ;
		struct LabeledTimeSeriesValues
		{
			// Unix Time in Nanoseconds
			uint64_t time;
			// Tuple list containing values and their labels
			std::vector< labeled_value > labeled_values;
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
				SingleTimeSeries(const int precision_decimal_places, const int time_precision_nanoseconds_pow, const double min, const double max);
				virtual ~SingleTimeSeries() {}
			protected:
				int m_decimal_places;
				int64_t m_min;
				int64_t m_max;
				double m_full_min;
				double m_full_max;
				uint64_t m_time_precision_divisor;
				int m_time_precision_nanoseconds_pow;

				static constexpr uint32_t k_full_timestamp = 0x1F;
				static constexpr uint32_t k_timestamp_size = 64;
				static constexpr uint32_t k_default_delta = 10;

				size_t m_bit_size;
				uint64_t m_previous_timestamp;
				uint64_t m_previous_delta;
		};
		class SingleTimeSeriesWriteBuffer : public SingleTimeSeries, public WriteByteBuffer
		{
			
		public:
			SingleTimeSeriesWriteBuffer(const int precision_decimal_places, const int time_precision_nanoseconds_pow, const double min, const double max, size_t size) :
				WriteByteBuffer(size), SingleTimeSeries(precision_decimal_places, time_precision_nanoseconds_pow, min, max)
			{}
			virtual ~SingleTimeSeriesWriteBuffer() {}
			virtual bool AddValue(SingleTimeSeriesValue ts_value);
			virtual bool AddValues(std::vector<SingleTimeSeriesValue> values, size_t *values_added);
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
			SingleTimeSeriesReadBuffer(const int precision_decimal_places, const int time_precision_nanoseconds_pow, const double min, const double max, void *data, size_t size) :
				ReadByteBuffer(data, size), SingleTimeSeries(precision_decimal_places, time_precision_nanoseconds_pow, min, max)
			{}
			SingleTimeSeriesReadBuffer(SingleTimeSeriesWriteBuffer& write_buffer) : ReadByteBuffer(write_buffer.RawData(), write_buffer.Size()), 
				SingleTimeSeries(write_buffer.m_decimal_places, write_buffer.m_time_precision_nanoseconds_pow, write_buffer.m_full_min, write_buffer.m_full_max)
			{

			}
			virtual ~SingleTimeSeriesReadBuffer() {}
			bool ReadNext(SingleTimeSeriesValue *ts_value);
			std::vector<SingleTimeSeriesValue> ReadAll();
			void ReadAll(std::vector<SingleTimeSeriesValue> *buffer);
		protected:
			bool m_ReadNextValue(double *value);
			bool m_ReadNextTime(uint64_t *time);
			double m_last_value;

		};

		class MultipleTimeSeriesWriteBuffer : public WriteByteBuffer
		{
			public:
				MultipleTimeSeriesWriteBuffer(const int time_precision_nanoseconds_pow, std::vector<ValueTypeDefinition> definitions, size_t size);
				virtual ~MultipleTimeSeriesWriteBuffer(){}
				virtual bool AddValue(LabeledTimeSeriesValues ts_value);
				virtual bool AddValues(std::vector<LabeledTimeSeriesValues> values, size_t *values_added);
			protected:
				bool mAddTimeStamp(uint64_t timestamp, bool first);
				bool mAddValue(ValueMetrics metrics, double value);
				bool mInit();
			private:	
				/***** 		TIME METRIC INFORMATION    ******/
				int m_time_precision_nanoseconds_pow;
				uint64_t m_time_precision_divisor;
				static constexpr uint32_t k_full_timestamp = 0x1F;
				static constexpr uint32_t k_timestamp_size = 64;
				static constexpr uint32_t k_default_delta = 10;
				uint64_t m_previous_timestamp;
				uint64_t m_previous_delta;
				/****** END TIME METRIC INFORMATION *********/

				bool m_first_time = true;
				int m_last_data_type_id;
				std::vector<ValueTypeDefinition> m_definitions;
				std::vector<ValueMetrics> m_metrics;
				std::unordered_map<std::string, ValueMetrics> m_label_to_metrics;
		};


		class MultipleTimeSeriesReadBuffer : public ReadByteBuffer
		{
		public:
			MultipleTimeSeriesReadBuffer(void *data, size_t size) : ReadByteBuffer(data, size),
			m_last_data_type_id(0)
			{
				m_time_metrics.previous_delta = m_time_metrics.previous_timestamp = m_time_metrics.time_precision_divisor = m_time_metrics.time_precision_nanoseconds_pow = 0;
			}
			virtual ~MultipleTimeSeriesReadBuffer() {}
			bool ReadNext(LabeledTimeSeriesValues *ts_value);
		protected:
			bool mInit();
			bool m_ReadNextValue(std::vector<labeled_value> *value);
			bool m_ReadNextTime(uint64_t *time);

		private:
			bool m_first_time = true;
			int m_last_data_type_id;
			TimeMetrics m_time_metrics;
			int m_data_type_label_size = 0;
			std::vector<ValueMetrics> m_metrics;
			std::unordered_map<std::string, ValueMetrics> m_label_to_metrics;

		};

	}
}
