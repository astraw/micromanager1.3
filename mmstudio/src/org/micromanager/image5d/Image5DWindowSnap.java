///////////////////////////////////////////////////////////////////////////////
//FILE:          Image5DWindowSnap.java
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


package org.micromanager.image5d;

import ij.ImagePlus;

import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;

import javax.swing.JButton;

import org.micromanager.PlaybackPanel;
import org.micromanager.acquisition.MMAcquisitionSnap;
import org.micromanager.image5d.Image5DWindow;
import org.micromanager.image5d.Image5D;
import org.micromanager.utils.MMScriptException;

import com.swtdesigner.SwingResourceManager;

public class Image5DWindowSnap extends Image5DWindow {
	private MMAcquisitionSnap acq_;
	
	public Image5DWindowSnap(Image5D imp, MMAcquisitionSnap acq) {
		super(imp);
		acq_ = acq;
		addSnapButton();
		System.out.println(imp);
	}
	
	public void addSnapButton() {
		JButton snapButton = new JButton();
		snapButton.setToolTipText("Snap new image in this window");
		snapButton.setIcon(SwingResourceManager.getIcon(PlaybackPanel.class, "/org/micromanager/icons/camera.png"));
		snapButton.setBounds(530,5,37,24);
		this.pb_.add(snapButton);
		snapButton.addActionListener(new ActionListener() {
			public void actionPerformed(final ActionEvent e) {
				doSnapReplace();
				//doSnapAppend();
			}
		});
	}
	
	private void doSnapReplace() {
		acq_.doSnapReplace();
	}
	
	private void addFrame() {
		//newCount = i5d.getNFrames() + 1;
		
	}
	
/*	**Incomplete
 * private void doSnapAppend() {
		acq_.increaseFrameCount();
	}
	*/
}

