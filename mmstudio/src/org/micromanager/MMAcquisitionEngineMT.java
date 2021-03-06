///////////////////////////////////////////////////////////////////////////////
//FILE:          MMAcquisitionEngineMT.java
//PROJECT:       Micro-Manager
//SUBSYSTEM:     mmstudio
//-----------------------------------------------------------------------------

//AUTHOR:       Nenad Amodaj, nenad@amodaj.com, Dec 1, 2007

//COPYRIGHT:    University of California, San Francisco, 2007, 2008, 2009
//              100X Imaging Inc, www.100ximaging.com, 2008

//LICENSE:      This file is distributed under the BSD license.
//License text is included with the source distribution.

//This file is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty
//of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

//IN NO EVENT SHALL THE COPYRIGHT OWNER OR
//CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
//INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES.

//CVS:          $Id: MMAcquisitionEngine.java 38 2007-04-03 01:26:31Z nenad $

package org.micromanager;

import ij.ImagePlus;
import ij.io.FileSaver;
import ij.measure.Calibration;
import ij.process.ByteProcessor;
import ij.process.ImageProcessor;
import ij.process.ImageStatistics;
import ij.process.ShortProcessor;

import java.awt.Color;
import java.awt.Rectangle;
import java.awt.event.WindowAdapter;
import java.awt.event.WindowEvent;
import java.awt.event.WindowListener;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Date;
import java.util.GregorianCalendar;
import java.util.Timer;
import java.util.TimerTask;
import java.util.prefs.BackingStoreException;
import java.util.prefs.Preferences;

import javax.swing.JOptionPane;
import javax.swing.SwingUtilities;

import mmcorej.CMMCore;
import mmcorej.Configuration;
import mmcorej.StrVector;

import org.json.JSONException;
import org.json.JSONObject;
import org.micromanager.api.AcquisitionEngine;
import org.micromanager.api.Autofocus;
import org.micromanager.api.DeviceControlGUI;
import org.micromanager.image5d.ChannelCalibration;
import org.micromanager.image5d.ChannelControl;
import org.micromanager.image5d.ChannelDisplayProperties;
import org.micromanager.image5d.Image5D;
import org.micromanager.image5d.Image5DWindow;
import org.micromanager.metadata.AcquisitionData;
import org.micromanager.metadata.DisplaySettings;
import org.micromanager.metadata.ImageKey;
import org.micromanager.metadata.ImagePropertyKeys;
import org.micromanager.metadata.MMAcqDataException;
import org.micromanager.metadata.SummaryKeys;
import org.micromanager.metadata.WellAcquisitionData;
import org.micromanager.navigation.MultiStagePosition;
import org.micromanager.navigation.PositionList;
import org.micromanager.navigation.StagePosition;
import org.micromanager.utils.Annotator;
import org.micromanager.utils.ChannelSpec;
import org.micromanager.utils.ContrastSettings;
import org.micromanager.utils.MMException;
import org.micromanager.utils.MMLogger;
import org.micromanager.utils.MemoryUtils;
import org.micromanager.utils.PositionMode;
import org.micromanager.utils.SliceMode;

/**
 * Multi-threaded acquisition engine.
 * Runs Image5d acquisition based on the protocol and generates
 * metadata in JSON format.
 */
public class MMAcquisitionEngineMT implements AcquisitionEngine {
 
   // Class to hold some image metadata.  Start to refactor out some parts of Image5D
   private class ImageData {
      public long imgWidth_;
      public long imgHeight_;
      public long imgDepth_;
      public double pixelSize_um_;
      public double pixelAspect_;
   }

   // persistent properties (app settings) 
   private Preferences prefs_;
   private Image5D img5d_[];
   private Image5DWindow i5dWin_[];
   private ImageData imgData_[];
   private String acqName_;
   private String rootName_;
   private int xWindowPos = 100;
   private int yWindowPos = 100;
   private boolean singleFrame_ = false;
   private boolean singleWindow_ = false;
   private Timer acqTimer_;
   private AcqFrameTask acqTask_;
   private AcquisitionData acqData_[];
   private MultiFieldThread multiFieldThread_;
   private double pixelSize_um_;
   private double pixelAspect_;

   private String fileSeparator_;
   protected String channelGroup_;
   protected String cameraConfig_ = "";
   protected Configuration oldChannelState_;
   protected double oldExposure_ = 10.0;

   protected int numFrames_;
   protected double frameIntervalMs_;
   protected int frameCount_;
   protected int posCount_;

   protected boolean saveFiles_ = false;
   protected boolean acquisitionLagging_ = false;

   protected CMMCore core_;
   protected PositionList posList_;
   protected DeviceControlGUI parentGUI_;
   protected String zStage_;

   ArrayList<ChannelSpec> channels_;
   double[] sliceDeltaZ_;
   double bottomZPos_;
   double topZPos_;
   double deltaZ_;

   protected double startZPosUm_;
   protected WellAcquisitionData well_;
   boolean absoluteZ_ = false;
   private String comment_ = "";
   protected boolean useSliceSetting_ = true;

   // physical dimensions which should be uniform for the entire acquisition
   private long imgWidth_ = 0;
   private long imgHeight_ = 0;
   private long imgDepth_ = 0;

   protected boolean useMultiplePositions_;

   protected int posMode_ = PositionMode.TIME_LAPSE;
   int sliceMode_ = SliceMode.CHANNELS_FIRST;
   protected boolean pause_ = false;

   protected int previousPosIdx_;
   protected boolean acqInterrupted_;
   boolean oldFocusEnabled_;
   protected boolean oldLiveRunning_;
   protected boolean acqFinished_;


   // continuous focus behavior
   boolean continuousFocusOffForZMove_ = true;
   boolean continuousFocusOffForXYMove_ = true;

   // auto-focus module
   Autofocus autofocusPlugin_;
   boolean autofocusEnabled_ = false;


   /**
    * Timer task routine triggered at each frame. 
    */
   private class AcqFrameTask extends TimerTask {
      private boolean running_ = false;
      private boolean active_ = true;
      private boolean coreLogInitialized_ = false;

      public void run() {
         running_ = true;

         // this is necessary because logging within core is set-up on per-thread basis
         // Timer task runs in a separate thread and unless we initialize we won't have any
         // log output when the task is executed
         if (!coreLogInitialized_) {
            core_.initializeLogging();
            coreLogInitialized_ = true;
         }

         if (pause_) {
            return;
         }

         if (useMultiplePositions_ && posMode_ == PositionMode.TIME_LAPSE) {
            for (int i=0; i<posList_.getNumberOfPositions(); i++)
               acquireOneFrame(i);
         } else {
            acquireOneFrame(posCount_);
         }

         running_ = false;
      }

      public boolean cancel() {
         boolean ret = super.cancel();
         active_ = false;
         running_ = false;
         return ret;
      }

      public synchronized boolean isRunning() {
         return running_;
      }
      public synchronized boolean isActive() {
         return active_;
      }
   }

