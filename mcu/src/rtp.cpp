#include <arpa/inet.h>
#include <stdlib.h>
#include "log.h"
#include "rtp.h"
#include "audio.h"
#include "h264/h264depacketizer.h"
#include "vp8/vp8depacketizer.h"
#include "bitstream.h"

void RTPPacket::ProcessExtensions(const RTPMap &extMap)
{
	//Check extensions
	if (GetX())
	{
		//Get extension data
		const BYTE* ext = GetExtensionData();
		//Get extesnion lenght
		WORD length = GetExtensionLength();
		//Read all
		while (length)
		{
			//Get header
			const BYTE header = *(ext++);
			//Decrease lenght
			length--;
			//If it is padding
			if (!header)
				//Next
				continue;
			//Get extension element id
			BYTE id = header >> 4;
			//GEt extenion element length
			BYTE n = (header & 0x0F) + 1;
			//Check consistency
			if (n>length)
				//Exit
				break;
			//Get mapped extension
			BYTE t = extMap.GetCodecForType(id);
			//Debug("-RTPExtension [type:%d,codec:%d]\n",id,t);
			//Check type
			switch (t)
			{
				case RTPHeaderExtension::SSRCAudioLevel:
					// The payload of the audio level header extension element can be
					// encoded using either the one-byte or two-byte 
					// 0                   1
					//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
					// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
					// |  ID   | len=0 |V| level       |
					// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
					// 0                   1                   2                   3
					//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
					// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
					// |  ID   | len=1 |V|   level     |      0x00     |      0x00     |
					// +-+-+-+-+-+-+-+-+-+-+-+-+-+-s+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
					//
					// Set extennsion
					extension.hasAudioLevel = true;
					extension.vad	= (*ext & 0x80) >> 7;
					extension.level	= (*ext & 0x7f);
					break;
				case RTPHeaderExtension::TimeOffset:
					//  0                   1                   2                   3
					//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
					// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
					// |  ID   | len=2 |              transmission offset              |
					// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
					//
					// Set extension
					extension.hasTimeOffset = true;
					extension.timeOffset = get3(ext,0);
					//Check if it is negative
					if (extension.timeOffset & 0x800000)
						  // Negative offset, correct sign for Word24 to Word32.
						extension.timeOffset |= 0xFF000000;
					break;
				case RTPHeaderExtension::AbsoluteSendTime:
					//  0                   1                   2                   3
					//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
					// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
					// |  ID   | len=2 |              absolute send time               |
					// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
					// Calculate absolute send time field (convert ms to 24-bit unsigned with 18 bit fractional part.
					// Encoding: Timestamp is in seconds, 24 bit 6.18 fixed point, yielding 64s wraparound and 3.8us resolution (one increment for each 477 bytes going out on a 1Gbps interface).
					// Set extension
					extension.hasAbsSentTime = true;
					extension.absSentTime = ((QWORD)get3(ext,0))*1000 >> 18;
					break;
				case RTPHeaderExtension::CoordinationOfVideoOrientation:
					// Bit#            7   6   5   4   3   2   1  0(LSB)
					// Definition      0   0   0   0   C   F   R1 R0
					// With the following definitions:
					// C = Camera: indicates the direction of the camera used for this video stream. It can be used by the MTSI client in receiver to e.g. display the received video differently depending on the source camera.
					//     0: Front-facing camera, facing the user. If camera direction is unknown by the sending MTSI client in the terminal then this is the default value used.
					// 1: Back-facing camera, facing away from the user.
					// F = Flip: indicates a horizontal (left-right flip) mirror operation on the video as sent on the link.
					//     0: No flip operation. If the sending MTSI client in terminal does not know if a horizontal mirror operation is necessary, then this is the default value used.
					//     1: Horizontal flip operation
					// R1, R0 = Rotation: indicates the rotation of the video as transmitted on the link. The receiver should rotate the video to compensate that rotation. E.g. a 90° Counter Clockwise rotation should be compensated by the receiver with a 90° Clockwise rotation prior to displaying.
					// Set extension
					extension.hasVideoOrientation = true;
					*((BYTE*)(&extension.cvo)) = *ext;
					break;
				case RTPHeaderExtension::TransportWideCC:
					//  0                   1                   2       
					//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 
					// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
					// |  ID   | L=1   |transport-wide sequence number | 
					// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
					extension.hasTransportWideCC = true;
					extension.transportSeqNum = (WORD)get2(ext,0);
					break;
				default:
					Debug("-Unknown or unmapped extension [%d]\n",id);
					break;
			}
			//Skip bytes
			ext+=n;
			length-=n;
		}

		//Debug("-RTPExtensions [vad=%d,level=%.2d,offset=%d,ts=%lld]\n",GetVAD(),GetLevel(),GetTimeOffset(),GetAbsSendTime());
	}
}

static DWORD GetRTCPHeaderLength(rtcp_common_t* header)
{
	return (ntohs(header->length)+1)*4;
}

static void SetRTCPHeaderLength(rtcp_common_t* header,DWORD size)
{
	header->length = htons(size/4-1);
}

class DummyAudioDepacketizer : public RTPDepacketizer
{
public:
	DummyAudioDepacketizer(DWORD codec) : RTPDepacketizer(MediaFrame::Audio,codec), frame((AudioCodec::Type)codec,8000)
	{

	}

	virtual ~DummyAudioDepacketizer()
	{

	}

	virtual void SetTimestamp(DWORD timestamp)
	{
		//Set timestamp
		frame.SetTimestamp(timestamp);
	}
	virtual MediaFrame* AddPacket(RTPPacket *packet)
	{
		//Check it is from same packet
		if (frame.GetTimeStamp()!=packet->GetTimestamp())
			//Reset frame
			ResetFrame();
		//Set timestamp
		frame.SetTimestamp(packet->GetTimestamp());
		//Add payload
		AddPayload(packet->GetMediaData(),packet->GetMediaLength());
		//If it is last return frame
		return packet->GetMark() ? &frame : NULL;
	}
	virtual MediaFrame* AddPayload(BYTE* payload,DWORD payload_len)
	{
		//And data
		DWORD pos = frame.AppendMedia(payload, payload_len);
		//Add RTP packet
		frame.AddRtpPacket(pos,payload_len,NULL,0);
		//Return it
		return &frame;
	}
	virtual void ResetFrame()
	{
		//Clear packetization info
		frame.ClearRTPPacketizationInfo();
		//Reset
		memset(frame.GetData(),0,frame.GetMaxMediaLength());
		//Clear length
		frame.SetLength(0);
	}
	virtual DWORD GetTimestamp() 
	{
		return frame.GetTimeStamp();
	}
private:
	AudioFrame frame;
};

RTPDepacketizer* RTPDepacketizer::Create(MediaFrame::Type mediaType,DWORD codec)
{
	 switch (mediaType)
	 {
		 case MediaFrame::Video:
			 //Depending on the codec
			 switch((VideoCodec::Type)codec)
			 {
				 case VideoCodec::H264:
					 return new H264Depacketizer();
				 case VideoCodec::VP8:
					 return new VP8Depacketizer();                
			 }
			 break;
		 case MediaFrame::Audio:
			 //Dummy depacketizer
			 return new DummyAudioDepacketizer(codec);
			 break;
	 }
	 return NULL;
}

void RTCPPacket::Dump()
{
	Debug("\t[RTCPpacket type=%s size=%d/]\n",TypeToString(type),GetSize());
}

