#include "PersistPch.h"
#include "Persist/ArchiveJson.h"

#include "Foundation/Endian.h"
#include "Foundation/FileStream.h"
#include "Foundation/Numeric.h"

#include "Reflect/Object.h"
#include "Reflect/Structure.h"
#include "Reflect/Registry.h"
#include "Reflect/TranslatorDeduction.h"

using namespace Helium;
using namespace Helium::Reflect;
using namespace Helium::Persist;

ArchiveWriterJson::ArchiveWriterJson( const FilePath& path, ObjectIdentifier* identifier )
	: ArchiveWriter( path, identifier )
	, m_Writer( m_Output )
{
}

ArchiveWriterJson::ArchiveWriterJson( Stream *stream, ObjectIdentifier* identifier )
	: ArchiveWriter( identifier )
	, m_Writer( m_Output )
{
	m_Stream.Reset( stream );
	m_Stream.Orphan( true );
	m_Output.SetStream( stream );
}

ArchiveType ArchiveWriterJson::GetType() const
{
	return ArchiveTypes::Json;
}

void ArchiveWriterJson::Open()
{
#if PERSIST_ARCHIVE_VERBOSE
	Log::Print(TXT("Opening file '%s'\n"), m_Path.c_str());
#endif

	FileStream* stream = new FileStream();
	stream->Open( m_Path, FileStream::MODE_WRITE );
	m_Stream.Reset( stream );
	m_Output.SetStream( stream );
}

void ArchiveWriterJson::Close()
{
	HELIUM_ASSERT( m_Stream );
	m_Stream->Close();
	m_Stream.Release();
	m_Output.SetStream( NULL );
}

void ArchiveWriterJson::Write( Object* object )
{
	PERSIST_SCOPE_TIMER( ("Reflect - Json Write") );

	// notify starting
	ArchiveStatus info( *this, ArchiveStates::Starting );
	e_Status.Raise( info );

	// the master object
	m_Objects.Push( object );

	// begin top level array of objects
	m_Writer.StartArray();

	// objects can get changed during this iteration (in Identify), so use indices
	for ( int index = 0; index < m_Objects.GetSize(); ++index )
	{
		Object* object = m_Objects.GetElement( index );
		const Class* objectClass = object->GetClass();

		m_Writer.StartObject();
		m_Writer.String( objectClass->m_Name );
		SerializeInstance( object, objectClass, object );
		m_Writer.EndObject();

		info.m_State = ArchiveStates::ObjectProcessed;
		info.m_Progress = (int)(((float)(index) / (float)m_Objects.GetSize()) * 100.0f);
		e_Status.Raise( info );
	}

	// end top level array
	m_Writer.EndArray();

	// notify completion of last object processed
	info.m_State = ArchiveStates::ObjectProcessed;
	info.m_Progress = 100;
	e_Status.Raise( info );

	// do cleanup
	m_Stream->Flush();

	// notify completion
	info.m_State = ArchiveStates::Complete;
	e_Status.Raise( info );
}

void ArchiveWriterJson::SerializeInstance( void* instance, const Structure* structure, Object* object )
{
#if PERSIST_ARCHIVE_VERBOSE
	Log::Print( TXT( "Serializing %s\n" ), structure->m_Name );
#endif

#pragma TODO("Declare a max depth for inheritance to save heap allocs -geoff")
	DynamicArray< const Structure* > bases;
	for ( const Structure* current = structure; current != NULL; current = current->m_Base )
	{
		bases.Push( current );
	}

#pragma TODO("Declare a max count for fields to save heap allocs -geoff")
	DynamicArray< const Field* > fields;
	while ( !bases.IsEmpty() )
	{
		const Structure* current = bases.Pop();
		DynamicArray< Field >::ConstIterator itr = current->m_Fields.Begin();
		DynamicArray< Field >::ConstIterator end = current->m_Fields.End();
		for ( ; itr != end; ++itr )
		{
			const Field* field = &*itr;
			if ( field->ShouldSerialize( instance, object ) )
			{
				fields.Push( field );
			}
		}
	}

	m_Writer.StartObject();
	object->PreSerialize( NULL );

	DynamicArray< const Field* >::ConstIterator itr = fields.Begin();
	DynamicArray< const Field* >::ConstIterator end = fields.End();
	for ( ; itr != end; ++itr )
	{
		const Field* field = *itr;
		object->PreSerialize( field );
		SerializeField( instance, field, object );
		object->PostSerialize( field );
	}

	object->PostSerialize( NULL );
	m_Writer.EndObject();
}

