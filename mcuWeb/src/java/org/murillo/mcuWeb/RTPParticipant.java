/*
 * RTPParticipant.java
 *
 * Copyright (C) 2007  Sergio Garcia Murillo
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

package org.murillo.mcuWeb;

import java.io.IOException;
import java.io.InputStream;
import java.security.SecureRandom;
import java.util.ArrayList;
import java.util.Date;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Vector;
import java.util.logging.Level;
import java.util.logging.Logger;
import javax.mail.BodyPart;
import javax.mail.MessagingException;
import javax.mail.Multipart;
import javax.mail.internet.MimeBodyPart;
import javax.mail.internet.MimeMultipart;
import javax.servlet.sip.Address;
import javax.servlet.sip.SipApplicationSession;
import javax.servlet.sip.SipFactory;
import javax.servlet.sip.SipServletRequest;
import javax.servlet.sip.SipServletResponse;
import javax.servlet.sip.SipSession;
import javax.servlet.sip.SipURI;
import javax.xml.bind.DatatypeConverter;
import org.apache.xmlrpc.XmlRpcException;
import org.murillo.MediaServer.Codecs;
import org.murillo.MediaServer.Codecs.MediaType;
import org.murillo.MediaServer.XmlRpcMcuClient;
import org.murillo.MediaServer.XmlRpcMcuClient.MediaStatistics;
import org.murillo.abnf.ParserException;
import org.murillo.sdp.Attribute;
import org.murillo.sdp.Bandwidth;
import org.murillo.sdp.Connection;
import org.murillo.sdp.CryptoAttribute;
import org.murillo.sdp.MediaDescription;
import org.murillo.sdp.RTPMapAttribute;
import org.murillo.sdp.SessionDescription;

/**
 *
 * @author Sergio
 */
public class RTPParticipant extends Participant {

    private Address address;
    private SipSession session = null;
    private SipApplicationSession appSession = null;
    private SipServletRequest inviteRequest = null;
    private String recIp;
    private Integer recAudioPort;
    private Integer recVideoPort;
    private Integer recTextPort;
    private Integer sendAudioPort;
    private String  sendAudioIp;
    private Integer sendVideoPort;
    private String  sendVideoIp;
    private Integer sendTextPort;
    private String  sendTextIp;
    private Integer audioCodec;
    private Integer videoCodec;
    private Integer textCodec;
    private String location;
    private Integer totalPacketCount;
    private Map<String, MediaStatistics> stats;

    private HashMap<String,List<Integer>> supportedCodecs = null;
    private HashMap<String,HashMap<Integer,Integer>> rtpInMediaMap = null;
    private HashMap<String,HashMap<Integer,Integer>> rtpOutMediaMap = null;
    private boolean isSendingAudio;
    private boolean isSendingVideo;
    private boolean isSendingText;
    private ArrayList<MediaDescription> rejectedMedias;
    private String videoContentType;
    private String h264profileLevelId;
    private int h264packetization;
    private final String h264profileLevelIdDefault = "428014";
    private int videoBitrate;
    private SessionDescription remoteSDP;
    private Boolean isSecure;
    private Boolean rtcpFeedBack;
    private HashMap<String,CryptoInfo> localCryptoInfo;
    private HashMap<String,CryptoInfo> remoteCryptoInfo;
    private boolean useICE;
    private HashMap<String,ICEInfo> localICEInfo;
    private HashMap<String,ICEInfo> remoteICEInfo;

    private static class CryptoInfo
    {
        String suite;
        String key;

        public static CryptoInfo Generate()
        {
            //Create crypto info for media
            CryptoInfo info = new CryptoInfo();
            //Set suite
            info.suite = "AES_CM_128_HMAC_SHA1_80";
            //Get random
            SecureRandom random = new SecureRandom();
            //Create key bytes
            byte[] key = new byte[30];
            //Generate it
            random.nextBytes(key);
            //Encode to base 64
            info.key = DatatypeConverter.printBase64Binary(key);
            //return it
            return info;
        }
        
        private CryptoInfo() {

        }

        public CryptoInfo(String suite, String key) {
            this.suite = suite;
            this.key = key;
        }

    }

    private static class ICEInfo
    {
        String ufrag;
        String pwd;

        public static ICEInfo Generate()
        {
            //Create ICE info for media
            ICEInfo info = new ICEInfo();
             //Get random
            SecureRandom random = new SecureRandom();
            //Create key bytes
            byte[] frag = new byte[8];
            byte[] pwd = new byte[22];
            //Generate them
            random.nextBytes(frag);
            random.nextBytes(pwd);
            //Create ramdom pwd
            info.ufrag = DatatypeConverter.printHexBinary(frag);
            info.pwd   =  DatatypeConverter.printHexBinary(pwd);
            //return it
            return info;
        }

        private ICEInfo() {
            
        }
        
        public ICEInfo(String ufrag, String pwd) {
            this.ufrag = ufrag;
            this.pwd = pwd;
        }

    }

    RTPParticipant(Integer id,String name,Integer mosaicId,Integer sidebarId,Conference conf) throws XmlRpcException {
        //Call parent
        super(id,name,mosaicId,sidebarId,conf,Type.SIP);
        //Not sending
        isSendingAudio = false;
        isSendingVideo = false;
        isSendingText = false;
        //No sending ports
        sendAudioPort = 0;
        sendVideoPort = 0;
        sendTextPort = 0;
        //No receiving ports
        recAudioPort = 0;
        recVideoPort = 0;
        recTextPort = 0;
        //Supported media
        audioSupported = true;
        videoSupported = true;
        textSupported = true;
        //Create supported codec map
        supportedCodecs = new HashMap<String, List<Integer>>();
        //Create media maps
        rtpInMediaMap = new HashMap<String,HashMap<Integer,Integer>>();
        rtpOutMediaMap = new HashMap<String,HashMap<Integer,Integer>>();
        //No rejected medias and no video content type
        rejectedMedias = new ArrayList<MediaDescription>();
        videoContentType = "";
        //Set default level and packetization
        h264profileLevelId = "";
        h264packetization = 0;
        //Not secure by default
        isSecure = false;
        //Not using ice by default
        useICE = false;
        //Create crypto info maps
        localCryptoInfo = new HashMap<String, CryptoInfo>();
        remoteCryptoInfo = new HashMap<String, CryptoInfo>();
         //Create ICE info maps
        localICEInfo = new HashMap<String, ICEInfo>();
        remoteICEInfo = new HashMap<String, ICEInfo>();
        //Has RTCP feedback
        rtcpFeedBack = false;
}

