/* 
 * File:   main.cpp
 * Author: Sergio
 *
 * Created on 5 de enero de 2013, 23:14
 */
#include "config.h"
#include "log.h"
#include "rtp.h"
#include "rtpsession.h"
#include <string.h>


class Sender : public RTPSession::Listener
{
public:
	Sender()
	{
		//Set default values
		ip = NULL;
		port = 0;
		localPort = 0;
		rate = 512000;
		rateMin = 0;
		fps = 30;
		gop = 300;
		maxRTPSize = 1300;
		scaleIP = 1;
		sending = false;
	}

	virtual void onFPURequested(RTPSession *session)
	{
		Log("-onFPURequested\n");
	}
	
	virtual void onReceiverEstimatedMaxBitrate(RTPSession *session,DWORD bitrate)
	{

	}

	virtual void onTempMaxMediaStreamBitrateRequest(RTPSession *session,DWORD bitrate,DWORD overhead)
	{
		
	}

	bool Start()
	{
		//Check params
		if (!ip)
			return Error("Destination IP not set\n");
		if (!port)
			return Error("Destination port not set\n");

		//Check if no min rate has been set
		if (!rateMin)
			//Set to max, so no variability
			rateMin = rate;

		//Ensure rate constrinst
		if (rateMin>rate)
			return Error("Rate [%d] has to be bigger than the min rate [%d]\n",rate,rateMin);

		//We are sending
		sending = 1;
		
		//Start thread
		return pthread_create(&thread,NULL,run,this)==0;
	}

	bool Stop()
	{
		//Stop
		sending = 0;
		//Wait for sending thread
		pthread_join(thread,NULL);
		//Ok
		return true;
	}

	void setSending(bool sending)		{ this->sending = sending;		}
        void setScaleIP(DWORD scaleIP)		{ this->scaleIP = scaleIP;		}
        void setMaxRTPSize(DWORD maxRTPSize)	{ this->maxRTPSize = maxRTPSize;        }
        void setGop(DWORD gop)			{ this->gop = gop;			}
        void setFps(DWORD fps)			{ this->fps = fps;			}
        void setRate(DWORD rate)		{ this->rate = rate;			}
	void setRateMin(DWORD rate)		{ this->rateMin = rate;			}
        void setPort(int port)			{ this->port = port;			}
	void setLocalPort(int port)		{ this->localPort = port;		}
        void setIp(char* ip)			{ this->ip = ip;			}

private:
	static void * run(void *par)
	{
		Sender *sender = (Sender *)par;
		sender->Run();
		pthread_exit(0);
	}
	
protected:
	void Run()
	{
		timeval ini;

		//RTP objects
		RTPSession sess(MediaFrame::Video,this);
		RTPPacket rtp(MediaFrame::Video,96);
		RTPMap map;

		//Set local port
		sess.SetLocalPort(localPort);
		
		//Init session
		sess.Init();

		//Set rtp map
		map[96] = 96;

		//Set map
		sess.SetSendingRTPMap(map);

		//Set codec
		sess.SetSendingCodec(96);

		//Set receiver ip and port
		sess.SetRemotePort(ip,port);

		//Calculate frame
		DWORD frameSize = (gop+scaleIP-1)*rate/(8*fps*gop);
		//Calculate min frame size
		DWORD frameSizeMin = (gop+scaleIP-1)*rateMin/(8*fps*gop);
		//Set packet clock rate
		rtp.SetClockRate(90000);

		//Initialize rtp values
		DWORD num = 0;
		QWORD ts = 0;
		DWORD seq = 0;

		//Get start time
		getUpdDifTime(&ini);

		//Until ctrl-c is pressed
		while(sending)
		{
			//Set P frame size
			DWORD size = frameSizeMin + ((QWORD)(frameSize-frameSizeMin))*rand()/RAND_MAX;

			//Check if its the firs frame of gop
			if (!(num % gop))
				//Set I frame size
				size = frameSize*scaleIP;
			
			//Set timestamp
			rtp.SetTimestamp(ts);
			//ReSet mark
			rtp.SetMark(false);
			//Packetize
			while(size)
			{
				//Get size
				DWORD len = size;
				//Check if too much
				if (len>maxRTPSize)
					//Limit it
					len = maxRTPSize;
				//Set packet length, content is random
				rtp.SetMediaLength(len);
				//Increase seq number
				rtp.SetSeqNum(seq++);
				//Decrease size to send
				size -= len;
				//If it is last
				if (!size)
					//Set mark
					rtp.SetMark(true);
				//Send it
				sess.SendPacket(rtp);
			}

			//Increase num of frames
			num++;
			//Increase timestamp for next one
			ts+=90000/fps;

			//Calculate sleep time until next frame
			QWORD diff = ts*1000/90-getDifTime(&ini);
			
			//And sleep
			msleep(diff);
		}

		//End it
		sess.End();

		//Exit
		pthread_exit(0);
	}
private:
	pthread_t thread;
	char* ip;
	int   port;
	int   localPort;
	DWORD rate;
	DWORD rateMin;
	DWORD fps;
	DWORD gop;
	DWORD maxRTPSize;
	DWORD scaleIP;
	bool  sending;
};