void RTCPCompoundPacket::Dump()
{
	Debug("[RTCPCompoundPacket count=%d size=%d]\n",packets.size(),GetSize());
	//For each one
	for(RTCPPackets::iterator it = packets.begin(); it!=packets.end(); ++it)
		//Dump
		(*it)->Dump();
	Debug("[/RTCPCompoundPacket]\n");
}
RTCPCompoundPacket* RTCPCompoundPacket::Parse(BYTE *data,DWORD size)
{
	//Check if it is an RTCP valid header
	if (!IsRTCP(data,size))
		//Exit
		return NULL;
	//Create pacekt
	RTCPCompoundPacket* rtcp = new RTCPCompoundPacket();
	//Init pointers
	BYTE *buffer = data;
	DWORD bufferLen = size;
	//Parse
	while (bufferLen)
	{
		RTCPPacket *packet = NULL;
		//Get header
		rtcp_common_t* header = (rtcp_common_t*) buffer;
		//Get type
		RTCPPacket::Type type = (RTCPPacket::Type)header->pt;
		//Create new packet
		switch (type)
		{
			case RTCPPacket::SenderReport:
				//Create packet
				packet = new RTCPSenderReport();
				break;
			case RTCPPacket::ReceiverReport:
				//Create packet
				packet = new RTCPReceiverReport();
				break;
			case RTCPPacket::SDES:
				//Create packet
				packet = new RTCPSDES();
				break;
			case RTCPPacket::Bye:
				//Create packet
				packet = new RTCPBye();
				break;
			case RTCPPacket::App:
				//Create packet
				packet = new RTCPApp();
				break;
			case RTCPPacket::RTPFeedback:
				//Create packet
				packet = new RTCPRTPFeedback();
				break;
			case RTCPPacket::PayloadFeedback:
				//Create packet
				packet = new RTCPPayloadFeedback();
				break;
			case RTCPPacket::FullIntraRequest:
				//Create packet
				packet = new RTCPFullIntraRequest();
				break;
			case RTCPPacket::NACK:
				//Create packet
				packet = new RTCPNACK();
				break;
			case RTCPPacket::ExtendedJitterReport:
				//Create packet
				packet = new RTCPExtendedJitterReport();
				break;
			default:
				//Skip
				Debug("Unknown rtcp packet type [%d]\n",header->pt);
		}
		//Get size of the packet
		DWORD len = GetRTCPHeaderLength(header);
		//Check len
		if (len>bufferLen)
		{
			//error
			Error("Wrong rtcp packet size\n");
			//Exit
			return NULL;
		}
		//parse
		if (packet && packet->Parse(buffer,len))
			//Add packet
			rtcp->AddRTCPacket(packet);
		//Remove size
		bufferLen -= len;
		//Increase pointer
		buffer    += len;
	}

	//Return it
	return rtcp;
}
RTCPSenderReport::RTCPSenderReport() : RTCPPacket(RTCPPacket::SenderReport)
{
	ssrc = 0;
	ntpSec = 0;
	ntpFrac = 0;
	rtpTimestamp = 0;
	packetsSent = 0;
	octectsSent = 0;
}

RTCPSenderReport::~RTCPSenderReport()
{
	for(Reports::iterator it = reports.begin();it!=reports.end();++it)
		delete(*it);
}

void RTCPSenderReport::SetTimestamp(timeval *tv)
{
	/*
	   Wallclock time (absolute date and time) is represented using the
	   timestamp format of the Network Time Protocol (NTP), which is in
	   seconds relative to 0h UTC on 1 January 1900 [4].  The full
	   resolution NTP timestamp is a 64-bit unsigned fixed-point number with
	   the integer part in the first 32 bits and the fractional part in the
	   last 32 bits.  In some fields where a more compact representation is
	   appropriate, only the middle 32 bits are used; that is, the low 16
	   bits of the integer part and the high 16 bits of the fractional part.
	   The high 16 bits of the integer part must be determined
	   independently.
	 */

	//Convert from ecpoch (JAN_1970) to NTP (JAN 1900);
	SetNTPSec(tv->tv_sec + 2208988800UL);
	//Convert microsecods to 32 bits fraction
	SetNTPFrac(tv->tv_usec*4294.967296);
}

void RTCPSenderReport::GetTimestamp(timeval *tv) const
{
	//Convert to epcoh JAN_1970
	tv->tv_sec = ntpSec - 2208988800UL;
	//Add fraction of
	tv->tv_usec = ntpFrac/4294.967296;
}

QWORD RTCPSenderReport::GetTimestamp() const
{
	//Convert to epcoh JAN_1970
	QWORD ts = ntpSec - 2208988800UL;
	//convert to microseconds
	ts *=1E6;
	//Add fraction
	ts += ntpFrac/4294.967296;
	//Return it
	return ts;
}

void RTCPSenderReport::Dump()
{
	Debug("\t[RTCPSenderReport ssrc=%u count=%u \n",ssrc,reports.size());
	Debug("\t\tntpSec=%u\n"		,ntpSec);
	Debug("\t\tntpFrac=%u\n"	,ntpFrac);
	Debug("\t\trtpTimestamp=%u\n"	,rtpTimestamp);
	Debug("\t\tpacketsSent=%u\n"	,packetsSent);
	Debug("\t\toctectsSent=%u\n"	,octectsSent);
	if (reports.size())
	{
		Debug("\t]\n");
		for(Reports::iterator it = reports.begin();it!=reports.end();++it)
			(*it)->Dump();
		Debug("\t[/RTCPSenderReport]\n");
	} else
		Debug("\t/]\n");
}

DWORD RTCPSenderReport::GetSize()
{
	return sizeof(rtcp_common_t)+24+24*reports.size();
}

DWORD RTCPSenderReport::Parse(BYTE* data,DWORD size)
{
	//Get header
	rtcp_common_t * header = (rtcp_common_t *)data;

	//Check size
	if (size<GetRTCPHeaderLength(header))
		//Exit
		return 0;
	//Skip headder
	DWORD len = sizeof(rtcp_common_t);
	//Get info
	ssrc		= get4(data,len);
	ntpSec		= get4(data,len+4);
	ntpFrac		= get4(data,len+8);
	rtpTimestamp	= get4(data,len+12);
	packetsSent	= get4(data,len+16);
	octectsSent	= get4(data,len+20);
	//Move forward
	len += 24;
	//for each
	for(int i=0;i<header->count&&size>=len+24;i++)
	{
		//New report
		RTCPReport* report = new RTCPReport();
		//parse
		len += report->Parse(data+len,size-len);
		//Add it
		AddReport(report);
	}
	
	//Return total size
	return len;
}

DWORD RTCPSenderReport::Serialize(BYTE* data,DWORD size)
{
	//Get packet size
	DWORD packetSize = GetSize();
	//Check size
	if (size<packetSize)
		//error
		return Error("Serialize RTCPSenderReport invalid size\n");
	//Set header
	rtcp_common_t * header = (rtcp_common_t *)data;
	//Set values
	header->count	= reports.size();
	header->pt	= GetType();
	header->p	= 0;
	header->version = 2;
	SetRTCPHeaderLength(header,packetSize);
	//Skip
	DWORD len = sizeof(rtcp_common_t);
	//Set values
	set4(data,len,ssrc);
	set4(data,len+4,ntpSec);
	set4(data,len+8,ntpFrac);
	set4(data,len+12,rtpTimestamp);
	set4(data,len+16,packetsSent);
	set4(data,len+20,octectsSent);
	//Next
	len += 24;
	//for each
	for(int i=0;i<header->count;i++)
		//Serialize
		len += reports[i]->Serialize(data+len,size-len);
	//return
	return len;
}

RTCPReceiverReport::RTCPReceiverReport() : RTCPPacket(RTCPPacket::ReceiverReport)
{

}

RTCPReceiverReport::~RTCPReceiverReport()
{
	for(Reports::iterator it = reports.begin();it!=reports.end();++it)
		delete(*it);
}