    @Override
    public void restart() {
        //Get mcu client
        XmlRpcMcuClient client = conf.getMCUClient();
        try {
            //Create participant in mixer conference and store new id
            id = client.CreateParticipant(id, name.replace('.', '_'), type.valueOf(),mosaicId,sidebarId);
            //Check state
            if (state!=State.CREATED)
            {
                //Start sending
                startSending();
                //And receiving
                startReceiving();
            }
        } catch (XmlRpcException ex) {
            //End it
            end();
        }
    }

    public void addSupportedCodec(String media,Integer codec) {
         //Check if we have the media
         if (!supportedCodecs.containsKey(media))
             //Create it
             supportedCodecs.put(media, new Vector<Integer>());
         //Add codec to media
         supportedCodecs.get(media).add(codec);
     }

    public Address getAddress() {
        return address;
    }

    public String getUsername() {
         //Get sip uris
        SipURI uri = (SipURI) address.getURI();
        //Return username
        return uri.getUser();
    }

    public String getDomain() {
        //Get sip uris
        SipURI uri = (SipURI) address.getURI();
        //Return username
        return uri.getHost();
    }

    public String getUsernameDomain() {
        //Get sip uris
        SipURI uri = (SipURI) address.getURI();
        //Return username
        return uri.getUser()+"@"+uri.getHost();
    }

     boolean equalsUser(Address user) {
        //Get sip uris
        SipURI us = (SipURI) address.getURI();
        SipURI them = (SipURI) user.getURI();
        //If we have the same username and host/domain
        if (us.getUser().equals(them.getUser()) && us.getHost().equals(them.getHost()))
            return true;
        else
            return false;
    }

    public Integer getRecAudioPort() {
        return recAudioPort;
    }

    public void setRecAudioPort(Integer recAudioPort) {
        this.recAudioPort = recAudioPort;
    }

    public Integer getRecTextPort() {
        return recTextPort;
    }

    public void setRecTextPort(Integer recTextPort) {
        this.recTextPort = recTextPort;
    }

    public String getRecIp() {
        return recIp;
    }

    public void setRecIp(String recIp) {
        this.recIp = recIp;
    }

    public Integer getRecVideoPort() {
        return recVideoPort;
    }

    public void setRecVideoPort(Integer recVideoPort) {
        this.recVideoPort = recVideoPort;
    }

    public Integer getSendAudioPort() {
        return sendAudioPort;
    }

    public void setSendAudioPort(Integer sendAudioPort) {
        this.sendAudioPort = sendAudioPort;
    }

    public Integer getSendVideoPort() {
        return sendVideoPort;
    }

    public void setSendVideoPort(Integer sendVideoPort) {
        this.sendVideoPort = sendVideoPort;
    }

    public Integer getSendTextPort() {
        return sendTextPort;
    }

    public void setSendTextPort(Integer sendTextPort) {
        this.sendTextPort = sendTextPort;
    }

    public String getSendAudioIp() {
        return sendAudioIp;
    }

    public void setSendAudioIp(String sendAudioIp) {
        this.sendAudioIp = sendAudioIp;
    }

    public String getSendTextIp() {
        return sendTextIp;
    }

    public void setSendTextIp(String sendTextIp) {
        this.sendTextIp = sendTextIp;
    }

    public String getSendVideoIp() {
        return sendVideoIp;
    }

    public void setSendVideoIp(String sendVideoIp) {
        this.sendVideoIp = sendVideoIp;
    }

    public Integer getAudioCodec() {
        return audioCodec;
    }

    public void setAudioCodec(Integer audioCodec) {
        this.audioCodec = audioCodec;
    }

    public Integer getTextCodec() {
        return textCodec;
    }

    public void setTextCodec(Integer textCodec) {
        this.textCodec = textCodec;
    }

    public Integer getVideoCodec() {
        return videoCodec;
    }

    public void setVideoCodec(Integer videoCodec) {
        this.videoCodec = videoCodec;
    }

    public String getLocation() {
        return location;
    }

    public void setLocation(String location) {
        this.location = location;
    }

    @Override
    public boolean setVideoProfile(Profile profile) {
        //Check video is supported
        if (!getVideoSupported())
            //Exit
            return false;
        //Set new video profile
        this.profile = profile;
        try {
            //Get client
            XmlRpcMcuClient client = conf.getMCUClient();
            //Get conf id
            Integer confId = conf.getId();
            //If it is sending video
            if (isSendingVideo)
            {
                //Stop sending video
                client.StopSending(confId, id, MediaType.VIDEO);
                //Get profile bitrate
                int bitrate = profile.getVideoBitrate();
                //Reduce to the maximum in SDP
                if (videoBitrate>0 && videoBitrate<bitrate)
                        //Reduce it
                        bitrate = videoBitrate;
                //Setup video with new profile
                client.SetVideoCodec(confId, id, getVideoCodec(), profile.getVideoSize(), profile.getVideoFPS(), bitrate, 0, 0, profile.getIntraPeriod());
                //Send video & audio
                client.StartSending(confId, id, MediaType.VIDEO, getSendVideoIp(), getSendVideoPort(), getRtpOutMediaMap("video"));
            }
        } catch (XmlRpcException ex) {
            Logger.getLogger("global").log(Level.SEVERE, null, ex);
            return false;
        }
        return true;
    }

    public HashMap<Integer,Integer> getRtpInMediaMap(String media) {
        //Return rtp mapping for media
        return rtpInMediaMap.get(media);
    }

    HashMap<Integer, Integer> getRtpOutMediaMap(String media) {
        //Return rtp mapping for media
        return rtpOutMediaMap.get(media);
    }

    public String createSDP() {

        SessionDescription sdp = new SessionDescription();

        //Set origin
        sdp.setOrigin("-", getId().toString(), Long.toString(new Date().getTime()), "IN", "IP4", getRecIp());
        //Set name
        sdp.setSessionName("MediaMixerSession");
        //Set connection info
        sdp.setConnection("IN", "IP4", getRecIp());
        //Set time
        sdp.addTime(0,0);
        //Check if supported
        if (audioSupported)
            //Add audio related lines to the sdp
            sdp.addMedia(createMediaDescription("audio",recAudioPort));
        //Check if supported
        if (videoSupported)
            //Add video related lines to the sdp
            sdp.addMedia(createMediaDescription("video",recVideoPort));
        //Check if supported
        if (textSupported)
            //Add text related lines to the sdp
            sdp.addMedia(createMediaDescription("text",recTextPort));

        //Add rejecteds medias
        for (MediaDescription md : rejectedMedias)
            //Add it
            sdp.addMedia(md);

        //Return
        return sdp.toString();
    }