void ArchiveWriterJson::SerializeField( void* instance, const Field* field, Object* object )
{
#if PERSIST_ARCHIVE_VERBOSE
	Log::Print(TXT("Serializing field %s\n"), field->m_Name);
#endif

	// write the actual string
	m_Writer.String( field->m_Name );

	if ( field->m_Count > 1 )
	{
		m_Writer.StartArray();

		for ( uint32_t i=0; i<field->m_Count; ++i )
		{
			SerializeTranslator( Pointer ( field, instance, object, i ), field->m_Translator, field, object );
		}

		m_Writer.EndArray();
	}
	else
	{
		SerializeTranslator( Pointer ( field, instance, object ), field->m_Translator, field, object );
	}
}

void ArchiveWriterJson::SerializeTranslator( Pointer pointer, Translator* translator, const Field* field, Object* object )
{
	switch ( translator->GetReflectionType() )
	{
	case ReflectionTypes::ScalarTranslator:
	case ReflectionTypes::EnumerationTranslator:
	case ReflectionTypes::PointerTranslator:
	case ReflectionTypes::TypeTranslator:
		{
			ScalarTranslator* scalar = static_cast< ScalarTranslator* >( translator );
			switch ( scalar->m_Type )
			{
			case ScalarTypes::Boolean:
				m_Writer.Bool( pointer.As<bool>() );
				break;

			case ScalarTypes::Unsigned8:
				m_Writer.Uint( pointer.As<uint8_t>() );
				break;

			case ScalarTypes::Unsigned16:
				m_Writer.Uint( pointer.As<uint16_t>() );
				break;

			case ScalarTypes::Unsigned32:
				m_Writer.Uint( pointer.As<uint32_t>() );
				break;

			case ScalarTypes::Unsigned64:
				m_Writer.Uint64( pointer.As<uint64_t>() );
				break;

			case ScalarTypes::Signed8:
				m_Writer.Int( pointer.As<int8_t>() );
				break;

			case ScalarTypes::Signed16:
				m_Writer.Int( pointer.As<int16_t>() );
				break;

			case ScalarTypes::Signed32:
				m_Writer.Int( pointer.As<int32_t>() );
				break;

			case ScalarTypes::Signed64:
				m_Writer.Int64( pointer.As<int64_t>() );
				break;

			case ScalarTypes::Float32:
				m_Writer.Double( pointer.As<float32_t>() );
				break;

			case ScalarTypes::Float64:
				m_Writer.Double( pointer.As<float64_t>() );
				break;

			case ScalarTypes::String:
				String str;
				scalar->Print( pointer, str, *this );
				m_Writer.String( str.GetData() );
				break;
			}
			break;
		}

	case ReflectionTypes::StructureTranslator:
		{
			StructureTranslator* structure = static_cast< StructureTranslator* >( translator );
			SerializeInstance( pointer.m_Address, structure->GetStructure(), object );
			break;
		}

	case ReflectionTypes::SetTranslator:
		{
			SetTranslator* set = static_cast< SetTranslator* >( translator );

			Translator* itemTranslator = set->GetItemTranslator();
			DynamicArray< Pointer > items;
			set->GetItems( pointer, items );

			m_Writer.StartArray();

			for ( DynamicArray< Pointer >::Iterator itr = items.Begin(), end = items.End(); itr != end; ++itr )
			{
				SerializeTranslator( *itr, itemTranslator, field, object );
			}

			m_Writer.EndArray();

			break;
		}

	case ReflectionTypes::SequenceTranslator:
		{
			SequenceTranslator* sequence = static_cast< SequenceTranslator* >( translator );

			Translator* itemTranslator = sequence->GetItemTranslator();
			DynamicArray< Pointer > items;
			sequence->GetItems( pointer, items );

			m_Writer.StartArray();

			for ( DynamicArray< Pointer >::Iterator itr = items.Begin(), end = items.End(); itr != end; ++itr )
			{
				SerializeTranslator( *itr, itemTranslator, field, object );
			}

			m_Writer.EndArray();

			break;
		}

	case ReflectionTypes::AssociationTranslator:
		{
			AssociationTranslator* association = static_cast< AssociationTranslator* >( translator );

			Translator* keyTranslator = association->GetKeyTranslator();
			Translator* valueTranslator = association->GetValueTranslator();
			DynamicArray< Pointer > keys, values;
			association->GetItems( pointer, keys, values );

			m_Writer.StartObject();

			for ( DynamicArray< Pointer >::Iterator keyItr = keys.Begin(), valueItr = values.Begin(), keyEnd = keys.End(), valueEnd = values.End();
				keyItr != keyEnd && valueItr != valueEnd;
				++keyItr, ++valueItr )
			{
				SerializeTranslator( *keyItr, keyTranslator, field, object );
				SerializeTranslator( *valueItr, valueTranslator, field, object );
			}

			m_Writer.EndObject();

			break;
		}

	default:
		// Unhandled reflection type in ArchiveWriterJson::SerializeTranslator
		HELIUM_ASSERT_FALSE();
	}
}