void RTCPReceiverReport::Dump()
{
	if (reports.size())
	{
		Debug("\t[RTCPReceiverReport ssrc=%u count=%u]\n",ssrc,reports.size());
		for(Reports::iterator it = reports.begin();it!=reports.end();++it)
			(*it)->Dump();
		Debug("\t[/RTCPReceiverReport]\n");
	} else
		Debug("\t[RTCPReceiverReport ssrc=%u]\n",ssrc);
}

DWORD RTCPReceiverReport::GetSize()
{
	return sizeof(rtcp_common_t)+4+24*reports.size();
}

DWORD RTCPReceiverReport::Parse(BYTE* data,DWORD size)
{
	//Get header
	rtcp_common_t * header = (rtcp_common_t *)data;

	//Check size
	if (size<GetRTCPHeaderLength(header))
		//Exit
		return 0;
	//Skip headder
	DWORD len = sizeof(rtcp_common_t);
	//Get info
	ssrc = get4(data,len);
	//Move forward
	len += 4;
	//for each
	for(int i=0;i<header->count&&size>=len+24;i++)
	{
		//New report
		RTCPReport* report = new RTCPReport();
		//parse
		len += report->Parse(data+len,size-len);
		//Add it
		AddReport(report);
	}

	//Return total size
	return len;
}

DWORD RTCPReceiverReport::Serialize(BYTE* data,DWORD size)
{
	//Get packet size
	DWORD packetSize = GetSize();
	//Check size
	if (size<packetSize)
		//error
		return Error("Serialize RTCPReceiverReport invalid size\n");
	//Set header
	rtcp_common_t * header = (rtcp_common_t *)data;
	//Set values
	header->count	= reports.size();
	header->pt	= GetType();
	header->p	= 0;
	header->version = 2;
	SetRTCPHeaderLength(header,packetSize);
	//Skip
	DWORD len = sizeof(rtcp_common_t);
	//Set values
	set4(data,len,ssrc);
	//Next
	len += 4;
	//for each
	for(int i=0;i<header->count;i++)
		//Serialize
		len += reports[i]->Serialize(data+len,size-len);
	//return
	return len;
}

RTCPBye::RTCPBye() : RTCPPacket(RTCPPacket::Bye)
{
	//No reason
	reason = NULL;
}

RTCPBye::~RTCPBye()
{
	//Free
	if (reason)
		free(reason);
}
DWORD RTCPBye::GetSize()
{
	DWORD len = sizeof(rtcp_common_t)+4*ssrcs.size();
	if (reason)
		len += strlen(reason)+1;
	return len;
}

DWORD RTCPBye::Parse(BYTE* data,DWORD size)
{
	//Get header
	rtcp_common_t * header = (rtcp_common_t *)data;

	DWORD packetSize = GetRTCPHeaderLength(header);
	//Check size
	if (size<packetSize)
		//Exit
		return 0;
	//Skip headder
	DWORD len = sizeof(rtcp_common_t);
	//for each
	for(int i=0;i<header->count;i++)
	{
		//Get ssrc
		ssrcs.push_back(get4(data,len));
		//Add lenght
		len+=4;
	}

	//Check if more preseng
	if (packetSize>len)
	{
		//Get len or reason
		DWORD n = data[len];
		//Allocate mem
		reason = (char*)malloc(n+1);
		//Copy
		memcpy(reason,data+len+1,n);
		//End it
		reason[n] = 0;
		//Move
		len += n+1;
	}

	//Return total size
	return len;
}

DWORD RTCPBye::Serialize(BYTE* data,DWORD size)
{
	//Get packet size
	DWORD packetSize = GetSize();
	//Check size
	if (size<packetSize)
		//error
		return Error("Serialize RTCPBye invalid size\n");
	//Set header
	rtcp_common_t * header = (rtcp_common_t *)data;
	//Set values
	header->count	= ssrcs.size();
	header->pt	= GetType();
	header->p	= 0;
	header->version = 2;
	SetRTCPHeaderLength(header,packetSize);
	//Skip
	DWORD len = sizeof(rtcp_common_t);
	//for each
	for(int i=0;i<ssrcs.size();i++)
	{
		//Set ssrc
		set4(data,len,ssrcs[i]);
		//skip
		len += 4;
	}
	//Optional reason
	if (reason)
	{
		//Set reason length
		data[len] = strlen(reason);

		//Copy reason
		memcpy(data+len+1,reason,strlen(reason));

		//Add len
		len +=strlen(reason)+1;
	}

	//return
	return len;
}



RTCPExtendedJitterReport::RTCPExtendedJitterReport() : RTCPPacket(RTCPPacket::ExtendedJitterReport)
{
}

DWORD RTCPExtendedJitterReport::GetSize()
{
	return sizeof(rtcp_common_t)+4*jitters.size();
}

DWORD RTCPExtendedJitterReport::Parse(BYTE* data,DWORD size)
{
	//Get header
	rtcp_common_t * header = (rtcp_common_t *)data;

	//Check size
	if (size<GetRTCPHeaderLength(header))
		//Exit
		return 0;
	//Skip headder
	DWORD len = sizeof(rtcp_common_t);
	//for each
	for(int i=0;i<header->count;i++)
	{
		//Get ssrc
		jitters.push_back(get4(data,len));
		//Add lenght
		len+=4;
	}

	//Return total size
	return len;
}

DWORD RTCPExtendedJitterReport::Serialize(BYTE* data,DWORD size)
{
	//Get packet size
	DWORD packetSize = GetSize();
	//Check size
	if (size<packetSize)
		//error
		return Error("Serialize RTCPExtendedJitterReport invalid size\n");
	//Set header
	rtcp_common_t * header = (rtcp_common_t *)data;
	//Set values
	header->count	= jitters.size();
	header->pt	= GetType();
	header->p	= 0;
	header->version = 2;
	SetRTCPHeaderLength(header,packetSize);
	//Skip
	DWORD len = sizeof(rtcp_common_t);
	//for each
	for(int i=0;i<jitters.size();i++)
	{
		//Set ssrc
		set4(data,len,jitters[i]);
		//skip
		len += 4;
	}
	//return
	return len;
}
RTCPApp::RTCPApp() : RTCPPacket(RTCPPacket::App)
{
	data = NULL;
	size = 0;
	subtype = 0;
}

RTCPApp::~RTCPApp()
{
	if (data)
		free(data);
}

DWORD RTCPApp::GetSize()
{
	return sizeof(rtcp_common_t)+8+size;
}

DWORD RTCPApp::Serialize(BYTE* data, DWORD size)
{
	//Get packet size
	DWORD packetSize = GetSize();
	//Check size
	if (size<packetSize)
		//error
		return Error("Serialize RTCPApp invalid size\n");
	//Set header
	rtcp_common_t * header = (rtcp_common_t *)data;
	//Set values
	header->count	= subtype;
	header->pt	= GetType();
	header->p	= 0;
	header->version = 2;
	SetRTCPHeaderLength(header,packetSize);
	//Set lenght
	DWORD len = sizeof(rtcp_common_t);
	//Copy
	set4(data,len,ssrc);
	//Move
	len += 4;
	//Copy name
	memcpy(data+len,name,4);
	//Inc len
	len += 4;
	//Copy
	memcpy(data+len,this->data,this->size);
	//add it
	len += this->size;
	//return it
	return len;
}

