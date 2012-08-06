/*
 * To change this template, choose Tools | Templates
 * and open the template in the editor.
 */

package org.murillo.MediaServer;

import org.apache.xmlrpc.XmlRpcException;
import org.apache.xmlrpc.client.TimingOutCallback;
import org.apache.xmlrpc.client.XmlRpcClient;

/**
 *
 * @author Sergio
 */
public class XmlRpcTimedClient extends XmlRpcClient {
    private final static int XML_RPC_TIMEOUT = 10000;
    private int timeout = XML_RPC_TIMEOUT;

    public int getTimeout() {
        return timeout;
    }

    public void setTimeout(int timeout) {
        this.timeout = timeout;
    }

    @Override
    public Object execute(String pMethodName, Object[] pParams) throws XmlRpcException {
        try {
            //Create timed out callback
            TimingOutCallback callback = new TimingOutCallback(timeout);
            //Execute async
            executeAsync(pMethodName, pParams, callback);
            //Return obcjet
            return callback.waitForResponse();
        } catch (Throwable ex) {
            //Launc exception
            throw new XmlRpcException("Request timeout", ex);
        }
    }
}