void ArchiveWriterJson::ToStream( Object* object, Stream& stream, ObjectIdentifier* identifier, uint32_t flags )
{
	ArchiveWriterJson archive ( &stream, identifier );
	archive.m_Flags = flags;
	archive.Write( object );
	archive.Close();
}

ArchiveReaderJson::ArchiveReaderJson( const FilePath& path, ObjectResolver* resolver )
	: ArchiveReader( path, resolver )
	, m_Stream( NULL )
	, m_Next( 0 )
	, m_Size( 0 )
{

}

ArchiveReaderJson::ArchiveReaderJson( Stream *stream, ObjectResolver* resolver )
	: ArchiveReader( resolver )
	, m_Stream( NULL )
	, m_Next( 0 )
	, m_Size( 0 )
{
	m_Stream.Reset( stream );
	m_Stream.Orphan( true );
}

ArchiveType ArchiveReaderJson::GetType() const
{
	return ArchiveTypes::Json;
}

void ArchiveReaderJson::Open()
{
#if PERSIST_ARCHIVE_VERBOSE
	Log::Print(TXT("Opening file '%s'\n"), m_Path.c_str());
#endif

	FileStream* stream = new FileStream();
	stream->Open( m_Path, FileStream::MODE_READ );
	m_Stream.Reset( stream );
}

void ArchiveReaderJson::Close()
{
	HELIUM_ASSERT( m_Stream );
	m_Stream->Close();
	m_Stream.Release();
}

void ArchiveReaderJson::Read( Reflect::ObjectPtr& object )
{
	PERSIST_SCOPE_TIMER( ("Reflect - Json Read") );

	Start();

	if ( m_Document.IsArray() )
	{
		uint32_t length = m_Document.Size();
		m_Objects.Reserve( length );
		for ( uint32_t i=0; i<length; i++ )
		{
			ObjectPtr object;
			ReadNext( object );
			m_Objects.Push( object );

			ArchiveStatus info( *this, ArchiveStates::ObjectProcessed );
			info.m_Progress = (int)(((float)(m_Stream->Tell()) / (float)m_Size) * 100.0f);
			e_Status.Raise( info );
			m_Abort |= info.m_Abort;
			if ( m_Abort )
			{
				break;
			}
		}
	}

	Resolve();

	object = m_Objects.GetFirst();
}

void Helium::Persist::ArchiveReaderJson::Start()
{
	ArchiveStatus info( *this, ArchiveStates::Starting );
	e_Status.Raise( info );
	m_Abort = false;

	// determine the size of the input stream
	m_Stream->Seek(0, SeekOrigins::End);
	m_Size = m_Stream->Tell();
	m_Stream->Seek(0, SeekOrigins::Begin);

	// fail on an empty input stream
	if ( m_Size == 0 )
	{
		throw Persist::StreamException( TXT( "Input stream is empty (%s)" ), m_Path.c_str() );
	}

	// read entire contents
	m_Buffer.Resize( m_Size + 1 );
	m_Stream->Read( m_Buffer.GetData(),  m_Size, 1 );
	m_Buffer[ m_Size ] = '\0';
	if ( m_Document.ParseInsitu< 0 >( reinterpret_cast< char* >( m_Buffer.GetData() ) ).HasParseError() )
	{
		throw Persist::Exception( "Error parsing JSON: %s", m_Document.GetParseError() );
	}
}