DWORD RTCPApp::Parse(BYTE* data,DWORD size)
{
	//Get header
	rtcp_common_t * header = (rtcp_common_t *)data;

	//Get packet size
	DWORD packetSize = GetRTCPHeaderLength(header);
	//Check size
	if (size<packetSize)
		//Exit
		return 0;
	//Get subtype
	subtype = header->count;
	//Skip headder
	DWORD len = sizeof(rtcp_common_t);
	//Get ssrc
	ssrc = get4(data,len);
	//Move
	len += 4;
	//Copy name
	memcpy(name,data+len,4);
	//Skip
	len += 4;
	//Set size
	this->size = packetSize-len;
	//Allocate mem
	this->data = (BYTE*)malloc(this->size);
	//Copy data
	memcpy(this->data,data+len,this->size);
	//Skip
	len += this->size;
	//Copy
	return len;
}

RTCPRTPFeedback::RTCPRTPFeedback() : RTCPPacket(RTCPPacket::RTPFeedback)
{

}
RTCPRTPFeedback::~RTCPRTPFeedback()
{
	//For each field
	for (Fields::iterator it=fields.begin();it!=fields.end();++it)
		//delete it
		delete(*it);
}

DWORD RTCPRTPFeedback::GetSize()
{
	DWORD len = 8+sizeof(rtcp_common_t);
	//For each field
	for (Fields::iterator it=fields.begin();it!=fields.end();++it)
		//add size
		len += (*it)->GetSize();
	return len;
}

DWORD RTCPRTPFeedback::Parse(BYTE* data,DWORD size)
{
	//Get header
	rtcp_common_t * header = (rtcp_common_t *)data;

	//Get size decalred in header
	DWORD packetSize = GetRTCPHeaderLength(header);
	//Check size
	if (size<packetSize)
		//Exit
		return 0;
	//Get subtype
	feedbackType = (FeedbackType)header->count;
	//Skip headder
	DWORD len = sizeof(rtcp_common_t);
	//Get ssrcs
	senderSSRC = get4(data,len);
	mediaSSRC = get4(data,len+4);
	//skip fields
	len += 8;
	//While we have more
	while (len<packetSize)
	{
		Field *field = NULL;
		//Depending on the type
		switch(feedbackType)
		{
			case NACK:
				field = new NACKField();
				break;
			case TempMaxMediaStreamBitrateRequest:
			case TempMaxMediaStreamBitrateNotification:
				field = new TempMaxMediaStreamBitrateField();
				break;
			case TransportWideFeedbackMessage:
				field = new TransportWideFeedbackMessageField();
				break;
			default:
				return Error("Unknown RTCPRTPFeedback type [%d]\n",header->count);
		}
		//Parse field
		DWORD parsed = field->Parse(data+len,packetSize-len);
		//If not parsed
		if (!parsed)
			//Error
			return 0;
		//Add field
		fields.push_back(field);
		//Skip
		len += parsed;
	}
	//Return consumed len
	return len+12;
}

void RTCPRTPFeedback::Dump()
{
	Debug("\t[RTCPPacket Feedback %s sender:%u media:%u]\n",TypeToString(feedbackType),senderSSRC,mediaSSRC);
	for (int i=0;i<fields.size();i++)
	{
		//Check type
		switch(feedbackType)
		{
			case RTCPRTPFeedback::NACK:
			{
				BYTE blp[2];
				char str[17];
				//Get field
				NACKField* field = (NACKField*)fields[i];
				//Get BLP in BYTE[]
				set2(blp,0,field->blp);
				//Convert to binary
				BitReader r(blp,2);
				for (int j=0;j<16;j++)
					str[j] = r.Get(1) ? '1' : '0';
				str[16] = 0;
				//Debug
				Debug("\t\t[NACK pid:%d blp:%s /]\n",field->pid,str);
				break;
			}
			
			case RTCPRTPFeedback::TempMaxMediaStreamBitrateRequest:
			case RTCPRTPFeedback::TempMaxMediaStreamBitrateNotification:
				break;
			case RTCPRTPFeedback::TransportWideFeedbackMessage:
			{
				//Get field
				TransportWideFeedbackMessageField *tw = (TransportWideFeedbackMessageField*)fields[i];
				//Debug
				Debug("\t\t[TransportWideFeedbackMessage seq:%d]\n",tw->feedbackPacketCount);
				//For each packet
				for (RTCPRTPFeedback::TransportWideFeedbackMessageField::Packets::iterator it = tw->packets.begin(); it!=tw->packets.end(); ++it)
					//DEbub
					Debug("\t\t\t[Pakcet seq:%u time=%llu/]\n",it->first,it->second);
				//Debug
				Debug("\t\t[TransportWideFeedbackMessage/]\n");
			}
		}
	}
	Debug("\t[/RTCPPacket Feedback %s]\n",TypeToString(feedbackType));
}
DWORD RTCPRTPFeedback::Serialize(BYTE* data,DWORD size)
{
	//Get packet size
	DWORD packetSize = GetSize();
	//Check size
	if (size<packetSize)
		//error
		return Error("Serialize RTCPRTPFeedback invalid size [size:%d,packetSize:%d]\n",size,packetSize);
	//Set header
	rtcp_common_t * header = (rtcp_common_t *)data;
	//Set values
	header->count	= feedbackType;
	header->pt	= GetType();
	header->p	= 0;
	header->version = 2;
	SetRTCPHeaderLength(header,packetSize);
	//Set lenght
	DWORD len = sizeof(rtcp_common_t);
	//Set ssrcs
	set4(data,len,senderSSRC);
	set4(data,len+4,mediaSSRC);
	//Inclrease len
	len += 8;
	//For each field
	for (Fields::iterator it=fields.begin();it!=fields.end();++it)
		//Serialize it
		len+=(*it)->Serialize(data+len,size-len);
	//Retrun writed data len
	return len;
}


RTCPPayloadFeedback::RTCPPayloadFeedback() : RTCPPacket(RTCPPacket::PayloadFeedback)
{

}

RTCPPayloadFeedback::~RTCPPayloadFeedback()
{
	//For each field
	for (Fields::iterator it=fields.begin();it!=fields.end();++it)
		//delete it
		delete(*it);
}

void RTCPPayloadFeedback::Dump()
{
	Debug("\t[RTCPPacket PayloadFeedback %s sender:%u media:%u]\n",TypeToString(feedbackType),senderSSRC,mediaSSRC);
	for (int i=0;i<fields.size();i++)
	{
		//Check type
		switch(feedbackType)
		{
			case RTCPPayloadFeedback::PictureLossIndication:
			case RTCPPayloadFeedback::FullIntraRequest:
			case RTCPPayloadFeedback::SliceLossIndication:
			case RTCPPayloadFeedback::ReferencePictureSelectionIndication:
			case RTCPPayloadFeedback::TemporalSpatialTradeOffRequest:
			case RTCPPayloadFeedback::TemporalSpatialTradeOffNotification:
			case RTCPPayloadFeedback::VideoBackChannelMessage:
				break;
			case RTCPPayloadFeedback:: ApplicationLayerFeeedbackMessage:
			{
				//Get field
				ApplicationLayerFeeedbackField* field = (ApplicationLayerFeeedbackField*)fields[i];
				//Get size and payload
				DWORD len		= field->GetLength();
				const BYTE* payload	= field->GetPayload();
				//Dump it
				::Dump(payload,len);
				//Check if it is a REMB
				if (len>8 && payload[0]=='R' && payload[1]=='E' && payload[2]=='M' && payload[3]=='B')
				{
					//Get num of ssrcs
					BYTE num = payload[4];
					//GEt exponent
					BYTE exp = payload[5] >> 2;
					DWORD mantisa = payload[5] & 0x03;
					mantisa = mantisa << 8 | payload[6];
					mantisa = mantisa << 8 | payload[7];
					//Get bitrate
					DWORD bitrate = mantisa << exp;
					//Log
					Debug("\t[REMB bitrate=%d exp=%d mantisa=%d/]\n",bitrate,exp,mantisa);
					//For each
					for (int i=0;i<num;++i)
						//Log
						Debug("\t[ssrc=%u/]\n",get4(payload,8+4*i));
					//Log
					Debug("\t[/REMB]\n");
					
				}
				
				break;
			}
		}
	}
	Debug("\t[/RTCPPacket PayloadFeedback %s]\n",TypeToString(feedbackType));
}
DWORD RTCPPayloadFeedback::GetSize()
{
	DWORD len = 8+sizeof(rtcp_common_t);
	//For each field
	for (Fields::iterator it=fields.begin();it!=fields.end();++it)
		//add size
		len += (*it)->GetSize();
	return len;
}

