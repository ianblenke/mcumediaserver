/*
 * To change this template, choose Tools | Templates
 * and open the template in the editor.
 */

package org.murillo.mscontrol.networkconnection;

import java.net.URI;
import java.util.Iterator;
import java.util.logging.Level;
import java.util.logging.Logger;
import javax.media.mscontrol.MediaConfig;
import javax.media.mscontrol.MediaObject;
import javax.media.mscontrol.MsControlException;
import javax.media.mscontrol.Parameters;
import javax.media.mscontrol.join.JoinableStream;
import javax.media.mscontrol.join.JoinableStream.StreamType;
import javax.media.mscontrol.networkconnection.NetworkConnection;
import javax.media.mscontrol.networkconnection.SdpPortManager;
import javax.media.mscontrol.resource.Action;
import org.apache.xmlrpc.XmlRpcException;
import org.murillo.MediaServer.Codecs.MediaType;
import org.murillo.mscontrol.resource.ContainerImpl;
import org.murillo.mscontrol.MediaServer;
import org.murillo.mscontrol.MediaSessionImpl;
import org.murillo.mscontrol.ParametersImpl;

/**
 *
 * @author Sergio
 */
public class NetworkConnectionImpl extends ContainerImpl implements NetworkConnection  {
    //Configuration pattern related to NetworkConnection.BASE
    public final static MediaConfig BASE_CONFIG = NetworkConnectionBasicConfigImpl.getConfig();
    
    private final int endpointId;
    private final SdpPortManagerImpl sdpPortManager;
    private final MediaServer mediaServer;

    public NetworkConnectionImpl(MediaSessionImpl sess, URI uri,Parameters params) throws MsControlException {
        //Call parent
        super(sess,uri,params);
        //The port manager
        sdpPortManager = new SdpPortManagerImpl(this,params);
        //Add streams
        AddStream(StreamType.audio, new NetworkConnectionJoinableStream(sess,this,StreamType.audio));
        AddStream(StreamType.video, new NetworkConnectionJoinableStream(sess,this,StreamType.video));
        AddStream(StreamType.message, new NetworkConnectionJoinableStream(sess,this,StreamType.message));
        //Get media server
        mediaServer = session.getMediaServer();
        try {
            //Create endpoint
            endpointId = mediaServer.EndpointCreate(session.getSessionId(), uri.toString(), true, true, true);
        } catch (XmlRpcException ex) {
            Logger.getLogger(NetworkConnectionImpl.class.getName()).log(Level.SEVERE, null, ex);
            //Trhow it
            throw new MsControlException("Could not create network connection",ex);
        }
    }

    public MediaServer getMediaServer() {
        return mediaServer;
    }

    @Override
    public SdpPortManager getSdpPortManager() throws MsControlException {
        return sdpPortManager;
    }

    @Override
    public JoinableStream getJoinableStream(StreamType type) throws MsControlException {
        //Return array
        return (JoinableStream) streams.get(type);
    }

    @Override
    public JoinableStream[] getJoinableStreams() throws MsControlException {
        //Return object array
        return (JoinableStream[]) streams.values().toArray(new JoinableStream[3]);
    }

    @Override
    public Parameters createParameters() {
        //Create new map
        return new ParametersImpl();
    }

    public int getEndpointId() {
        return endpointId;
    }

    public int getSessId() {
        return session.getSessionId();
    }

