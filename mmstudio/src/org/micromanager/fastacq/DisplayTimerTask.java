package org.micromanager.fastacq;
import java.util.TimerTask;

import mmcorej.CMMCore;

import org.micromanager.api.DeviceControlGUI;

public class DisplayTimerTask extends TimerTask {

   private CMMCore core_;
   private String cameraName_;
   private DeviceControlGUI parentGUI_;
   
   public DisplayTimerTask(CMMCore core, DeviceControlGUI pg) {
      parentGUI_ = pg;
      core_ = core;
      // obtain camera name
      cameraName_ = core_.getCameraDevice();
   }

   public void run() {

      try {
         if (core_.isSequenceRunning()) 
         {
            updateImage();
         }
      } catch (Exception e) {
         //e.printStackTrace();
         cancel();
      }
   }
   
   public void updateImage() {
      try {
         // update image window
         Object img = core_.getLastImage();
         if (img != null)
            parentGUI_.displayImage(img);
         //
      } catch (Exception e){
         // TODO Auto-generated catch block
         e.printStackTrace();
      }
   }   
}