void Helium::Persist::ArchiveReaderJson::ReadNext( Reflect::ObjectPtr& object )
{
	rapidjson::Value& value = m_Document[ m_Next++ ];
	if ( value.IsObject() )
	{
		rapidjson::Value::Member* member = value.MemberBegin();
		if ( member != value.MemberEnd() )
		{
			uint32_t objectClassCrc = 0;
			if ( member->name.IsString() )
			{
				String typeStr;
				typeStr = member->name.GetString();
				objectClassCrc = Helium::Crc32( typeStr.GetData() );
			}

			const Class* objectClass = NULL;
			if ( objectClassCrc != 0 )
			{
				objectClass = Registry::GetInstance()->GetClass( objectClassCrc );
			}
			
			if ( !object && objectClass )
			{
				object = Registry::GetInstance()->CreateInstance( objectClass );
			}

			if ( object.ReferencesObject() )
			{
				DeserializeInstance( member->value, object, object->GetClass(), object );
			}
		}
	}
}

void Helium::Persist::ArchiveReaderJson::Resolve()
{
	ArchiveStatus info( *this, ArchiveStates::ObjectProcessed );
	info.m_Progress = 100;
	e_Status.Raise( info );

	// finish linking objects (unless we have a custom handler)
	for ( DynamicArray< Fixup >::ConstIterator itr = m_Fixups.Begin(), end = m_Fixups.End(); itr != end; ++itr )
	{
		ArchiveReader::Resolve( itr->m_Identity, itr->m_Pointer, itr->m_PointerClass );
	}

	info.m_State = ArchiveStates::Complete;
	e_Status.Raise( info );
}

void ArchiveReaderJson::DeserializeInstance( rapidjson::Value& value, void* instance, const Structure* structure, Object* object )
{
#if PERSIST_ARCHIVE_VERBOSE
	Log::Print(TXT("Deserializing %s\n"), structure->m_Name);
#endif

	object->PreDeserialize( NULL );

	if ( value.IsObject() )
	{
		for ( rapidjson::Value::MemberIterator itr = value.MemberBegin(), end = value.MemberEnd(); itr != end; ++itr )
		{
			uint32_t fieldCrc = 0;
			if ( itr->name.IsString() )
			{
				String fieldStr;
				fieldStr = itr->name.GetString();
				fieldCrc = Helium::Crc32( fieldStr.GetData() );
			}

			const Field* field = structure->FindFieldByName( fieldCrc );
			if ( field )
			{
				object->PreDeserialize( field );

				DeserializeField( itr->value, instance, field, object );

				object->PostDeserialize( field );
			}
		}
	}

	object->PostDeserialize( NULL );
}

void ArchiveReaderJson::DeserializeField( rapidjson::Value& value, void* instance, const Field* field, Object* object )
{
#if PERSIST_ARCHIVE_VERBOSE
	Log::Print(TXT("Deserializing field %s\n"), field->m_Name);
#endif
	
	if ( field->m_Count > 1 )
	{
		if ( value.IsArray() )
		{
			for ( uint32_t i=0; i<value.Size(); ++i )
			{
				if ( i < field->m_Count )
				{
					DeserializeTranslator( value[ i ], Pointer ( field, instance, object, i ), field->m_Translator, field, object );
				}
			}
		}
		else
		{
			DeserializeTranslator( value, Pointer ( field, instance, object, 0 ), field->m_Translator, field, object );
		}
	}
	else
	{
		DeserializeTranslator( value, Pointer ( field, instance, object ), field->m_Translator, field, object );
	}
}

