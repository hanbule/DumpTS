#pragma once

#include <stdint.h>
#include "Bitstream.h"
#include "combase.h"
#include <algorithm>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4200)
#pragma warning(disable:4201)
#pragma warning(disable:4101)
#pragma warning(disable:4100)
#pragma warning(disable:4189)
#pragma warning(disable:4127)
#pragma pack(push,1)
#define PACKED
#else
#define PACKED __attribute__ ((__packed__))
#endif

namespace MPEG4System
{
	enum DescriptorTag
	{
		Forbidden = 0,
		ObjectDescrTag,
		InitialObjectDescrTag,
		ES_DescrTag,
		DecoderConfigDescrTag,
		DecSpecificInfoTag,
		SLConfigDescrTag,
		ContentIdentDescrTag,
		SupplContentIdentDescrTag,
		IPI_DescrPointerTag,
		IPMP_DescrPointerTag,
		IPMP_DescrTag,
		QoS_DescrTag,
		RegistrationDescrTag,
		ES_ID_IncTag,
		ES_ID_RefTag,
		MP4_IOD_Tag,
		MP4_OD_Tag,
		IPL_DescrPointerRefTag,
		ExtensionProfileLevelDescrTag,
		profileLevelIndicationIndexDescrTag,
		// 0x15-0x3F Reserved for ISO use
		ContentClassificationDescrTag = 0x40,
		KeyWordDescrTag,
		RatingDescrTag,
		LanguageDescrTag,
		ShortTextualDescrTag,
		ExpandedTextualDescrTag,
		ContentCreatorNameDescrTag,
		ContentCreationDateDescrTag,
		OCICreatorNameDescrTag,
		OCICreationDateDescrTag,
		SmpteCameraPositionDescrTag,
		SegmentDescrTag,
		MediaTimeDescrTag,
		// 0x4D-0x5F Reserved for ISO use (OCI extensions)
		IPMP_ToolsListDescrTag = 0x60,
		IPMP_ToolTag,
		M4MuxTimingDescrTag,
		M4MuxCodeTableDescrTag,
		ExtSLConfigDescrTag,
		M4MuxBufferSizeDescrTag,
		M4MuxIdentDescrTag,
		DependencyPointerTag,
		DependencyMarkerTag,
		M4MuxChannelDescrTag,
	};

	constexpr uint32_t GetDescHeaderSize(uint32_t size)
	{
		return size < (1 << 7) ? 1 : (
			   size < (1 << 14) ? 2 : (
			   size < (1 << 21) ? 3 : 4));
	}

	struct BaseDescriptor
	{
		uint64_t	start_bitpos = 0;
		uint8_t		tag;
		uint8_t		header_size = 0;
		uint8_t		reserved[2];
		uint32_t	sizeOfInstance = 0;

		virtual ~BaseDescriptor(){}

		virtual int Unpack(CBitstream& bs)
		{
			uint64_t left_bits = 0;
			start_bitpos = bs.Tell(&left_bits);
			AMP_Assert(start_bitpos % 8 == 0);
			
			if (left_bits < 8)
				return RET_CODE_ERROR;

			tag = bs.GetByte();
			left_bits -= 8;
			header_size++;

			uint8_t nextByte = 0, sizeByte = 0;
			do
			{
				if (left_bits < 8 || header_size >= 5)
					return RET_CODE_ERROR;

				nextByte = (uint8_t)bs.GetBits(1);
				sizeByte = (uint8_t)bs.GetBits(7);
				left_bits -= 8;
				header_size++;

				sizeOfInstance = (sizeOfInstance << 7) | sizeByte;
			} while (nextByte);

			return 0;
		}

		void SkipLeftBits(CBitstream& bs)
		{
			uint64_t left_bytes = LeftBytes(bs);
			if (left_bytes > 0)
				bs.SkipBits(left_bytes << 3);
		}