   /**
    * Multi-field thread. 
    */
   private class MultiFieldThread extends Thread {
      public void run() {
         posCount_=0;
         while (posCount_ < posList_.getNumberOfPositions()) {
            MultiStagePosition pos = posList_.getPosition(posCount_);
            try {
               MultiStagePosition.goToPosition(pos, core_);
               core_.waitForSystem();
            } catch (Exception e1) {
               // TODO Auto-generated catch block
               e1.printStackTrace();
            }
            startAcquisition();

            // wait until acquisition is done
            while (isAcquisitionRunning() || !acqFinished_) {
               try {
                  Thread.sleep(1000);
               } catch (InterruptedException e) {
                  return;
               }
            }

            if (acqInterrupted_ == true) {
               break;
            }
            posCount_++;

            // shut down window if more data is coming       
//            if (/*posCount_ < posList_.getNumberOfPositions() && */  saveFiles_) {
//               SwingUtilities.invokeLater((new DisposeI5d(i5dWin_[0])));
//               i5dWin_[0] = null;
//               img5d_[0] = null;
//            }
         }
      }
   }


   /**
    * Threadsafe image5d window update.
    */
   private class RefreshI5d implements Runnable {
      private Image5DWindow i5dWin_;

      public RefreshI5d(Image5DWindow i5dw) {
         i5dWin_ = i5dw;
      }
      public void run() {
         i5dWin_.getImagePlus().updateAndDraw();
         i5dWin_.getCanvas().paint(i5dWin_.getCanvas().getGraphics());                              
      }
   }

   /**
    * Threadsafe image5d window shutdown.
    */
   @SuppressWarnings("unused")
   private class DisposeI5d implements Runnable {
      private Image5DWindow i5dWin_;

      public DisposeI5d(Image5DWindow i5dw) {
         i5dWin_ = i5dw;
      }
      public void run() {
         i5dWin_.close();
         cleanup();
      }
   }

   /**
    * Multi-threaded acquisition engine.
    */
   public MMAcquisitionEngineMT() {

      channels_ = new ArrayList<ChannelSpec>();
      //channels_.add(new ChannelSpec());
      sliceDeltaZ_ = new double[1];
      sliceDeltaZ_[0] = 0.0; // um
      bottomZPos_ = 0.0;
      topZPos_ = 0.0;
      deltaZ_ = 0.0;
      numFrames_ = 1;
      frameIntervalMs_ = 1.0; // ms
      frameCount_ = 0;
      acqInterrupted_ = false;
      acqFinished_ = true;
      posCount_ = 0;
      //metadata_ = new JSONObject();
      rootName_ = new String(DEFAULT_ROOT_NAME);
      channelGroup_ = new String(ChannelSpec.DEFAULT_CHANNEL_GROUP);
      posList_ = new PositionList();
      autofocusPlugin_ = new CoreAutofocus();
 
      // fileSeparator_ is used for display in Window title 
      fileSeparator_ = System.getProperty("file.separator");
      if (fileSeparator_ == null)
         fileSeparator_ = "/";
   }


   @SuppressWarnings("unchecked")
   public String installAutofocusPlugin(String className) {
      // instantiate auto-focusing module
      String msg = new String(className + " module loaded.");
      try {
         Class cl = Class.forName(className);
         autofocusPlugin_ = (Autofocus) cl.newInstance();
      } catch (ClassNotFoundException e) {
         msg = className + " plugin not found.";
      } catch (InstantiationException e) {
         msg = className + " instantiation to PlugIn interface failed.";
      } catch (IllegalAccessException e) {
         msg = "Illegal access exception!";
      } catch (NoClassDefFoundError e) {
         msg = className + " class definition nor found.";
      }
      System.out.println(msg);

      return msg;
   }

   public void setCore(CMMCore c) {
      core_ = c;
      if (autofocusPlugin_ != null)
         autofocusPlugin_.setMMCore(c);
   }

   public void setPositionList(PositionList posList) {
      posList_ = posList;
   }

   public ArrayList<ChannelSpec> getChannels() {
      return channels_;
   }

   public void setChannels(ArrayList<ChannelSpec> ch) {
      channels_ = ch;

      // if the list empty create one "dummy" channel
      if (channels_.size() == 0) {
         ChannelSpec cs = new ChannelSpec();
         try {
            cs.exposure_ = core_.getExposure();
         } catch (Exception e) {
            cs.exposure_ = 10.0;;
         }
         channels_.add(cs);

      }
   }

   /**
    * Set the channel group if the current hardware configuration permits.
    * @param group
    * @return - true if successful
    */
   public boolean setChannelGroup(String group) {
      if (core_.isGroupDefined(group)) {
         channelGroup_ = group;
         return true;
      } else {
         channelGroup_ = ChannelSpec.DEFAULT_CHANNEL_GROUP;
         return false;
      }
   }

   public String getChannelGroup() {
      return channelGroup_;
   }

   public void setParentGUI(DeviceControlGUI parent) {
      parentGUI_ = parent;
   }

   /**
    * Starts acquisition, based on the current protocol.
    * @throws MMAcqDataException 
    * @throws Exception
    */
   public void acquire() throws MMException, MMAcqDataException{

      zStage_ = core_.getFocusDevice();
      pause_ = false; // clear pause flag

      // check conditions for starting acq.
      if (isAcquisitionRunning()) {
         throw new MMException("Busy with the current acquisition.");
      }

      if (useMultiplePositions_ && (posList_ == null || posList_.getNumberOfPositions() < 1))
         throw new MMException("Multiple position mode is selected but position list is not defined");

      // check if the parent GUI is in the adequate state
      if (parentGUI_ != null)
      {
         oldLiveRunning_ = parentGUI_.getLiveMode();
         parentGUI_.stopAllActivity();
         if (!parentGUI_.okToAcquire())
            throw new MMException( "Unable to start acquisition.\n" +
            "Cancel 'Live' mode or other currently executing process in the main control panel.");
      }

      oldChannelState_ = null;
      try {
         oldExposure_ = core_.getExposure();
         String channelConfig = core_.getCurrentConfig(channelGroup_);
         if (channelConfig.length() > 0){
            oldChannelState_ = core_.getConfigGroupState(channelGroup_);
         }

         if (cameraConfig_.length() > 0) {
            core_.getConfigState(cameraGroup_, cameraConfig_);
            core_.setConfig(cameraGroup_, cameraConfig_);
         }

         // wait until all devices are ready
         core_.waitForSystem();
      } catch (Exception e) {
         throw new MMException(e.getMessage());
      }

      if (autofocusEnabled_ && autofocusPlugin_ == null) {
         throw new MMException( "Auto-focus plugin module (MMAutofocus_.jar) was not found.\n" +
         "Auto-focus option can not be used in this context.");                     
      }

      acquisitionLagging_ = false;
      posCount_ = 0;

      well_ = null;
      if (useMultiplePositions_) {
         // create metadata structures
         well_ = new WellAcquisitionData();
         if (saveFiles_)
            well_.createNew(acqName_, rootName_, true); // disk mapped
         else
            well_.createNew(rootName_, true); // memory mapped
         if (posMode_ == PositionMode.TIME_LAPSE) {
            startAcquisition();
         } else {
            multiFieldThread_ = new MultiFieldThread();
            multiFieldThread_.start();
         }
      } else {
         startAcquisition();
      }      
   }
   
