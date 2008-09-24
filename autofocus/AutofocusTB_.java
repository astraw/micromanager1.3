import ij.IJ;
import ij.ImagePlus;
import ij.gui.ImageWindow;
import ij.plugin.PlugIn;
import ij.process.ByteProcessor;
import ij.process.ImageProcessor;
import ij.process.ShortProcessor;

import java.awt.Color;
import java.util.prefs.Preferences;
import java.util.Date;
import java.lang.System;
import java.awt.Rectangle;

import org.micromanager.api.Autofocus;

import mmcorej.CMMCore;

/*
*  Created on June 2nd 2007
*  author: Pakpoom Subsoontorn & Hernan Garcia
*/
/**
*  ImageJ plugin wrapper for uManager.
*
*@author     thomas
*@created    30 mai 2008
*/

/*
*  This plugin take a stack of snapshots and computes their sharpness
*/
public class AutofocusTB_ implements PlugIn, Autofocus {

   private final static String KEY_SIZE_FIRST = "size_first";
   private final static String KEY_NUM_FIRST = "num_first";
   private final static String KEY_SIZE_SECOND = "size_second";
   private final static String KEY_NUM_SECOND = "num_second";
   private final static String KEY_THRES = "thres";
   private final static String KEY_CROP_SIZE = "crop_size";
   private final static String KEY_CHANNEL1 = "channel1";
   private final static String KEY_CHANNEL2 = "channel2";
   private final static String AF_SETTINGS_NODE = "micro-manager/extensions/autofocus";

   private CMMCore core_;
   private ImageProcessor ipCurrent_ = null;

   /**
    *  Description of the Field
    */
   public double SIZE_FIRST = 2;
   //
   /**
    *  Description of the Field
    */
   public double NUM_FIRST = 1;
   // +/- #of snapshot
   /**
    *  Description of the Field
    */
   public double SIZE_SECOND = 0.2;
   /**
    *  Description of the Field
    */
   public double NUM_SECOND = 5;
   /**
    *  Description of the Field
    */
   public double THRES = 0.02;
   /**
    *  Description of the Field
    */
   public double CROP_SIZE = 0.2;
   /**
    *  Description of the Field
    */
   public String CHANNEL1 = "Bright-Field";
   /**
    *  Description of the Field
    */
   public String CHANNEL2 = "DAPI";

   private double indx = 0;
   //snapshot show new window iff indx = 1

   private boolean verbose_ = true;
   // displaying debug info or not

   private Preferences prefs_;
   //********

   private double curDist;
   private double baseDist;
   private double bestDist;
   private double curSh;
   private double bestSh;
   private long t0;
   private long tPrev;
   private long tcur;


   /**
    *  Constructor for the Autofocus_ object
    */
   public AutofocusTB_() {
      //constructor!!!
      Preferences root = Preferences.userNodeForPackage(this.getClass());
      prefs_ = root.node(root.absolutePath() + "/" + AF_SETTINGS_NODE);
      loadSettings();
   }