    public void createRTPMap(String media)
    {
        //Get supported codecs for media
        List<Integer> codecs = supportedCodecs.get(media);

        //Check if it supports media
        if (codecs!=null)
        {
            //Create map
            HashMap<Integer, Integer> rtpInMap = new HashMap<Integer, Integer>();
            //Check if rtp map exist already for outgoing
            HashMap<Integer, Integer> rtpOutMap = rtpOutMediaMap.get(media);
            //If we do not have it
            if (rtpOutMap==null)
            {
                //Add all supported audio codecs with default values
                for(Integer codec : codecs)
                    //Append it
                    rtpInMap.put(codec, codec);
            } else {
                //Add all supported audio codecs with already known mappings
                for(Map.Entry<Integer,Integer> pair : rtpOutMap.entrySet())
                    //Check if it is supported
                    if (codecs.contains(pair.getValue()))
                        //Append it
                        rtpInMap.put(pair.getKey(), pair.getValue());
            }

            //Put the map back in the map
            rtpInMediaMap.put(media, rtpInMap);
        }
    }

    private Integer findTypeForCodec(HashMap<Integer, Integer> rtpMap, Integer codec) {
        for (Map.Entry<Integer,Integer> pair  : rtpMap.entrySet())
            if (pair.getValue()==codec)
                return pair.getKey();
        return -1;
    }

    private MediaDescription createMediaDescription(String mediaName, Integer port)
    {
        //Create AVP profile
        String rtpProfile = "AVP";
        //If is secure
        if (isSecure)
            //Prepend S
            rtpProfile = "S"+rtpProfile;
        //If has feedback
        if (rtcpFeedBack)
            //Append F
            rtpProfile += "F";
        //Create new meida description with default values
        MediaDescription md = new MediaDescription(mediaName,port,"RTP/"+rtpProfile);

        //Enable rtcp muxing
        md.addAttribute("rtcp-mux");

        //Check if rtp map exist
        HashMap<Integer, Integer> rtpInMap = rtpInMediaMap.get(mediaName);

        //Check not null
        if (rtpInMap==null)
        {
            //Log
            Logger.getLogger(RTPParticipant.class.getName()).log(Level.FINE, "addMediaToSdp rtpInMap is null. Disabling media {0} ", new Object[]{mediaName});
            //Disable media
            md.setPort(0);
            //Return empty media
            return md;
        }

        //If we are using ice
        if (useICE)
        {
            //Add host candidate for RTP
            md.addCandidate("1", 1, "UDP", 33554432-1, getRecIp(), port, "host");
            //Add host candidate for RTCP
            md.addCandidate("1", 2, "UDP", 33554432-2, getRecIp(), port+1, "host");
            //Get ICE info
            ICEInfo info = localICEInfo.get(mediaName);
            //If ge have it
            if (info!=null)
            {
                //Add ice lite attribute
                md.addAttribute("ice-lite");
                //Set credentials
                md.addAttribute("ice-ufrag",info.ufrag);
                md.addAttribute("ice-pwd",info.pwd);
            }
        }

        //Get Crypto info for media
        CryptoInfo info = remoteCryptoInfo.get(mediaName);

        //f we have crytpo info
        if (info!=null)
            //Append attribute
            md.addAttribute( new CryptoAttribute(1, info.suite, "inline", info.key));

        //Add rtmpmap for each codec in supported order
        for (Integer codec : supportedCodecs.get(mediaName))
        {
            //Search for the codec
            for (Entry<Integer,Integer> mapping : rtpInMap.entrySet())
            {
                //Check codec
                if (mapping.getValue()==codec)
                {
                    //Get fmt mapping
                    Integer fmt = mapping.getKey();
                    //Append fmt
                    md.addFormat(fmt);
                    //Add rtmpmap
                    md.addRTPMapAttribute(fmt, Codecs.getNameForCodec(mediaName, codec), Codecs.getRateForCodec(mediaName,codec));
                    if (codec==Codecs.H264)
                    {
                        //Check if we are offering first
                        if (h264profileLevelId.isEmpty())
                            //Set default profile
                            h264profileLevelId = h264profileLevelIdDefault;

                        //Check packetization mode
                        if (h264packetization>0)
                            //Add profile and packetization mode
                            md.addFormatAttribute(fmt,"profile-level-id="+h264profileLevelId+";packetization-mode="+h264packetization);
                        else
                            //Add profile id
                            md.addFormatAttribute(fmt,"profile-level-id="+h264profileLevelId);
                    } else if (codec==Codecs.H263_1996) {
                        //Add h263 supported sizes
                        md.addFormatAttribute(fmt,"CIF=1;QCIF=1");
                    } else if (codec == Codecs.T140RED) {
                        //Find t140 codec
                        Integer t140 = findTypeForCodec(rtpInMap,Codecs.T140);
                        //Check that we have founf it
                        if (t140>0)
                            //Add redundancy fmt
                            md.addFormatAttribute(fmt,t140 + "/" + t140 + "/" + t140);
                    }
                }
            }
        }

        //If it is video and we have found the content attribute
        if (mediaName.equals("video") && !videoContentType.isEmpty())
            //Add attribute
            md.addAttribute("content",videoContentType);

        //If not format has been found
        if (md.getFormats().isEmpty())
        {
            //Log
            Logger.getLogger(RTPParticipant.class.getName()).log(Level.FINE, "addMediaToSdp no compatible codecs found for media {0} ", new Object[]{mediaName});
            //Disable
            md.setPort(0);
        }
        //Return the media descriptor
        return md;
    }

    private void proccesContent(String type, Object content) throws IOException {
        //No SDP
        String sdp = null;
        //Depending on the type
        if (type.equalsIgnoreCase("application/sdp"))
        {
            //Get it
            sdp = new String((byte[])content);
        } else if (type.startsWith("multipart/mixed")) {
            try {
                //Get multopart
                Multipart multipart = (Multipart) content;
                //For each content
                for (int i = 0; i < multipart.getCount(); i++)
                {
                    //Get content type
                    BodyPart bodyPart = multipart.getBodyPart(i);
                    //Get body type
                    String bodyType = bodyPart.getContentType();
                    //Check type
                    if (bodyType.equals("application/sdp"))
                    {
                        //Get input stream
                        InputStream inputStream = bodyPart.getInputStream();
                        //Create array
                        byte[] arr = new byte[inputStream.available()];
                        //Read them
                        inputStream.read(arr, 0, inputStream.available());
                        //Set length
                        sdp = new String(arr);
                    } else if (bodyType.equals("application/pidf+xml")) {
                        //Get input stream
                        InputStream inputStream = bodyPart.getInputStream();
                        //Create array
                        byte[] arr = new byte[inputStream.available()];
                        //Read them
                        inputStream.read(arr, 0, inputStream.available());
                        //Set length
                        location = new String(arr);
                    }
                }
            } catch (MessagingException ex) {
                Logger.getLogger(RTPParticipant.class.getName()).log(Level.SEVERE, null, ex);
            }
        }
        try {
        //Parse sdp
            remoteSDP = processSDP(sdp);
        } catch (IllegalArgumentException ex) {
            Logger.getLogger(RTPParticipant.class.getName()).log(Level.SEVERE, null, ex);
        } catch (ParserException ex) {
            Logger.getLogger(RTPParticipant.class.getName()).log(Level.SEVERE, null, ex);
    }
    }