   /**
    * Starts acquisition of a single well, based on the current protocol, using the supplied
    * acquisition data structure.
    * This command is specially designed for plate scanning and will automatically re-set
    * all appropriate parameters.
    * @throws MMAcqDataException 
    * @throws Exception
    */
   public void acquireWellScan(WellAcquisitionData wad) throws MMException, MMAcqDataException{

      zStage_ = core_.getFocusDevice();
      pause_ = false; // clear pause flag
      
      // force settings adequate for the well scanning
      useMultiplePositions_ = true;
      posMode_ = PositionMode.MULTI_FIELD;
      saveFiles_ = true;

      // check conditions for starting acq.
      if (isAcquisitionRunning()) {
         throw new MMException("Busy with the current acquisition.");
      }

      if (posList_ == null || posList_.getNumberOfPositions() < 1)
         throw new MMException("Multiple position mode is selected but position list is not defined");

      // check if the parent GUI is in the adequate state
      if (parentGUI_ != null)
      {
         oldLiveRunning_ = parentGUI_.getLiveMode();
         parentGUI_.stopAllActivity();
         if (!parentGUI_.okToAcquire())
            throw new MMException( "Unable to start acquisition.\n" +
            "Cancel 'Live' mode or other currently executing process in the main control panel.");
      }

      oldChannelState_ = null;
      try {
         oldExposure_ = core_.getExposure();
         String channelConfig = core_.getCurrentConfig(channelGroup_);
         if (channelConfig.length() > 0){
            oldChannelState_ = core_.getConfigGroupState(channelGroup_);
         }

         if (cameraConfig_.length() > 0) {
            core_.getConfigState(cameraGroup_, cameraConfig_);
            core_.setConfig(cameraGroup_, cameraConfig_);
         }

         // wait until all devices are ready
         core_.waitForSystem();
      } catch (Exception e) {
         throw new MMException(e.getMessage());
      }

      if (autofocusEnabled_ && autofocusPlugin_ == null) {
         throw new MMException( "Auto-focus plugin module (MMAutofocus_.jar) was not found.\n" +
         "Auto-focus option can not be used in this context.");                     
      }

      acquisitionLagging_ = false;
      posCount_ = 0;

      well_ = wad;
      multiFieldThread_ = new MultiFieldThread();
      multiFieldThread_.start();
   }


   /**
    * Resets the engine.
    */
   public void clear() {
      channels_.clear();
      frameCount_ = 0;
      posCount_= 0;
   }

   /**
    * Add new channel if the current state of the hardware permits.
    * 
    * @param config - configuration name
    * @param exp
    * @param zOffset
    * @param c8
    * @param c16
    * @param c
    * @return - true if successful
    */
   public boolean addChannel(String config, double exp, double zOffset, ContrastSettings c8, ContrastSettings c16, int skip, Color c) {
      if (isConfigAvailable(config)) {
         ChannelSpec channel = new ChannelSpec();
         channel.config_ = config;
         channel.exposure_ = exp;
         channel.zOffset_ = zOffset;
         channel.contrast8_ = c8;
         channel.contrast16_ = c16;
         channel.color_ = c;
         channel.skipFactorFrame_ = skip;
         channels_.add(channel);
         return true;
      } else
         return false;
   }

   public void setFrames(int numFrames, double deltaT) {
      numFrames_ = numFrames;
      frameIntervalMs_ = deltaT;
   }

   public int getCurrentFrameCount() {
      return frameCount_;
   }

   public void setSlices(double bottom, double top, double zStep, boolean absolute) {
      absoluteZ_  = absolute;
      bottomZPos_ = bottom;
      topZPos_ = top;
      if (top >= bottom)
         deltaZ_ = Math.abs(zStep);
      else
         deltaZ_ = -Math.abs(zStep);

      int numSlices = 0;
      if (Math.abs(zStep) >= getMinZStepUm())
         numSlices = (int) (Math.abs(top - bottom) / zStep + 0.5) + 1;
      sliceDeltaZ_ = new double[numSlices];
      for (int i=0; i<sliceDeltaZ_.length; i++){
         sliceDeltaZ_[i] = bottom + deltaZ_*i;
      }
      if (numSlices == 0)
      {
         sliceDeltaZ_ = new double[1];
         sliceDeltaZ_[0] = 0.0;
      }
   }

   public void setZStageDevice(String label) {
      zStage_ = label;
   }

   public void setComment(String txt) {
      comment_ = txt;
   }

   /**	
    * Get first available config group
    */
   public String getFirstConfigGroup() {
      if (core_ == null)
         return new String("");
      
      StrVector groups = core_.getAvailableConfigGroups();
      if (!groups.isEmpty())
    	  return groups.get(0);
      
      return new String("");
   }

   /**
    * Find out which channels are currently available for the selected channel group.
    * @return - list of channel (preset) names
    */
   public String[] getChannelConfigs() {
      if (core_ == null)
         return new String[0];
      StrVector vcfgs = core_.getAvailableConfigs(channelGroup_);
      String cfgs[] = new String[(int)vcfgs.size()];
      for (int i=0; i<cfgs.length; i++)
         cfgs[i] = vcfgs.get(i);
      return cfgs;
   }

   /**
    * Find out if the configuration is compatible with the current group.
    * This method should be used to verify if the acquisition protocol is consistent
    * with the current settings.
    */
   public boolean isConfigAvailable(String config) {
      StrVector vcfgs = core_.getAvailableConfigs(channelGroup_);
      for (int i=0; i<vcfgs.size(); i++)
         if (config.compareTo(vcfgs.get(i)) == 0)
            return true;
      return false;
   }

   public String[] getCameraConfigs() {
      if (core_ == null)
         return new String[0];
      StrVector vcfgs = core_.getAvailableConfigs(cameraGroup_);
      String cfgs[] = new String[(int)vcfgs.size()];
      for (int i=0; i<cfgs.length; i++)
         cfgs[i] = vcfgs.get(i);
      return cfgs;
   }

   public int getNumFrames() {
      return numFrames_;
   }

   public double getFrameIntervalMs() {
      return frameIntervalMs_;
   }

   public double getSliceZBottomUm() {
      return bottomZPos_;
   }

   public double getSliceZStepUm() {
      return deltaZ_;
   }

   public double getZTopUm() {
      return topZPos_;
   }

   public void setChannel(int row, ChannelSpec channel) {
      channels_.set(row, channel);  
   }

   public void setUpdateLiveWindow(boolean update) {
   }

   public boolean isAcquisitionLagging() {
      return acquisitionLagging_;
   }

   public void setCameraConfig(String cfg) {
      cameraConfig_ = cfg;
   }

   /**
    * Starts the acquisition.
    */
   public void startAcquisition(){
      previousPosIdx_ = -1; // initialize
      acqInterrupted_ = false;
      acqFinished_ = false;
      frameCount_ = 0;
      Runtime.getRuntime().gc();

      try {
         if (isFocusStageAvailable())
            startZPosUm_ = core_.getPosition(zStage_);
         else
            startZPosUm_ = 0;
      } catch (Exception e) {
         //i5dWin_.setTitle("Acquisition (error)");
         JOptionPane.showMessageDialog(null, e.getMessage());     
      }

      //acqTimer_ = new Timer((int)frameIntervalMs_, timerHandler);
      acqTimer_ = new Timer();
      acqTask_ = new AcqFrameTask();
      // a frame interval of 0 ms does not make sense to the timer.  set it to the smallest possible value
      if (frameIntervalMs_ < 1)
         frameIntervalMs_ = 1;
      if (numFrames_ > 0)
         acqTimer_.schedule(acqTask_, 0, (long)frameIntervalMs_);

   }