    void startReceiving(SdpPortManagerImpl sdp) throws MsControlException
    {
	try {
	    //Check if using DTLS
	    if (sdp.getUseDTLS())
	    {
		//Get fingerprint
		String localFingerprint = mediaServer.EndpointGetLocalCryptoDTLSFingerprint(sdp.getLocalHash());
		//Set it
		sdp.setLocalFingerprint(localFingerprint);
	    }

	    //If supported and not already receiving
	    if (sdp.getAudioSupported() && sdp.getRecAudioPort()==0)
	    {
		//Check if we are secure
		if (sdp.getIsSecure())
		{
		    //Create new cypher
		    CryptoInfo info = CryptoInfo.Generate();
		    //Set it
		    mediaServer.EndpointSetLocalCryptoSDES(session.getSessionId(), endpointId,  MediaType.AUDIO, info.suite, info.key);
		    //Set it
		    sdp.setLocalCryptoInfo("audio",info);
		}

		//Check if using ICE
		if (sdp.getUseIce())
		{
		    //Create new ICE Info
		    ICEInfo info = ICEInfo.Generate();
		    //Set them
		    mediaServer.EndpointSetLocalSTUNCredentials(session.getSessionId(), endpointId, MediaType.AUDIO, info.ufrag, info.pwd);
		    //Set it
		    sdp.setLocalIceInfo("audio", info);
		}
		//Create rtp map for audio
                sdp.createRTPMap("audio");
                //Get receiving ports
                Integer recAudioPort = mediaServer.EndpointStartReceiving(session.getSessionId(), endpointId, MediaType.AUDIO, sdp.getRtpInMediaMap("audio"));
                //Set ports
                sdp.setRecAudioPort(recAudioPort);
	    }

	    //If supported and not already receiving
	    if (sdp.getVideoSupported() && sdp.getRecVideoPort()==0)
	    {
		//Check if we are secure
		if (sdp.getIsSecure())
		{
		    //Create new cypher
		    CryptoInfo info = CryptoInfo.Generate();
		    //Set it
		    mediaServer.EndpointSetLocalCryptoSDES(session.getSessionId(), endpointId,  MediaType.VIDEO, info.suite, info.key);
		    //Set it
		    sdp.setLocalCryptoInfo("video",info);
		}

		//Check if using ICE
		if (sdp.getUseIce())
		{
		    //Create new ICE Info
		    ICEInfo info = ICEInfo.Generate();
		    //Set them
		    mediaServer.EndpointSetLocalSTUNCredentials(session.getSessionId(), endpointId, MediaType.VIDEO, info.ufrag, info.pwd);
		    //Set it
		    sdp.setLocalIceInfo("video", info);
		}
		//Create rtp map for video
                sdp.createRTPMap("video");
                //Get receiving ports
                Integer recVideoPort = mediaServer.EndpointStartReceiving(session.getSessionId(), endpointId, MediaType.VIDEO, sdp.getRtpInMediaMap("video"));
                //Set ports
                sdp.setRecVideoPort(recVideoPort);
	    }

	    //If supported and not already receiving
	    if (sdp.getTextSupported() && sdp.getRecTextPort()==0)
	    {
		//Check if we are secure
		if (sdp.getIsSecure())
		{
		    //Create new cypher
		    CryptoInfo info = CryptoInfo.Generate();
		    //Set it
		    mediaServer.EndpointSetLocalCryptoSDES(session.getSessionId(), endpointId,  MediaType.TEXT, info.suite, info.key);
		    //Set it
		    sdp.setLocalCryptoInfo("text",info);
		}

		//Check if using ICE
		if (sdp.getUseIce())
		{
		    //Create new ICE Info
		    ICEInfo info = ICEInfo.Generate();
		    //Set them
		    mediaServer.EndpointSetLocalSTUNCredentials(session.getSessionId(), endpointId, MediaType.TEXT, info.ufrag, info.pwd);
		    //Set it
		    sdp.setLocalIceInfo("text", info);
		}
		//Create rtp map for text
                sdp.createRTPMap("text");
                //Get receiving ports
                Integer recTextPort = mediaServer.EndpointStartReceiving(session.getSessionId(), endpointId, MediaType.TEXT, sdp.getRtpInMediaMap("text"));
                //Set ports
                sdp.setRecTextPort(recTextPort);
	    }

	    //And set the sender ip
	    sdp.setRecIp(mediaServer.getIp());
	} catch (XmlRpcException ex) {
	    Logger.getLogger(NetworkConnectionImpl.class.getName()).log(Level.SEVERE, null, ex);
	    //Trhow it
	    throw new MsControlException("Could not start receiving audio",ex);
        }
    }

    protected void startSending(SdpPortManagerImpl sdp) throws MsControlException, XmlRpcException
    {
        //Check audio
        if (sdp.getSendAudioPort()!=0)
	{
	    //Get the auido stream
	    NetworkConnectionJoinableStream stream = (NetworkConnectionJoinableStream)getJoinableStream(StreamType.audio);
	    //Update ssetAudioCodecneding codec
	    stream.requestAudioCodec(sdp.getAudioCodec());
	    //Send
	    mediaServer.EndpointStartSending(session.getSessionId(), endpointId, MediaType.AUDIO, sdp.getSendAudioIp(), sdp.getSendAudioPort(), sdp.getRtpOutMediaMap("audio"));
        }

        //Check video
        if (sdp.getSendVideoPort()!=0)
	{
	    //Get the auido stream
	    NetworkConnectionJoinableStream stream = (NetworkConnectionJoinableStream)getJoinableStream(StreamType.video);
	    //Update sneding codec
	    stream.requestVideoCodec(sdp.getVideoCodec());
	    //Send
	    mediaServer.EndpointStartSending(session.getSessionId(), endpointId, MediaType.VIDEO, sdp.getSendVideoIp(), sdp.getSendVideoPort(), sdp.getRtpOutMediaMap("video"));
	}

        //Check text
        if (sdp.getSendTextPort()!=0)
	{
	    //Send
	    mediaServer.EndpointStartSending(session.getSessionId(), endpointId, MediaType.TEXT, sdp.getSendTextIp(), sdp.getSendTextPort(), sdp.getRtpOutMediaMap("text"));
        }
    }

    @Override
    public void triggerAction(Action action) {
        //Check if it is a picture_fast_update
        if (action.toString().equalsIgnoreCase("org.murillo.mscontrol.picture_fast_update"))
        {
            try {
                //Send FPU
                mediaServer.EndpointRequestUpdate(session.getSessionId(), endpointId, MediaType.VIDEO);
            } catch (XmlRpcException ex) {
                Logger.getLogger(NetworkConnectionImpl.class.getName()).log(Level.SEVERE, null, ex);
            }
        }
    }