   /**
    *  Main processing method for the Autofocus_ object
    *
    *@param  arg  Description of the Parameter
    */
   public void run(String arg) {
      t0 = System.currentTimeMillis();
      bestDist = 5000;
      bestSh = 0;
      //############# CHECK INPUT ARG AND CORE ########
      if (arg.compareTo("silent") == 0) {
         verbose_ = false;
      } else {
         verbose_ = true;
      }

      if (arg.compareTo("options") == 0) {
         showOptionsDialog();
      }

      if (core_ == null) {
         // if core object is not set attempt to get its global handle
         core_ = MMStudioPlugin.getMMCoreInstance();
      }

      if (core_ == null) {
         IJ.error("Unable to get Micro-Manager Core API handle.\n" +
               "If this module is used as ImageJ plugin, Micro-Manager Studio must be running first!");
         return;
      }

      //######################## START THE ROUTINE ###########

      try {
         IJ.write("Autofocus TB started.");
         //########System setup##########
         core_.setConfig("Channel", CHANNEL1);
         core_.waitForSystem();
         core_.waitForDevice(core_.getShutterDevice());
         //delay_time(3000);


         //Snapshot, zdistance and sharpNess before AF
         /*
          *  curDist = core_.getPosition(core_.getFocusDevice());
          *  indx =1;
          *  snapSingleImage();
          *  indx =0;
          *  tPrev = System.currentTimeMillis();
          *  curSh = sharpNess(ipCurrent_);
          *  tcur = System.currentTimeMillis()-tPrev;
          */
         //set z-distance to the lowest z-distance of the stack
         curDist = core_.getPosition(core_.getFocusDevice());
         baseDist = curDist - SIZE_FIRST * NUM_FIRST;
         core_.setPosition(core_.getFocusDevice(), baseDist);
         core_.waitForDevice(core_.getFocusDevice());
         delay_time(100);

         //core_.setShutterOpen(true);
         //core_.setAutoShutter(false);

         IJ.write("Before rough search: " + String.valueOf(curDist));

         //Rough search
         for (int i = 0; i < 2 * NUM_FIRST + 1; i++) {
            tPrev = System.currentTimeMillis();

            core_.setPosition(core_.getFocusDevice(), baseDist + i * SIZE_FIRST);
            core_.waitForDevice(core_.getFocusDevice());

            curDist = core_.getPosition(core_.getFocusDevice());
            // indx =1;
            snapSingleImage();
            // indx =0;


            curSh = sharpNess(ipCurrent_);

            if (curSh > bestSh) {
               bestSh = curSh;
               bestDist = curDist;
            } else if (bestSh - curSh > THRES * bestSh) {
               break;
            }
            tcur = System.currentTimeMillis() - tPrev;

            //===IJ.write(String.valueOf(curDist)+" "+String.valueOf(curSh)+" " +String.valueOf(tcur));
         }

         //===IJ.write("BEST_DIST_FIRST"+String.valueOf(bestDist)+" BEST_SH_FIRST"+String.valueOf(bestSh));

         baseDist = bestDist - SIZE_SECOND * NUM_SECOND;
         core_.setPosition(core_.getFocusDevice(), baseDist);
         delay_time(100);

         bestSh = 0;

         core_.setConfig("Channel", CHANNEL2);
         core_.waitForSystem();
         core_.waitForDevice(core_.getShutterDevice());

         //Fine search
         for (int i = 0; i < 2 * NUM_SECOND + 1; i++) {
            tPrev = System.currentTimeMillis();
            core_.setPosition(core_.getFocusDevice(), baseDist + i * SIZE_SECOND);
            core_.waitForDevice(core_.getFocusDevice());

            curDist = core_.getPosition(core_.getFocusDevice());
            // indx =1;
            snapSingleImage();
            // indx =0;

            curSh = sharpNess(ipCurrent_);

            if (curSh > bestSh) {
               bestSh = curSh;
               bestDist = curDist;
            } else if (bestSh - curSh > THRES * bestSh) {
               break;
            }
            tcur = System.currentTimeMillis() - tPrev;

            //===IJ.write(String.valueOf(curDist)+" "+String.valueOf(curSh)+" "+String.valueOf(tcur));
         }

         IJ.write("BEST_DIST_SECOND= " + String.valueOf(bestDist) + " BEST_SH_SECOND= " + String.valueOf(bestSh));

         core_.setPosition(core_.getFocusDevice(), bestDist);
         // indx =1;
         snapSingleImage();
         // indx =0;
         //core_.setShutterOpen(false);
         //core_.setAutoShutter(true);

         IJ.write("Total Time: " + String.valueOf(System.currentTimeMillis() - t0));
      } catch (Exception e) {
         IJ.error(e.getMessage());
      }
   }



   //take a snapshot and save pixel values in ipCurrent_
   /**
    *  Description of the Method
    *
    *@return    Description of the Return Value
    */
   private boolean snapSingleImage() {

      try {
         core_.snapImage();
         Object img = core_.getImage();
         ImagePlus implus = newWindow();
         // this step will create a new window iff indx = 1
         implus.getProcessor().setPixels(img);
         ipCurrent_ = implus.getProcessor();
      } catch (Exception e) {
         IJ.write(e.getMessage());
         IJ.error(e.getMessage());
         return false;
      }

      return true;
   }


   //waiting
   /**
    *  Description of the Method
    *
    *@param  delay  Description of the Parameter
    */
   private void delay_time(double delay) {
      Date date = new Date();
      long sec = date.getTime();
      while (date.getTime() < sec + delay) {
         date = new Date();
      }
   }