   /**
    * Acquires a single frame in the acquisition sequence.
    *
    */
   public void acquireOneFrame(int posIdx) {

      GregorianCalendar cldStart = new GregorianCalendar();
      int numSlices = useSliceSetting_ ? sliceDeltaZ_.length : 1;

      int posIndexNormalized;
      if (!useMultiplePositions_ || posMode_ == PositionMode.TIME_LAPSE)
         posIndexNormalized = posIdx;
      else
         posIndexNormalized = 0;

      // move to the required position
      try {
         oldFocusEnabled_ = core_.isContinuousFocusEnabled();
         if (useMultiplePositions_ /* && posMode_ == PositionMode.TIME_LAPSE */) {
            // time lapse logic
            MultiStagePosition pos = null;
            if (posIdx != previousPosIdx_) {
               if (continuousFocusOffForXYMove_ && oldFocusEnabled_) {
                  core_.enableContinuousFocus(false);
                  // TODO: The following needs testing (request by James Lock)
                  // Update positionList with the current Z-position
                  MultiStagePosition previousPos = posList_.getPosition(previousPosIdx_);
                  if (previousPos != null)
                  {
                     double zFocus = core_.getPosition(zStage_);
                     StagePosition sp = previousPos.get(zStage_);
                     if (sp != null) 
                        sp.x = zFocus; // assuming this is a single-axis stage set the first axis to the z value
                  }

               }
               pos = posList_.getPosition(posIdx);
               // TODO: in the case of multi-field mode the command below is redundant
               MultiStagePosition.goToPosition(pos, core_);
               core_.waitForSystem();
               if (continuousFocusOffForXYMove_ && oldFocusEnabled_) {
                  core_.enableContinuousFocus(true);
                  waitForFocusLock();
               }
            } else
               pos = posList_.getPosition(previousPosIdx_);

            // perform auto focusing if the module is available
            if (autofocusPlugin_ != null && autofocusEnabled_) {
               // switch off hardware autofocus if it was still on
               boolean focusEnabled = core_.isContinuousFocusEnabled();
               if (focusEnabled)
                  core_.enableContinuousFocus(false);
               
               // check for any autofocusing instructions
               if (pos.hasProperty(PositionList.AF_KEY)) {
                  // check if recognize any tags and see if we can do anything about it
                  if (pos.getProperty(PositionList.AF_KEY).equals(PositionList.AF_VALUE_INCREMENTAL))
                     autofocusPlugin_.incrementalFocus();
                  else if (pos.getProperty(PositionList.AF_KEY).equals(PositionList.AF_VALUE_FULL))
                     autofocusPlugin_.fullFocus();
                  else if (pos.getProperty(PositionList.AF_KEY).equals(PositionList.AF_VALUE_NONE))
                     ; // do nothing
                  else
                     // unrecognized tag
                     throw new MMException("Unrecognized Auto-focus property in position list");
               } else {
                  // no instructions, so do full focus on each position
                  autofocusPlugin_.fullFocus();
               }
               
               // update the Z-position based on the autofocus
               if (pos != null)
               {
                  double zFocus = core_.getPosition(zStage_);
                  StagePosition sp = pos.get(zStage_);
                  if (sp != null)
                     sp.x = zFocus; // assuming this is a single-axis stage set the first axis to the z value
               }
               // if hardware autofocus was on, re-enable
               if (focusEnabled)
                  core_.enableContinuousFocus(true);
            }

            previousPosIdx_ = posIdx;

            // refresh the current z position
            try {
               if (isFocusStageAvailable())
                  startZPosUm_ = core_.getPosition(zStage_);
               else
                  startZPosUm_ = 0;
            } catch (Exception e) {
               //i5dWin_.setTitle("Acquisition (error)");
               JOptionPane.showMessageDialog(null, e.getMessage());     
            }
         }

         if (oldFocusEnabled_) {
            waitForFocusLock();
         }

         if (numSlices > 1 && oldFocusEnabled_ && continuousFocusOffForZMove_) {
            core_.enableContinuousFocus(false);
         }
         if (sliceMode_ == SliceMode.CHANNELS_FIRST) {
            for (int j=0; j<numSlices; j++) {         
               double z = startZPosUm_;
               double zCur = z;

               if (useSliceSetting_) {
                  if (absoluteZ_) {
                     z = sliceDeltaZ_[j];
                  } else {
                     z = startZPosUm_ + sliceDeltaZ_[j];
                  }
               }

               if (isFocusStageAvailable() && numSlices > 1) {
                  core_.setPosition(zStage_, z);
                  if (oldFocusEnabled_ && !continuousFocusOffForZMove_) 
                     waitForFocusLock();
                  zCur = z;
               }
               for (int k=0; k<channels_.size(); k++) {
                  ChannelSpec cs = channels_.get(k);

                  executeProtocolBody(cs, z, zCur, j, k, posIdx, numSlices, posIndexNormalized);
               }
            }
         } else if (sliceMode_ == SliceMode.SLICES_FIRST) {

            for (int k=0; k<channels_.size(); k++) {
               ChannelSpec cs = channels_.get(k);
               for (int j=0; j<numSlices; j++) {         
                  double z = startZPosUm_;
                  double zCur = z;

                  if (useSliceSetting_) {
                     if (absoluteZ_) {
                        z = sliceDeltaZ_[j];
                     } else {
                        z = startZPosUm_ + sliceDeltaZ_[j];
                     }
                  }

                  if (isFocusStageAvailable() && numSlices > 1) {
                     core_.setPosition(zStage_, z);
                     zCur = z;
                  }
                  executeProtocolBody(cs, z, zCur, j, k, posIdx, numSlices, posIndexNormalized);                  
               }
            }
         } else {
            throw new MMException("Unrecognized slice mode: " + sliceMode_);
         }

         // return to the starting position
         if (isFocusStageAvailable() && numSlices > 1) {
            core_.setPosition(zStage_, startZPosUm_);
            core_.waitForDevice(zStage_);
         }
 
         // turn the continuous focus back again       
         if (numSlices > 1 && oldFocusEnabled_ && continuousFocusOffForZMove_) {
            core_.enableContinuousFocus(true);
            waitForFocusLock();
         }


      } catch(MMException e) {
         stop(true);
         restoreSystem();
         acqFinished_ = true;
         if (e.getMessage().length() > 0)
            JOptionPane.showMessageDialog(null, e.getMessage()); 
         return;
      }  catch (OutOfMemoryError e) {
         stop(true);
         restoreSystem();
         acqFinished_ = true;
         JOptionPane.showMessageDialog(null, e.getMessage() + "\nOut of memory - acquistion stopped.\n" +
         "In the future you can try to increase the amount of memory available to the Java VM (ImageJ).");     
         return;       
      } catch (IOException e) {
         stop(true);
         restoreSystem();
         acqFinished_ = true;
         JOptionPane.showMessageDialog(null, e.getMessage()); 
         return;
      } catch (JSONException e) {
         stop(true);
         restoreSystem();
         acqFinished_ = true;
         JOptionPane.showMessageDialog(null, e.getMessage()); 
         return;
      } catch (Exception e) {
         e.printStackTrace();
         stop(true);
         restoreSystem();
         acqFinished_ = true;
         if (e.getMessage().length() > 0) 
            JOptionPane.showMessageDialog(null, e.getMessage()); 
         return;
      }

      // Processing for the first frame in the sequence
      if (frameCount_ == 0) {
         // insert contrast settings metadata
         try {
            // contrast settings
            for (int i=0; i<channels_.size(); i++) {

            	int index=(null!=img5d_[posIndexNormalized])
            		?posIndexNormalized
            		:0;
               
               ChannelDisplayProperties cdp = img5d_[index].getChannelDisplayProperties(i+1);
               DisplaySettings ds = new DisplaySettings();
               ds.min = cdp.getMinValue();
               ds.max = cdp.getMaxValue();
               acqData_[posIndexNormalized].setChannelDisplaySetting(i, ds);
               
            }            
         } catch (MMAcqDataException e) {
            // TODO Auto-generated catch block
            e.printStackTrace();
         }
      }

      if (i5dWin_[posIndexNormalized] != null) {
    	   i5dWin_[posIndexNormalized].startCountdown((long)frameIntervalMs_ - (GregorianCalendar.getInstance().getTimeInMillis() - cldStart.getTimeInMillis()), numFrames_ - frameCount_);
      }

      try {
         acqData_[posIndexNormalized].setDimensions(frameCount_+1, channels_.size(), useSliceSetting_ ? sliceDeltaZ_.length : 1);
      } catch (MMAcqDataException e) {
         // TODO Auto-generated catch block
         e.printStackTrace();
      }

      // update frame counter
      if (useMultiplePositions_ && posMode_ == PositionMode.TIME_LAPSE) {
         if (posIdx == posList_.getNumberOfPositions() - 1)
            frameCount_++;
      } else {
         frameCount_++;      
      }

      // check the termination criterion
      if(frameCount_ >= numFrames_) {
         // acquisition finished
         stop(false);

         if (singleWindow_) {
            String statusLine = "Acquisition Completed";
            parentGUI_.displayStatusLine(statusLine);
         } else {
            // TODO: since stop sets i5dWin_ to null, the following has no effect!
            // adjust the title
            setImageNames(posIdx, " (completed) ");
         }
         
         // return to initial state
         restoreSystem();
         acqFinished_ = true;
         if (posMode_ != PositionMode.MULTI_FIELD) 
            cleanup();
         return;
      }
   }

