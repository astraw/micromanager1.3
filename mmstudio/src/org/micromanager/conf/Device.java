///////////////////////////////////////////////////////////////////////////////
//FILE:          Device.java
//PROJECT:       Micro-Manager
//SUBSYSTEM:     mmstudio
//-----------------------------------------------------------------------------
//
// AUTHOR:       Nenad Amodaj, nenad@amodaj.com, October 27, 2006
//
// COPYRIGHT:    University of California, San Francisco, 2006
//
// LICENSE:      This file is distributed under the BSD license.
//               License text is included with the source distribution.
//
//               This file is distributed in the hope that it will be useful,
//               but WITHOUT ANY WARRANTY; without even the implied warranty
//               of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//
//               IN NO EVENT SHALL THE COPYRIGHT OWNER OR
//               CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
//               INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES.
//
// CVS:          $Id$
//

package org.micromanager.conf;

import java.util.ArrayList;
import java.util.Hashtable;

import mmcorej.CMMCore;
import mmcorej.LongVector;
import mmcorej.MMCoreJ;
import mmcorej.StrVector;
import mmcorej.DeviceType;

/**
 * Data structure describing a general MM device.
 * Part of the MicroscopeModel. 
 *
 */public class Device {
   private String name_;
   private String adapterName_;
   private String library_;
   private Property properties_[];
   private ArrayList<Property> setupProperties_;
   private String description_;
   private DeviceType type_;
   private Hashtable<Integer, Label> setupLabels_;
   private double delayMs_;
   private boolean usesDelay_;
   private int numPos_ = 0;
   
   public Device(String name, String lib, String adapterName, String descr) {
      name_ = name;
      library_ = lib;
      adapterName_ = adapterName;
      description_ = descr;
      type_ = DeviceType.AnyType;
      setupLabels_ = new Hashtable<Integer, Label>();
      properties_ = new Property[0];
      setupProperties_ = new ArrayList<Property>();
      usesDelay_ = false;
      delayMs_ = 0.0;
   }
   public Device(String name, String lib, String adapterName) {
      this(name, lib, adapterName, "");
   }
   
   public void setTypeByInt(int typeNum) {
      type_ = DeviceType.swigToEnum(typeNum);
   }
   
   public int getTypeAsInt() {
      return type_.swigValue();
   }
   
   /**
    * Obtain all properties and their current values.
    * @param core
    * @throws Exception
    */
   public void loadDataFromHardware(CMMCore core) throws Exception {
      StrVector propNames = core.getDevicePropertyNames(name_);
      properties_ = new Property[(int) propNames.size()];
      
      // delayMs_ = core.getDeviceDelayMs(name_);
      // NOTE: do not load the delay value from the hardware
      // we will always use settings defined in the config file
      type_ = core.getDeviceType(name_);
      usesDelay_ = core.usesDeviceDelay(name_);
      
      for (int j=0; j<propNames.size(); j++){
         properties_[j] = new Property();
         properties_[j].name_ = propNames.get(j);
         properties_[j].value_ = core.getProperty(name_, propNames.get(j));
         properties_[j].readOnly_ = core.isPropertyReadOnly(name_, propNames.get(j));
         properties_[j].preInit_ = core.isPropertyPreInit(name_, propNames.get(j));
         StrVector values = core.getAllowedPropertyValues(name_, propNames.get(j));
         properties_[j].allowedValues_ = new String[(int)values.size()];
         for (int k=0; k<values.size(); k++){
            properties_[j].allowedValues_[k] = values.get(k);
         }
      }
      
      if (type_ == DeviceType.StateDevice) {
         numPos_ = core.getNumberOfStates(name_);
      } else
         numPos_ = 0;
   }
   
   public static Device[] getLibraryContents(String libName, CMMCore core) throws Exception {
      StrVector adapterNames = core.getAvailableDevices(libName);
      StrVector devDescrs = core.getAvailableDeviceDescriptions(libName);
      LongVector devTypes = core.getAvailableDeviceTypes(libName);
      
      Device[] devList = new Device[(int)adapterNames.size()];
      for (int i=0; i<adapterNames.size(); i++) {
         devList[i] = new Device("Undefined", libName, adapterNames.get(i), devDescrs.get(i));
         devList[i].setTypeByInt(devTypes.get(i));
      }
      
      return devList;
   }

   /**
    * @return Returns the name.
    */
   public String getName() {
      return name_;
   }

   /**
    * @return Returns the adapterName.
    */
   public String getAdapterName() {
      return adapterName_;
   }
   public String getDescription() {
      return description_;
   }
   
   public void addSetupProperty(Property prop) {
      setupProperties_.add(prop);
   }
   
   public void addSetupLabel(Label lab) {
      setupLabels_.put(new Integer(lab.state_), lab);
   }

   public void getSetupLabelsFromHardware(CMMCore core) throws Exception {
      // we can only add the state labels after initialization of the device!!
      if (type_ == DeviceType.StateDevice)  {
         int numPos = 0;
         numPos = core.getNumberOfStates(name_);
         StrVector stateLabels = core.getStateLabels(name_);
         for (int state = 0; state < numPos_; state++) {
            setSetupLabel(state, stateLabels.get(state));
         }
      }
   }

   public String getLibrary() {
      return library_;
   }
   
   public int getNumberOfProperties() {
      return properties_.length;
   }
   
   public Property getProperty(int idx) {
      return properties_[idx];
   }
   
   public String getPropertyValue(String propName) throws MMConfigFileException {

      Property p = findProperty(propName);
      if (p == null)
         throw new MMConfigFileException("Property " + propName + " is not defined");
      return p.value_;
   }
   
   public void setPropertyValue(String name, String value) throws MMConfigFileException {
      Property p = findProperty(name);
      if (p == null)
         throw new MMConfigFileException("Property " + name + " is not defined");
      p.value_ = value;
   }
   
   public int getNumberOfSetupProperties() {
      return setupProperties_.size();
   }
   
   public Property getSetupProperty(int idx) {
      return setupProperties_.get(idx);
   }
   
   public String getSetupPropertyValue(String propName) throws MMConfigFileException {

      Property p = findSetupProperty(propName);
      if (p == null)
         throw new MMConfigFileException("Property " + propName + " is not defined");
      return p.value_;
   }
   
   public void setSetupPropertyValue(String name, String value) throws MMConfigFileException {
      Property p = findSetupProperty(name);
      if (p == null)
         throw new MMConfigFileException("Property " + name + " is not defined");
      p.value_ = value;
   }

   public boolean isStateDevice() {
      return type_ == DeviceType.StateDevice;
   }
   
   public boolean isSerialPort() {      
      return type_ == DeviceType.SerialDevice;
   }
   
   public boolean isCamera() {
      return type_ == DeviceType.CameraDevice;
   }

   public int getNumberOfSetupLabels() {

      return setupLabels_.size();
   }

//   public String[] getLabels() {
//      return labels_;
//    }
   
   public Label getSetupLabel(int j) {
      return (Label) setupLabels_.values().toArray()[j];
   }
   
   public void setSetupLabel(int pos, String label) {
      Label l = setupLabels_.get(new Integer(pos));
      if (l == null) {
         // label does not exist so we must create one
         setupLabels_.put(new Integer(pos), new Label(label, pos));
      } else
         l.label_ = label;
   }
   
   public boolean isCore() {
      return name_.contentEquals(new StringBuffer().append(MMCoreJ.getG_Keyword_CoreDevice()));
   }
   public void setName(String newName) {
      name_ = newName;
   }
   
   public Property findProperty(String name) {
      for (int i=0; i<properties_.length; i++) {
         Property p = properties_[i];
         if (p.name_.contentEquals(new StringBuffer().append(name)))
            return p;
      }
      return null;
   }
   
   public Property findSetupProperty(String name) {
      for (int i=0; i<setupProperties_.size(); i++) {
         Property p = setupProperties_.get(i);
         if (p.name_.contentEquals(new StringBuffer().append(name)))
            return p;
      }
      return null;
   }
   public double getDelay() {
      return delayMs_;
   }
   
   public void setDelay(double delayMs) {
      delayMs_ = delayMs;
   }
   
   public boolean usesDelay() {
      return usesDelay_;
   }
   
   public int getNumberOfStates() {
      return numPos_;
   }
   public String getVerboseType() {
//      String devType = new String("unknown");
//      if (type_ == DeviceType.CameraDevice) {
//         devType = "Camera";
//      } else if (type_ == DeviceType.SerialDevice) {
//         devType = "Serial port";
//      } else if (type_ == DeviceType.ShutterDevice) {
//         devType = "Shutter";
//      } else if (type_ == DeviceType.)
      if (type_ == DeviceType.AnyType)
         return "";
      else
         return type_.toString();
   }
}
