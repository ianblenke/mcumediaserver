#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <signal.h>
#include <math.h>
#include "videostream.h"
#include "h263/h263codec.h"
#include "h263/mpeg4codec.h"
#include "h264/h264encoder.h"
#include "h264/h264decoder.h"
#include "log.h"
#include "tools.h"
#include "acumulator.h"
#include "RTPSmoother.h"


/**********************************
* VideoStream
*	Constructor
***********************************/
VideoStream::VideoStream(Listener* listener) : rtp(MediaFrame::Video,listener)
{
	//Inicializamos a cero todo
	sendingVideo=0;
	receivingVideo=0;
	videoInput=NULL;
	videoOutput=NULL;
	videoCodec=VideoCodec::H263_1996;
	videoCaptureMode=0;
	videoGrabWidth=0;
	videoGrabHeight=0;
	videoFPS=0;
	videoBitrate=0;
	videoIntraPeriod=0;
	videoBitrateLimit=0;
	videoBitrateLimitCount=0;
	sendFPU = false;
	this->listener = listener;
	mediaListener = NULL;
	muted = false;
	//Create objects
	pthread_mutex_init(&mutex,NULL);
	pthread_cond_init(&cond,NULL);
}

/*******************************
* ~VideoStream
*	Destructor. Cierra los dispositivos
********************************/
VideoStream::~VideoStream()
{
	//Clean object
	pthread_mutex_destroy(&mutex);
	pthread_cond_destroy(&cond);
}

/**********************************************
* SetVideoCodec
*	Fija el modo de envio de video 
**********************************************/
int VideoStream::SetVideoCodec(VideoCodec::Type codec,int mode,int fps,int bitrate,int quality, int fillLevel,int intraPeriod)
{
	Log("-SetVideoCodec [%s,%dfps,%dkbps,intra:%d]\n",VideoCodec::GetNameFor(codec),fps,bitrate,intraPeriod);

	//LO guardamos
	videoCodec=codec;

	//Guardamos el bitrate
	videoBitrate=bitrate;

	//The intra period
	if (intraPeriod>0)
		videoIntraPeriod = intraPeriod;

	//Get width and height
	videoGrabWidth = GetWidth(mode);
	videoGrabHeight = GetHeight(mode);

	//Check size
	if (!videoGrabWidth || !videoGrabHeight)
		//Error
		return Error("Unknown video mode\n");

	//Almacenamos el modo de captura
	videoCaptureMode=mode;

	//Y los fps
	videoFPS=fps;

	return 1;
}

int VideoStream::SetTemporalBitrateLimit(int bitrate)
{
	//Check limit with comfigured bitrate
	if (bitrate>videoBitrate)
		//Do nothing
		return 1;
	//Set bitrate limit
	videoBitrateLimit = bitrate;
	//Set limit of bitrate to 1 second;
	videoBitrateLimitCount = videoFPS;
	//Exit
	return 1;
}

/***************************************
* Init
*	Inicializa los devices 
***************************************/
int VideoStream::Init(VideoInput *input,VideoOutput *output)
{
	Log(">Init video stream\n");

	//Iniciamos el rtp
	if(!rtp.Init())
		return Error("No hemos podido abrir el rtp\n");

	//Init smoother
	smoother.Init(&rtp);

	//Guardamos los objetos
	videoInput  = input;
	videoOutput = output;
	
	//No estamos haciendo nada
	sendingVideo=0;
	receivingVideo=0;

	Log("<Init video stream\n");

	return 1;
}

int VideoStream::SetLocalCryptoSDES(const char* suite, const char* key64)
{
	return rtp.SetLocalCryptoSDES(suite,key64);
}

int VideoStream::SetRemoteCryptoSDES(const char* suite, const char* key64)
{
	return rtp.SetRemoteCryptoSDES(suite,key64);
}

int VideoStream::SetLocalSTUNCredentials(const char* username, const char* pwd)
{
	return rtp.SetLocalSTUNCredentials(username,pwd);
}

