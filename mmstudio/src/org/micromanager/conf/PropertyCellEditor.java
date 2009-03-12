///////////////////////////////////////////////////////////////////////////////
//FILE:          PropertyCellEditor.java
//PROJECT:       Micro-Manager
//SUBSYSTEM:     mmstudio
//-----------------------------------------------------------------------------
//
// AUTHOR:       Nenad Amodaj, nenad@amodaj.com, October 29, 2006
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

import java.awt.Component;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.event.ItemEvent;
import java.awt.event.ItemListener;

import javax.swing.AbstractCellEditor;
import javax.swing.JComboBox;
import javax.swing.JTable;
import javax.swing.JTextField;
import javax.swing.event.DocumentEvent;
import javax.swing.event.DocumentListener;
import javax.swing.table.TableCellEditor;

/**
 * In-place table editor for string cells.
 *
 */
public class PropertyCellEditor extends AbstractCellEditor implements TableCellEditor {
   private static final long serialVersionUID = 1L;
   // This is the component that will handle the editing of the cell value
   JTextField text_ = new JTextField();
   JComboBox combo_ = new JComboBox();
   int editingCol_;
   Property item_;
   
   public PropertyCellEditor() {
      super();
   }
   
   // This method is called when a cell value is edited by the user.
   public Component getTableCellEditorComponent(JTable table, Object value,
         boolean isSelected, int rowIndex, int colIndex) {
      
      editingCol_ = colIndex;
               
      PropertyTableModel data = (PropertyTableModel)table.getModel();
      item_ = data.getPropertyItem(rowIndex);
      
      // Configure the component with the specified value
      
      if (colIndex == 2) {
         if (item_.allowedValues_.length == 0) {
            text_.setText((String)value);
            text_.addActionListener(new ActionListener() {
               public void actionPerformed(ActionEvent e) {
                  fireEditingStopped();
               }
            });
            text_.getDocument().addDocumentListener(new DocumentListener() {
               public void changedUpdate (DocumentEvent e) {
                  fireEditingStopped();
               }
               public void insertUpdate (DocumentEvent e) {
                  fireEditingStopped();
               }
               public void removeUpdate (DocumentEvent e) {
                  fireEditingStopped();
               }
            });

            return text_;
         } else {    
	         ActionListener[] l = combo_.getActionListeners();
	         for (int i=0; i<l.length; i++) 
	            combo_.removeActionListener(l[i]);
	         ItemListener[] il = combo_.getItemListeners();
	         for (int i=0; i<il.length; i++) 
	            combo_.removeItemListener(il[i]);
	         combo_.removeAllItems();

	         // When the user clicks the label and goes into editing mode,
	         // the first item selected is #0. Because of a bug in JDK,
	         // ActionListeners aren't thrown if #0 is selected (because the
	         // selection hasn't changed). So we need to manual set the item
	         // value beforehand, which will be overridden if a different item
	         // is chosen.
	         if (item_.value_.equals(""))
	        	 item_.value_ = item_.allowedValues_[0];
	         
	         for (int i=0; i<item_.allowedValues_.length; i++){
	            combo_.addItem(item_.allowedValues_[i]);
	         }
	         combo_.setSelectedItem(item_.value_);
	         
	         // end editing on selection change
	         combo_.addActionListener(new ActionListener() {
	            public void actionPerformed(ActionEvent e) {
		           fireEditingStopped();
		           item_.value_ = combo_.getSelectedItem().toString();

	            }
	         });
	         // end editing on selection change
	         combo_.addItemListener(new ItemListener() {
	            public void itemStateChanged(ItemEvent e) {
		               fireEditingStopped();
		               item_.value_ = combo_.getSelectedItem().toString();
	            }
	         });
	                    
	         return combo_;
         }
      }
      return null;
   }
   
   // This method is called when editing is completed.
   // It must return the new value to be stored in the cell.
   public Object getCellEditorValue() {
      if (editingCol_ == 2) {
         if (item_.allowedValues_.length == 0)
            return text_.getText();
         else
            return combo_.getSelectedItem();
      }
      return null;
   }
}