   static public boolean saveImageFile(String fname, Object img, int width, int height) {
      ImageProcessor ip;
      if (img instanceof byte[]) {
         ip = new ByteProcessor(width, height);
         ip.setPixels((byte[])img);
      }
      else if (img instanceof short[]) {
         ip = new ShortProcessor(width, height);
         ip.setPixels((short[])img);
      }
      else
         return false;

      ImagePlus imp = new ImagePlus(fname, ip);
      FileSaver fs = new FileSaver(imp);
      return fs.saveAsTiff(fname);
   }

   public void restoreSystem() {
      try {
         core_.setExposure(oldExposure_);
         
         // The following might be superfluous, since the stage already returned to its original position
         // Not sure if it is always the case, so leave it in until that is assured.  This movement will 
         // kill the continuous focus, so test for its state first
         if (isFocusStageAvailable()  && !core_.isContinuousFocusEnabled()) {
            core_.setPosition(zStage_, startZPosUm_);
            core_.waitForDevice(zStage_);
         }
         
         if (oldChannelState_ != null) {
            core_.setSystemState(oldChannelState_);
            core_.waitForSystem();
            if (oldLiveRunning_)
               parentGUI_.enableLiveMode(true);
         }
         // Continuous Focus is likely to be on already at this point
         core_.enableContinuousFocus(oldFocusEnabled_);
         core_.waitForSystem();

      } catch (Exception e) {
         // do not complain here
      }      
   }

   public String getVerboseSummary() {
      int numSlices = useSliceSetting_ ? sliceDeltaZ_.length : 1;
      int totalImages = numFrames_ * numSlices * channels_.size();
      double totalDurationSec = frameIntervalMs_ * numFrames_ / 1000.0;
      int hrs = (int)(totalDurationSec / 3600);
      double remainSec = totalDurationSec - hrs*3600;
      int mins = (int)(remainSec / 60);
      remainSec = remainSec - mins * 60;

      Runtime rt = Runtime.getRuntime();
      rt.gc();

      String txt;
      txt = "Number of channels: " + channels_.size() + 
      "\nNumber of slices: " + numSlices +
      "\nNumber of frames: " + numFrames_ +
      "\nTotal images: " + totalImages +
      "\nDuration: " + hrs + "h " + mins + "m " + remainSec + "s";
      String order = new String("\nOrder: ");
      if (useMultiplePositions_) {
         if (posMode_ == PositionMode.TIME_LAPSE)
         {
            order += "Frame,Position";
         } else if (posMode_ == PositionMode.MULTI_FIELD) {
            order += "Position,Frame";
         }
      } else
         order += "Frame";

      if (sliceMode_ == SliceMode.CHANNELS_FIRST)
         order += ",Slice,Channel";
      else
         order += ",Channel,Slice";

      return txt + order;
   }

   public void stop(boolean interrupted) {
      acqInterrupted_ = interrupted;

      if (acqTask_ != null) {
         //acqTimer_.stop();
         acqTask_.cancel();
         // wait until task finishes
         while (isAcquisitionRunning()) {
            try {
               Thread.sleep(50);
            } catch (InterruptedException e) {
               // TODO Auto-generated catch block
               e.printStackTrace();
            }
         }
      }

      if (i5dWin_ == null)
         return;

      if (acqData_ == null)
         return;

      for (int i=0; i<i5dWin_.length; i++) {
         if (saveFiles_ && acqData_[i] != null) {
            try {
               acqData_[i].saveMetadata();
            } catch (MMAcqDataException e) {
               // TODO Auto-generated catch block
               e.printStackTrace();
            }
         }

         if (i5dWin_[i] != null) {
            i5dWin_[i].stopCountdown();
            Date enddate = GregorianCalendar.getInstance().getTime();
            if (useMultiplePositions_ && (well_ != null))
               i5dWin_[i].setTitle(well_.getLabel() + fileSeparator_ + posList_.getPosition(i).getLabel() + " (finished) " + enddate);
            else
               i5dWin_[i].setTitle(acqData_[i].getName() + " (finished) " + enddate);
            i5dWin_[i].setAcquisitionData(acqData_[i]);
            i5dWin_[i].setActive(false);
            i5dWin_[i].setAcquitionEngine(null); // disengage from the acquisition
            i5dWin_[i].setPlaybackFrames(frameCount_);
         }
      }
      if ((posMode_ != PositionMode.MULTI_FIELD) || (posCount_ >= posList_.getNumberOfPositions())) 
         cleanup();
   }

   public boolean isAcquisitionRunning() {
      if (acqTask_ == null)
         return false;

      return acqTask_.isActive() || acqTask_.isRunning();
   }
   
   public boolean isMultiFieldRunning() {
      return multiFieldThread_.isAlive();
   }

   protected boolean isFocusStageAvailable() {
      if (zStage_ != null && zStage_.length() > 0)
         return true;
      else
         return false;
   }

   private void waitForFocusLock()  throws Exception{
      // wait up to 3 second for focus to lock:
      int waitMs= 0;
      int interval = 100;
      while (!core_.isContinuousFocusLocked() && waitMs < 3000) {
		 Thread.sleep(interval);
         waitMs += interval;
      }
   }

   /**
    * Unconditional shutdown
    *
    */
   public void shutdown() {
      if (isAcquisitionRunning())
         stop(false);
      acqFinished_ = true;
      if (multiFieldThread_ != null)
         multiFieldThread_.interrupt();
      cleanup();
   }

   private void cleanup () {
      MMLogger.getLogger().info("cleaning up acquisition engine");
      if (singleWindow_ && i5dWin_ != null && i5dWin_[0] != null)
         i5dWin_[0].close();
	   i5dWin_ = null;
	   img5d_ = null;
	   acqData_ = null;
	   well_ = null;
   }
   