int VideoStream::SetRemoteSTUNCredentials(const char* username, const char* pwd)
{
	return rtp.SetRemoteSTUNCredentials(username,pwd);
}
int VideoStream::SetRTPProperties(const RTPSession::Properties& properties)
{
	return rtp.SetProperties(properties);
}
/**************************************
* startSendingVideo
*	Function helper for thread
**************************************/
void* VideoStream::startSendingVideo(void *par)
{
	Log("SendVideoThread [%d]\n",getpid());

	//OBtenemos el objeto
	VideoStream *conf = (VideoStream *)par;

	//Bloqueamos las se�ales
	blocksignals();

	//Y ejecutamos la funcion
	pthread_exit((void *)conf->SendVideo());
}

/**************************************
* startReceivingVideo
*	Function helper for thread
**************************************/
void* VideoStream::startReceivingVideo(void *par)
{
	Log("RecVideoThread [%d]\n",getpid());

	//Obtenemos el objeto
	VideoStream *conf = (VideoStream *)par;

	//Bloqueamos las se�a�es
	blocksignals();

	//Y ejecutamos
	pthread_exit( (void *)conf->RecVideo());
}

/***************************************
* StartSending
*	Comienza a mandar a la ip y puertos especificados
***************************************/
int VideoStream::StartSending(char *sendVideoIp,int sendVideoPort,RTPMap& rtpMap)
{
	Log(">StartSending video [%s,%d]\n",sendVideoIp,sendVideoPort);

	//Si estabamos mandando tenemos que parar
	if (sendingVideo)
		//Y esperamos que salga
		StopSending();

	//Si tenemos video
	if (sendVideoPort==0)
		return Error("No video\n");

	//Iniciamos las sesiones rtp de envio
	if(!rtp.SetRemotePort(sendVideoIp,sendVideoPort))
		return Error("Error abriendo puerto rtp\n");

	//Set sending map
	rtp.SetSendingRTPMap(rtpMap);
	
	//Set video codec
	if(!rtp.SetSendingCodec(videoCodec))
		//Error
		return Error("%s video codec not supported by peer\n",VideoCodec::GetNameFor(videoCodec));

	//Estamos mandando
	sendingVideo=1;

	//Arrancamos los procesos
	createPriorityThread(&sendVideoThread,startSendingVideo,this,0);

	//LOgeamos
	Log("<StartSending video [%d]\n",sendingVideo);

	return 1;
}

/***************************************
* StartReceiving
*	Abre los sockets y empieza la recetpcion
****************************************/
int VideoStream::StartReceiving(RTPMap& rtpMap)
{
	//Si estabamos reciviendo tenemos que parar
	if (receivingVideo)
		StopReceiving();	

	//Iniciamos las sesiones rtp de recepcion
	int recVideoPort= rtp.GetLocalPort();

	//Set receving map
	rtp.SetReceivingRTPMap(rtpMap);

	//Estamos recibiendo
	receivingVideo=1;

	//Arrancamos los procesos
	createPriorityThread(&recVideoThread,startReceivingVideo,this,0);

	//Logeamos
	Log("-StartReceiving Video [%d]\n",recVideoPort);

	return recVideoPort;
}

/***************************************
* End
*	Termina la conferencia activa
***************************************/
int VideoStream::End()
{
	Log(">End\n");

	//Terminamos la recepcion
	if (sendingVideo)
		StopSending();

	//Y el envio
	if(receivingVideo)
		StopReceiving();

	//Close smoother
	smoother.End();

	//Cerramos la session de rtp
	rtp.End();

	Log("<End\n");

	return 1;
}

/***************************************
* StopSending
*	Termina el envio
***************************************/
int VideoStream::StopSending()
{
	Log(">StopSending [%d]\n",sendingVideo);

	//Esperamos a que se cierren las threads de envio
	if (sendingVideo)
	{
		//Paramos el envio
		sendingVideo=0;

		//Cencel video grab
		videoInput->CancelGrabFrame();

		//Cancel sending
		pthread_cond_signal(&cond);

		//Y esperamos
		pthread_join(sendVideoThread,NULL);
	}

	Log("<StopSending\n");

	return 1;
}

