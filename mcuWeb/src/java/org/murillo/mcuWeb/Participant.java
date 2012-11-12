/*
 * Participant.java
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
import java.util.HashSet;
import java.util.logging.Level;
import java.util.logging.Logger;
import org.apache.xmlrpc.XmlRpcException;
import org.murillo.MediaServer.Codecs.MediaType;
import org.murillo.MediaServer.XmlRpcMcuClient;

/**
 *
 * @author Sergio
 */
public abstract class Participant  implements Serializable {
    protected Integer id;
    protected String sessionId;
    protected Type type;
    protected String name;
    protected Profile profile;
    protected Boolean audioMuted;
    protected Boolean videoMuted;
    protected Boolean textMuted;
    protected Boolean audioSupported;
    protected Boolean videoSupported;
    protected Boolean textSupported;
    protected State state;
    protected Integer mosaicId;
    protected Integer sidebarId;
    protected HashSet<Listener> listeners = null;
    protected Conference conf = null;
    private boolean autoAccept;

    public interface Listener{
        public void onStateChanged(Participant part,State state);
    };

    public enum State {CREATED,CONNECTING,WAITING_ACCEPT,CONNECTED,ERROR,TIMEOUT,BUSY,DECLINED,NOTFOUND,DISCONNECTED,DESTROYED}
    public static enum Type {
        SIP("SIP",  XmlRpcMcuClient.RTP),
        WEB("WEB",  XmlRpcMcuClient.RTMP);

        public final String name;
        public final Integer value;

        Type(String name,Integer value){
            this.name = name;
            this.value = value;
        }

        public Integer valueOf() {
            return value;
        }

        public String getName() {
                return name;
        }
    };

    Participant() {
        //Default constructor for Xml Serialization
    }

    Participant(Integer id,String name,Integer mosaicId,Integer sidebarId,Conference conf,Type type) {
        //Save values
        this.id = id;
        this.conf = conf;
        this.type = type;
        this.name = name;
        this.mosaicId = mosaicId;
        this.sidebarId = sidebarId;
        //Get initial profile
        this.profile = conf.getProfile();
        //Not muted
        this.audioMuted = false;
        this.videoMuted = false;
        this.textMuted = false;
        //Supported media
        this.audioSupported = true;
        this.videoSupported = true;
        this.textSupported = true;
        //Autoaccept by default
        autoAccept = false;
        //Create listeners
        listeners = new HashSet<Listener>();
        //Initial state
        state = State.CREATED;
    }

    public String getSessionId() {
        return sessionId;
    }

    public void setSessionId(String sessionId) {
        this.sessionId = sessionId;
    }

     public Conference getConference() {
        return conf;
    }

    public Integer getId() {
        return id;
    }

    public String getName() {
        return name;
    }

    public boolean isAutoAccept() {
        return autoAccept;
    }

    public void setAutoAccept(boolean autoAccept) {
        this.autoAccept = autoAccept;
    }
    
    public Profile getVideoProfile() {
        return profile;
    }

    protected void error(State state,String message)
    {
        //Set the state
        setState(state);
        //Check cdr
        //Teminate
        destroy();
    }

    protected void error(State state,String message,Integer code)
    {
        //Set the state
        setState(state);
        //Teminate
        destroy();
    }

    protected void setState(State state) {
        //Call listeners
        for(Listener listener : listeners)
            //Call it
            listener.onStateChanged(this,state);
        //Change it
        this.state = state;
    }

    public void setName(String name) {
        this.name = name;
    }

    public State getState() {
        return state;
    }

    public Boolean getAudioSupported() {
        return audioSupported;
    }

    public Boolean getTextSupported() {
        return textSupported;
    }

    public Boolean getVideoSupported() {
        return videoSupported;
    }

    public void setListener(Listener listener) {
        listeners.add(listener);
    }

    public Type getType() {
        return type;
    }

    public void setAudioMuted(Boolean flag)
    {
        try {
            //Get client
            XmlRpcMcuClient client = conf.getMCUClient();
            //Delete participant
            client.SetMute(conf.getId(), id, MediaType.AUDIO, flag);
            //Set audio muted
            audioMuted = flag;
        } catch (XmlRpcException ex) {
            Logger.getLogger(Participant.class.getName()).log(Level.SEVERE, "Failed to mute participant.", ex);
            }
        }
    public void setVideoMuted(Boolean flag)
    {
            try {
            //Get client
            XmlRpcMcuClient client = conf.getMCUClient();
            //Delete participant
            client.SetMute(conf.getId(), id, MediaType.VIDEO, flag);
        //Set it
            videoMuted = flag;
        } catch (XmlRpcException ex) {
            Logger.getLogger(Participant.class.getName()).log(Level.SEVERE, "Failed to mute participant.", ex);
        }
    }
    public void setTextMuted(Boolean flag)
        {
            try {
            //Get client
            XmlRpcMcuClient client = conf.getMCUClient();
            //Delete participant
            client.SetMute(conf.getId(), id, MediaType.TEXT, flag);
            //Set it
            textMuted = flag;
            } catch (XmlRpcException ex) {
            Logger.getLogger(Participant.class.getName()).log(Level.SEVERE, "Failed to mute participant.", ex);
            }
            }

    public Boolean getAudioMuted() {
        return audioMuted;
            }

    public Boolean getTextMuted() {
        return textMuted;
        }

    public Boolean getVideoMuted() {
        return videoMuted;
        }

    public Integer getMosaicId() {
        return mosaicId;
    }

    public Integer getSidebarId() {
        return sidebarId;
    }

    public void setMosaicId(Integer mosaicId) {
        this.mosaicId = mosaicId;
    }

    public void setSidebarId(Integer sidebarId) {
        this.sidebarId = sidebarId;
    }


    /*** Must be overrriden by children */
    public boolean setVideoProfile(Profile profile)     { return false;}
    public boolean accept()                             { return false;}
    public boolean reject(Integer code,String reason)   { return false;}
    public void restart()                               {}
    public void end()                                   {}
    public void destroy()                               {}
    void requestFPU()                                   {}
}