    @Override
    public <R> R getResource(Class<R> type) throws MsControlException {
        throw new UnsupportedOperationException("Not supported yet.");
    }

    @Override
    public MediaConfig getConfig() {
        return BASE_CONFIG;
    }

    @Override
    public void release() {
        //Free joins
        releaseJoins();
        try {
            //Delete endpoint
            mediaServer.EndpointDelete(session.getSessionId(),endpointId);
        } catch (XmlRpcException ex) {
            Logger.getLogger(NetworkConnectionImpl.class.getName()).log(Level.SEVERE, null, ex);
        }
    }

    @Override
    public Iterator<MediaObject> getMediaObjects() {
        throw new UnsupportedOperationException("Not supported yet.");
    }

    @Override
    public <T extends MediaObject> Iterator<T> getMediaObjects(Class<T> type) {
        throw new UnsupportedOperationException("Not supported yet.");
    }

    public void onSDPNegotiationDone(SdpPortManagerImpl sdp) throws XmlRpcException {
	
        //Get conf id
        Integer sessId = session.getSessionId();

        //If supported
        if (sdp.getAudioSupported())
        {

	    //Check if DTLS enabled
	    if (sdp.getUseDTLS())
	    {
		//Get cryto info
		DTLSInfo info = sdp.getRemoteDTLSInfo("audio");
		//If present
		if (info!=null)
		    //Set it
		    mediaServer.EndpointSetRemoteCryptoDTLS(sessId, endpointId, MediaType.AUDIO, info.setup, info.hash, info.fingerprint);
	    } else {
		//Get cryto info
		CryptoInfo info = sdp.getRemoteCryptoInfo("audio");
		//If present
		if (info!=null)
                    //Set it
	           mediaServer.EndpointSetRemoteCryptoSDES(sessId, endpointId, MediaType.AUDIO, info.suite, info.key);
	    }

            //Get ice info
            ICEInfo ice = sdp.getRemoteICEInfo("audio");
            //If present
            if (ice!=null)
                //Set it
               mediaServer.EndpointSetRemoteSTUNCredentials(sessId, endpointId, MediaType.AUDIO, ice.ufrag, ice.pwd);
            //Set RTP properties
            mediaServer.EndpointSetRTPProperties(sessId, endpointId, MediaType.AUDIO, sdp.getRTPMediaProperties("audio"));
        }

        //If supported
        if (sdp.getVideoSupported())
        {

	    //Check if DTLS enabled
	    if (sdp.getUseDTLS())
	    {
		//Get cryto info
		DTLSInfo info = sdp.getRemoteDTLSInfo("video");
		//If present
		if (info!=null)
		    //Set it
		    mediaServer.EndpointSetRemoteCryptoDTLS(sessId, endpointId, MediaType.VIDEO, info.setup, info.hash, info.fingerprint);
	    } else {
		//Get cryto info
		CryptoInfo info = sdp.getRemoteCryptoInfo("video");
		//If present
		if (info!=null)
                    //Set it
	           mediaServer.EndpointSetRemoteCryptoSDES(sessId, endpointId, MediaType.VIDEO, info.suite, info.key);
	    }

            //Get ice info
            ICEInfo ice = sdp.getRemoteICEInfo("video");
            //If present
            if (ice!=null)
                //Set it
               mediaServer.EndpointSetRemoteSTUNCredentials(sessId, endpointId, MediaType.VIDEO, ice.ufrag, ice.pwd);
            //Set RTP properties
            mediaServer.EndpointSetRTPProperties(sessId, endpointId, MediaType.VIDEO, sdp.getRTPMediaProperties("video"));
        }

        //If supported
        if (sdp.getTextSupported())
        {
	    //Check if DTLS enabled
	    if (sdp.getUseDTLS())
	    {
		//Get cryto info
		DTLSInfo info = sdp.getRemoteDTLSInfo("text");
		//If present
		if (info!=null)
		    //Set it
		    mediaServer.EndpointSetRemoteCryptoDTLS(sessId, endpointId, MediaType.TEXT, info.setup, info.hash, info.fingerprint);
	    } else {
		//Get cryto info
		CryptoInfo info = sdp.getRemoteCryptoInfo("text");
		//If present
		if (info!=null)
                    //Set it
	           mediaServer.EndpointSetRemoteCryptoSDES(sessId, endpointId, MediaType.TEXT, info.suite, info.key);
	    }

            //Get ice info
            ICEInfo ice = sdp.getRemoteICEInfo("text");
            //If present
            if (ice!=null)
                //Set it
               mediaServer.EndpointSetRemoteSTUNCredentials(sessId, endpointId, MediaType.TEXT, ice.ufrag, ice.pwd);
            //Set RTP properties
            mediaServer.EndpointSetRTPProperties(sessId, endpointId, MediaType.TEXT, sdp.getRTPMediaProperties("text"));
        }
    }
    
}
