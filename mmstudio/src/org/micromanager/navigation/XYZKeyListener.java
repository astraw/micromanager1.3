///////////////////////////////////////////////////////////////////////////////
// FILE:          XYZKeyListener.java
// PROJECT:       Micro-Manager
// SUBSYSTEM:     mmstudio
//-----------------------------------------------------------------------------

// COPYRIGHT:    University of California, San Francisco, 2008

// LICENSE:      This file is distributed under the BSD license.
// License text is included with the source distribution.

// This file is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty
// of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

// IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES.

package org.micromanager.navigation;

import ij.ImagePlus;
import ij.WindowManager;
import ij.gui.ImageWindow;
import java.awt.event.KeyEvent;
import java.awt.event.KeyListener;
import javax.swing.JOptionPane;
import mmcorej.MMCoreJ;
import ij.gui.*;
import mmcorej.CMMCore;
import org.micromanager.utils.MMLogger;
/**
 * @author OD
 *
 */
public final class XYZKeyListener implements KeyListener {
	   private CMMCore core_;
	   private ImageCanvas canvas_;
	   private static boolean isRunning_ = false;
	   private boolean mirrorX_;
	   private boolean mirrorY_;
	   private boolean transposeXY_;
	   private boolean correction_;
	   private static final double zmoveIncrement_ = 0.20;
	   private static final double xymoveIncrement_ = 1.0;
	   public static int slowStep=1;
	   public static int normalStep=2;
	   public static int fastStep=10;
	   private int step;
	  
	   
	   public XYZKeyListener(CMMCore core) {
		      core_ = core;
		   }	   
	   
	public void keyPressed(KeyEvent e) {

		MMLogger.getLogger().info(String.valueOf(e.getKeyCode()));
		
		step=slowStep;   
		if(e.isControlDown())
			   step=normalStep;
		   else if (e.isShiftDown())
			   step=fastStep;
		
		switch (e.getKeyCode())
		{
		case KeyEvent.VK_LEFT:
		{
			IncrementXY(-step,0);
		}break;
		case KeyEvent.VK_RIGHT:
		{
			IncrementXY(step,0);
		}break;
		case KeyEvent.VK_UP:
		{
			IncrementXY(0,step);
		}break;
		case KeyEvent.VK_DOWN:
		{
			IncrementXY(0,-step);
		}break;
		case KeyEvent.VK_LESS:
		case KeyEvent.VK_COMMA:
		{
			IncrementZ(-step);
		}break;
		case KeyEvent.VK_GREATER:
		case KeyEvent.VK_PERIOD:
		{
			IncrementZ(step);
		}break;
		default:
		}
		
	}
	public void keyReleased(KeyEvent arg0) {}
	public void keyTyped(KeyEvent arg0) {}
	
    public void start () {
	   // Get a handle to the AcqWindow
	   if (WindowManager.getCurrentWindow() != null) {
	      start (WindowManager.getCurrentWindow());
	   }
	}
	
   public void start (ImageWindow img) {
	  if (isRunning_)
	     stop(); 
	  
      isRunning_ = true;
	  if (img != null) {
		  attach(img);
	  }
	  getOrientation();
   }

      
	   public void stop() {
	      if (canvas_ != null) {
	         canvas_.removeKeyListener(this);
	      }
	      isRunning_ = false;
	   }

	   public boolean isRunning() {
	      return isRunning_;
	   }

	   public void attach(ImageWindow win) {
	      if (!isRunning_)
	         return;
	      canvas_ = win.getCanvas();
	      canvas_.addKeyListener(this);
	   }

	   public void IncrementXY(int stepX, int stepY) {
		      // Get needed info from core
		      getOrientation();
		      String xyStage = core_.getXYStageDevice();
		      if (xyStage == null)
		         return;
		      try {
		         if (core_.deviceBusy(xyStage))
		            return;
		      } catch (Exception ex) {
		         JOptionPane.showMessageDialog(null, ex.getMessage()); 
		         return;
		      }

		      double pixSizeUm = core_.getPixelSizeUm();
		      if (! (pixSizeUm > 0.0)) {
		         JOptionPane.showMessageDialog(null, "Please provide pixel size calibration data before using this function");
		         return;
		      }

		      // calculate needed relative movement
		      double tmpXUm = xymoveIncrement_*stepX;
		      double tmpYUm = xymoveIncrement_*stepY;

		      tmpXUm *= pixSizeUm;
		      tmpYUm *= pixSizeUm;
		      double mXUm = tmpXUm;
		      double mYUm = tmpYUm;
		      // if camera does not correct image orientation, we'll correct for it here:
		      if (!correction_) {
		         // Order: swapxy, then mirror axis
		         if (transposeXY_) {mXUm = tmpYUm; mYUm = tmpXUm;}
		         if (mirrorX_) {mXUm = -mXUm;}
		         if (mirrorY_) {mYUm = -mYUm;}
		      }

		      // Move the stage
		      try {
		         core_.setRelativeXYPosition(xyStage, mXUm, mYUm);
		      } catch (Exception ex) {
		         JOptionPane.showMessageDialog(null, ex.getMessage()); 
		         return;
		      }
		   } 
	   
	   
	   public void IncrementZ(int step) {
	      // Get needed info from core
	      String zStage = core_.getFocusDevice();
	      if (zStage == null)
	         return;
	 
	      double moveIncrement = zmoveIncrement_*step;
	      double pixSizeUm = core_.getPixelSizeUm();
	      if (pixSizeUm > 0.0) {
	         moveIncrement = 2 * pixSizeUm;
	      }

	      // Move the stage
	      try {
	         core_.setRelativePosition(zStage, moveIncrement);
	      } catch (Exception ex) {
	         JOptionPane.showMessageDialog(null, ex.getMessage()); 
	         return;
	      }

	   } 
	   public void getOrientation() {
		      String camera = core_.getCameraDevice();
		      if (camera == null) {
		         JOptionPane.showMessageDialog(null, "This function does not work without a camera");
		         return;
		      }
		      try {
		         String tmp = core_.getProperty(camera, "TransposeCorrection");
		         if (tmp.equals("0")) 
		            correction_ = false;
		         else 
		            correction_ = true;
		         tmp = core_.getProperty(camera, MMCoreJ.getG_Keyword_Transpose_MirrorX());
		         if (tmp.equals("0")) 
		            mirrorX_ = false;
		         else 
		            mirrorX_ = true;
		         tmp = core_.getProperty(camera, MMCoreJ.getG_Keyword_Transpose_MirrorY());
		         if (tmp.equals("0")) 
		            mirrorY_ = false;
		         else 
		            mirrorY_ = true;
		         tmp = core_.getProperty(camera, MMCoreJ.getG_Keyword_Transpose_SwapXY());
		         if (tmp.equals("0")) 
		            transposeXY_ = false;
		         else 
		            transposeXY_ = true;
		      } catch(Exception exc) {
		         exc.printStackTrace();
		         JOptionPane.showMessageDialog(null, "Exception encountered. Please report to the Micro-Manager support team");
		         return;
		      }
		   }

}
