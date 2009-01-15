///////////////////////////////////////////////////////////////////////////////
//FILE:          MMAcquisitionSnap.java
//PROJECT:       Micro-Manager
//SUBSYSTEM:     mmstudio
//-----------------------------------------------------------------------------

//AUTHOR:       Arthur Edelstein, arthuredelstein@gmail.com, January 16, 2009

//COPYRIGHT:    University of California, San Francisco, 2009

//LICENSE:      This file is distributed under the BSD license.
//License text is included with the source distribution.

//This file is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty
//of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

//IN NO EVENT SHALL THE COPYRIGHT OWNER OR
//CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
//INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES.

package org.micromanager.acquisition;

import ij.ImagePlus;
import ij.ImageStack;
import ij.process.ImageProcessor;

import mmcorej.CMMCore;

import org.micromanager.MMStudioMainFrame;
import org.micromanager.acquisition.MMAcquisition;
import org.micromanager.image5d.Image5D;
import org.micromanager.image5d.Image5DWindow;
import org.micromanager.image5d.Image5DWindowSnap;
import org.micromanager.utils.MMScriptException;

public class MMAcquisitionSnap extends MMAcquisition {
	MMStudioMainFrame gui_;
	
	public MMAcquisitionSnap(String name, String dir) {
		super(name, dir);
	}

	public MMAcquisitionSnap(String name, String dir, MMStudioMainFrame gui, boolean show) {
		super(name, dir, show);
		gui_ = gui;
	}
	
	protected Image5DWindow createImage5DWindow(Image5D img5d) {
		return new Image5DWindowSnap(img5d, this);
	}
	
	public void increaseFrameCount() {
		numFrames_++;
	}
	
	public void doSnapReplace() {
		try {
			gui_.snapAndAddImage(name_ ,0,0,0);
		} catch (MMScriptException e) {
			System.err.println(e);
		}
		//System.err.print("MMAcquisitionSnap.doSnap();");
	}
	
/*  Incomplete
	public void doSnapAppend() {
		try {
			ImageProcessor ip = gui_.snapSingleImageHidden().getProcessor();
	//		imgWin_.getImage5D().;
			numFrames_++;
		} catch (Exception e) {
			System.err.println(e);
		}
	}
	*/
}