///////////////////////////////////////////////////////////////////////////////
// FILE:          DeviceListBuilder.java
// PROJECT:       Micro-Manager
// SUBSYSTEM:     mmstudio
//-----------------------------------------------------------------------------
//
// AUTHOR:        Nenad Amodaj, nenad@amodaj.com, November 2006
//
// COPYRIGHT:     University of California, San Francisco, 2006
//
// LICENSE:       This file is distributed under the BSD license.
//                License text is included with the source distribution.
//
//                This file is distributed in the hope that it will be useful,
//                but WITHOUT ANY WARRANTY; without even the implied warranty
//                of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//
//                IN NO EVENT SHALL THE COPYRIGHT OWNER OR
//                CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
//                INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES.
//
// CVS:           $Id$

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;

import org.micromanager.conf.MicroscopeModel;

/**
 * Utility to generate a list of available devices for use with
 * Micro-Manager Configuration Wizard.
 */
public class DeviceListBuilder {

   public static void main(String[] args) {
	   
      if (MicroscopeModel.generateDeviceListFile())
         System.out.println("Device list generated.");
      else
         System.out.println("Device list not generated or invalid.");
 
   }

}