int main(int argc, char** argv)
{
	char c;
	//Create sender object
	Sender sender;

	//Get all parameters
	for(int i=1;i<argc;i++)
	{
		//Check options
		if (strcmp(argv[i],"-h")==0 || strcmp(argv[i],"--help")==0)
		{
			//Show usage
			printf("Usage: sender [-h] [-smooth] [--ip ip] [--port port] [--local-port port [--rate bps] [--rate-min bps] [--gop-size frames] [--fps fps] [--rtp-max-size size] [--IP-scale scale]\r\n\r\n"
				"Options:\r\n"
				" -h,--help        Print help\r\n"
				" --ip             Destination IP\r\n"
				" --port           Destination port\r\n"
				" --local-port     Local port\r\n"
				" --rate           Initial bitrate\r\n"
				" --rate-min       Minimum bitrate, target bitrate will then randomly in [min-rate,rate]\r\n"
				" --gop-size       GOP size in number of frames\r\n"
				" --fps            Frames per second\r\n"
				" --rtp-max-size   Max RTP packet size\r\n"
				" --IP-scale       I frame size vs P frame size\r\n"
				" --smooth         Smooth sending of rtp packets of a frame (Traffic shaping)\r\n");
			//Exit
			return 0;
		} else if (strcmp(argv[i],"-smooth")==0) {
			//Nothing yet
		} else if (strcmp(argv[i],"--ip")==0 && (i+1<argc)) {
			//Get ip
			sender.setIp(argv[++i]);
		} else if (strcmp(argv[i],"--port")==0 && (i+1<argc)) {
			//Get port
			sender.setPort(atoi(argv[++i]));
		} else if (strcmp(argv[i],"--local-port")==0 && (i+1<argc)) {
			//Get port
			sender.setLocalPort(atoi(argv[++i]));
		} else if (strcmp(argv[i],"--rate")==0 && (i+1<argc)) {
			//Get rate
			sender.setRate(atoi(argv[++i]));
		} else if (strcmp(argv[i],"--rate-min")==0 && (i+1<argc)) {
			//Get rate
			sender.setRateMin(atoi(argv[++i]));
		} else if (strcmp(argv[i],"--gop-size")==0 && (i+1<argc)) {
			//Get gop size
			sender.setGop(atoi(argv[++i]));
		} else if (strcmp(argv[i],"--fps")==0 && (i+1<argc)) {
			//Get fps
			sender.setFps(atoi(argv[++i]));
		} else if (strcmp(argv[i],"--rtp-max-size")==0 && (i+1<argc)) {
			//Get rtmp port
			sender.setMaxRTPSize(atoi(argv[++i]));
		} else if (strcmp(argv[i],"--IP-scale")==0 && (i+1<=argc)) {
			//Get rtmp port
			sender.setScaleIP(atoi(argv[++i]));
		} else {
			Error("Unknown parameter [%s]\n",argv[i]);
		}
	}

	//Start
	if (!sender.Start())
		//Error
		return -1;
	
	//We are sending
	printf("Sending... press [q] to stop\n");
	//Read until q is pressed
	while ((c=getchar())!='q');

	//Stop
	sender.Stop();

	//OK
	return 0;
}
