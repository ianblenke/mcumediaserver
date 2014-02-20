/*
 * MediaMixer.java
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
import java.io.Serializable;
import java.net.MalformedURLException;
import java.net.UnknownHostException;
import java.util.HashMap;
import java.util.HashSet;
import java.util.logging.Level;
import java.util.logging.Logger;
import org.murillo.MediaServer.Codecs;
import org.murillo.MediaServer.XmlRpcBroadcasterClient;
import org.murillo.MediaServer.XmlRpcMcuClient;
import org.murillo.util.SubNetInfo;
/**
 *
 * @author Sergio Garcia Murillo
 */
public class MediaMixer implements Serializable {
    private String name;
    private String url;
    private String ip;
    private String publicIp;
    private SubNetInfo localNet;

    private HashSet<XmlRpcMcuClient> mcuClients;
    private XmlRpcMcuClient client;
    private String state;

    /** Creates a new instance of MediaMixer */
    public MediaMixer(String name,String url,String ip,String publicIp,String localNet) throws MalformedURLException {
        //Save Values
        this.name = name;
        //Check if it ends with "/"
        if (url.endsWith("/"))
            //Remove it
            this.url = url.substring(0,url.length()-1);
        else
            //Copy all
            this.url = url;
        this.ip = ip;
        this.publicIp = publicIp;
        //Create default client
        client = new XmlRpcMcuClient(url + "/mcu");
        //Create client list
        mcuClients = new HashSet<XmlRpcMcuClient>();
        try {
            //parse it
            this.localNet = new SubNetInfo(localNet);
        } catch (UnknownHostException ex) {
            //Log
            Logger.getLogger(MediaMixer.class.getName()).log(Level.SEVERE, null, ex);
            //Create empty one
            this.localNet = new SubNetInfo(new byte[]{0,0,0,0},0);
        }
        //NO state
        state = "";
    }

    public String getName() {
        return name;
    }

    public String getUrl() {
        return url;
    }
    
    public String getIp() {
        return ip;
    }

    public String getPublicIp() {
        return publicIp;
    }

    public SubNetInfo getLocalNet() {
        return localNet;
    }

    public boolean isNated(String ip){
        try {
            //Check if it is a private network address  and not in local address
            if (SubNetInfo.isPrivate(ip) && !localNet.contains(ip))
                //It is nated
                return true;
        } catch (UnknownHostException ex) {
            //Log
            Logger.getLogger(MediaMixer.class.getName()).log(Level.WARNING, "Wrong IP address, doing NAT {0}", ip);
            //Do nat
            return true;
        }

        //Not nat
        return false;
    }

    public String getUID() {
        return name+"@"+url;
    }
    
    public XmlRpcBroadcasterClient createBroadcastClient() {
        XmlRpcBroadcasterClient client = null;
        try {
            client = new XmlRpcBroadcasterClient(url + "/broadcaster");
        } catch (MalformedURLException ex) {
            Logger.getLogger(MediaMixer.class.getName()).log(Level.SEVERE, null, ex);
        }
        return client;
    }

    public XmlRpcMcuClient createMcuClient() {
        XmlRpcMcuClient mcuClient = null;
        try {
            //Create client
            mcuClient = new XmlRpcMcuClient(url + "/mcu");
            //Append to set
            mcuClients.add(mcuClient);
        } catch (MalformedURLException ex) {
            Logger.getLogger(MediaMixer.class.getName()).log(Level.SEVERE, null, ex);
        }
        return mcuClient;
    }
    