DWORD RTCPPayloadFeedback::Parse(BYTE* data,DWORD size)
{
	//Get header
	rtcp_common_t * header = (rtcp_common_t *)data;

	//Get packet size
	DWORD packetSize = GetRTCPHeaderLength(header);
	//Check size
	if (size<packetSize)
		//Exit
		return 0;
	//Get subtype
	feedbackType = (FeedbackType)header->count;
	//Skip headder
	DWORD len = sizeof(rtcp_common_t);
	//Get ssrcs
	senderSSRC = get4(data,len);
	mediaSSRC = get4(data,len+4);
	//skip fields
	len += 8;
	//While we have more
	while (len<packetSize)
	{
		Field *field = NULL;
		//Depending on the type
		switch(feedbackType)
		{
			case PictureLossIndication:
				return Error("PictureLossIndication with body\n");
			case SliceLossIndication:
				field = new SliceLossIndicationField();
				break;
			case ReferencePictureSelectionIndication:
				field = new ReferencePictureSelectionField();
				break;
			case FullIntraRequest:
				field = new FullIntraRequestField();
				break;
			case TemporalSpatialTradeOffRequest:
			case TemporalSpatialTradeOffNotification:
				field = new TemporalSpatialTradeOffField();
				break;
			case VideoBackChannelMessage:
				field = new VideoBackChannelMessageField();
				break;
			case ApplicationLayerFeeedbackMessage:
				field = new ApplicationLayerFeeedbackField();
				break;
			default:
				return Error("Unknown RTCPPayloadFeedback type [%d]\n",header->count);
		}
		//Parse field
		DWORD parsed = field->Parse(data+len,packetSize-len);
		//If not parsed
		if (!parsed)
			//Error
			return 0;
		//Add field
		fields.push_back(field);
		//Skip
		len += parsed;
	}
	//Return consumed len
	return len;
}

DWORD RTCPPayloadFeedback::Serialize(BYTE* data,DWORD size)
{
	//Get packet size
	DWORD packetSize = GetSize();
	//Check size
	if (size<packetSize)
		//error
		return Error("Serialize RTCPPayloadFeedback invalid size\n");
	//Set header
	rtcp_common_t * header = (rtcp_common_t *)data;
	//Set values
	header->count	= feedbackType;
	header->pt	= GetType();
	header->p	= 0;
	header->version = 2;
	SetRTCPHeaderLength(header,packetSize);
	//Set lenght
	DWORD len = sizeof(rtcp_common_t);
	//Set ssrcs
	set4(data,len,senderSSRC);
	set4(data,len+4,mediaSSRC);
	//Inclrease len
	len += 8;
	//For each field
	for (Fields::iterator it=fields.begin();it!=fields.end();++it)
		//Serialize it
		len+=(*it)->Serialize(data+len,size-len);
	//Retrun writed data len
	return len;
}

RTCPFullIntraRequest::RTCPFullIntraRequest() : RTCPPacket(RTCPPacket::FullIntraRequest)
{

}

DWORD RTCPFullIntraRequest::GetSize()
{
	return sizeof(rtcp_common_t)+4;
}

DWORD RTCPFullIntraRequest::Parse(BYTE* data,DWORD size)
{
	//Get header
	rtcp_common_t * header = (rtcp_common_t *)data;

	//Check size
	if (size<GetRTCPHeaderLength(header))
		//Exit
		return 0;
	//Skip headder
	DWORD len = sizeof(rtcp_common_t);
	//Get ssrcs
	ssrc = get4(data,len);
	//Return consumed len
	return len+4;
}

DWORD RTCPFullIntraRequest::Serialize(BYTE* data,DWORD size)
{
	//Get packet size
	DWORD packetSize = GetSize();
	//Check size
	if (size<packetSize)
		//error
		return Error("Serialize RTCPFullIntraRequest invalid size\n");
	//Set header
	rtcp_common_t * header = (rtcp_common_t *)data;
	//Set values
	header->count	= 0;
	header->pt	= GetType();
	header->p	= 0;
	header->version = 2;
	SetRTCPHeaderLength(header,packetSize);
	//Set lenght
	DWORD len = sizeof(rtcp_common_t);
	//Set ssrcs
	set4(data,len,ssrc);
	//Retrun writed data len
	return len+4;
}

RTCPNACK::RTCPNACK() : RTCPPacket(RTCPPacket::NACK)
{

}

DWORD RTCPNACK::GetSize()
{
	return sizeof(rtcp_common_t)+8;
}

DWORD RTCPNACK::Parse(BYTE* data,DWORD size)
{
	//Get header
	rtcp_common_t * header = (rtcp_common_t *)data;

	//Check size
	if (size<GetRTCPHeaderLength(header))
		//Exit
		return 0;
	//Skip headder
	DWORD len = sizeof(rtcp_common_t);
	//Get ssrcs
	ssrc = get4(data,len);
	fsn = get2(data,len+4);
	blp = get2(data,len+2);
	//Return consumed len
	return len+8;
}

DWORD RTCPNACK::Serialize(BYTE* data,DWORD size)
{
	//Get packet size
	DWORD packetSize = GetSize();
	//Check size
	if (size<packetSize)
		//error
		return Error("Serialize RTCPNACK invalid size\n");
	//Set header
	rtcp_common_t * header = (rtcp_common_t *)data;
	//Set values
	header->count	= 0;
	header->pt	= GetType();
	header->p	= 0;
	header->version = 2;
	SetRTCPHeaderLength(header,packetSize);
	//Set lenght
	DWORD len = sizeof(rtcp_common_t);
	//Set ssrcs
	set4(data,len,ssrc);
	set2(data,len+4,fsn);
	set2(data,len+6,blp);
	//Retrun writed data len
	return len+8;
}

RTCPSDES::RTCPSDES() : RTCPPacket(RTCPPacket::SDES)
{

}
RTCPSDES::~RTCPSDES()
{
	for (Descriptions::iterator it=descriptions.begin();it!=descriptions.end();++it)
		delete(*it);
}

void RTCPSDES::Dump()
{
	
	if (descriptions.size())
	{
		Debug("\t[RTCPSDES count=%u]\n",descriptions.size());
		for(Descriptions::iterator it = descriptions.begin();it!=descriptions.end();++it)
			(*it)->Dump();
		Debug("\t[/RTCPSDES]\n");
	} else
		Debug("\t[RTCPSDES/]\n");
}
DWORD RTCPSDES::GetSize()
{
	DWORD len = sizeof(rtcp_common_t);
	//For each field
	for (Descriptions::iterator it=descriptions.begin();it!=descriptions.end();++it)
		//add size
		len += (*it)->GetSize();
	return len;
}
DWORD RTCPSDES::Parse(BYTE* data,DWORD size)
{
	//Get header
	rtcp_common_t * header = (rtcp_common_t *)data;

	//Check size
	if (size<GetRTCPHeaderLength(header))
		//Exit
		return 0;
	//Skip headder
	DWORD len = sizeof(rtcp_common_t);
	//Parse fields
	DWORD i = 0;
	//While we have
	while (size>len && i<header->count)
	{
		Description *desc = new Description();
		//Parse field
		DWORD parsed = desc->Parse(data+len,size-len);
		//If not parsed
		if (!parsed)
			//Error
			return 0;
		//Add field
		descriptions.push_back(desc);
		//Skip
		len += parsed;
	}
	//Return consumed len
	return len;
}