    public SessionDescription processSDP(String body) throws IllegalArgumentException, ParserException
    {
        //Connnection IP
        String ip = null;
        //ICE credentials
        String remoteICEFrag = null;
        String remtoeICEPwd = null;

        //Parse conent
        SessionDescription sdp = SessionDescription.Parse(body);

        //Get the connection field
        Connection conn = sdp.getConnection();

        if (conn!=null)
        {
            //Get IP addr
            ip = conn.getAddress();

        //Check if ip should be nat for this media mixer
        if (conf.getMixer().isNated(ip))
            //Do natting
            ip = "0.0.0.0";
        }

        //Disable supported media
        audioSupported = false;
        videoSupported = false;
        textSupported = false;

        //NO bitrate by default
        videoBitrate = 0;

        //For each bandwith
        for (Bandwidth band : sdp.getBandwidths())
        {
            //Get bitrate value
            int rate = Integer.parseInt(band.getBandwidth());
            //Check bandwith type
            if (band.getType().equalsIgnoreCase("TIAS"))
                    //Convert to kbps
                    rate = rate/1000;
            //Check bandwith type
            if (band.getBandwidth().equalsIgnoreCase("TIAS"))
                    //Convert to kbps
                    videoBitrate = videoBitrate/1000;
            // Let some room for audio.
            if (rate>=128)
                //Remove maximum rate
                rate -= 64;
            //Check if is less
            if (videoBitrate==0 || rate<videoBitrate)
                //Set it
                videoBitrate = rate;
        }

        //Check for global ice credentials
        Attribute ufragAtrr = sdp.getAttribute("ice-ufrag");
        Attribute pwdAttr = sdp.getAttribute("ice-pwd");

        //Check if both present
        if (ufragAtrr!=null && pwdAttr!=null)
        {
            //Using ice
            useICE = true;
            //Get values
            remoteICEFrag = ufragAtrr.getValue();
            remtoeICEPwd = pwdAttr.getValue();
        }

        for (MediaDescription md : sdp.getMedias())
        {
            //No default bitrate
            int mediaBitrate = 0;

            //Get media type
            String media = md.getMedia();

            //Get port
            Integer port = md.getPort();
            //Get transport
            ArrayList<String> proto = md.getProto();

            //If it its not RTP
            if (!proto.get(0).equals("RTP"))
            {
                //Create media descriptor
                MediaDescription rejected = new MediaDescription(media,0,md.getProtoString());
                //set all  formats
                rejected.setFormats(md.getFormats());
                //add to rejected media
                rejectedMedias.add(rejected);
                //Not supported media type
                continue;
            }

            //Get bandwiths
            for (Bandwidth band : md.getBandwidths())
            {
                //Get bitrate value
                int rate = Integer.parseInt(band.getBandwidth());
                //Check bandwith type
                 if (band.getType().equalsIgnoreCase("TIAS"))
                    //Convert to kbps
                    rate = rate/1000;
                //Check if less than current
                if (mediaBitrate==0 || rate<mediaBitrate)
                    //Set it
                    mediaBitrate = rate;
                //Check bandwith type
                 if (band.getBandwidth().equalsIgnoreCase("TIAS"))
                    //Convert to kbps
                    mediaBitrate = mediaBitrate/1000;
            }

            //Add support for the media
            if (media.equals("audio")) {
                //Set as supported
                audioSupported = true;
            } else if (media.equals("video")) {
                //Get content attribute
                Attribute content = md.getAttribute("content");
                //Check if we found it inside this media
                if (content!=null)
                {
                    //Get it
                    String mediaContentType = content.getValue();
                    //Check if it is not main
                    if (!mediaContentType.equalsIgnoreCase("main"))
                    {
                        //Create media descriptor
                        MediaDescription rejected = new MediaDescription(media,0,md.getProtoString());
                        //set all  formats
                        rejected.setFormats(md.getFormats());
                        //Add content attribute
                        rejected.addAttribute(content);
                        //add to rejected media
                        rejectedMedias.add(rejected);
                        //Skip it
                        continue;
                    } else {
                        //Add the content type to the line
                        videoContentType = mediaContentType;
                    }
                }
                //Check if we have a media rate
                if (mediaBitrate>0)
                //Store bitrate
                videoBitrate = mediaBitrate;
                //Set as supported
                videoSupported = true;
            } else if (media.equals("text")) {
                //Set as supported
                textSupported = true;
            } else {
                //Create media descriptor
                MediaDescription rejected = new MediaDescription(media,0,md.getProtoString());
                //set all  formats
                rejected.setFormats(md.getFormats());
                //add to rejected media
                rejectedMedias.add(rejected);
                //Not supported media type
                continue;
            }

            //Check if we have input map for that
            if (!rtpOutMediaMap.containsKey(media))
                //Create new map
                rtpOutMediaMap.put(media, new HashMap<Integer, Integer>());

            //Get all codecs
            //No codec priority yet
            Integer priority = Integer.MAX_VALUE;

            //By default the media IP is the general IO
            String mediaIp = ip;

            //Get connection info
            for (Connection c : md.getConnections())
            {
                //Get it
                mediaIp = c.getAddress();
                //Check if ip should be nat for this media mixer
                if (conf.getMixer().isNated(mediaIp))
                    //Do natting
                    mediaIp = "0.0.0.0";
            }

            //Get rtp profile
            String rtpProfile = proto.get(1);
            //Check if it is secure
            if (rtpProfile.startsWith("S"))
            {
                //Secure (WARNING: if one media is secure, all will be secured, FIX!!)
                isSecure = true;
                //Create media crypto params
                CryptoInfo info = new CryptoInfo();
                //Get crypto header
                CryptoAttribute crypto = (CryptoAttribute) md.getAttribute("crypto");
                //Get suite
                info.suite = crypto.getSuite();
                //Get key
                info.key = crypto.getFirstKeyParam().getInfo();
                //Add it
                remoteCryptoInfo.put(media, info);
            }
            //Check if has rtcp
            if (rtpProfile.endsWith("F"))
                //With feedback (WARNING: if one media has feedback, all will have feedback, FIX!!)
                rtcpFeedBack = true;


            //FIX
            Integer h264type = 0;
            String maxh264profile = "";

            //For each format
            for (String fmt : md.getFormats())
            {
                Integer type = 0;
                try {
                    //Get codec
                    type = Integer.parseInt(fmt);
                } catch (Exception e) {
                    //Ignore non integer codecs, like '*' on application
                    continue;
                }

                //If it is dinamic
                if (type>=96)
                {
                    //Get map
                    RTPMapAttribute rtpMap = md.getRTPMap(type);
                    //Check it has mapping
                    if (rtpMap==null)
                        //Skip this one
                        continue;
                //Get the media type
                    String codecName = rtpMap.getName();
                //Get codec for name
                Integer codec = Codecs.getCodecForName(media,codecName);
                //if it is h264 TODO: FIIIIX!!!
                if (codec==Codecs.H264)
                {
                        int k = -1;
                        //Get ftmp line
                        String ftmpLine = md.getFormatParameters(type);
                        //Check if got format
                        if (ftmpLine!=null)
                            //Find profile
                        k = ftmpLine.indexOf("profile-level-id=");
                        //Check it
                        if (k!=-1)
                        {
                            //Get profile
                            String profile = ftmpLine.substring(k+17,k+23);
                            //Convert and compare
                            if (maxh264profile.isEmpty() || Integer.parseInt(profile,16)>Integer.parseInt(maxh264profile,16))
                            {
                                //Store this type provisionally
                                h264type = type;
                                //store new profile value
                                maxh264profile = profile;
                                //Check if it has packetization parameter
                                if (ftmpLine.indexOf("packetization-mode=1")!=-1)
                                    //Set it
                                    h264packetization = 1;
                            }
                    } else {
                        //check if no profile has been received so far
                        if (maxh264profile.isEmpty())
                            //Store this type provisionally
                            h264type = type;
                    }
                } else if (codec!=-1) {
                    //Set codec mapping
                     rtpOutMediaMap.get(media).put(type,codec);
                }
                } else {
                    //Static, put it in the map
                    rtpOutMediaMap.get(media).put(type,type);
            }
            }

            //Check if we have type for h264
            if (h264type>0)
            {
                //Store profile level
                h264profileLevelId = maxh264profile;
                //add it
                rtpOutMediaMap.get(media).put(h264type,Codecs.H264);
            }

            //ICE credentials
            String remoteMediaICEFrag = remoteICEFrag;
            String remtoeMediaICEPwd = remtoeICEPwd;
            //Check for global ice credentials
            ufragAtrr = md.getAttribute("ice-ufrag");
            pwdAttr = md.getAttribute("ice-pwd");

            //Check if both present
            if (ufragAtrr!=null && pwdAttr!=null)
            {
                //Using ice
                useICE = true;
                //Get values
                remoteMediaICEFrag = ufragAtrr.getValue();
                remtoeMediaICEPwd = pwdAttr.getValue();
            }

            //For each entry
            for (Integer codec : rtpOutMediaMap.get(media).values())
            {
                //Check the media type
                if (media.equals("audio"))
                {
                    //Get suppoorted codec for media
                    List<Integer> audioCodecs = supportedCodecs.get("audio");
                    //Get index
                    for (int index=0;index<audioCodecs.size();index++)
                    {
                        //Check codec
                        if (audioCodecs.get(index)==codec)
                        {
                            //Check if it is first codec for audio
                            if (priority==Integer.MAX_VALUE)
                            {
                                //Set port
                                setSendAudioPort(port);
                                //And Ip
                                setSendAudioIp(mediaIp);
                            }
                            //Check if we have a lower priority
                            if (index<priority)
                            {
                                //Store priority
                                priority = index;
                                //Set codec
                                setAudioCodec(codec);
                            }
                        }
                    }
                }
                else if (media.equals("video"))
                {
                    //Get suppoorted codec for media
                    List<Integer> videoCodecs = supportedCodecs.get("video");
                    //Get index
                    for (int index=0;index<videoCodecs.size();index++)
                    {
                        //Check codec
                        if (videoCodecs.get(index)==codec)
                        {
                            //Check if it is first codec for audio
                            if (priority==Integer.MAX_VALUE)
                            {
                                //Set port
                                setSendVideoPort(port);
                                //And Ip
                                setSendVideoIp(mediaIp);
                            }
                            //Check if we have a lower priority
                            if (index<priority)
                            {
                                //Store priority
                                priority = index;
                                //Set codec
                                setVideoCodec(codec);
                            }
                        }
                    }
                }
                else if (media.equals("text"))
                {
                    //Get suppoorted codec for media
                    List<Integer> textCodecs = supportedCodecs.get("text");
                    //Get index
                    for (int index=0;index<textCodecs.size();index++)
                    {
                        //Check codec
                        if (textCodecs.get(index)==codec)
                        {
                            //Check if it is first codec for audio
                            if (priority==Integer.MAX_VALUE)
                            {
                                //Set port
                                setSendTextPort(port);
                                //And Ip
                                setSendTextIp(mediaIp);
                            }                            //Check if we have a lower priority
                            if (index<priority)
                            {
                                //Store priority
                                priority = index;
                                //Set codec
                                setTextCodec(codec);
                            }
                        }
                    }
                }
            }
            //Check ice credentials
            if (remoteMediaICEFrag!=null && remtoeMediaICEPwd!=null)
                    //Create info and add to remote ones
                    remoteICEInfo.put(media, new ICEInfo(remoteMediaICEFrag,remtoeMediaICEPwd));
        }
        return sdp;
    }

