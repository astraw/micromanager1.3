package org.micromanager;

import javax.swing.JOptionPane;

import mmcorej.CMMCore;

import org.micromanager.api.Autofocus;

public class CoreAutofocus implements Autofocus {
   
   CMMCore core_;

   public void focus(double coarseStep, int numCoarse, double fineStep, int numFine) {
      // TODO throw exception here
      // command is obsolete
      
   }

   public double fullFocus() {
      if (core_ == null)
         return 0.0;
      
      try {
         core_.fullFocus();
      } catch (Exception e) {
         // TODO Auto-generated catch block
         e.printStackTrace();
         return 0.0;
      }
      
      try {
         return core_.getFocusScore();
      } catch (Exception e) {
         e.printStackTrace();
         return 0.0;
      }
   }

   public String getVerboseStatus() {
      return new String("No message at this time!");
   }

   public double incrementalFocus() {
      if (core_ == null)
         return 0.0;
      
      try {
         core_.incrementalFocus();
      } catch (Exception e) {
         // TODO Auto-generated catch block
         e.printStackTrace();
         return 0.0;
      }
      
      try {
         return core_.getFocusScore();
      } catch (Exception e) {
         e.printStackTrace();
         return 0.0;
      }

   }

   public void setMMCore(CMMCore core) {
      core_ = core;
   }

   public void showOptionsDialog() {
      JOptionPane.showMessageDialog(null, "Use Autofocus device properties to set parameters.");
   }

}