DWORD RTCPSDES::Serialize(BYTE* data,DWORD size)
{
	//Get packet size
	DWORD packetSize = GetSize();
	//Check size
	if (size<packetSize)
		//error
		return Error("Serialize RTCPSDES invalid size\n");
	//Set header
	rtcp_common_t * header = (rtcp_common_t *)data;
	//Set values
	header->count	= descriptions.size();
	header->pt	= GetType();
	header->p	= 0;
	header->version = 2;
	SetRTCPHeaderLength(header,packetSize);
	//Skip headder
	DWORD len = sizeof(rtcp_common_t);
	//For each field
	for (Descriptions::iterator it=descriptions.begin();it!=descriptions.end();++it)
		//Serilize it
		len += (*it)->Serialize(data+len,size-len);

	//Return
	return len;
 }


RTCPSDES::Description::Description()
{

}
RTCPSDES::Description::Description(DWORD ssrc)
{
	this->ssrc = ssrc;
}
RTCPSDES::Description::~Description()
{
	for (Items::iterator it=items.begin();it!=items.end();++it)
		delete(*it);
}
void RTCPSDES::Description::Dump()
{
	if (items.size())
	{
		Debug("\t\t[Description ssrc=%u count=%u\n",ssrc,items.size());
		for(Items::iterator it=items.begin();it!=items.end();++it)
			Debug("\t\t\t[%s '%.*s'/]\n",RTCPSDES::Item::TypeToString((*it)->GetType()),(*it)->GetSize(),(*it)->GetData());
		Debug("\t\t[/Description]\n");
	} else
		Debug("\t\t[Description ssrc=%u/]\n",ssrc);
}

DWORD RTCPSDES::Description::GetSize()
{
	DWORD len = 4;
	//For each field
	for (Items::iterator it=items.begin();it!=items.end();++it)
		//add data size and header
		len += (*it)->GetSize()+2;
	//ADD end
	len+=1;
	//Return
	return pad32(len);
}
DWORD RTCPSDES::Description::Parse(BYTE* data,DWORD size)
{
	//Check size
	if (size<4)
		//Exit
		return 0;
	//Get ssrc
	ssrc = get4(data,0);
	//Skip ssrc
	DWORD len = 4;
	//While we have
	while (size>len+2 && data[len])
	{
		//Get tvalues
		RTCPSDES::Item::Type type = (RTCPSDES::Item::Type)data[len];
		BYTE length = data[len+1];
		//Check size
		if (len+2+length>size)
			//Error
			return 0;
		//Add item
		items.push_back( new Item(type,data+len+2,length));
		//Move
		len += length+2;
	}
	//Skip last
	len++;
	//Return consumed len
	return pad32(len);
}

DWORD RTCPSDES::Description::Serialize(BYTE* data,DWORD size)
{
	//Get packet size
	DWORD packetSize = GetSize();
	//Check size
	if (size<packetSize)
		//error
		return Error("Serialize RTCPSDES Description invalid size\n");
	//Set ssrc
	set4(data,0,ssrc);
	//Skip headder
	DWORD len = 4;
	//For each field
	for (Items::iterator it=items.begin();it!=items.end();++it)
	{
		//Get item
		Item *item = (*it);
		//Serilize it
		data[len++] = item->GetType();
		data[len++] = item->GetSize();
		//Copy data
		memcpy(data+len,item->GetData(),item->GetSize());
		//Move
		len += item->GetSize();
	}
	//Add null item
	data[len++] = 0;
	//Append nulls till pading
	memset(data+len,0,pad32(len)-len);
	//Return padded size
	return pad32(len);
 }

RTPRedundantPacket::RTPRedundantPacket(MediaFrame::Type media,BYTE *data,DWORD size) : RTPTimedPacket(media,data,size)
{
	//NO primary data yet
	primaryCodec = 0;
	primaryType = 0;
	primaryData = NULL;
	primarySize = 0;
	
	//Number of bytes to skip of text until primary data
	WORD skip = 0;

	//The the payload
	BYTE *payload = GetMediaData();

	//redundant counter
	WORD i = 0;

	//Check if it is the last
	bool last = !(payload[i]>>7);

	//Read redundant headers
	while(!last)
	{
		//Check it
		/*
		    0                   1                    2                   3
		    0 1 2 3 4 5 6 7 8 9 0 1 2 3  4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
		   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		   |F|   block PT  |  timestamp offset         |   block length    |
		   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		   F: 1 bit First bit in header indicates whether another header block
		       follows.  If 1 further header blocks follow, if 0 this is the
		       last header block.

		   block PT: 7 bits RTP payload type for this block.

		   timestamp offset:  14 bits Unsigned offset of timestamp of this block
		       relative to timestamp given in RTP header.  The use of an unsigned
		       offset implies that redundant payload must be sent after the primary
		       payload, and is hence a time to be subtracted from the current
		       timestamp to determine the timestamp of the payload for which this
		       block is the redundancy.

		   block length:  10 bits Length in bytes of the corresponding payload
		       block excluding header.

		 */

		//Get Type
		BYTE type = payload[i++] & 0x7F;
		//Get offset
		WORD offset = payload[i++];
		offset = offset <<6 | payload[i]>>2;
		//Get size
		WORD size = payload[i++] & 0x03;
		size = size <<6 | payload[i++];
		//Append new red header
		headers.push_back(RedHeader(type,offset,skip,size));
		//Skip the redundant payload
		skip += size;
		//Check if it is the last
		last = !(payload[i]>>7);
	}
	//Get primaty type
	primaryType = payload[i] & 0x7F;
	//Skip it
	i++;
	//Get redundant payload
	redundantData = payload+i;
	//Get prymary payload
	primaryData = redundantData+skip;
	//Get size of primary payload
	primarySize = GetMediaLength()-i-skip;
}

RTPTimedPacket* RTPRedundantPacket::CreatePrimaryPacket()
{
	//Create new pacekt
	RTPTimedPacket* packet = new RTPTimedPacket(GetMedia(),primaryCodec,primaryType);
	//Set attributes
	packet->SetClockRate(GetClockRate());
	packet->SetMark(GetMark());
	packet->SetSeqNum(GetSeqNum());
	packet->SetSeqCycles(GetSeqCycles());
	packet->SetTimestamp(GetTimestamp());
	packet->SetSSRC(GetSSRC());
	//Set paylaod
	packet->SetPayload(primaryData,primarySize);
	//Set time
	packet->SetTime(packet->GetTime());
	//Return it
	return packet;
}


RTPLostPackets::RTPLostPackets(WORD num)
{
	//Store number of packets
	size = num;
	//Create buffer
	packets = (QWORD*) malloc(num*sizeof(QWORD));
	//Set to 0
	memset(packets,0,size*sizeof(QWORD));
	//No first packet
	first = 0;
	//None yet
	len = 0;
}

void RTPLostPackets::Reset()
{
	//Set to 0
	memset(packets,0,size*sizeof(QWORD));
	//No first packet
	first = 0;
	//None yet
	len = 0;
}

RTPLostPackets::~RTPLostPackets()
{
	free(packets);
}