/***************************************
* StopReceiving
*	Termina la recepcion
***************************************/
int VideoStream::StopReceiving()
{
	Log(">StopReceiving\n");

	//Y esperamos a que se cierren las threads de recepcion
	if (receivingVideo)
	{
		//Dejamos de recivir
		receivingVideo=0;

		//Cancel rtp
		rtp.CancelGetPacket();
		
		//Esperamos
		pthread_join(recVideoThread,NULL);
	}

	Log("<StopReceiving\n");

	return 1;
}

/*******************************************
* SendVideo
*	Capturamos el video y lo mandamos
*******************************************/
int VideoStream::SendVideo()
{
	timeval first;
	timeval prev;

	Acumulator bitrateAcu(1000);
	Acumulator fpsAcu(1000);
	
	Log(">SendVideo [width:%d,size:%d,bitrate:%d,fps:%d,intra:%d]\n",videoGrabWidth,videoGrabHeight,videoBitrate,videoFPS,videoIntraPeriod);

	//Creamos el encoder
	VideoEncoder* videoEncoder = VideoCodecFactory::CreateEncoder(videoCodec);

	//Comprobamos que se haya creado correctamente
	if (videoEncoder == NULL)
		//error
		return Error("Can't create video encoder\n");

	//Comrpobamos que tengamos video de entrada
	if (videoInput == NULL)
		return Error("No video input");

	//Iniciamos el tama�o del video
	if (!videoInput->StartVideoCapture(videoGrabWidth,videoGrabHeight,videoFPS))
		return Error("Couldn't set video capture\n");

	//Start at 80%
	int current = videoBitrate*0.8;

	//Iniciamos el birate y el framerate
	videoEncoder->SetFrameRate(videoFPS,current,videoIntraPeriod);

	//No wait for first
	DWORD frameTime = 0;

	//Iniciamos el tamama�o del encoder
 	videoEncoder->SetSize(videoGrabWidth,videoGrabHeight);

	//The time of the first one
	gettimeofday(&first,NULL);

	//The time of the previos one
	gettimeofday(&prev,NULL);
	
	//Started
	Log("-Sending video\n");

	//Mientras tengamos que capturar
	while(sendingVideo)
	{

		//Nos quedamos con el puntero antes de que lo cambien
		BYTE *pic = videoInput->GrabFrame();

		//Check picture
		if (!pic)
			//Exit
			continue;

		//Check if we need to send intra
		if (sendFPU)
		{
			//Set it
			videoEncoder->FastPictureUpdate();
			//Do not send anymore
			sendFPU = false;
		}

		//Calculate target bitrate
		int target = current;

		//Check temporal limits for estimations
		if (bitrateAcu.IsInWindow())
		{
			//Get real sent bitrate during last second and convert to kbits (*1000/1000)
			DWORD instant = bitrateAcu.GetInstantAvg();
			//Check if are not in quarentine period or sending below limits
			if (videoBitrateLimitCount || instant<videoBitrateLimit)
				//Increase a 8% each second o 10kbps
				target += fmax(target*0.08,10000)/videoFPS+1;
			else
				//Calculate decrease rate and apply it
				target = videoBitrateLimit;
		}

		//check max bitrate
		if (target>videoBitrate)
			//Set limit to max bitrate
			target = videoBitrate;

		//Check limits counter
		if (videoBitrateLimitCount>0)
			//One frame less of limit
			videoBitrateLimitCount--;

		//Check if we have a new bitrate
		if (target && target!=current)
		{
			//Reset bitrate
			videoEncoder->SetFrameRate(videoFPS,target,videoIntraPeriod);
			//Upate current
			current = target;
		}
		
		//Procesamos el frame
		VideoFrame *videoFrame = videoEncoder->EncodeFrame(pic,videoInput->GetBufferSize());

		//If was failed
		if (!videoFrame)
			//Next
			continue;

		//Increase frame counter
		fpsAcu.Update(getTime()/1000,1);
		
		//Check
		if (frameTime)
		{
			timespec ts;
			//Lock
			pthread_mutex_lock(&mutex);
			//Calculate timeout
			calcAbsTimeout(&ts,&prev,frameTime);
			//Wait next or stopped
			int canceled  = !pthread_cond_timedwait(&cond,&mutex,&ts);
			//Unlock
			pthread_mutex_unlock(&mutex);
			//Check if we have been canceled
			if (canceled)
				//Exit
				break;
		}
		
		//Set next one
		frameTime = 1000/videoFPS;

		//Add frame size in bits to bitrate calculator
		bitrateAcu.Update(getDifTime(&first)/1000,videoFrame->GetLength()*8);

		//Set frame timestamp
		videoFrame->SetTimestamp(getDifTime(&first)/1000);

		//Check if we have mediaListener
		if (mediaListener)
			//Call it
			mediaListener->onMediaFrame(*videoFrame);

		//Set sending time of previous frame
		getUpdDifTime(&prev);
		
		//Send it smoothly
		smoother.SendFrame(videoFrame,frameTime);
	}

	Log("-SendVideo out of loop\n");

	//Terminamos de capturar
	videoInput->StopVideoCapture();

	//Check
	if (videoEncoder)
		//Borramos el encoder
		delete videoEncoder;

	//Salimos
	Log("<SendVideo [%d]\n",sendingVideo);

	return 0;
}