   /*
    *  calculate the sharpness of a given image (in "impro").
    */
   /**
    *  Description of the Method
    *
    *@param  impro  Description of the Parameter
    *@return        Description of the Return Value
    */
   private double sharpNessp(ImageProcessor impro) {

      int width = (int) (CROP_SIZE * core_.getImageWidth());
      int height = (int) (CROP_SIZE * core_.getImageHeight());
      int ow = (int) (((1 - CROP_SIZE) / 2) * core_.getImageWidth());
      int oh = (int) (((1 - CROP_SIZE) / 2) * core_.getImageHeight());

      double[][] medPix = new double[width][height];
      double sharpNess = 0;
      double[] windo = new double[9];

      /*
       *  Apply 3x3 median filter to reduce noise
       */
      for (int i = 1; i < width - 1; i++) {
         for (int j = 1; j < height - 1; j++) {

            windo[0] = (double) impro.getPixel(ow + i - 1, oh + j - 1);
            windo[1] = (double) impro.getPixel(ow + i, oh + j - 1);
            windo[2] = (double) impro.getPixel(ow + i + 1, oh + j - 1);
            windo[3] = (double) impro.getPixel(ow + i - 1, oh + j);
            windo[4] = (double) impro.getPixel(ow + i, oh + j);
            windo[5] = (double) impro.getPixel(ow + i + 1, oh + j);
            windo[6] = (double) impro.getPixel(ow + i - 1, oh + j + 1);
            windo[7] = (double) impro.getPixel(ow + i, oh + j + 1);
            windo[8] = (double) impro.getPixel(ow + i + 1, oh + j + 1);

            medPix[i][j] = findMed(windo);
         }
      }

      /*
       *  Edge detection using a 3x3 filter: [-2 -1 0; -1 0 1; 0 1 2]. Then sum all pixel values. Ideally, the sum is large if most edges are sharp
       */
      double edgehoriz;
      double edvevert;
      for (int k = 1; k < width - 1; k++) {
         for (int l = 1; l < height - 1; l++) {
            edgehoriz = Math.pow((-2 * medPix[k - 1][l] - medPix[k - 1][l - 1] - medPix[k - 1][l + 1] + medPix[k + 1][l - 1] + medPix[k + 1][l + 1] + 2 * medPix[k + 1][l]), 2);
            edvevert = Math.pow((-2 * medPix[k][l - 1] - medPix[k - 1][l - 1] - medPix[k + 1][l - 1] + medPix[k - 1][l + 1] + medPix[k + 1][l + 1] + 2 * medPix[k][l + 1]), 2);
            sharpNess += (edgehoriz + edvevert);
            //sharpNess = sharpNess + Math.pow((-2 * medPix[k - 1][l - 1] - medPix[k][l - 1] - medPix[k - 1][l] + medPix[k + 1][l] + medPix[k][l + 1] + 2 * medPix[k + 1][l + 1]), 2);

         }
      }
      return sharpNess;
   }


   /*
    *  calculate the sharpness of a given image (in "impro").
    */
   /**
    *  Description of the Method
    *
    *@param  impro  Description of the Parameter
    *@return        Description of the Return Value
    */
   private double sharpNess(ImageProcessor impro) {

      int width = (int) (CROP_SIZE * core_.getImageWidth());
      int height = (int) (CROP_SIZE * core_.getImageHeight());
      int sx = (int) (core_.getImageWidth() - width) / 2;
      int sy = (int) (core_.getImageHeight() - height) / 2;
      //int ow = (int) (((1 - CROP_SIZE) / 2) * core_.getImageWidth());
      //int oh = (int) (((1 - CROP_SIZE) / 2) * core_.getImageHeight());

      // double[][] medPix = new double[width][height];
      double sharpNess = 0;
      //double[] windo = new double[9];

      /*
       *  Apply 3x3 median filter to reduce noise
       */
      /*
       *  for (int i=0; i<width; i++){
       *  for (int j=0; j<height; j++){
       *  windo[0] = (double)impro.getPixel(ow+i-1,oh+j-1);
       *  windo[1] = (double)impro.getPixel(ow+i,oh+j-1);
       *  windo[2] = (double)impro.getPixel(ow+i+1,oh+j-1);
       *  windo[3] = (double)impro.getPixel(ow+i-1,oh+j);
       *  windo[4] = (double)impro.getPixel(ow+i,oh+j);
       *  windo[5] = (double)impro.getPixel(ow+i+1,oh+j);
       *  windo[6] = (double)impro.getPixel(ow+i-1,oh+j+1);
       *  windo[7] = (double)impro.getPixel(ow+i,oh+j+1);
       *  windo[8] = (double)impro.getPixel(ow+i+1,oh+j+1);
       *  medPix[i][j] = findMed(windo);
       *  }
       *  }
       */
      //tPrev = System.currentTimeMillis();

      impro.setRoi(new Rectangle(sx, sy, width, height));
      impro.crop();
      impro.medianFilter();
      impro.findEdges();

      //int[] ken = {2, 1, 0, 1, 0, -1, 0, -1, -2};
      //impro.convolve3x3(ken);

      for (int i = 0; i < width; i++) {
         for (int j = 0; j < height; j++) {
            sharpNess += impro.getPixelValue(i, j);
         }
      }

      // tcur = System.currentTimeMillis()-tPrev;

      /*
       *  Edge detection using a 3x3 filter: [-2 -1 0; -1 0 1; 0 1 2]. Then sum all pixel values. Ideally, the sum is large if most edges are sharp
       */
      /*
       *  for (int k=1; k<width-1; k++){
       *  for (int l=1; l<height-1; l++){
       *  sharpNess = sharpNess + Math.pow((-2*medPix[k-1][l-1]- medPix[k][l-1]-medPix[k-1][l]+medPix[k+1][l]+medPix[k][l+1]+2*medPix[k+1][l+1]),2);
       *  }
       *  }
       */
      return sharpNess;
   }