WORD RTPLostPackets::AddPacket(const RTPTimedPacket *packet)
{
	int lost = 0;
	
	//Get the packet number
	DWORD extSeq = packet->GetExtSeqNum();
	
	//Check if is befor first
	if (extSeq<first)
		//Exit, very old packet
		return 0;

	//If we are first
	if (!first)
		//Set to us
		first = extSeq;
	       
	//Get our position
	WORD pos = extSeq-first;
	
	//Check if we are still in window
	if (pos+1>size) 
	{
		//How much do we need to remove?
		int n = pos+1-size;
		//Check if we have to much to remove
		if (n>size)
			//cap it
			n = size;
		//Move
		memmove(packets,packets+n,(size-n)*sizeof(QWORD));
		//Fill with 0 the new ones
		memset(packets+(size-n),0,n*sizeof(QWORD));
		//Set first
		first = extSeq-size+1;
		//Full
		len = size;
		//We are last
		pos = size-1;
	} 
	
	//Check if it is last
	if (len-1<=pos)
	{
		//lock until we find a non lost and increase counter in the meanwhile
		for (int i=pos-1;i>=0 && !packets[i];--i)
			//Lost
			lost++;
		//Update last
		len = pos+1;
	}
	
	//Set
	packets[pos] = packet->GetTime();
	
	//Return lost ones
	return lost;
}


std::list<RTCPRTPFeedback::NACKField*> RTPLostPackets::GetNacks()
{
	std::list<RTCPRTPFeedback::NACKField*> nacks;
	WORD lost = 0;
	BYTE mask[2];
	BitWritter w(mask,2);
	int n = 0;
	
	//Iterate packets
	for(WORD i=0;i<len;i++)
	{
		//Are we in a lost count?
		if (lost)
		{
			//It was lost?
			w.Put(1,packets[i]==0);
			//Increase mask len
			n++;
			//If we are enought
			if (n==16)
			{
				//Flush
				w.Flush();
				//Add new NACK field to list
				nacks.push_back(new RTCPRTPFeedback::NACKField(lost,mask));
				//Empty for next
				w.Reset();
				//Reset counters
				n = 0;
				lost = 0;
			}
		}
		//Is this the first one lost
		else if (!packets[i]) {
			//This is the first one
			lost = first+i;
		}
		
	}
	
	//Are we in a lost count?
	if (lost)
	{
		//Fill reset with 0
		w.Put(16-n,0);
		//Flush
		w.Flush();
		//Add new NACK field to list
		nacks.push_back(new RTCPRTPFeedback::NACKField(lost,mask));
	}
	
	
	return nacks;
}

void  RTPLostPackets::Dump()
{
	Debug("[RTPLostPackets size=%d first=%d len=%d]\n",size,first,len);
	for(int i=0;i<len;i++)
		Debug("[%.3d,%.8d]\n",i,packets[i]);
	Debug("[/RTPLostPackets]\n");
}

DWORD RTCPRTPFeedback::TransportWideFeedbackMessageField::GetSize()
{
	//If we have no packets
	if (packets.size()==0)
		return 0;
	
	//Calculate temporal info
	WORD baseSeqNumber	= packets.begin()->first;
	DWORD referenceTime	= 0;
	WORD packetStatusCount	= packets.size();
		
	//Initial time in us
	QWORD time = 0;
	
	//Store delta array
	std::list<int> deltas;
	std::list<PacketStatus> statuses;
	PacketStatus lastStatus = PacketStatus::Reserved;
	PacketStatus maxStatus = PacketStatus::NotReceived;
	bool allsame = true;
	
	//Header
	DWORD len = 8;
	
	//For each packet 
	for (Packets::iterator it = packets.begin(); it!=packets.end(); ++it)
	{
		PacketStatus status = PacketStatus::NotReceived;
		
		//If got packet
		if (it->second)
		{
			//If first received
			if (!referenceTime)
			{
				//Set it
				referenceTime = (DWORD)(it->second/64000);
				//Get initial time
				time = referenceTime * 64000;
			}
			//Get delta
			int delta = (it->second-time)/250;
			//If it is negative or to big
			if (delta<0 || delta> 127)
				//Big one
				len += 2;
			else
				//Small
				len++;
		}
		
		//Check if they are different
		if (allsame && lastStatus!=PacketStatus::Reserved && status!=lastStatus)
		{
			//How big was the same run
			if ((maxStatus==PacketStatus::LargeOrNegativeDelta && statuses.size()>7) || (maxStatus<PacketStatus::LargeOrNegativeDelta && statuses.size()>14))
			{
				//One chunk
				len++;
				//Remove all statuses
				statuses.clear();
				//REset
				lastStatus = PacketStatus::Reserved;
				maxStatus = PacketStatus::NotReceived;
				allsame = true;
			}
			//Not same
			allsame = false;
		}
		//If it is bigger
		if (status>maxStatus)
			//Store it
			maxStatus = status;
		//Store las status
		lastStatus = status;

		//Check 
		if (maxStatus==PacketStatus::LargeOrNegativeDelta && statuses.size()>6)
		{
			//One chunk
			len++;
			//REset
			lastStatus = PacketStatus::Reserved;
			maxStatus = PacketStatus::NotReceived;
			allsame = true;
			//Calculate max of the rest
			for (Packets::iterator it=packets.begin();it!=packets.end();++it)
			{
				//Check if they are different
				if (allsame && lastStatus!=PacketStatus::Reserved && status!=lastStatus)
					//Not the same
					allsame = false;
				//If it is bigger
				if (status>maxStatus)
					//Store it
					maxStatus = status;
				//Store las status
				lastStatus = status;
			}
		} else if (statuses.size()>13) {
			//One chunk
			len++;
			//REset
			lastStatus = PacketStatus::Reserved;
			maxStatus = PacketStatus::NotReceived;
			allsame = true;
		} else {
			//Push back statuses, it will be handled later
			statuses.push_back(status);
		}
	}
	
	//If not finished yet
	if (statuses.size()>0)
		//One chunk more
		len++;
	
	//Add zero padding
	if (len%4)
		//DWORD boundary
		len += 4 - (len%4);
	
	//Done
	return len;
}

