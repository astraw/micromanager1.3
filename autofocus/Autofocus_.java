///////////////////////////////////////////////////////////////////////////////
//FILE:           Autofocus_.java
//PROJECT:        Micro-Manager
//SUBSYSTEM:      Autofocusing plug-in for mciro-manager and ImageJ
//-----------------------------------------------------------------------------
//
//AUTHOR:         Pakpoom Subsoontorn & Hernan Garcia, June, 2007
//                Nenad Amodaj, nenad@amodaj.com
//
//COPYRIGHT:      California Institute of Technology
//                University of California San Francisco
//                100X Imaging Inc, www.100ximaging.com
//                
//LICENSE:        This file is distributed under the BSD license.
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
//CVS:            $Id: MetadataDlg.java 1275 2008-06-03 21:31:24Z nenad $

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

import org.micromanager.api.Autofocus;

import mmcorej.CMMCore;

/*
 * Created on June 2nd 2007
 * author: Pakpoom Subsoontorn & Hernan Garcia
 */

/**
 * ImageJ plugin wrapper for uManager.
 */

/* This plugin take a stack of snapshots and computes their sharpness

 */
public class Autofocus_ implements PlugIn, Autofocus {

   private static final String KEY_SIZE_FIRST = "size_first";
   private static final String KEY_NUM_FIRST = "num_first";
   private static final String KEY_SIZE_SECOND = "size_second";
   private static final String KEY_NUM_SECOND = "num_second";
   private static final String KEY_THRES    = "thres";
   private static final String KEY_CROP_SIZE = "crop_size";
   private static final String KEY_CHANNEL = "channel";
   private static final String AF_SETTINGS_NODE = "micro-manager/extensions/autofocus";

   private CMMCore core_;
   private ImageProcessor ipCurrent_ = null;

   public double SIZE_FIRST = 2;//
   public int NUM_FIRST = 1; // +/- #of snapshot
   public  double SIZE_SECOND = 0.2;
   public  int NUM_SECOND = 5;
   public double THRES = 0.02;
   public double CROP_SIZE = 0.2; 
   public String CHANNEL="Bright-Field";


   private double indx = 0; //snapshot show new window iff indx = 1 

   private boolean verbose_ = true; // displaying debug info or not

   private Preferences prefs_;//********

   private double curDist;
   private double baseDist;
   private double bestDist;
   private double curSh;
   private double bestSh;
   private long t0;
   private long tPrev;
   private long tcur;

   public Autofocus_(){ //constructor!!!
      Preferences root = Preferences.userNodeForPackage(this.getClass());
      prefs_ = root.node(root.absolutePath()+"/"+AF_SETTINGS_NODE);
      loadSettings();
   }