    public static HashMap<Integer,String> getSizes() {
        //The map
        HashMap<Integer,String> sizes = new HashMap<Integer,String>();
        //Add values
	//Set values
	sizes.put(XmlRpcMcuClient.QCIF,	    "QCIF\t176x144:1,22");
	sizes.put(XmlRpcMcuClient.CIF,	    "CIF\t352x288:1,22");
	sizes.put(XmlRpcMcuClient.VGA,	    "VGA\t640x480:1,33");
	sizes.put(XmlRpcMcuClient.PAL,	    "PAL\t768x576:1,33");
	sizes.put(XmlRpcMcuClient.HVGA,	    "HVGA\t480x320:1,50");
	sizes.put(XmlRpcMcuClient.QVGA,	    "QVGA\t320x240:1,33");
	sizes.put(XmlRpcMcuClient.HD720P,   "HD720P\t1280x720:1,78");
	sizes.put(XmlRpcMcuClient.WQVGA,    "WQVGA\t400x240:1,67");
	sizes.put(XmlRpcMcuClient.W448P,    "W448P\t768x448:1,71");
	sizes.put(XmlRpcMcuClient.SD448P,   "SD448P\t576x448:1,29");
	sizes.put(XmlRpcMcuClient.W288P,    "W288P\t512x288:1,78");
	sizes.put(XmlRpcMcuClient.W576,	    "W576\t1024x576:1,78");
	sizes.put(XmlRpcMcuClient.FOURCIF,  "FOURCIF\t704x576:1,22");
	sizes.put(XmlRpcMcuClient.FOURSIF,  "FOURSIF\t704x480:1,47");
	sizes.put(XmlRpcMcuClient.XGA,	    "XGA\t1024x768:1,33");
	sizes.put(XmlRpcMcuClient.WVGA,	    "WVGA\t800x480:1,67");
	sizes.put(XmlRpcMcuClient.DCIF,	    "DCIF\t528x384:1,38");
	sizes.put(XmlRpcMcuClient.SIF,	    "SIF\t352x240:1,47");
	sizes.put(XmlRpcMcuClient.QSIF,	    "QSIF\t176x120:1,47");
	sizes.put(XmlRpcMcuClient.SD480P,   "SD480P\t480x360:1,33");
	sizes.put(XmlRpcMcuClient.SQCIF,    "SQCIF\t128x96:1,33");
	sizes.put(XmlRpcMcuClient.SCIF,	    "SCIF\t256x192:1,33");
        //Return map
        return sizes;
    }
    
    public static HashMap<Integer,String> getVADModes() {
        //The map
        HashMap<Integer,String> modes = new HashMap<Integer,String>();
        //Add values
        modes.put(XmlRpcMcuClient.VADNONE,"None");
        modes.put(XmlRpcMcuClient.VADFULL,"Full");
        //Return map
        return modes;
    }

    public static HashMap<Integer,String> getMosaics() {
        //The map
        HashMap<Integer,String> mosaics = new HashMap<Integer,String>();
        //Add values
        mosaics.put(XmlRpcMcuClient.MOSAIC1x1 ,"MOSAIC1x1");
        mosaics.put(XmlRpcMcuClient.MOSAIC2x2 ,"MOSAIC2x2");
        mosaics.put(XmlRpcMcuClient.MOSAIC3x3 ,"MOSAIC3x3");
        mosaics.put(XmlRpcMcuClient.MOSAIC3p4 ,"MOSAIC3+4");
        mosaics.put(XmlRpcMcuClient.MOSAIC1p7 ,"MOSAIC1+7");
        mosaics.put(XmlRpcMcuClient.MOSAIC1p5 ,"MOSAIC1+5");
        mosaics.put(XmlRpcMcuClient.MOSAIC1p1 ,"MOSAIC1+1");
        mosaics.put(XmlRpcMcuClient.MOSAICPIP1,"MOSAICPIP1");
        mosaics.put(XmlRpcMcuClient.MOSAICPIP3,"MOSAICPIP3");
        mosaics.put(XmlRpcMcuClient.MOSAIC4x4 ,"MOSAIC4x4");
        mosaics.put(XmlRpcMcuClient.MOSAIC1p4 ,"MOSAIC1+4");
        //Return map
        return mosaics;
    }

    public String getState() {
        return state;
    }

    void releaseMcuClient(XmlRpcMcuClient client) {
        //Release client
        mcuClients.remove(client);
    }
}
