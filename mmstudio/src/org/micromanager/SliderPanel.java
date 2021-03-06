///////////////////////////////////////////////////////////////////////////////
//FILE:          SliderPanel.java
//PROJECT:       Micro-Manager
//SUBSYSTEM:     mmstudio
//-----------------------------------------------------------------------------

//AUTHOR:       Nenad Amodaj, nenad@amodaj.com, Dec 20, 2007

//COPYRIGHT:    University of California, San Francisco, 2007

//LICENSE:      This file is distributed under the BSD license.
//              License text is included with the source distribution.

//              This file is distributed in the hope that it will be useful,
//              but WITHOUT ANY WARRANTY; without even the implied warranty
//              of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

//              IN NO EVENT SHALL THE COPYRIGHT OWNER OR
//              CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
//              INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES.

//CVS:          $Id: ConfigGroupPad.java 747 2008-01-04 01:08:50Z nenad $

package org.micromanager;

import java.awt.Color;
import java.awt.Dimension;
import java.awt.Font;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.event.MouseAdapter;
import java.text.ParseException;

import javax.swing.JOptionPane;
import javax.swing.JPanel;
import javax.swing.JScrollBar;
import javax.swing.JTextField;
import javax.swing.SpringLayout;
import javax.swing.event.ChangeEvent;
import javax.swing.event.ChangeListener;

import org.micromanager.utils.NumberUtils;

public class SliderPanel extends JPanel {
   private static final long serialVersionUID = -6039226355990936685L;
   private SpringLayout springLayout_;
	private JTextField textField_;
	private double lowerLimit_ = 0.0;
	private double upperLimit_ = 10.0;
	private final int STEPS = 1000;
	private double factor_ = 1.0;
	private boolean integer_ = false;

	private JScrollBar slider_;
	/**
	 * Create the panel
	 */
	public SliderPanel() {
		super();
		springLayout_ = new SpringLayout();
		setLayout(springLayout_);
		setPreferredSize(new Dimension(304, 18));

		textField_ = new JTextField();
		textField_.setFont(new Font("", Font.PLAIN, 10));
		textField_.addActionListener(new ActionListener() {
			public void actionPerformed(final ActionEvent arg0) {
				onEditChange();
			}
		});
		add(textField_);
		springLayout_.putConstraint(SpringLayout.EAST, textField_, 40, SpringLayout.WEST, this);
		springLayout_.putConstraint(SpringLayout.WEST, textField_, 0, SpringLayout.WEST, this);
      springLayout_.putConstraint(SpringLayout.SOUTH, textField_, 0, SpringLayout.SOUTH, this);
      springLayout_.putConstraint(SpringLayout.NORTH, textField_, 0, SpringLayout.NORTH, this);

		slider_ = new JScrollBar(JScrollBar.HORIZONTAL);
		slider_.getModel().addChangeListener(new ChangeListener() {
			public void stateChanged(final ChangeEvent arg0) {
				onSliderMove();
			}
		});
		add(slider_);
		springLayout_.putConstraint(SpringLayout.EAST, slider_, 0, SpringLayout.EAST, this);
		springLayout_.putConstraint(SpringLayout.WEST, slider_, 41, SpringLayout.WEST, this);
		springLayout_.putConstraint(SpringLayout.SOUTH, slider_, 0, SpringLayout.SOUTH, this);
		springLayout_.putConstraint(SpringLayout.NORTH, slider_, 0, SpringLayout.NORTH, this);
	}

	public void setText(String txt) {
      try {
         if (integer_) {
            int val = enforceLimits(NumberUtils.StringToInt(txt));
            slider_.setValue((int)(val + 0.5));
            textField_.setText(NumberUtils.NumberToString(val));
         } else {
            double val = enforceLimits(NumberUtils.StringToDouble(txt));
            slider_.setValue((int)((val - lowerLimit_)/factor_ + 0.5));
            textField_.setText(NumberUtils.NumberToString(val));
         }
		} catch (ParseException p) {
         handleException (p);
      }
	}
	
	public String getText() {
	   // implicitly enforce limits
		setText(textField_.getText());
		return textField_.getText();
	}

	public void enableEdit(boolean state) {
		textField_.setEditable(state);
	}
	
	public void setLimits(double lowerLimit, double upperLimit) {
		factor_ = (upperLimit - lowerLimit) / (STEPS);
		slider_.setMinimum(0);
		slider_.setMaximum(STEPS + slider_.getVisibleAmount());
		upperLimit_ = upperLimit;
		lowerLimit_ = lowerLimit;
		integer_ = false;
	}
	
	public void setLimits(int lowerLimit, int upperLimit) {
		factor_ = 1.0;
		upperLimit_ = upperLimit;
		lowerLimit_ = lowerLimit;
      slider_.setMinimum(lowerLimit);
      slider_.setMaximum(upperLimit + slider_.getVisibleAmount());
		integer_ = true;
	}

	private void onSliderMove() {
		double value = 0.0;
		if (integer_) {
			value = slider_.getValue();
	      textField_.setText(NumberUtils.NumberToString((int)(value)));
		} else { 
			value = slider_.getValue() * factor_ + lowerLimit_;
		   textField_.setText(NumberUtils.NumberToString(value));
		}	
	}

	protected void onEditChange() {
		int sliderPos;
      try {
         if (integer_) {
            sliderPos = enforceLimits(NumberUtils.StringToInt(textField_.getText()));
            slider_.setValue((int)(sliderPos + 0.5));
         } else {
            double val = enforceLimits(NumberUtils.StringToDouble(textField_.getText()));
            sliderPos = (int)((val - lowerLimit_)/factor_ + 0.5);
            slider_.setValue(sliderPos);
         }
      } catch (ParseException p) {
         handleException (p);
      }
	}
	
	private double enforceLimits(double value) {
	   double val = value;
      if (val < lowerLimit_)
         val = lowerLimit_;
      if (val > upperLimit_)
         val = upperLimit_;
      return val;
	}
	
   private int enforceLimits(int value) {
      int val = value;
      if (val < lowerLimit_)
         val = (int)(lowerLimit_);
      if (val > upperLimit_)
         val = (int)(upperLimit_);
      return val;
   }
   
	public void addEditActionListener(ActionListener al) {
	   textField_.addActionListener(al);
	}

	public void addSliderMouseListener(MouseAdapter md) {
      slider_.addMouseListener(md);
   }
	
	public void setBackground(Color bg) {
	   super.setBackground(bg);
	   if (slider_ != null)
	      slider_.setBackground(bg);
	   if (textField_ != null)
	      textField_.setBackground(bg);
	}

   private void handleException (Exception e) {
      String errText = "Exeption occured: " + e.getMessage();
      JOptionPane.showMessageDialog(null, errText);
   }

}
