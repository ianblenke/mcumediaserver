package org.murillo.mscontrol;

import java.util.HashMap;

import javax.media.mscontrol.Parameter;
import javax.media.mscontrol.Parameters;

/**
 * 
 * @author amit bhayani
 * 
 */
public class ParametersImpl extends HashMap<Parameter, Object> implements Parameters {

    public boolean hasParameter(Parameter key) {
        return containsKey(key);
    }
    public Object getParameter(Parameter key) {
        return get(key);
    }

    public Integer getIntParameter(Parameter key,Integer defaultValue) {
	//Define default value
	Integer value = defaultValue;
	//Try to conver ti
	try { value = (Integer)getParameter(key); } catch (Exception e) {}
	//return converted or default
        return value;
    }

}