		uint32_t LeftBytes(CBitstream& bs)
		{
			if (sizeOfInstance == 0)
				return UINT32_MAX;

			uint64_t unpack_bits = bs.Tell() - start_bitpos;
			if ((unpack_bits >> 3) >= (1 << 28))
				return UINT32_MAX;

			AMP_Assert(unpack_bits % 8 == 0);

			if (sizeOfInstance + header_size  > ((unpack_bits) >> 3))
				return (uint32_t)(sizeOfInstance + header_size - ((unpack_bits) >> 3));

			return 0;
		}

		static int LoadDescriptor(CBitstream& bs, BaseDescriptor** ppDescr = nullptr);

	}PACKED;

	struct IPI_DescrPointer : public BaseDescriptor
	{
		uint16_t		IPI_ES_Id;

		virtual int Unpack(CBitstream& bs)
		{
			int iRet = BaseDescriptor::Unpack(bs);
			if (iRet < 0)
				return iRet;

			uint32_t left_bytes = LeftBytes(bs);
			if (left_bytes < sizeof(IPI_ES_Id))
			{
				iRet = RET_CODE_BUFFER_TOO_SMALL;
				goto done;
			}

			IPI_ES_Id = bs.GetWord();
			
		done:
			SkipLeftBits(bs);
			return iRet;
		}
	}PACKED;

	struct SupplementaryContentIdentificationDescriptor : public BaseDescriptor
	{
		uint8_t					languageCode[3];
		uint8_t					supplContentIdentifierTitleLength;
		std::vector<uint8_t>	supplContentIdentifierTitle;
		uint8_t					supplContentIdentifierValueLength;
		std::vector<uint8_t>	supplContentIdentifierValue;

		virtual int Unpack(CBitstream& bs)
		{
			int iRet = BaseDescriptor::Unpack(bs);
			if (iRet < 0)
				return iRet;

			uint32_t left_bytes = LeftBytes(bs);
			if (left_bytes < 4)
			{
				iRet = RET_CODE_BUFFER_TOO_SMALL;
				goto done;
			}

			bs.Read(languageCode, 3);
			supplContentIdentifierTitleLength = bs.GetByte();
			left_bytes -= 4;

			if (left_bytes < supplContentIdentifierTitleLength)
			{
				iRet = RET_CODE_BUFFER_TOO_SMALL;
				goto done;
			}

			supplContentIdentifierTitle.reserve(supplContentIdentifierTitleLength);
			bs.Read(&supplContentIdentifierTitle[0], supplContentIdentifierTitleLength);
			left_bytes -= supplContentIdentifierTitleLength;

			if (left_bytes < 1)
			{
				iRet = RET_CODE_BUFFER_TOO_SMALL;
				goto done;
			}

			supplContentIdentifierValueLength = bs.GetByte();
			left_bytes--;

			if (left_bytes < supplContentIdentifierValueLength)
			{
				iRet = RET_CODE_BUFFER_TOO_SMALL;
				goto done;
			}

			supplContentIdentifierValue.reserve(supplContentIdentifierValueLength);
			bs.Read(&supplContentIdentifierValue[0], supplContentIdentifierValueLength);

		done:
			SkipLeftBits(bs);
			return iRet;
		}
	};

	struct OCI_Descriptor : public BaseDescriptor
	{
	}PACKED;

	struct LanguageDescriptor : public OCI_Descriptor
	{
		uint8_t				languageCode[3];

		virtual int Unpack(CBitstream& bs)
		{
			int iRet = RET_CODE_SUCCESS;

			if ((iRet = OCI_Descriptor::Unpack(bs)) < 0)
				return iRet;

			uint32_t left_bytes = LeftBytes(bs);
			if (left_bytes < 3)
			{
				iRet = RET_CODE_BUFFER_TOO_SMALL;
				goto done;
			}

		done:
			SkipLeftBits(bs);
			return iRet; 
		}
	}PACKED;

	struct UnimplDescriptor : public BaseDescriptor
	{
		virtual int Unpack(CBitstream& bs)
		{
			int iRet = BaseDescriptor::Unpack(bs);
			SkipLeftBits(bs);
			return iRet;
		}
	}PACKED;

} // namespace  ISOBMFF

#ifdef _MSC_VER
#pragma pack(pop)
#pragma warning(pop)
#endif
#undef PACKED

