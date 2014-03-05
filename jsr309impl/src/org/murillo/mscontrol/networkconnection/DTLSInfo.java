package org.murillo.mscontrol.networkconnection;

class DTLSInfo {

    String setup;
    String hash;
    String fingerprint;

    public DTLSInfo(String setup, String hash, String fingerprint) {
	this.setup = setup;
	this.hash = hash;
	this.fingerprint = fingerprint;
    }
}