    public void onInfoRequest(SipServletRequest request) throws IOException {
        //Check content type
        if (request.getContentType().equals("application/media_control+xml"))
        {
            //Send FPU
            sendFPU();
            //Send OK
            SipServletResponse req = request.createResponse(200, "OK");
            //Send it
            req.send();
        } else {
            SipServletResponse req = request.createResponse(500, "Not supported");
            //Send it
            req.send();
        }
    }

    public void onCancelRequest(SipServletRequest request) {
        try {
            //Create final response
            SipServletResponse resp = request.createResponse(200, "Ok");
            //Send it
            resp.send();
        } catch (IOException ex) {
            Logger.getLogger(RTPParticipant.class.getName()).log(Level.SEVERE, null, ex);
        }
        //Disconnect
        setState(State.DISCONNECTED);
        //Check cdr
        //And terminate
        destroy();
    }

    public void onCancelResponse(SipServletResponse resp) {
        //Teminate
        destroy();
    }

    public void onInfoResponse(SipServletResponse resp) {
    }

    public void onOptionsRequest(SipServletRequest request) {
        try {
            //Linphone and other UAs may send options before invite to determine public IP
            SipServletResponse response = request.createResponse(200);
            //return it
            response.send();
        } catch (IOException ex) {
            Logger.getLogger(RTPParticipant.class.getName()).log(Level.SEVERE, null, ex);
        }
    }