void ArchiveReaderJson::DeserializeTranslator( rapidjson::Value& value, Pointer pointer, Translator* translator, const Field* field, Object* object )
{
	if ( value.IsBool() )
	{
		if ( translator->GetReflectionType() == ReflectionTypes::ScalarTranslator )
		{
			ScalarTranslator* scalar = static_cast< ScalarTranslator* >( translator );
			if ( scalar->m_Type == ScalarTypes::Boolean )
			{
				pointer.As<bool>() = value.IsTrue();
			}
		}
	}
	else if ( value.IsNumber() )
	{
		if ( translator->GetReflectionType() == ReflectionTypes::ScalarTranslator )
		{
			ScalarTranslator* scalar = static_cast< ScalarTranslator* >( translator );
			bool clamp = true;
			switch ( scalar->m_Type )
			{
			case ScalarTypes::Unsigned8:
				RangeCastInteger( value.GetUint(), pointer.As<uint8_t>(), clamp );
				break;

			case ScalarTypes::Unsigned16:
				RangeCastInteger( value.GetUint(), pointer.As<uint16_t>(), clamp );
				break;

			case ScalarTypes::Unsigned32:
				RangeCastInteger( value.GetUint(), pointer.As<uint32_t>(), clamp );
				break;

			case ScalarTypes::Unsigned64:
				RangeCastInteger( value.GetUint64(), pointer.As<uint64_t>(), clamp );
				break;

			case ScalarTypes::Signed8:
				RangeCastInteger( value.GetInt(), pointer.As<int8_t>(), clamp );
				break;

			case ScalarTypes::Signed16:
				RangeCastInteger( value.GetInt(), pointer.As<int16_t>(), clamp );
				break;

			case ScalarTypes::Signed32:
				RangeCastInteger( value.GetInt(), pointer.As<int32_t>(), clamp );
				break;

			case ScalarTypes::Signed64:
				RangeCastInteger( value.GetInt64(), pointer.As<int64_t>(), clamp );
				break;

			case ScalarTypes::Float32:
				RangeCastFloat( value.GetDouble(), pointer.As<float32_t>(), clamp );
				break;

			case ScalarTypes::Float64:
				RangeCastFloat( value.GetDouble(), pointer.As<float64_t>(), clamp );
				break;
			}
		}
	}
	else if ( value.IsString() )
	{
		if ( translator->HasReflectionType( ReflectionTypes::ScalarTranslator ) )
		{
			ScalarTranslator* scalar = static_cast< ScalarTranslator* >( translator );
			if ( scalar->m_Type == ScalarTypes::String )
			{
				String str ( value.GetString() );
				scalar->Parse( str, pointer, *this, m_Flags | ArchiveFlags::Notify ? true : false );
			}
		}
	}
	else if ( value.IsArray() )
	{
		if ( translator->GetReflectionType() == ReflectionTypes::SetTranslator )
		{
			SetTranslator* set = static_cast< SetTranslator* >( translator );
			Translator* itemTranslator = set->GetItemTranslator();
			uint32_t length = value.Size();
			for ( uint32_t i=0; i<length; ++i )
			{
				Variable item ( itemTranslator );
				DeserializeTranslator( value[ i ], item, itemTranslator, field, object );
				set->InsertItem( pointer, item );
			}
		}
		else if ( translator->GetReflectionType() == ReflectionTypes::SequenceTranslator )
		{
			SequenceTranslator* sequence = static_cast< SequenceTranslator* >( translator );
			Translator* itemTranslator = sequence->GetItemTranslator();
			uint32_t length = value.Size();
			sequence->SetLength(pointer, length);
			for ( uint32_t i=0; i<length; ++i )
			{
				Pointer item = sequence->GetItem( pointer, i );
				DeserializeTranslator( value[ i ], item, itemTranslator, field, object );
			}
		}
	}
	else if ( value.IsObject() )
	{
		if ( translator->GetReflectionType() == ReflectionTypes::StructureTranslator )
		{
			StructureTranslator* structure = static_cast< StructureTranslator* >( translator );
			DeserializeInstance( value, pointer.m_Address,  structure->GetStructure(), object );
		}
		else if ( translator->GetReflectionType() == ReflectionTypes::AssociationTranslator )
		{
			AssociationTranslator* assocation = static_cast< AssociationTranslator* >( translator );
			Translator* keyTranslator = assocation->GetKeyTranslator();
			Translator* valueTranslator = assocation->GetValueTranslator();
			Variable keyVariable ( keyTranslator );
			Variable valueVariable ( valueTranslator );
			for ( rapidjson::Value::MemberIterator itr = value.MemberBegin(), end = value.MemberEnd(); itr != end; ++itr )
			{
				DeserializeTranslator( itr->name, keyVariable, keyTranslator, field, object );
				DeserializeTranslator( itr->value, valueVariable, valueTranslator, field, object );
				assocation->SetItem( pointer, keyVariable, valueVariable );
			}
		}
	}
}

ObjectPtr ArchiveReaderJson::FromStream( Stream& stream, ObjectResolver* resolver, uint32_t flags )
{
	ArchiveReaderJson archive( &stream, resolver );
	archive.m_Flags = flags;
	ObjectPtr object;
	archive.Read( object );
	archive.Close();
	return object;
}