/****************************************
* RecVideo
*	Obtiene los packetes y los muestra
*****************************************/
int VideoStream::RecVideo()
{
	VideoDecoder*	videoDecoder = NULL;
	VideoCodec::Type type;
	timeval 	before;
	timeval		lastFPURequest;
	DWORD		lostCount=0;
	DWORD		frameSeqNum = RTPPacket::MaxExtSeqNum;
	DWORD		lastSeq = RTPPacket::MaxExtSeqNum;
	bool		waitIntra = false;
	
	Log(">RecVideo\n");
	
	//Inicializamos el tiempo
	gettimeofday(&before,NULL);

	//Not sent FPU yet
	setZeroTime(&lastFPURequest);

	//Mientras tengamos que capturar
	while(receivingVideo)
	{
		//Obtenemos el paquete
		RTPPacket* packet = rtp.GetPacket();

		//Check
		if (!packet)
			//Next
			continue;

		//Get packet data
		BYTE* buffer = packet->GetMediaData();
		DWORD size = packet->GetMediaLength();

		//Get type
		type = (VideoCodec::Type)packet->GetCodec();

		//Check if it is a redundant packet
		if (type==VideoCodec::RED)
		{
			//Get redundant packet
			RTPRedundantPacket* red = (RTPRedundantPacket*)packet;
			//Get primary codec
			type = (VideoCodec::Type)red->GetPrimaryCodec();
			//Check it is not ULPFEC redundant packet
			if (type==VideoCodec::ULPFEC)
				//Skip
				continue;
			//Update primary redundant payload
			buffer = red->GetPrimaryPayloadData();
			size = red->GetPrimaryPayloadSize();
		}
		
		//Comprobamos el tipo
		if ((videoDecoder==NULL) || (type!=videoDecoder->type))
		{
			//Si habia uno nos lo cargamos
			if (videoDecoder!=NULL)
				delete videoDecoder;

			//Creamos uno dependiendo del tipo
			videoDecoder = VideoCodecFactory::CreateDecoder(type);

			//Si es nulo
			if (videoDecoder==NULL)
			{
				Error("Error creando nuevo decodificador de video [%d]\n",type);
				//Delete packet
				delete(packet);
				//Next
				continue;
			}
		}

		//Get extended sequence number
		DWORD seq = packet->GetExtSeqNum();

		//Check if we have lost the last packet from the previous frame
		if (seq>frameSeqNum)
		{
			//Try to decode what is in the buffer
			videoDecoder->DecodePacket(NULL,0,1,1);
			//Get picture
			BYTE *frame = videoDecoder->GetFrame();
			DWORD width = videoDecoder->GetWidth();
			DWORD height = videoDecoder->GetHeight();
			//Check values
			if (frame && width && height)
			{
				//Set frame size
				videoOutput->SetVideoSize(width,height);

				//Check if muted
				if (!muted)
					//Send it
					videoOutput->NextFrame(frame);
			}
		}

		//Lost packets since last
		DWORD lost = 0;

		//If not first
		if (lastSeq!=RTPPacket::MaxExtSeqNum)
			//Calculate losts
			lost = seq-lastSeq-1;

		//Increase total lost count
		lostCount += lost;

		//Update last sequence number
		lastSeq = seq;

		//Si hemos perdido un paquete or still have not got an iframe
		if(lostCount>1 || waitIntra)
		{
			//Check if we got listener and more than two seconds have elapsed from last request
			if (listener && getDifTime(&lastFPURequest)>1000000)
			{
				//Reset count
				lostCount = 0;
				//Debug
				Log("-Requesting FPU\n");
				//Request it
				listener->onRequestFPU();
				//Request also over rtp
				rtp.RequestFPU();
				//Update time
				getUpdDifTime(&lastFPURequest);
				//Waiting for refresh
				waitIntra = true;
			}
		}
		
		//Lo decodificamos
		if(!videoDecoder->DecodePacket(buffer,size,lost,packet->GetMark()))
		{
			//Check if we got listener and more than two seconds have elapsed from last request
			if (listener && getDifTime(&lastFPURequest)>1000000)
			{
				//Debug
				Log("-Requesting FPU\n");
				//Reset count
				lostCount = 0;
				//Request it
				listener->onRequestFPU();
				//Request also over rtp
				rtp.RequestFPU();
				//Update time
				getUpdDifTime(&lastFPURequest);
				//Waiting for refresh
				waitIntra = true;
			}
			//Delete packet
			delete(packet);
			//Next frame
			continue;
		}

		//Si es el ultimo
		if(packet->GetMark())
		{
			if (videoDecoder->IsKeyFrame())
				Log("-Got Intra\n");
			
			//No seq number for frame
			frameSeqNum = RTPPacket::MaxExtSeqNum;

			//Get picture
			BYTE *frame = videoDecoder->GetFrame();
			DWORD width = videoDecoder->GetWidth();
			DWORD height = videoDecoder->GetHeight();
			//Check values
			if (frame && width && height)
			{
				//Set frame size
				videoOutput->SetVideoSize(width,height);
				
				//Check if muted
				if (!muted)
					//Send it
					videoOutput->NextFrame(frame);
			}
			//Check if we got the waiting refresh
			if (waitIntra && videoDecoder->IsKeyFrame())
				//Do not wait anymore
				waitIntra = false;
		}
		//Delete packet
		delete(packet);
	}

	//Borramos el encoder
	delete videoDecoder;

	Log("<RecVideo\n");

	//Salimos
	pthread_exit(0);
}

int VideoStream::SetMediaListener(MediaFrame::Listener *listener)
{
	//Set it
	this->mediaListener = listener;
}

int VideoStream::SendFPU()
{
	//Next shall be an intra
	sendFPU = true;
	
	return 1;
}

MediaStatistics VideoStream::GetStatistics()
{
	MediaStatistics stats;

	//Fill stats
	stats.isReceiving	= IsReceiving();
	stats.isSending		= IsSending();
	stats.lostRecvPackets   = rtp.GetLostRecvPackets();
	stats.numRecvPackets	= rtp.GetNumRecvPackets();
	stats.numSendPackets	= rtp.GetNumSendPackets();
	stats.totalRecvBytes	= rtp.GetTotalRecvBytes();
	stats.totalSendBytes	= rtp.GetTotalSendBytes();

	//Return it
	return stats;
}

int VideoStream::SetMute(bool isMuted)
{
	//Set it
	muted = isMuted;
	//Exit
	return 1;
}