   public void run(String arg) {
      t0 = System.currentTimeMillis();
      bestDist = 5000;
      bestSh = 0;
      //############# CHECK INPUT ARG AND CORE ########
      if (arg.compareTo("silent") == 0)
         verbose_ = false;
      else
         verbose_ = true;

      if (arg.compareTo("options") == 0){
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

      try{
         IJ.write("Autofocus started.");
         core_.setShutterOpen(true);
         core_.setAutoShutter(false);


         //########System setup##########
         core_.setConfig("Channel",CHANNEL);
         core_.waitForSystem(); 
         core_.waitForDevice(core_.getShutterDevice()); 
         //delay_time(3000);


         //Snapshot, zdistance and sharpNess before AF 
         /* curDist = core_.getPosition(core_.getFocusDevice());
         indx =1;
         snapSingleImage();
         indx =0;

         tPrev = System.currentTimeMillis();
         curSh = sharpNess(ipCurrent_);
         tcur = System.currentTimeMillis()-tPrev;*/



         //set z-distance to the lowest z-distance of the stack
         curDist = core_.getPosition(core_.getFocusDevice());
         baseDist = curDist-SIZE_FIRST*NUM_FIRST;
         core_.setPosition(core_.getFocusDevice(),baseDist);
         core_.waitForDevice(core_.getFocusDevice());
         delay_time(300);

         IJ.write(" Before rough search: " +String.valueOf(curDist));


         //Rough search
         for(int i = 0; i < 2*NUM_FIRST+1; i++ ){
            tPrev = System.currentTimeMillis();

            core_.setPosition(core_.getFocusDevice(),baseDist+i*SIZE_FIRST);
            core_.waitForDevice(core_.getFocusDevice());


            curDist = core_.getPosition(core_.getFocusDevice());
            // indx =1;
            snapSingleImage();
            // indx =0;    



            curSh = sharpNess(ipCurrent_);

            if(curSh > bestSh){
               bestSh = curSh;
               bestDist = curDist;
            } 
            else if (bestSh - curSh > THRES*bestSh && bestDist < 5000) {
               break;
            }
            tcur = System.currentTimeMillis()-tPrev;

            //===IJ.write(String.valueOf(curDist)+" "+String.valueOf(curSh)+" " +String.valueOf(tcur));	
         }


         //===IJ.write("BEST_DIST_FIRST"+String.valueOf(bestDist)+" BEST_SH_FIRST"+String.valueOf(bestSh));

         baseDist = bestDist-SIZE_SECOND*NUM_SECOND;
         core_.setPosition(core_.getFocusDevice(),baseDist);
         delay_time(100);

         bestSh = 0;

         //Fine search
         for(int i = 0; i < 2*NUM_SECOND+1; i++ ){
            tPrev = System.currentTimeMillis();
            core_.setPosition(core_.getFocusDevice(),baseDist+i*SIZE_SECOND);
            core_.waitForDevice(core_.getFocusDevice());

            curDist = core_.getPosition(core_.getFocusDevice());
            // indx =1;
            snapSingleImage();
            // indx =0;    

            curSh = sharpNess(ipCurrent_);

            if(curSh > bestSh){
               bestSh = curSh;
               bestDist = curDist;
            } 
            else if (bestSh - curSh > THRES*bestSh && bestDist < 5000){
               break;
            }
            tcur = System.currentTimeMillis()-tPrev;

            //===IJ.write(String.valueOf(curDist)+" "+String.valueOf(curSh)+" "+String.valueOf(tcur));
         }


         IJ.write("BEST_DIST_SECOND"+String.valueOf(bestDist)+" BEST_SH_SECOND"+String.valueOf(bestSh));

         core_.setPosition(core_.getFocusDevice(),bestDist);
         // indx =1;
         snapSingleImage();
         // indx =0;  
         core_.setShutterOpen(false);
         core_.setAutoShutter(true);


         IJ.write("Total Time: "+ String.valueOf(System.currentTimeMillis()-t0));
      }
      catch(Exception e)
      {
         IJ.error(e.getMessage());
      }     
   }







   //take a snapshot and save pixel values in ipCurrent_
   private boolean snapSingleImage() {

      try {
         core_.snapImage();
         Object img = core_.getImage();
         ImagePlus implus = newWindow();// this step will create a new window iff indx = 1
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
   private void delay_time(double delay){
      Date date = new Date();
      long sec = date.getTime();
      while(date.getTime()<sec+delay){
         date = new Date();
      }
   }



   /*calculate the sharpness of a given image (in "impro").*/
   private double sharpNessp(ImageProcessor impro){


      int width =  (int)(CROP_SIZE*core_.getImageWidth());
      int height = (int)(CROP_SIZE*core_.getImageHeight());
      int ow = (int)(((1-CROP_SIZE)/2)*core_.getImageWidth());
      int oh = (int)(((1-CROP_SIZE)/2)*core_.getImageHeight());

      double[][] medPix = new double[width][height];
      double sharpNess = 0;
      double[] windo = new double[9];

      /*Apply 3x3 median filter to reduce noise*/
      for (int i=0; i<width; i++){
         for (int j=0; j<height; j++){

            windo[0] = (double)impro.getPixel(ow+i-1,oh+j-1);
            windo[1] = (double)impro.getPixel(ow+i,oh+j-1);
            windo[2] = (double)impro.getPixel(ow+i+1,oh+j-1);
            windo[3] = (double)impro.getPixel(ow+i-1,oh+j);
            windo[4] = (double)impro.getPixel(ow+i,oh+j);
            windo[5] = (double)impro.getPixel(ow+i+1,oh+j);
            windo[6] = (double)impro.getPixel(ow+i-1,oh+j+1);
            windo[7] = (double)impro.getPixel(ow+i,oh+j+1);
            windo[8] = (double)impro.getPixel(ow+i+1,oh+j+1);

            medPix[i][j] = findMed(windo);
         } 
      }

      /*Edge detection using a 3x3 filter: [-2 -1 0; -1 0 1; 0 1 2]. Then sum all pixel values. Ideally, the sum is large if most edges are sharp*/

      for (int k=1; k<width-1; k++){
         for (int l=1; l<height-1; l++){

            sharpNess = sharpNess + Math.pow((-2*medPix[k-1][l-1]- medPix[k][l-1]-medPix[k-1][l]+medPix[k+1][l]+medPix[k][l+1]+2*medPix[k+1][l+1]),2);

         } 
      }
      return sharpNess;
   }


   /*calculate the sharpness of a given image (in "impro").*/
   private double sharpNess(ImageProcessor impro){


      int width =  (int)(CROP_SIZE*core_.getImageWidth());
      int height = (int)(CROP_SIZE*core_.getImageHeight());
      int ow = (int)(((1-CROP_SIZE)/2)*core_.getImageWidth());
      int oh = (int)(((1-CROP_SIZE)/2)*core_.getImageHeight());

      // double[][] medPix = new double[width][height];
      double sharpNess = 0;
      //double[] windo = new double[9];

      /*Apply 3x3 median filter to reduce noise*/

      /*   for (int i=0; i<width; i++){
         for (int j=0; j<height; j++){

	    windo[0] = (double)impro.getPixel(ow+i-1,oh+j-1);
	    windo[1] = (double)impro.getPixel(ow+i,oh+j-1);
            windo[2] = (double)impro.getPixel(ow+i+1,oh+j-1);
            windo[3] = (double)impro.getPixel(ow+i-1,oh+j);
            windo[4] = (double)impro.getPixel(ow+i,oh+j);
            windo[5] = (double)impro.getPixel(ow+i+1,oh+j);
            windo[6] = (double)impro.getPixel(ow+i-1,oh+j+1);
            windo[7] = (double)impro.getPixel(ow+i,oh+j+1);
            windo[8] = (double)impro.getPixel(ow+i+1,oh+j+1);

            medPix[i][j] = findMed(windo);
         } 
	 }*/        

      //tPrev = System.currentTimeMillis();     

      impro.medianFilter();
      int[] ken = {2, 1, 0, 1, 0, -1, 0, -1, -2};
      impro.convolve3x3(ken);
      for (int i=0; i<width; i++){
         for (int j=0; j<height; j++){

            sharpNess = sharpNess + Math.pow(impro.getPixel(ow+i,oh+j),2);
         } 
      }

      // tcur = System.currentTimeMillis()-tPrev;

      /*Edge detection using a 3x3 filter: [-2 -1 0; -1 0 1; 0 1 2]. Then sum all pixel values. Ideally, the sum is large if most edges are sharp*/

      /*  for (int k=1; k<width-1; k++){
         for (int l=1; l<height-1; l++){

	     sharpNess = sharpNess + Math.pow((-2*medPix[k-1][l-1]- medPix[k][l-1]-medPix[k-1][l]+medPix[k+1][l]+medPix[k][l+1]+2*medPix[k+1][l+1]),2);

         } 
	 }*/

      return sharpNess;
   }


   //making a new window for a new snapshot.
   private ImagePlus newWindow(){
      ImagePlus implus;
      ImageProcessor ip;
      long byteDepth = core_.getBytesPerPixel();

      if (byteDepth == 1){
         ip = new ByteProcessor((int)core_.getImageWidth(),(int)core_.getImageHeight());
      } else  {
         ip = new ShortProcessor((int)core_.getImageWidth(), (int)core_.getImageHeight());
      }
      ip.setColor(Color.black);
      ip.fill();

      implus = new ImagePlus(String.valueOf(curDist), ip);
      if(indx == 1){
         if (verbose_) {
            // create image window if we are in the verbose mode
            ImageWindow imageWin = new ImageWindow(implus);
         }
      }
      return implus;
   }

   private double findMed(double[] arr){ 
      double tmp;
      for(int i=0; i<8; i++){
         for(int j=0; j<8-i; j++){
            if (arr[j+1]<arr[j]){
               tmp = arr[j];
               arr[j]=arr[j+1];
               arr[j+1]=tmp;
            }
         }
      }
      return arr[5];

   }

   public double fullFocus() {
      run("silent");
      return 0;
   }

   public String getVerboseStatus() {
      return new String("OK");
   }

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

   public void setMMCore(CMMCore core) {
      core_ = core;
   }

   public void showOptionsDialog() {
      AFOptionsDlg dlg = new AFOptionsDlg(this);
      dlg.setVisible(true);
      saveSettings(); 
   }

   public void loadSettings() {
      SIZE_FIRST = prefs_.getDouble(KEY_SIZE_FIRST, SIZE_FIRST);
      NUM_FIRST =  prefs_.getInt(KEY_NUM_FIRST, NUM_FIRST);
      SIZE_SECOND = prefs_.getDouble(KEY_SIZE_SECOND, SIZE_SECOND);
      NUM_SECOND =  prefs_.getInt(KEY_NUM_SECOND, NUM_SECOND);
      THRES = prefs_.getDouble(KEY_THRES, THRES);
      CROP_SIZE = prefs_.getDouble(KEY_CROP_SIZE, CROP_SIZE); 
      CHANNEL= prefs_.get(KEY_CHANNEL, CHANNEL);
   }

   public void saveSettings() {
      prefs_.putDouble(KEY_SIZE_FIRST, SIZE_FIRST);
      prefs_.putDouble(KEY_NUM_FIRST, NUM_FIRST);
      prefs_.putDouble(KEY_SIZE_SECOND, SIZE_SECOND);
      prefs_.putDouble(KEY_NUM_SECOND, NUM_SECOND);
      prefs_.putDouble(KEY_THRES, THRES);
      prefs_.putDouble(KEY_CROP_SIZE, CROP_SIZE); 
      prefs_.put(KEY_CHANNEL, CHANNEL);
   }
   
   void setCropSize(double cs) {
      CROP_SIZE = cs;
   }
   
   void setThreshold(double thr) {
      THRES = thr;
   }
   
   void setChannel(String channel) {
      CHANNEL = channel;
   }
   
   public void focus(double coarseStep, int numCoarse, double fineStep, int numFine) {
      SIZE_FIRST = coarseStep;
      NUM_FIRST = numCoarse;
      SIZE_SECOND = fineStep;
      NUM_SECOND = numFine;
      
      run("");
   }   
}   