    public void onInviteRequest(SipServletRequest request) throws IOException {
        //Store address
        address = request.getFrom();
        //Get name
        name = getUsernameDomain();
        //Get call id
        setSessionId(request.getCallId());
        //Create ringing
        SipServletResponse resp = request.createResponse(180, "Ringing");
        //Send it
        resp.send();
        //Set state
        setState(State.WAITING_ACCEPT);
        //Check cdr
        //Get sip session
        session = request.getSession();
        //Get sip application session
        appSession = session.getApplicationSession();
        //Set expire time to wait for ACK
        appSession.setExpires(1);
        //Set reference in sessions
        appSession.setAttribute("user", this);
        session.setAttribute("user", this);
        //Do not invalidate
        appSession.setInvalidateWhenReady(false);
        session.setInvalidateWhenReady(false);
        //Check if it has content
        if (request.getContentLength()>0)
            //Process it
            proccesContent(request.getContentType(),request.getContent());
        //Store invite request
        inviteRequest = request;
        //Check if we need to autoaccapt
        if (isAutoAccept())
            //Accept it
            accept();
    }

    @Override
    public boolean accept() {
        //Check state
        if (state!=State.WAITING_ACCEPT)
        {
            //LOG
            Logger.getLogger(RTPParticipant.class.getName()).log(Level.WARNING, "Accepted participant is not in WAITING_ACCEPT state  [id:{0},state:{1}].", new Object[]{id,state});
            //Error
            return false;
        }

        try {
            //Start receiving
            startReceiving();
            //Create final response
            SipServletResponse resp = inviteRequest.createResponse(200, "Ok");
            //Attach body
            resp.setContent(createSDP(),"application/sdp");
            //Send it
            resp.send();
        } catch (Exception ex) {
            try {
                //Create final response
                SipServletResponse resp = inviteRequest.createResponse(500, ex.getMessage());
                //Send it
                resp.send();
                //Log
                Logger.getLogger(RTPParticipant.class.getName()).log(Level.SEVERE, null, ex);
            } catch (IOException ex1) {
                //Log
                Logger.getLogger(RTPParticipant.class.getName()).log(Level.SEVERE, null, ex1);
            }
            //Terminate
            error(State.ERROR, "Error");
            //Error
            return false;
        }
        //Ok
        return true;
    }

    @Override
    public boolean reject(Integer code, String reason) {
        //Check state
        if (state!=State.WAITING_ACCEPT)
        {
            //LOG
            Logger.getLogger(RTPParticipant.class.getName()).log(Level.WARNING, "Rejected participant is not in WAITING_ACCEPT state [id:{0},state:{1}].", new Object[]{id,state});
            //Error
            return false;
        }

        try {
            //Create final response
            SipServletResponse resp = inviteRequest.createResponse(code, reason);
            //Send it
            resp.send();
        } catch (IOException ex1) {
            //Log
            Logger.getLogger(RTPParticipant.class.getName()).log(Level.SEVERE, null, ex1);
            //Terminate
            error(State.ERROR, "Error");
            //Error
            return false;
        }
        //Terminate
        error(State.DECLINED,"Rejected");
        //Exit
        return true;
    }

    void doInvite(SipFactory sf, Address from,Address to) throws IOException, XmlRpcException {
        doInvite(sf,from,to,null,1,null);
    }

    void doInvite(SipFactory sf, Address from,Address to,int timeout) throws IOException, XmlRpcException {
        doInvite(sf,from,to,null,timeout,null);
    }

    void doInvite(SipFactory sf, Address from,Address to,SipURI proxy,int timeout,String location) throws IOException, XmlRpcException {
        //Store to as participant address
        address = to;
        //Start receiving media
        startReceiving();
        //Create the application session
        appSession = sf.createApplicationSession();
        // create an INVITE request to the first party from the second
        inviteRequest = sf.createRequest(appSession, "INVITE", from, to);
        //Check if we have a proxy
        if (proxy!=null)
            //Set proxy
            inviteRequest.pushRoute(proxy);
        //Get call id
        setSessionId(inviteRequest.getCallId());
        //Get sip session
        session = inviteRequest.getSession();
        //Set reference in sessions
        appSession.setAttribute("user", this);
        session.setAttribute("user", this);
        //Do not invalidate
        appSession.setInvalidateWhenReady(false);
        session.setInvalidateWhenReady(false);
        //Set expire time
        appSession.setExpires(timeout);
        //Create sdp
        String sdp = createSDP();
        //If it has location info
        if (location!=null && !location.isEmpty())
        {
            try {
                //Get SIP uri of calling user
                SipURI uri = (SipURI)from.getURI();
                //Add location header
                inviteRequest.addHeader("Geolocation","<cid:"+uri.getUser()+"@"+uri.getHost()+">;routing-allowed=yes");
                inviteRequest.addHeader("Geolocation-Routing","yes");
                //Create multipart body
                Multipart body = new MimeMultipart();
                //Create sdp body
                BodyPart sdpPart = new MimeBodyPart();
                //Set content
                sdpPart.setContent(sdp, "application/sdp");
                //Set content headers
                sdpPart.setHeader("Content-Type","application/sdp");
                sdpPart.setHeader("Content-Length", Integer.toString(sdp.length()));
                //Add sdp
                body.addBodyPart(sdpPart);
                //Create slocation body
                BodyPart locationPart = new MimeBodyPart();
                //Set content
                locationPart.setContent(location, "application/pidf+xml");
                //Set content headers
                locationPart.setHeader("Content-Type","application/pidf+xml");
                locationPart.setHeader("Content-ID","<"+uri.getUser()+"@"+uri.getHost()+">");
                locationPart.setHeader("Content-Length", Integer.toString(location.length()));
                //Add sdp
                body.addBodyPart(locationPart);
                //Add content
                inviteRequest.setContent(body, body.getContentType().replace(" \r\n\t",""));
            } catch (MessagingException ex) {
                Logger.getLogger(RTPParticipant.class.getName()).log(Level.SEVERE, null, ex);
            }
        } else {
            //Attach body
            inviteRequest.setContent(sdp,"application/sdp");
        }
        //Send it
        inviteRequest.send();
        //Log
        Logger.getLogger(RTPParticipant.class.getName()).log(Level.WARNING, "doInvite [idSession:{0}]",new Object[]{session.getId()});
        //Set state
        setState(State.CONNECTING);
        //Check cdr
    }

