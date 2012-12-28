/*
 * To change this template, choose Tools | Templates
 * and open the template in the editor.
 */

package org.murillo.mcuWeb;

import java.util.HashMap;

/**
 *
 * @author Sergio
 */
public class ConferenceTemplate {


    private String uid;
    private String name;
    private String did;
    private MediaMixer mixer;
    private Integer size;
    private Integer compType;
    private Profile profile;
    private String audioCodecs;
    private String videoCodecs;
    private String textCodecs;
    private Integer vad;
    private HashMap<String,String> properties;

    ConferenceTemplate(String name, String did, MediaMixer mixer, Integer size, Integer compType, Integer vad, Profile profile, String audioCodecs,String videoCodecs,String textCodecs) {
       //Set values
        this.uid = did;
        this.name = name;
        this.did = did;
        this.mixer = mixer;
        this.size = size;
        this.compType = compType;
        this.profile = profile;
        this.vad = vad;
        this.audioCodecs = audioCodecs;
        this.videoCodecs = videoCodecs;
        this.textCodecs = textCodecs;
        //Create property map
        this.properties = new HashMap<String, String>();
    }

    public String getUID() {
        return uid;
    }

     public Integer getCompType() {
        return compType;
    }

    public String getDID() {
        return did;
    }

    public MediaMixer getMixer() {
        return mixer;
    }

    public String getName() {
        return name;
    }

    public Profile getProfile() {
        return profile;
    }

    public Integer getSize() {
        return size;
    }

    public Boolean isDIDMatched(String did) {
        //Check did
        if (did==null)
            //not matched
            return false;
        //Check for default one
        if (did.equals("*"))
            //Matched
            return true;
        //Get length
        int len = did.length();
        //First check length
        if (this.did.length()!=len)
            //not matched
            return false;
        //Compare each caracter
        for(int i=0;i<len;i++)
            //They have to be the same or the pattern an X
            if (this.did.charAt(i)!='X' && this.did.charAt(i)!=did.charAt(i))
                //Not matched
                return false;
        //Matched!
        return true;
    }

    String getAudioCodecs() {
        return audioCodecs;
}

    String getVideoCodecs() {
        return videoCodecs;
    }

    String getTextCodecs() {
        return textCodecs;
    }

    Integer getVADMode() {
        return vad;
}

    public HashMap<String,String> getProperties() {
        return properties;
    }
    public void addProperty(String key,String value) {
        //Add property
        properties.put(key, value);
    }

    public void addProperties(HashMap<String,String> props) {
        //Add all
        properties.putAll(props);
    }
}