   //making a new window for a new snapshot.
   /**
    *  Description of the Method
    *
    *@return    Description of the Return Value
    */
   private ImagePlus newWindow() {
      ImagePlus implus;
      ImageProcessor ip;
      long byteDepth = core_.getBytesPerPixel();

      if (byteDepth == 1) {
         ip = new ByteProcessor((int) core_.getImageWidth(), (int) core_.getImageHeight());
      } else {
         ip = new ShortProcessor((int) core_.getImageWidth(), (int) core_.getImageHeight());
      }
      ip.setColor(Color.black);
      ip.fill();

      implus = new ImagePlus(String.valueOf(curDist), ip);
      if (indx == 1) {
         if (verbose_) {
            // create image window if we are in the verbose mode
            ImageWindow imageWin = new ImageWindow(implus);
         }
      }
      return implus;
   }


   /**
    *  Description of the Method
    *
    *@param  arr  Description of the Parameter
    *@return      Description of the Return Value
    */
   private double findMed(double[] arr) {
      double tmp;
      boolean sorted = false;
      int i = 0;
      while (!sorted) {
         sorted = true;
         for (int j = 0; j < 8 - i; j++) {
            if (arr[j + 1] < arr[j]) {
               sorted = false;
               tmp = arr[j];
               arr[j] = arr[j + 1];
               arr[j + 1] = tmp;
            }
         }
         i++;
      }
      return arr[5];
   }


   /**
    *  Description of the Method
    *
    *@return    Description of the Return Value
    */
   public double fullFocus() {
      run("silent");
      return 0;
   }


   /**
    *  Gets the verboseStatus attribute of the Autofocus_ object
    *
    *@return    The verboseStatus value
    */
   public String getVerboseStatus() {
      return new String("OK");
   }


   /**
    *  Description of the Method
    *
    *@return    Description of the Return Value
    */
   public double incrementalFocus() {
      run("silent");
      return 0;
   }

   public void focus(double coarseStep, int numCoarse, double fineStep, int numFine) {
      SIZE_FIRST = coarseStep;
      NUM_FIRST = numCoarse;
      SIZE_SECOND = fineStep;
      NUM_SECOND = numFine;

      run("silent");
   }


   /**
    *  Sets the mMCore attribute of the Autofocus_ object
    *
    *@param  core  The new mMCore value
    */
   public void setMMCore(CMMCore core) {
      core_ = core;
   }


   /**
    *  Description of the Method
    */
   public void showOptionsDialog() {
      AFOptionsDlgTB dlg = new AFOptionsDlgTB(this);
      dlg.setVisible(true);
      saveSettings();
   }


   /**
    *  Description of the Method
    */
   public void loadSettings() {
      SIZE_FIRST = prefs_.getDouble(KEY_SIZE_FIRST, SIZE_FIRST);
      NUM_FIRST = prefs_.getDouble(KEY_NUM_FIRST, NUM_FIRST);
      SIZE_SECOND = prefs_.getDouble(KEY_SIZE_SECOND, SIZE_SECOND);
      NUM_SECOND = prefs_.getDouble(KEY_NUM_SECOND, NUM_SECOND);
      THRES = prefs_.getDouble(KEY_THRES, THRES);
      CROP_SIZE = prefs_.getDouble(KEY_CROP_SIZE, CROP_SIZE);
      CHANNEL1 = prefs_.get(KEY_CHANNEL1, CHANNEL1);
      CHANNEL2 = prefs_.get(KEY_CHANNEL2, CHANNEL2);
   }


   /**
    *  Description of the Method
    */
   public void saveSettings() {
      prefs_.putDouble(KEY_SIZE_FIRST, SIZE_FIRST);
      prefs_.putDouble(KEY_NUM_FIRST, NUM_FIRST);
      prefs_.putDouble(KEY_SIZE_SECOND, SIZE_SECOND);
      prefs_.putDouble(KEY_NUM_SECOND, NUM_SECOND);
      prefs_.putDouble(KEY_THRES, THRES);
      prefs_.putDouble(KEY_CROP_SIZE, CROP_SIZE);
      prefs_.put(KEY_CHANNEL1, CHANNEL1);
      prefs_.put(KEY_CHANNEL2, CHANNEL2);
   }

}