    public void onInviteResponse(SipServletResponse resp) throws IOException {
        //Check state
        if (state!=State.CONNECTING)
        {
            //Log
            Logger.getLogger(RTPParticipant.class.getName()).log(Level.WARNING, "onInviteResponse while not CONNECTING [id:{0},state:{1}]",new Object[]{id,state});
            //Exit
            return;
        }
        //Get code
        Integer code = resp.getStatus();
        //Check response code
        if (code<200) {
            //Check code
            switch (code)
            {
                case 180:
                    break;
                default:
                    //DO nothing
            }
        } else if (code >= 200 && code < 300) {
            //Extend expire time one minute
            appSession.setExpires(1);
            //Update name
            address = resp.getTo();
            //Update name
            name = getUsernameDomain();
            try {
                //Parse sdp
                remoteSDP = processSDP(new String((byte[])resp.getContent()));
                //Create ringing
                SipServletRequest ack = resp.createAck();
                //Send it
                ack.send();
                //Set state before joining
                setState(State.CONNECTED);
                //Join it to the conference
                conf.joinParticipant(this);
                //Start sending
                startSending();
            } catch (Exception ex) {
                Logger.getLogger(RTPParticipant.class.getName()).log(Level.SEVERE, "Error processing invite respose", ex);
                //Terminate
                error(State.ERROR,"Error");
            }
        } else if (code>=400) {
            //Check code
            switch (code) {
                case 404:
                    //Terminate
                    error(State.NOTFOUND,"NOT_FOUND");
                    break;
                case 486:
                    //Terminate
                    error(State.BUSY,"BUSY");
                    break;
                case 603:
                    //Terminate
                    error(State.DECLINED,"DECLINED");
                    break;
                case 408:
                case 480:
                case 487:
                    //Terminate
                    error(State.TIMEOUT,"TIMEOUT",code);
                    break;
                default:
                    //Terminate
                    error(State.ERROR,"ERROR",code);
                    break;
            }
            //Set expire time
            appSession.setExpires(1);
        }
    }

    public void onTimeout() {
        //Check state
        if (state==State.CONNECTED) {
            //Extend session two minutes
            appSession.setExpires(1);
            //Get statiscits
            stats = conf.getParticipantStats(id);
            //Calculate acumulated packets
            Integer num = 0;
            //For each media
            for (MediaStatistics s : stats.values())
                //Increase packet count
                num += s.numRecvPackets;
            //Check
            if (!num.equals(totalPacketCount)) {
                //Update
                totalPacketCount = num;
            }  else {
                //Terminate
                error(State.TIMEOUT,"TIMEOUT",id);
            }
        } else if (state==State.CONNECTING) {
            //Cancel request
            doCancel(true);
        } else {
            //Teminate
            destroy();
        }
    }

    public void onAckRequest(SipServletRequest request) throws IOException {
        //Check if it has content
        if (request.getContentLength()>0)
            //Process it
            proccesContent(request.getContentType(),request.getContent());
        try {
            //Set state before joining
            setState(State.CONNECTED);
            //Join it to the conference
            conf.joinParticipant(this);
            //Start sending
            startSending();

        } catch (XmlRpcException ex) {
            Logger.getLogger(RTPParticipant.class.getName()).log(Level.SEVERE, null, ex);
        }
    }

    void doCancel(boolean timedout) {
        try{
            //Create BYE request
            SipServletRequest req = inviteRequest.createCancel();
            //Send it
            req.send();
        } catch(IOException ex){
            Logger.getLogger(RTPParticipant.class.getName()).log(Level.SEVERE, null, ex);
        } catch(IllegalStateException ex){
            Logger.getLogger(RTPParticipant.class.getName()).log(Level.SEVERE, null, ex);
        }
        //Set expire time
        appSession.setExpires(1);
        //Check which state we have to set
        if (timedout)
            //TImeout
            setState(State.TIMEOUT);
        else
            //Disconnected
            setState(State.DISCONNECTED);
        //Terminate
        destroy();
    }

    void doBye() {
        try{
            //Create BYE request
            SipServletRequest req = session.createRequest("BYE");
            //Send it
            req.send();
        } catch(IOException ex){
            Logger.getLogger(RTPParticipant.class.getName()).log(Level.SEVERE, null, ex);
        } catch(IllegalStateException ex){
            Logger.getLogger(RTPParticipant.class.getName()).log(Level.SEVERE, null, ex);
        }
        try {
            //Set expire time
            appSession.setExpires(1);
        } catch (Exception ex) {
            Logger.getLogger(RTPParticipant.class.getName()).log(Level.SEVERE, "Error expiring user", ex);
        }
        //Disconnect
        setState(State.DISCONNECTED);
        //Terminate
        destroy();
    }

    public void onByeResponse(SipServletResponse resp) {
    }

    public void onByeRequest(SipServletRequest request) {
        try {
            //Create final response
            SipServletResponse resp = request.createResponse(200, "Ok");
            //Send it
            resp.send();
        } catch (IOException ex) {
            Logger.getLogger(RTPParticipant.class.getName()).log(Level.SEVERE, null, ex);
        }
        //Set expire time
        appSession.setExpires(1);
        //Disconnect
        setState(State.DISCONNECTED);
        //Terminate
        destroy();
    }

    @Override
    public void end() {
        //Log
        Logger.getLogger(RTPParticipant.class.getName()).log(Level.INFO, "Ending RTP user id:{0} in state {1}", new Object[]{id,state});
        //Depending on the state
        switch (state)
        {
            case CONNECTING:
                doCancel(false);
                break;
            case CONNECTED:
                doBye();
                break;
            default:
                //Destroy
                destroy();
        }
    }

    @Override
    public void destroy() {
        try {
            //Get client
            XmlRpcMcuClient client = conf.getMCUClient();
            //Delete participant
            client.DeleteParticipant(conf.getId(), id);
        } catch (XmlRpcException ex) {
            Logger.getLogger(RTPParticipant.class.getName()).log(Level.SEVERE, null, ex);
        }

        try {
            //If ther was a session
            if (session!=null && session.isValid())
            {
                //Remove participant from session
                session.removeAttribute("user");
                //Invalidate the session when appropiate
                session.setInvalidateWhenReady(true);
            }
            //If there was an application session
            if (appSession!=null && appSession.isValid())
            {
                //Remove participant from session
                appSession.removeAttribute("user");
                //Set expire time to let it handle any internal stuff
                appSession.setExpires(1);
                //Invalidate the session when appropiate
                appSession.setInvalidateWhenReady(true);
            }
        } catch (Exception ex) {
            Logger.getLogger(RTPParticipant.class.getName()).log(Level.SEVERE, null, ex);
        }
        //Set state
        setState(State.DESTROYED);
    }

