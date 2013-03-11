#pragma once

#include "Persist/Archive.h"

#ifdef REFLECT_REFACTOR

namespace Helium
{
    namespace Persist
    {
        class HELIUM_PERSIST_API ArchiveJson : public Archive
        {
        public: 
            static const uint32_t CURRENT_VERSION; 

        private:
            friend class Archive;

            // File format version
            uint32_t m_Version;

            // File size
            std::streamoff m_Size;

            // Skip flag
            bool m_Skip;

            // The xml data
            JsonDocument m_Document;

            // The current element
            JsonDocument::Iterator m_Iterator;

            // The stream for element bodies
            TCharStream* m_Body;

            // The stream for the file
            TCharStreamPtr m_Stream;

        public:
            ArchiveJson( const FilePath& path, ByteOrder byteOrder = Helium::PlatformByteOrder );
            
            // PMD: Added to give more control to caller to step through objects one-by-one.
            ArchiveJson( TCharStream *stream, bool write = false );
            ~ArchiveJson();

        private:
            ArchiveJson();

        public:
            // Stream access
            TCharStream& GetStream()
            {
				TCharStream* stream = m_Mode == ArchiveModes::Read ? m_Body : m_Stream;
				HELIUM_ASSERT( stream );
				return *stream;
            }

            virtual void Close() HELIUM_OVERRIDE; 

        protected:
            // The type
            virtual ArchiveType GetType() const
            {
                return ArchiveTypes::Json;
            }

            virtual void Open( bool write = false ) HELIUM_OVERRIDE;
            void OpenStream(TCharStream* stream, bool write = false );

            // Begins parsing the InputStream
            virtual void Read() HELIUM_OVERRIDE;

            // Write to the OutputStream
            virtual void Write() HELIUM_OVERRIDE;


        public:
            void SerializeInstance( Object* object );
            void SerializeInstance( void* structure, const Structure* type );

        protected:
            void SerializeInstance( Object* object, const tchar_t* fieldName );
            void SerializeInstance( void* structure, const Structure* type, const tchar_t* fieldName );

		public:
			void SerializeFields( Object* object );
            void SerializeFields( void* structure, const Structure* type );
            void SerializeArray( const std::vector< ObjectPtr >& elements, uint32_t flags = 0 );
            void SerializeArray( const DynamicArray< ObjectPtr >& elements, uint32_t flags = 0 );

        protected:
            template< typename ConstIteratorType >
            void SerializeArray( ConstIteratorType begin, ConstIteratorType end, uint32_t flags );

        public:
            // Deserialize
            void DeserializeInstance( ObjectPtr& object );
            void DeserializeInstance( void* structure, const Structure* type );
            void DeserializeFields( Object* object );
            void DeserializeFields( void* object, const Structure* type );
            void DeserializeArray( std::vector< ObjectPtr >& elements, uint32_t flags = 0 );
            void DeserializeArray( DynamicArray< ObjectPtr >& elements, uint32_t flags = 0 );

        public:
            void WriteFileHeader();
            void WriteFileFooter();

            void WriteSingleObject(Object& object);

            // Only pass false for _reparse if you know that something else has caused ParseStream to be called
            // since the stream was last changed. For example, Read() does this internally before calling ReadFileHeader, 
            // so it passes false. Client code will almost always want to reparse.
            void ReadFileHeader(bool _reparse = true);
            void ReadFileFooter();

            bool BeginReadingSingleObjects();
            bool ReadSingleObject(ObjectPtr& object);

            void ParseStream();

        protected:
            // Helpers
            template< typename ArrayPusher >
            void DeserializeArray( ArrayPusher& push, uint32_t flags );
            ObjectPtr Allocate();

        public:
            // Reading and writing single object from string data
            static void       ToString( Object* object, tstring& xml );
            static ObjectPtr  FromString( const tstring& xml, const Class* searchClass = NULL );

            // Reading and writing multiple elements from string data
            static void       ToString( const std::vector< ObjectPtr >& elements, tstring& xml );
            static void       FromString( const tstring& xml, std::vector< ObjectPtr >& elements );

        public:
            // Blindly read/write a string to xml stream. Allows Data implementations to write strings
            // the same way.
            void ReadString(tstring &str);
            void WriteString(const tstring &str);

        public:
            struct DeserializingField
            {
                void* m_Instance;
                const Field* m_Field;
            };

            const DeserializingField *GetDeserializingField()
            {
                if (!m_DeserializingFieldStack.IsEmpty())
                {
                    return &m_DeserializingFieldStack.GetLast();
                }

                return NULL;
            }
       
        private:
            DynamicArray<DeserializingField> m_DeserializingFieldStack;
        };
    }
}

#endif