DWORD RTCPRTPFeedback::TransportWideFeedbackMessageField::Serialize(BYTE* data,DWORD size)
{
	//If we have no packets
	if (packets.size()==0)
		return 0;
	
	//Calculate temporal info
	WORD baseSeqNumber	= packets.begin()->first;
	QWORD referenceTime	= 0;
	WORD packetStatusCount	= packets.size();
			
	
	//Set data
	set2(data,0,baseSeqNumber);
	set2(data,2,packetStatusCount);
	//Set3 referenceTime when first received
	set1(data,7,feedbackPacketCount);
	
	//Initial time in us
	QWORD time = 0;
	
	//Store delta array
	std::list<int> deltas;
	std::list<PacketStatus> statuses;
	PacketStatus lastStatus = PacketStatus::Reserved;
	PacketStatus maxStatus = PacketStatus::NotReceived;
	bool allsame = true;
	
	//Bitwritter for the rest
	BitWritter writter(data+8,size-8);
	
	//For each packet 
	for (Packets::iterator it = packets.begin(); it!=packets.end(); ++it)
	{
		PacketStatus status = PacketStatus::NotReceived;
		
		//If got packet
		if (it->second)
		{
			//If first received
			if (!referenceTime)
			{
				//Set it
				referenceTime = (it->second/64000);
				//Get initial time
				time = referenceTime * 64000;
				//also in bufffer
				set3(data,4,referenceTime);
				
			}
			
			//Get delta
			int delta = (it->second-time)/250;
			//If it is negative or to big
			if (delta<0 || delta> 127)
				//Big one
				status = PacketStatus::LargeOrNegativeDelta;
			else
				//Small
				status = PacketStatus::SmallDelta;
			//Store delta
			deltas.push_back(delta);
			//Set next time
			time = it->second;
		}
		
		//Push back statuses, it will be handled later
		statuses.push_back(status);
		
		//Check if they are different
		if (allsame && lastStatus!=PacketStatus::Reserved && status!=lastStatus)
		{
			//How big was the same run
			if ((maxStatus==PacketStatus::LargeOrNegativeDelta && statuses.size()>7) || (maxStatus<PacketStatus::LargeOrNegativeDelta && statuses.size()>14))
			{
				//Write run!
				/*
					0                   1
					0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
				       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				       |T| S |       Run Length        |
				       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
					T = 0
				 */
				writter.Put(1,0);
				writter.Put(2,status);
				writter.Put(13,statuses.size());
				//Remove all statuses
				statuses.clear();
				//REset
				lastStatus = PacketStatus::Reserved;
				maxStatus = PacketStatus::NotReceived;
				allsame = true;
			}
			//Not same
			allsame = false;
		}
		//If it is bigger
		if (status>maxStatus)
			//Store it
			maxStatus = status;
		//Store las status
		lastStatus = status;

		//Check 
		if (maxStatus==PacketStatus::LargeOrNegativeDelta && statuses.size()>6)
		{
			/*
				0                   1
				0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
			       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
			       |T|S|        Symbols            |
			       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				T = 1
				S = 1
			 */
			writter.Put(1,1);
			writter.Put(1,1);
			//Set next 7
			for (int i=0;i<7;++i)
			{
				//Write
				writter.Put(2,(BYTE)statuses.front());
				//Remove
				statuses.pop_front();
			}
			//REset
			lastStatus = PacketStatus::Reserved;
			maxStatus = PacketStatus::NotReceived;
			allsame = true;
			//Calculate max of the rest
			for (Packets::iterator it=packets.begin();it!=packets.end();++it)
			{
				//Check if they are different
				if (allsame && lastStatus!=PacketStatus::Reserved && status!=lastStatus)
					//Not the same
					allsame = false;
				//If it is bigger
				if (status>maxStatus)
					//Store it
					maxStatus = status;
				//Store las status
				lastStatus = status;
			}
		} else if (statuses.size()>13) {
			/*
				0                   1
				0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
			       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
			       |T|S|       symbol list         |
			       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				 T = 1
				 S = 0
			 */
			writter.Put(1,1);
			writter.Put(1,0);
			//Set next 7
			for (int i=0;i<14;++i)
			{
				//Write
				writter.Put(1,(BYTE)statuses.front());
				//Remove
				statuses.pop_front();
			}
			//REset
			lastStatus = PacketStatus::Reserved;
			maxStatus = PacketStatus::NotReceived;
			allsame = true;
		} 
	}
	
	//If not finished yet
	if (statuses.size()>0)
	{
		//How big was the same run
		if (allsame)
		{
			//Write run!
			writter.Put(1,0);
			writter.Put(2,lastStatus);
			writter.Put(13,statuses.size());
		} else if (maxStatus==PacketStatus::LargeOrNegativeDelta) {
			//Write chunk
			writter.Put(1,1);
			writter.Put(1,1);
			//Wirte rest
			for (std::list<PacketStatus>::iterator it = statuses.begin(); it!= statuses.end(); ++it)
				//Write
				writter.Put(2,(BYTE)*it);
			//Write pending
			writter.Put(14-statuses.size()*2,0);
		} else {
			//Write chunck
			writter.Put(1,1);
			writter.Put(1,0);
			//Wirte rest
			for (std::list<PacketStatus>::iterator it = statuses.begin(); it!= statuses.end(); ++it)
				//Write
				writter.Put(1,(BYTE)*it);
			//Write pending
			writter.Put(14-statuses.size(),0);
		}
		
	}
	
	//Flush wirtter and aling, count also header
	DWORD len = writter.Flush()+8;
	
	//Write now the deltas
	for (std::list<int>::iterator it = deltas.begin(); it!=deltas.end(); ++it)
	{
		//Check size
		if (*it<0 || *it>128)
		{
			//2 bytes
			set2(data,len,(short)*it);
			//Inc
			len += 2;
		} else {
			//2 bytes
			set1(data,len,(BYTE)*it);
			//Inc
			len ++;
		}
	}
	//Add zero padding
	while (len%4)
		//Add padding
		data[len++] = 0;
	//Done
	return len;
}
DWORD RTCPRTPFeedback::TransportWideFeedbackMessageField::Parse(BYTE* data,DWORD size)
{
	//Create the  status count
	std::vector<PacketStatus> statuses;
	
	if (size<8) return 0;
	
	//This are temporal, only packet list count
	WORD baseSeqNumber	= get2(data,0);
	WORD packetStatusCount	= get2(data,2);
	QWORD referenceTime	= get3(data,4);
	//Store packet count
	feedbackPacketCount	= get1(data,7);

	//Rseserve initial space
	statuses.reserve(packetStatusCount);
	
	//Where we are 
	WORD len = 8;
	//Debug("-packetcount %d\n",packetStatusCount);
	//Get all
	while (statuses.size()<packetStatusCount)
	{
		//Ensure we have enought
		if (len+2>size)
			return 0;
		
		//Get chunk
		WORD chunk = get2(data,len);
		//Skip it
		len += 2;
		
		//Check packet type
		if (chunk>>15) 
		{
			//Debug("symbol %x\n",chunk);
			/*
				0                   1
				0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
			       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
			       |T|S|       symbol list         |
			       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				 T = 1
			*/ 
			//Get status size
			if (chunk>>14 & 1)
			{
				//Debug("-2 bits states\n");
				//S=1 => 7 states, 2 bits per state
				for (int j=0;j<7;++j)
				{
					//Get status
					PacketStatus status = (PacketStatus)((chunk >> 2 * (7 - 1 - j)) & 0x03);
					//Debug("status %d\n",status);
					//Push it back
					statuses.push_back(status);
					
				}
			} else {
				//Debug("-1 bits states\n");
				//S=> 14 states, 1 bit per state
				for (int j=0;j<14;++j)
				{
					//Get status
					PacketStatus status = (PacketStatus)((chunk >> (14 - 1 - j)) & 0x01);
					//Debug("status %d\n",status);
					//Push it back
					statuses.push_back(status);
					
				}
			}

		} else {
			
			/*
				0                   1
				0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
			       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
			       |T| S |       Run Length        |
			       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				T = 0
			 */
			//Get status
			PacketStatus status = (PacketStatus)(chunk>>13) ;
			//Run lengh
			WORD run = chunk & 0x1FFF;
			//Debug("run %d status %d\n",run,status);
			//For eachone
			for (WORD j=0;j<run;++j)
				//Append it
				statuses.push_back(status);
		}
	}
	QWORD time = referenceTime * 64000;
		
	//For each packet
	for (int i=0;i<packetStatusCount;++i)
	{
		//Depending on the status
		switch (statuses[i])
		{
			case PacketStatus::NotReceived:
				//Append not received
				packets[baseSeqNumber+i] = 0;
				break;
			case PacketStatus::SmallDelta:
			{
				//Check size
				if (len+1>size)
					return 0;
				//Read 1 length delta
				int delta = get1(data,len)* 250 ;
				//Add it to time
				time += delta;
				//Increase delta
				len += 1;
				//Append it
				packets[baseSeqNumber+i] = time;
				break;
			}
			case PacketStatus::LargeOrNegativeDelta:
			{
				//Check size
				if (len+2>size)
					return 0;
				//Read 2 length delta as signed short
				int delta = ((short)get2(data,len)) * 250 ;
				len += 2;
				//Increase delta
				time += delta;
				//Append it
				packets[baseSeqNumber+i] = time;
				break;	
			}
		}
	}
	//Skip zero padding
	if (len%4)
		//DWORD boundary
		len += 4 - (len%4);
	//Parsed
	return len;
}