    public void startSending() throws XmlRpcException {
        //Get client
        XmlRpcMcuClient client = conf.getMCUClient();
        //Get conf id
        Integer confId = conf.getId();

        //Check audio
        if (getSendAudioPort()!=0)
        {
            //Set codec
            client.SetAudioCodec(confId, id, getAudioCodec());
            //Get cryto info
            CryptoInfo info = remoteCryptoInfo.get("audio");
            //If present
            if (info!=null)
                //Set it
               client.SetRemoteCryptoSDES(confId, id, MediaType.AUDIO, info.suite, info.key);
            //Send
            client.StartSending(confId, id, MediaType.AUDIO, getSendAudioIp(), getSendAudioPort(), getRtpOutMediaMap("audio"));
            //Sending Audio
            isSendingAudio = true;
        }

        //Check video
        if (getSendVideoPort()!=0)
        {
            //Get profile bitrat
            int bitrate = profile.getVideoBitrate();
            //Reduce to the maximum in SDP
            if (videoBitrate>0 && videoBitrate<bitrate)
                    //Reduce it
                    bitrate = videoBitrate;
            //Set codec
            client.SetVideoCodec(confId, id, getVideoCodec(), profile.getVideoSize() , profile.getVideoFPS(), bitrate,0, 0, profile.getIntraPeriod());
            //Get cryto info
            CryptoInfo info = remoteCryptoInfo.get("video");
            //If present
            if (info!=null)
                //Set it
               client.SetRemoteCryptoSDES(confId, id, MediaType.VIDEO, info.suite, info.key);
            //Send
            client.StartSending(confId, id, MediaType.VIDEO, getSendVideoIp(), getSendVideoPort(), getRtpOutMediaMap("video"));
            //Sending Video
            isSendingVideo = true;
        }

        //Check text
        if (getSendTextPort()!=0)
        {
            //Set codec
            client.SetTextCodec(confId, id, getTextCodec());
            //Get cryto info
            CryptoInfo info = remoteCryptoInfo.get("text");
            //If present
            if (info!=null)
                //Set it
               client.SetRemoteCryptoSDES(confId, id, MediaType.TEXT, info.suite, info.key);
            //Send
            client.StartSending(confId, id, MediaType.TEXT, getSendTextIp(), getSendTextPort(), getRtpOutMediaMap("text"));
            //Sending Text
            isSendingText = true;
        }
    }

    public void startReceiving() throws XmlRpcException {
        //Get client
        XmlRpcMcuClient client = conf.getMCUClient();
        //Get conf id
        Integer confId = conf.getId();

        //If supported
        if (getAudioSupported())
        {
            //Create rtp map for audio
            createRTPMap("audio");
            //Check if we are secure
            if (isSecure)
            {
                //Create new cypher
                CryptoInfo info = CryptoInfo.Generate();
                //Set it
                client.SetLocalCryptoSDES(confId, id, MediaType.AUDIO, info.suite, info.key);
                //Add to local info
                localCryptoInfo.put("audio", info);
            }
            //Check if using ICE
            if (useICE)
            {
                //Create new ICE Info
                ICEInfo info = ICEInfo.Generate();
                //Set them
                client.SetLocalSTUNCredentials(confId, id, MediaType.AUDIO, info.pwd, info.pwd);
                //Add to local info
                localICEInfo.put("audio", info);
            }
            //Get receiving ports
            recAudioPort = client.StartReceiving(confId, id, MediaType.AUDIO, getRtpInMediaMap("audio"));
        }

        //If supported
        if (getVideoSupported())
        {
            //Create rtp map for video
            createRTPMap("video");
            //Check if we are secure
            if (isSecure)
            {
                //Create new cypher
                CryptoInfo info = CryptoInfo.Generate();
                //Set it
                client.SetLocalCryptoSDES(confId, id, MediaType.VIDEO, info.suite, info.key);
                //Add to local info
                localCryptoInfo.put("video", info);
            }
            //Check if using ICE
            if (useICE)
            {
                //Create new ICE Info
                ICEInfo info = ICEInfo.Generate();
                //Set them
                client.SetLocalSTUNCredentials(confId, id, MediaType.VIDEO, info.pwd, info.pwd);
                //Add to local info
                localICEInfo.put("video", info);
            }
            //Get receiving ports
            recVideoPort = client.StartReceiving(confId, id, MediaType.VIDEO, getRtpInMediaMap("video"));
        }

        //If supported
        if (getTextSupported())
        {
            //Create rtp map for text
            createRTPMap("text");
            //Check if we are secure
            if (isSecure)
            {
                //Create new cypher
                CryptoInfo info = CryptoInfo.Generate();
                //Set it
                client.SetLocalCryptoSDES(confId, id, MediaType.TEXT, info.suite, info.key);
                //Add to local info
                localCryptoInfo.put("text", info);
            }
            //Check if using ICE
            if (useICE)
            {
                //Create new ICE Info
                ICEInfo info = ICEInfo.Generate();
                //Set them
                client.SetLocalSTUNCredentials(confId, id, MediaType.TEXT, info.pwd, info.pwd);
                //Add to local info
                localICEInfo.put("text", info);
            }
            //Get receiving ports
            recTextPort = client.StartReceiving(confId, id, MediaType.TEXT, getRtpInMediaMap("text"));
        }

        //And ip
        setRecIp(conf.getRTPIp());
    }

   public void sendFPU() {
        //Get client
        XmlRpcMcuClient client = conf.getMCUClient();
        //Get id
        Integer confId = conf.getId();
        try {
            //Send fast pcture update
            client.SendFPU(confId, id);
        } catch (XmlRpcException ex) {
            Logger.getLogger(Conference.class.getName()).log(Level.SEVERE, null, ex);
        }
    }

    @Override
    void requestFPU() {
        //Send FPU
        String xml ="<?xml version=\"1.0\" encoding=\"utf-8\" ?>\r\n<media_control>\r\n<vc_primitive>\r\n<to_encoder>\r\n<picture_fast_update></picture_fast_update>\r\n</to_encoder>\r\n</vc_primitive>\r\n</media_control>\r\n";
        try {
             //Create ack
            SipServletRequest info = session.createRequest("INFO");
            //Set content
            info.setContent(xml, "application/media_control+xml");
            //Send it
            info.send();
        } catch (IOException ex) {
            //Log it
            Logger.getLogger(RTPParticipant.class.getName()).log(Level.SEVERE, "Error while requesting FPU for participant", ex);
        }
    }
}