   public double getCurrentZPos() {
      if (isFocusStageAvailable()) {
         double z = 0.0;
         try {
            //core_.waitForDevice(zStage_);
            // NS: make sure we work with the current Focus device
            z = core_.getPosition(core_.getFocusDevice());
         } catch (Exception e) {
            // TODO Auto-generated catch block
            e.printStackTrace();
         }
         return z;
      }
      return 0;
   }

   public double getMinZStepUm() {
      // TODO: obtain this information from hardware
      // hard-coded to 0.1 um
      return 0.1;
   }

   public void setDirName(String dirName_) {
      this.acqName_ = dirName_;
   }

   public String getDirName() {
      return acqName_;
   }

   public void setRootName(String rootName_) {
      this.rootName_ = rootName_;
   }

   public String getRootName() {
      return rootName_;
   }

   public void setSaveFiles(boolean saveFiles) {
      saveFiles_ = saveFiles;
   }

   public boolean getSaveFiles() {
      return saveFiles_;
   }

   // depreciated
   public void setSingleFrame(boolean singleFrame) {
      singleFrame_ = singleFrame;
   }
   //
   // depreciated
   public void setSingleWindow(boolean singleWindow) {
	  singleWindow_ = singleWindow;
   }

   public void setDisplayMode(int mode) {
      if (mode == 0) {
         singleFrame_ = false;
         singleWindow_ = false;
      } else if (mode == 1) {
         singleFrame_ = true;
         singleWindow_ = false;
      } else if (mode == 2) {
         singleFrame_ = false;
         singleWindow_ = true;
      }
   }

   public int getDisplayMode() {
      if (singleFrame_)
         return 1;
      if (singleWindow_)
         return 2;
      return 0;
   }

   public void setParameterPreferences(Preferences prefs) {
      prefs_ = prefs;
   }

   /**
    * Acquisition directory set-up.
    * @throws IOException 
    * @throws JSONException 
    * @throws MMAcqDataException 
    */
   private void acquisitionSetup(int posIdx) throws IOException, MMAcqDataException {
      if (useMultiplePositions_) {
         if (posMode_ == PositionMode.TIME_LAPSE) {
            acqData_ = new AcquisitionData[posList_.getNumberOfPositions()];
            for (int i=0; i<acqData_.length; i++)
               if (saveFiles_)
                  acqData_[i] = well_.createNewImagingSite(posList_.getPosition(i).getLabel(), false);
               else
                  acqData_[i] = well_.createNewImagingSite();
         } else {
            acqData_ = new AcquisitionData[1];
            if (saveFiles_)
               acqData_[0] = well_.createNewImagingSite(posList_.getPosition(posIdx).getLabel(), false);
            else
               acqData_[0] = well_.createNewImagingSite();
         }

      } else {
         acqData_ = new AcquisitionData[1];
         acqData_[0] = new AcquisitionData();
         if (saveFiles_)
            acqData_[0].createNew(acqName_, rootName_, true); // disk mapped
         else
            acqData_[0].createNew(); // memory mapped
      }

      // save a copy of the acquisition parameters
      if (saveFiles_) {
         for (int i=0; i<acqData_.length; i++) {
            // TODO: do this elsewhere
            //i5dWin_[i].setAcqSavePath(acqData_[i].getBasePath());
            FileOutputStream os = new FileOutputStream(acqData_[i].getBasePath() + "/" + ImageKey.ACQUISITION_FILE_NAME);

            if (prefs_ != null) {              
               try {
                  prefs_.exportNode(os);
               } catch (BackingStoreException e) {
                  // TODO Auto-generated catch block
                  e.printStackTrace();
               }
            }
         }
      }

      for (int i=0; i<imgData_.length; i++) {
    	   int index = (null != imgData_[i])?i:0;
    	   /*
    	   ImageProcessor ip = i5dWin_[index].getImagePlus().getProcessor();         
         int imgDepth = 0;
         if (ip instanceof ByteProcessor)
            imgDepth = 1;
         else if (ip instanceof ShortProcessor)
            imgDepth = 2;
         */
    	   
         //acqData_[i].setImagePhysicalDimensions(ip.getWidth(), ip.getHeight(), imgDepth);
         acqData_[i].setImagePhysicalDimensions((int) imgData_[index].imgWidth_, 
               (int) imgData_[index].imgHeight_, (int) imgData_[index].imgDepth_);
         acqData_[i].setDimensions(0, channels_.size(), useSliceSetting_ ? sliceDeltaZ_.length : 1);
         acqData_[i].setComment(comment_);
         acqData_[i].setPixelSizeUm(pixelSize_um_);
         acqData_[i].setImageIntervalMs(frameIntervalMs_);
         acqData_[i].setSummaryValue(SummaryKeys.IMAGE_Z_STEP_UM, Double.toString(deltaZ_));
         acqData_[i].setSummaryValue(SummaryKeys.IMAGE_PIXEL_ASPECT, Double.toString(pixelAspect_));

         for (int j=0; j < channels_.size(); j++) {
            Color c = channels_.get(j).color_;
            acqData_[i].setChannelColor(j, c.getRGB());
            acqData_[i].setChannelName(j, channels_.get(j).config_);
         }

         if (useMultiplePositions_) {
            MultiStagePosition mps = posList_.getPosition(posIdx);

            // insert position label
            acqData_[i].setSummaryValue(SummaryKeys.POSITION, mps.getLabel());

            // insert grid coordinates
            acqData_[i].setSummaryValue(SummaryKeys.GRID_ROW, Integer.toString(mps.getGridRow()));
            acqData_[i].setSummaryValue(SummaryKeys.GRID_COLUMN, Integer.toString(mps.getGridColumn()));

            // insert properties inherited from the position list
            String keys[] = mps.getPropertyNames();
            for (int k=0; k<keys.length; k++) {
               acqData_[i].setPositionProperty(keys[k], mps.getProperty(keys[k]));
            }
         }
      }
   }

   public void enableZSliceSetting(boolean b) {
      useSliceSetting_  = b;
   }

   public boolean isZSliceSettingEnabled() {
      return useSliceSetting_;
   }

   public void enableMultiPosition(boolean b) {
      useMultiplePositions_ = b;
   }

   public boolean isMultiPositionEnabled() {
      return useMultiplePositions_;
   }

   public String[] getAvailableGroups() {
      StrVector groups = core_.getAvailableConfigGroups();
      String strGroups[] = new String[(int)groups.size()];
      for (int i=0; i<groups.size(); i++) {
         strGroups[i] = groups.get(i);
      }
      return strGroups;
   }

