#pragma once

#include "Foundation/DynamicArray.h"
#include "Foundation/Endian.h"
#include "Foundation/Stream.h"
#include "Foundation/String.h"
#include "Foundation/Numeric.h"

#include "Persist/API.h"
#include "Persist/Exceptions.h"

namespace Helium
{
	namespace Persist
	{
		//
		// http://wiki.msgpack.org/display/MSGPACK/Format+specification
		//  This implementation is from the spec as of Feb 20, 2013
		//  All multi-byte values are big-endian with UTF-8 strings
		//

		namespace MessagePackTypes
		{
			enum Type
			{
				// Single byte objects
				FixNumPositive          = 0x00, // 0XXXXXXX
				FixNumNegative          = 0xe0, // 111XXXXX
				Nil                     = 0xc0, // 11000000
				False                   = 0xc2, // 11000001
				True                    = 0xc3, // 11000010

				// Fixed size objects
				Float32                 = 0xca, // 11001010
				Float64                 = 0xcb, // 11001011
				UInt8                   = 0xcc, // 11001100
				UInt16                  = 0xcd, // 11001101
				UInt32                  = 0xce, // 11001110
				UInt64                  = 0xcf, // 11001111
				Int8                    = 0xd0, // 11010000
				Int16                   = 0xd1, // 11010001
				Int32                   = 0xd2, // 11010010
				Int64                   = 0xd3, // 11010011

				// Variable size objects
				FixRaw                  = 0xa0, // 101XXXXX
				FixArray                = 0x90, // 1001XXXX
				FixMap                  = 0x80, // 1000XXXX
				Raw16                   = 0xda, // 11011010
				Raw32                   = 0xdb, // 11011011
				Array16                 = 0xdc, // 11011100
				Array32                 = 0xdd, // 11011101
				Map16                   = 0xde, // 11011110
				Map32                   = 0xdf, // 11011111
			};
		};
		typedef MessagePackTypes::Type MessagePackType;

		namespace MessagePackMasks
		{
			enum Type
			{
				FixNumPositiveType      = 0x80, // 10000000
				FixNumPositiveValue     = 0x7f, // 01111111
				FixNumNegativeType      = 0xe0, // 11100000
				FixNumNegativeValue     = 0x1f, // 00011111
				FixRawType              = 0xe0, // 11100000
				FixRawCount             = 0x1f, // 00011111
				FixArrayType            = 0xf0, // 11110000
				FixArrayCount           = 0x0f, // 00001111
				FixMapType              = 0xf0, // 11110000
				FixMapCount             = 0x0f, // 00001111
			};
		};
		typedef MessagePackMasks::Type MessagePackMask;

		namespace MessagePackContainers
		{
			enum Type
			{
				Array,
				Map,
			};
		};
		typedef MessagePackContainers::Type MessagePackContainer;

		class HELIUM_PERSIST_API MessagePackWriter
		{
		public:
			inline MessagePackWriter( Stream* stream = NULL );
			inline void SetStream( Stream* stream );

			void WriteNil();
			void Write( bool value );
			void Write( float32_t value );
			void Write( float64_t value );
			void Write( uint8_t value );
			void Write( uint16_t value );
			void Write( uint32_t value );
			void Write( uint64_t value );
			void Write( int8_t value );
			void Write( int16_t value );
			void Write( int32_t value );
			void Write( int64_t value );

			void Write( const tchar_t* str );
			void WriteRaw( const void* bytes, uint32_t length );

			void BeginArray( uint32_t length );
			void EndArray();
			
			void BeginMap( uint32_t length );
			void EndMap();

		private:
			Stream*                              stream;
			DynamicArray< MessagePackContainer > container; // for bookeeping container termination
			DynamicArray< uint32_t >             size;      // for bookeeping container size
		};

		class HELIUM_PERSIST_API MessagePackReader
		{
		public:
			inline MessagePackReader( Stream* stream = NULL );
			inline void SetStream( Stream* stream );

			inline uint8_t Advance();
			inline bool IsBoolean();
			inline bool IsNumber();
			inline bool IsRaw();
			inline bool IsArray();
			inline bool IsMap();
			void Skip();

			bool Read( bool& value );
			bool Read( float32_t& value );
			bool Read( float64_t& value );
			bool Read( uint8_t& value );
			bool Read( uint16_t& value );
			bool Read( uint32_t& value );
			bool Read( uint64_t& value );
			bool Read( int8_t& value );
			bool Read( int16_t& value );
			bool Read( int32_t& value );
			bool Read( int64_t& value );

			template< class T >
			bool ReadNumber( T& value, bool clamp );

			void Read( String& value );
			uint32_t ReadRawLength();
			void ReadRaw( void* bytes, uint32_t length );

			uint32_t ReadArrayLength();
			void BeginArray( uint32_t length );
			void EndArray();
			
			uint32_t ReadMapLength();
			void BeginMap( uint32_t length );
			void EndMap();

		private:
			void ReadFloat( float64_t& value );
			void ReadUnsigned( uint64_t& value );
			void ReadSigned( int64_t& value );

			Stream*                              stream;
			uint8_t                              type;
			DynamicArray< MessagePackContainer > container; // for bookeeping container termination
			DynamicArray< uint32_t >             size;      // for bookeeping container size
		};
	}
}

#include "Persist/MessagePack.inl"