   /**
    * Creates and configures the Image5d and associated window based
    * on the acquisition protocol.
    *  
    * @throws Exception
    */
   private void setupImage5d(int posIndex) throws MMException {
      imgWidth_ = core_.getImageWidth();
      imgHeight_ = core_.getImageHeight();
      imgDepth_ = core_.getBytesPerPixel();
      pixelSize_um_ = core_.getPixelSizeUm();
      pixelAspect_ = 1.0; // TODO: obtain from core

      if (singleWindow_ && posIndex > 0) 
         return;
      
      int type;
      if (imgDepth_ == 1)
         type = ImagePlus.GRAY8;
      else if (imgDepth_ == 2)
         type = ImagePlus.GRAY16;
      else
         throw new MMException("Unsupported pixel depth");

      // create a new Image5D object
      int numSlices = 1;
      if (useSliceSetting_)
         numSlices = sliceDeltaZ_.length;

      if (useMultiplePositions_ && posMode_ == PositionMode.TIME_LAPSE) {
         img5d_ = new Image5D[posList_.getNumberOfPositions()]; 
         i5dWin_ = new Image5DWindow[posList_.getNumberOfPositions()];
         imgData_ = new ImageData[posList_.getNumberOfPositions()];
      } else if (useMultiplePositions_ && posMode_ == PositionMode.MULTI_FIELD) {
         // multi-field mode special handling
         if (posIndex == 0) {
            if (img5d_ == null || img5d_.length != 1) {
               img5d_ = new Image5D[1];
               i5dWin_ = new Image5DWindow[1];
               imgData_ = new ImageData[1];
            }
         } else {
            // reset
         }
      } else {
         img5d_ = new Image5D[1];
         i5dWin_ = new Image5DWindow[1];
         imgData_ = new ImageData[1];
      }

      int n = 1;
      if (!singleWindow_)
         n = img5d_.length;
      for (int i=0; i < n ; i++) 
      {
         int actualFrames = singleFrame_ ? 1 : numFrames_;
         boolean newWindow = false;
         if (!(useMultiplePositions_ && posMode_ == PositionMode.MULTI_FIELD) || posIndex == 0) {
            if (posMode_ == PositionMode.MULTI_FIELD) {
               if (img5d_[i] == null || img5d_[i].getType() != type || img5d_[i].getWidth() != imgWidth_ || img5d_[i].getHeight() != imgHeight_ ||
                   img5d_[i].getNSlices() != numSlices || img5d_[i].getNFrames() != actualFrames || img5d_[i].getNChannels() != channels_.size() ||
                   (i5dWin_[i] != null && i5dWin_[i].isClosed())) {
                  img5d_[i] = new Image5D(acqName_, type, (int)imgWidth_, (int)imgHeight_, channels_.size(), numSlices, actualFrames, false);
                  newWindow = true;
               }
            } else {
               img5d_[i] = new Image5D(acqName_, type, (int)imgWidth_, (int)imgHeight_, channels_.size(), numSlices, actualFrames, false);
               newWindow = true;
            }
         }
         imgData_[i] = new ImageData();
         imgData_[i].imgWidth_ = core_.getImageWidth();
         imgData_[i].imgHeight_ = core_.getImageHeight();
         imgData_[i].imgDepth_ = core_.getBytesPerPixel();
         imgData_[i].pixelSize_um_ = core_.getPixelSizeUm();
         imgData_[i].pixelAspect_ = 1.0;
            
         // Set ImageJ calibration:
         Calibration cal = new Calibration();
         if (pixelSize_um_ != 0)
         {
            cal.setUnit("um");
            cal.pixelWidth = pixelSize_um_;
            cal.pixelHeight = pixelSize_um_ * pixelAspect_;
         }
         if (numSlices > 1) {
            cal.pixelDepth = sliceDeltaZ_.length;
            cal.setUnit("um");
         }
         if (numFrames_ > 1) {
            cal.frameInterval = frameIntervalMs_;                                      
            cal.setTimeUnit("ms");                                             
         }
         img5d_[i].setCalibration(cal);

         for (int j=0; j<channels_.size(); j++) {
            ChannelCalibration chcal = new ChannelCalibration();
            chcal.setLabel(channels_.get(j).config_);
            img5d_[i].setChannelCalibration(j+1, chcal);

            // set color
            img5d_[i].setChannelColorModel(j+1, ChannelDisplayProperties.createModelFromColor(channels_.get(j).color_));            
         }

         // pop-up 5d image window
         if (singleWindow_ && i5dWin_[i] == null)
            i5dWin_[i] = new Image5DWindow(img5d_[i], false);
         else if (newWindow || i5dWin_[i] == null)
            i5dWin_[i] = new Image5DWindow(img5d_[i]);


         // set the desired display mode.  This needs to be called after opening the Window
         // Note that OVERLAY mode is much slower than others, so show a single channel in a fast mode
         if (channels_.size()==1)
            img5d_[i].setDisplayMode(ChannelControl.ONE_CHANNEL_COLOR);
         else
            img5d_[i].setDisplayMode(ChannelControl.OVERLAY);

         //WindowManager.addWindow(i5dWin_);
         ChannelSpec[] cs = new ChannelSpec[channels_.size()];
         for (int j=0; j<channels_.size(); j++) {
            cs[j] = channels_.get(j);
         }
         i5dWin_[i].setMMChannelData(cs);
         i5dWin_[i].setLocation(xWindowPos + i*30, yWindowPos + i*30);

         if (i==0 && !singleWindow_) {
            // add listener to the IJ window to detect when it closes
            // (use only the first window in the multi-pos case)
            WindowListener wndCloser = new WindowAdapter() {
               public void windowClosing(WindowEvent e) {
                  // TODO: this does not work anymore since i5dWin_ will be null at this point
                  if (i5dWin_ != null) {
                     Rectangle r = i5dWin_[0].getBounds();
                     // record the position of the IJ window
                     xWindowPos = r.x;
                     yWindowPos = r.y;
                  }
               }
            };      

            i5dWin_[0].addWindowListener(wndCloser);
         }

         // hook up with the acquisition engine
         i5dWin_[i].setAcquitionEngine(this);

         i5dWin_[i].setActive(true);
      }
   }

   private void setImageNames(int posIndex, String comment) {
      if (singleWindow_)
         return;

      GregorianCalendar cld = new GregorianCalendar();
      if (i5dWin_ != null && acqData_ != null) {
         for (int i = 0; i < i5dWin_.length; i++) {
            if (useMultiplePositions_ && well_ != null) {
               if (posMode_ == PositionMode.TIME_LAPSE)
                  // time-lapse
                  i5dWin_[i].setTitle(well_.getLabel() + fileSeparator_ + posList_.getPosition(i).getLabel() + comment + cld.getTime());
               else
                  // multi-field
                  i5dWin_[i].setTitle(well_.getLabel() + fileSeparator_ +  posList_.getPosition(posIndex).getLabel() + comment + cld.getTime());
            } else
               // single position
               i5dWin_[i].setTitle(acqData_[i].getName() + comment + cld.getTime());
         }
      }
   }


   public int getPositionMode() {
      return posMode_;
   }

   public int getSliceMode() {
      return sliceMode_;
   }

   public void setPositionMode(int mode) {
      posMode_ = mode;
   }

   public void setSliceMode(int mode) {
      sliceMode_ = mode;
   }

   public void enableAutoFocus(boolean enable) {
      autofocusEnabled_ = enable;
   }

   public boolean isAutoFocusEnabled() {
      return autofocusEnabled_;
   }

   public void setContinuousFocusOffForXYMove(boolean state) {
      continuousFocusOffForXYMove_ = state;
   }

   public void setContinuousFocusOffForZMove(boolean state) {
      continuousFocusOffForZMove_ = state;
   }

   /*
    * Acquires a single frame during acquisition
    *
    * Displays in an Image5D (if requested) or in Live Window
    * Attached metadata
    * @param cs - channel specification
    * @param z - Current Z position 
    * @param zCur - Used to return the now Current Z position 
    * @param sliceIdx - Number of (z) slice
    * @param channelIdx - Number of channel
    * @param posIdx - Number of position (when using multi-field)
    * @param numSlices - Total number of (z) slices
    * @param posIndexNormalized - ???
    */
   private void executeProtocolBody(ChannelSpec cs, double z, double zCur, int sliceIdx,
         int channelIdx, int posIdx, int numSlices, int posIndexNormalized) throws MMException, IOException, JSONException, MMAcqDataException{

      int actualFrameCount = singleFrame_ ? 0 : frameCount_;

      // check if we need to skip
      if (frameCount_> 0 && frameCount_ % (Math.abs(cs.skipFactorFrame_)+1) != 0) {

         // attempt to fill in the gap by using the most recent channel data
         // TODO: check what happens in case of singleWindow_
         if (!singleFrame_) {
            int offset = frameCount_ % (Math.abs(cs.skipFactorFrame_) + 1);

            int index = 0;
            if (img5d_[posIndexNormalized] != null)
          	   index = posIndexNormalized;

            Object previousImg = img5d_[index].getPixels(channelIdx+1, sliceIdx+1, actualFrameCount + 1 - offset);
            if (previousImg != null)
               img5d_[0].setPixels(previousImg, channelIdx+1, sliceIdx+1, actualFrameCount + 1);
         }
         return; // skip this frame
      }

      // apply z-offsets
      // GregorianCalendar cldAcq = new GregorianCalendar();
      double exposureMs = cs.exposure_;
      Object img;
      boolean focusSwitchedOff = false;
      try {
         if (isFocusStageAvailable() && cs.zOffset_ != 0.0 && cs.config_.length() > 0) {
            core_.waitForDevice(zStage_);
            if (continuousFocusOffForZMove_ && oldFocusEnabled_ && core_.isContinuousFocusEnabled()) {
               core_.enableContinuousFocus(false);
               focusSwitchedOff = true;
            }
            double zOffset = z + cs.zOffset_;
            core_.setPosition(zStage_, zOffset);
            zCur = zOffset;
         }

         if (cs.config_.length() > 0) {
            core_.setConfig(channelGroup_, cs.config_);
            core_.setExposure(cs.exposure_);
         }
  
         // snap and retrieve pixels
         core_.snapImage();
         img = core_.getImage();

         if (img != null && singleWindow_) {
        	   String statusLine = String.format("Frame %d Channel %s Slice %d Pos %d ",
        			 frameCount_, cs.config_, sliceIdx, posIndexNormalized);
        	   parentGUI_.displayImageWithStatusLine(img,statusLine);
         }

         // Restore autofocus lock if we had switched it off 
         if (focusSwitchedOff) {
            core_.enableContinuousFocus(true);
            waitForFocusLock();
         }
         
      } catch (Exception e) {
         throw new MMException(e.getMessage());
      }
      
      long width = core_.getImageWidth();
      long height = core_.getImageHeight();
      long depth = core_.getBytesPerPixel();

      // processing for the first image in the entire sequence
      if (sliceIdx==0 && channelIdx==0 && frameCount_ == 0) {
         if (!useMultiplePositions_ || posMode_ == PositionMode.TIME_LAPSE) {
            if (posIdx == 0) {
               setupImage5d(posIdx);
               acquisitionSetup(posIdx);
               setImageNames(posIdx, " (started) ");
            }
         } else {
            setupImage5d(posIdx);
            acquisitionSetup(posIdx);
            setImageNames(posIdx, " (started) ");
         }
      }

      // processing for the first image in a frame
      if (sliceIdx==0 && channelIdx==0) {                 
         // check if we have enough memory to acquire the entire frame
         long freeBytes = MemoryUtils.freeMemory();
         long requiredBytes = ((long)numSlices * channels_.size() + 10) * (width * height * depth);
         MMLogger.getLogger().info("Remaining memory " + freeBytes + " bytes. Required: " + requiredBytes);
         int tries = 0;
         while (freeBytes <  requiredBytes && tries <5) {
        	   System.gc();
            freeBytes = MemoryUtils.freeMemory();
            tries++;
         }
         if (freeBytes < requiredBytes) {
            throw new OutOfMemoryError("Remaining memory " + FMT2.format(freeBytes/1048576.0) +
                  " MB. Required for the next step: " + FMT2.format(requiredBytes/1048576.0) + " MB");
         }
      }

      // we won't try to adjust type mismatch
      if (imgDepth_ != depth) {
         throw new MMException("The byte depth does not match between channels or slices");
      }

      // re-scale image if necessary to conform to the entire sequence
      if (imgWidth_!=width || imgHeight_!=height) {
         MMLogger.getLogger().info("Scaling from: " + width + "," + height);
         ImageProcessor imp;
         if (imgDepth_ == 1)
            imp = new ByteProcessor((int)width, (int)height);
         else
            imp = new ShortProcessor((int)width, (int)height);
         imp.setPixels(img);
         ImageProcessor ip2 = imp.resize((int)imgWidth_, (int)imgHeight_);
         img = ip2.getPixels();
      }

      // set Image5D
      if (!singleWindow_ && i5dWin_[posIndexNormalized] != null)
      {
    	  img5d_[posIndexNormalized].setPixels(img, channelIdx+1, sliceIdx+1, actualFrameCount + 1);
    	  if (!i5dWin_[posIndexNormalized].isPlaybackRunning())
    	  {
    		  img5d_[posIndexNormalized].setCurrentPosition(0, 0, channelIdx, sliceIdx, actualFrameCount);
    	  }
    	  // auto-scale channels based on the first slice of the first frame
    	  if (sliceIdx==0 && frameCount_==0) {
    		  ImageStatistics stats = img5d_[posIndexNormalized].getStatistics(); // get uncalibrated stats
    		  double min = stats.min;
    		  double max = stats.max;
    		  img5d_[posIndexNormalized].setChannelMinMax(channelIdx+1, min, max);                  
    	  }
    	  
    	   RefreshI5d refresh = new RefreshI5d(i5dWin_[posIndexNormalized]); 
         SwingUtilities.invokeLater(refresh);
      }

      // generate meta-data
      JSONObject state = Annotator.generateJSONMetadata(core_.getSystemStateCache());

      acqData_[posIndexNormalized].insertImageMetadata(frameCount_, channelIdx, sliceIdx);
      acqData_[posIndexNormalized].setImageValue(frameCount_, channelIdx, sliceIdx, ImagePropertyKeys.EXPOSURE_MS, exposureMs);
      acqData_[posIndexNormalized].setImageValue(frameCount_, channelIdx, sliceIdx, ImagePropertyKeys.Z_UM, zCur);
      if (useMultiplePositions_) {
         acqData_[posIndexNormalized].setImageValue(frameCount_, channelIdx, sliceIdx, ImagePropertyKeys.X_UM, posList_.getPosition(posIdx).getX());
         acqData_[posIndexNormalized].setImageValue(frameCount_, channelIdx, sliceIdx, ImagePropertyKeys.Y_UM, posList_.getPosition(posIdx).getY());
      }
      acqData_[posIndexNormalized].setSystemState(frameCount_, channelIdx, sliceIdx, state);

      if (saveFiles_)
         acqData_[posIndexNormalized].attachImage(img, frameCount_, channelIdx, sliceIdx);
   }

   public void setPause(boolean state) {
      pause_ = state;
   }

   public boolean isPaused() {
      return pause_;
   }

   public Autofocus getAutofocus() {
      return autofocusPlugin_;
   }

   public void setFinished() {
      acqFinished_ = true;
   }
}
