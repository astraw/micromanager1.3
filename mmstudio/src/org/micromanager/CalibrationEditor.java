///////////////////////////////////////////////////////////////////////////////
//FILE:          CalibrationEditor.java
//PROJECT:       Micro-Manager
//SUBSYSTEM:     mmstudio
//-----------------------------------------------------------------------------
//
// AUTHOR:       Nico Stuurman, nico@cmp.ucsf.edu, June 2008
//               Based on PresetEditor.java by Nenad Amodaj
//
// COPYRIGHT:    University of California, San Francisco, 2008
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

package org.micromanager;

import java.awt.Component;
import java.awt.Font;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.event.MouseAdapter;
import java.awt.event.MouseEvent;
import java.awt.event.WindowAdapter;
import java.awt.event.WindowEvent;
import java.util.ArrayList;
import java.util.prefs.Preferences;

import javax.swing.AbstractCellEditor;
import javax.swing.JButton;
import javax.swing.JCheckBox;
import javax.swing.JComboBox;
import javax.swing.JLabel;
import javax.swing.JOptionPane;
import javax.swing.JScrollPane;
import javax.swing.JTable;
import javax.swing.JTextArea;
import javax.swing.JTextField;
import javax.swing.ListSelectionModel;
import javax.swing.SpringLayout;
import javax.swing.WindowConstants;
import javax.swing.border.BevelBorder;
import javax.swing.table.AbstractTableModel;
import javax.swing.table.TableCellEditor;
import javax.swing.table.TableCellRenderer;
import javax.swing.table.TableColumn;

import mmcorej.CMMCore;
import mmcorej.Configuration;
import mmcorej.DeviceType;
import mmcorej.PropertySetting;
import mmcorej.PropertyType;
import mmcorej.StrVector;

import org.micromanager.api.DeviceControlGUI;
import org.micromanager.utils.MMDialog;
import org.micromanager.utils.ShowFlags;

/**
 * Dialog based GUI component for generic editing of device properties.
 * Represents the entire system state as a list of triplets:
 * device - property - value
 * 
 * Specifically targeted for editing configuration presets.
 */
public class CalibrationEditor extends MMDialog {
   private static final long serialVersionUID = 1L;
   private JTextArea textArea_;
   private JTextField presetSizeField_;
   private JTextField presetLabelField_;
   private String label_;
   private String pixelSize_;
   private SpringLayout springLayout;
   Boolean changed_;
   
   private JTable table_;
   private PropertyTableData data_;
   private DeviceControlGUI parentGUI_;
   private ShowFlags flags_;
   
   private JCheckBox showCamerasCheckBox_;
   private JCheckBox showShuttersCheckBox_;
   private JCheckBox showStagesCheckBox_;
   private JCheckBox showStateDevicesCheckBox_;
   private JCheckBox showOtherCheckBox_;
   private Configuration initialCfg_;
   private JScrollPane scrollPane_;
    
   public CalibrationEditor(String label, String pixelSize) {
      super();
      setModal(true);
      label_ = new String(label);
      pixelSize_ = new String(pixelSize);
      changed_ = new Boolean(false);
      Preferences root = Preferences.userNodeForPackage(this.getClass());
      // Share Prefs with PresetEditor
      setPrefsNode(root.node(root.absolutePath() + "/PresetEditor"));
      initialCfg_ = new Configuration();
      
      flags_ = new ShowFlags();
      flags_.load(getPrefsNode());
      
      springLayout = new SpringLayout();
      getContentPane().setLayout(springLayout);
      setSize(551, 562);
      addWindowListener(new WindowAdapter() {
         public void windowClosing(WindowEvent e) {
            savePosition();
            flags_.save(getPrefsNode());
         }
         public void windowOpened(WindowEvent e) {
            // restore values from the previous session
            data_.updateFlags();
            data_.updateStatus();
            data_.showOriginalSelection();
        }
         public void windowClosed(WindowEvent arg0) {
            savePosition();
            flags_.save(getPrefsNode());
         }
      });
      setTitle("Calibration Group Editor");

      loadPosition(100, 100, 400, 300);
      setDefaultCloseOperation(WindowConstants.DISPOSE_ON_CLOSE);

      scrollPane_ = new JScrollPane();
      scrollPane_.setFont(new Font("Arial", Font.PLAIN, 10));
      scrollPane_.setBorder(new BevelBorder(BevelBorder.LOWERED));
      getContentPane().add(scrollPane_);
      springLayout.putConstraint(SpringLayout.SOUTH, scrollPane_, -5, SpringLayout.SOUTH, getContentPane());
      springLayout.putConstraint(SpringLayout.NORTH, scrollPane_, 160, SpringLayout.NORTH, getContentPane());
      springLayout.putConstraint(SpringLayout.EAST, scrollPane_, -5, SpringLayout.EAST, getContentPane());
      springLayout.putConstraint(SpringLayout.WEST, scrollPane_, 5, SpringLayout.WEST, getContentPane());

      table_ = new JTable();
      table_.setSelectionMode(ListSelectionModel.MULTIPLE_INTERVAL_SELECTION);
      table_.setAutoCreateColumnsFromModel(false);
      scrollPane_.setViewportView(table_);
      
      showCamerasCheckBox_ = new JCheckBox();
      showCamerasCheckBox_.setFont(new Font("", Font.PLAIN, 10));
      showCamerasCheckBox_.addActionListener(new ActionListener() {
         public void actionPerformed(ActionEvent arg0) {
            flags_.cameras_ = showCamerasCheckBox_.isSelected();
            data_.updateStatus();
         }
      });
      showCamerasCheckBox_.setText("Show cameras");
      getContentPane().add(showCamerasCheckBox_);
      springLayout.putConstraint(SpringLayout.SOUTH, showCamerasCheckBox_, 28, SpringLayout.NORTH, getContentPane());
      springLayout.putConstraint(SpringLayout.EAST, showCamerasCheckBox_, 105, SpringLayout.WEST, getContentPane());

      showShuttersCheckBox_ = new JCheckBox();
      showShuttersCheckBox_.setFont(new Font("", Font.PLAIN, 10));
      showShuttersCheckBox_.addActionListener(new ActionListener() {
         public void actionPerformed(ActionEvent arg0) {
            flags_.shutters_ = showShuttersCheckBox_.isSelected();
            data_.updateStatus();
         }
      });
      showShuttersCheckBox_.setText("Show shutters");
      getContentPane().add(showShuttersCheckBox_);
      springLayout.putConstraint(SpringLayout.EAST, showShuttersCheckBox_, 111, SpringLayout.WEST, getContentPane());
      springLayout.putConstraint(SpringLayout.WEST, showShuttersCheckBox_, 10, SpringLayout.WEST, getContentPane());
      springLayout.putConstraint(SpringLayout.SOUTH, showShuttersCheckBox_, 50, SpringLayout.NORTH, getContentPane());

      showStagesCheckBox_ = new JCheckBox();
      showStagesCheckBox_.setFont(new Font("", Font.PLAIN, 10));
      showStagesCheckBox_.addActionListener(new ActionListener() {
         public void actionPerformed(ActionEvent arg0) {
            flags_.stages_ = showStagesCheckBox_.isSelected();
            data_.updateStatus();
         }
      });
      showStagesCheckBox_.setText("Show stages");
      getContentPane().add(showStagesCheckBox_);
      springLayout.putConstraint(SpringLayout.EAST, showStagesCheckBox_, 111, SpringLayout.WEST, getContentPane());
      springLayout.putConstraint(SpringLayout.WEST, showStagesCheckBox_, 10, SpringLayout.WEST, getContentPane());
      springLayout.putConstraint(SpringLayout.SOUTH, showStagesCheckBox_, 73, SpringLayout.NORTH, getContentPane());
      springLayout.putConstraint(SpringLayout.NORTH, showStagesCheckBox_, 50, SpringLayout.NORTH, getContentPane());

      showStateDevicesCheckBox_ = new JCheckBox();
      showStateDevicesCheckBox_.setFont(new Font("", Font.PLAIN, 10));
      showStateDevicesCheckBox_.addActionListener(new ActionListener() {
         public void actionPerformed(ActionEvent arg0) {
            flags_.state_ = showStateDevicesCheckBox_.isSelected();
            data_.updateStatus();
         }
      });
      showStateDevicesCheckBox_.setText("Show wheels, turrets,etc.");
      getContentPane().add(showStateDevicesCheckBox_);
      springLayout.putConstraint(SpringLayout.EAST, showStateDevicesCheckBox_, 165, SpringLayout.WEST, getContentPane());
      springLayout.putConstraint(SpringLayout.WEST, showStateDevicesCheckBox_, 10, SpringLayout.WEST, getContentPane());
      springLayout.putConstraint(SpringLayout.SOUTH, showStateDevicesCheckBox_, 95, SpringLayout.NORTH, getContentPane());
      springLayout.putConstraint(SpringLayout.NORTH, showStateDevicesCheckBox_, 72, SpringLayout.NORTH, getContentPane());

      showOtherCheckBox_ = new JCheckBox();
      showOtherCheckBox_.setFont(new Font("", Font.PLAIN, 10));
      showOtherCheckBox_.addActionListener(new ActionListener() {
         public void actionPerformed(ActionEvent arg0) {
            flags_.other_ = showOtherCheckBox_.isSelected();
            data_.updateStatus();
         }
      });
      showOtherCheckBox_.setText("Show other devices");
      getContentPane().add(showOtherCheckBox_);
      springLayout.putConstraint(SpringLayout.EAST, showOtherCheckBox_, 155, SpringLayout.WEST, getContentPane());
      springLayout.putConstraint(SpringLayout.WEST, showOtherCheckBox_, 10, SpringLayout.WEST, getContentPane());
      springLayout.putConstraint(SpringLayout.NORTH, showOtherCheckBox_, 95, SpringLayout.NORTH, getContentPane());

      final JLabel presetLabel = new JLabel();
      presetLabel.setText("Label: ");
      presetLabel.setFont(new Font("", Font.PLAIN, 10));
      getContentPane().add(presetLabel);
      springLayout.putConstraint(SpringLayout.EAST, presetLabel, 340, SpringLayout.WEST, getContentPane());
      springLayout.putConstraint(SpringLayout.WEST, presetLabel, 190, SpringLayout.WEST, getContentPane());
      springLayout.putConstraint(SpringLayout.SOUTH, presetLabel, 41, SpringLayout.NORTH, getContentPane());
      springLayout.putConstraint(SpringLayout.NORTH, presetLabel, 30, SpringLayout.NORTH, getContentPane());

      presetLabelField_ = new JTextField(label_);
      getContentPane().add(presetLabelField_);
      springLayout.putConstraint(SpringLayout.EAST, presetLabelField_, 340, SpringLayout.WEST, getContentPane());
      springLayout.putConstraint(SpringLayout.WEST, presetLabelField_, 190, SpringLayout.WEST, getContentPane());
      springLayout.putConstraint(SpringLayout.SOUTH, presetLabelField_, 61, SpringLayout.NORTH, getContentPane());
      springLayout.putConstraint(SpringLayout.NORTH, presetLabelField_, 42, SpringLayout.NORTH, getContentPane());

      final JLabel presetSizeLabel = new JLabel();
      presetSizeLabel.setFont(new Font("", Font.PLAIN, 10));
      presetSizeLabel.setText("Pixel Size (um/pixel):");
      getContentPane().add(presetSizeLabel);
      springLayout.putConstraint(SpringLayout.SOUTH, presetSizeLabel, 85, SpringLayout.NORTH, getContentPane());
      springLayout.putConstraint(SpringLayout.NORTH, presetSizeLabel, 71, SpringLayout.NORTH, getContentPane());
      springLayout.putConstraint(SpringLayout.EAST, presetSizeLabel, 340, SpringLayout.WEST, getContentPane());
      springLayout.putConstraint(SpringLayout.WEST, presetSizeLabel, 190, SpringLayout.WEST, getContentPane());

      presetSizeField_ = new JTextField(pixelSize_);
      getContentPane().add(presetSizeField_);
      springLayout.putConstraint(SpringLayout.EAST, presetSizeField_, 340, SpringLayout.WEST, getContentPane());
      springLayout.putConstraint(SpringLayout.WEST, presetSizeField_, 190, SpringLayout.WEST, getContentPane());
      springLayout.putConstraint(SpringLayout.SOUTH, presetSizeField_, 105, SpringLayout.NORTH, getContentPane());
      springLayout.putConstraint(SpringLayout.NORTH, presetSizeField_, 86, SpringLayout.NORTH, getContentPane());

      final JButton okButton = new JButton();
      okButton.addActionListener(new ActionListener() {
         public void actionPerformed(ActionEvent arg0) {
            if (applySettings()) {
               changed_ = Boolean.TRUE;
               dispose();
            }
         }
      });
      okButton.setText("OK");
      getContentPane().add(okButton);
      springLayout.putConstraint(SpringLayout.SOUTH, okButton, 49, SpringLayout.NORTH, getContentPane());
      springLayout.putConstraint(SpringLayout.NORTH, okButton, 26, SpringLayout.NORTH, getContentPane());
      springLayout.putConstraint(SpringLayout.EAST, okButton, 475, SpringLayout.WEST, getContentPane());
      springLayout.putConstraint(SpringLayout.WEST, okButton, 385, SpringLayout.WEST, getContentPane());

      final JButton cancelButton = new JButton();
      cancelButton.addActionListener(new ActionListener() {
         public void actionPerformed(ActionEvent arg0) {
            dispose();
         }
      });
      cancelButton.setText("Cancel");
      getContentPane().add(cancelButton);
      springLayout.putConstraint(SpringLayout.EAST, cancelButton, 475, SpringLayout.WEST, getContentPane());
      springLayout.putConstraint(SpringLayout.WEST, cancelButton, 385, SpringLayout.WEST, getContentPane());
      springLayout.putConstraint(SpringLayout.SOUTH, cancelButton, 75, SpringLayout.NORTH, getContentPane());
      springLayout.putConstraint(SpringLayout.NORTH, cancelButton, 52, SpringLayout.NORTH, getContentPane());

      textArea_ = new JTextArea();
      textArea_.setFont(new Font("Arial", Font.PLAIN, 10));
      textArea_.setWrapStyleWord(true);
      textArea_.setText("Select all settings you want use in this calibration and press OK to save changes.\n" +
                       "You can also change settings by clicking in the right hand column.");
      textArea_.setEditable(false);
      textArea_.setOpaque(false);
      getContentPane().add(textArea_);
      springLayout.putConstraint(SpringLayout.EAST, textArea_, -5, SpringLayout.EAST, getContentPane());
      springLayout.putConstraint(SpringLayout.WEST, textArea_, 10, SpringLayout.WEST, getContentPane());
      springLayout.putConstraint(SpringLayout.SOUTH, textArea_, 155, SpringLayout.NORTH, getContentPane());
      springLayout.putConstraint(SpringLayout.NORTH, textArea_, 122, SpringLayout.NORTH, getContentPane());
   }
   

   protected boolean applySettings() {

      // stop all editing
      int column = table_.getEditingColumn(); 
      if (column >    -1) { 
         TableCellEditor cellEditor = table_.getColumnModel().getColumn(column).getCellEditor();
         if (cellEditor == null) { 
            cellEditor = table_.getDefaultEditor(table_.getColumnClass(column));
         } 
         if (cellEditor != null) { 
            cellEditor.stopCellEditing(); 
         } 
      } 


      if (!data_.isEditingGroup())
         data_.updateStatus(); // to set the "use" flags properly

      String duplicate = data_.findMatchingPreset();
      if (duplicate != null && duplicate.compareTo(label_)!=0) {
         JOptionPane.showMessageDialog(this, "This calibration is the same as the already existing one: " + duplicate +
         "\nYou must choose a unique set of values.");
         return false;
      }

      return data_.applySettings(label_, presetLabelField_.getText(), presetSizeField_.getText());
   }
   
   public void setCore(CMMCore core){
      data_ = new PropertyTableData(core, flags_);
      table_ = new JTable();
      table_.setSelectionMode(ListSelectionModel.MULTIPLE_INTERVAL_SELECTION);
      table_.setAutoCreateColumnsFromModel(false);
      scrollPane_.setViewportView(table_);
      table_.setModel(data_);

      if (core.isPixelSizeConfigDefined(label_)) {
         try {
            core.setPixelSizeConfig(label_);
         } catch (Exception e) {
            handleException (e);
         }
      }
 

      
      PropertyCellEditor cellEditor = new PropertyCellEditor();
      PropertyCellRenderer renderer = new PropertyCellRenderer();
     
      for (int k=0; k < data_.getColumnCount(); k++) {
         TableColumn column = new TableColumn(k, 200, renderer, cellEditor);
         table_.addColumn(column);
      }
            
      showCamerasCheckBox_.setSelected(flags_.cameras_);
      showStagesCheckBox_.setSelected(flags_.stages_);
      showShuttersCheckBox_.setSelected(flags_.shutters_);
      showStateDevicesCheckBox_.setSelected(flags_.state_);
      showOtherCheckBox_.setSelected(flags_.other_);
      
      data_.updateFlags();
      data_.updateStatus();
      
      if (!data_.isEditingGroup()) {
         getContentPane().remove(showCamerasCheckBox_);
         getContentPane().remove(showStagesCheckBox_);
         getContentPane().remove(showShuttersCheckBox_);
         getContentPane().remove(showStateDevicesCheckBox_);
         getContentPane().remove(showOtherCheckBox_);
         textArea_.setText("Choose values for the properties contained in this calibration.\n" +
         "Available properties are determined by the first preset defined for this group");
      }
  }
   
   public void setParentGUI(DeviceControlGUI parent) {
      parentGUI_ = parent;
   }
   
   private void handleException (Exception e) {
      String errText = "Exeption occured: " + e.getMessage();
      JOptionPane.showMessageDialog(this, errText);
   }
      
   
   /**
    * Property descriptor, representing MMCore data
    */
   private class PropertyItem {
      public String device;  // device name (label)
      public String name;    // property name
      public String value;   // property value
      public boolean readOnly = false;    // is it read-only ?
      public boolean preInit = false; // is it pre initialization property ?
      public String allowed[];            // the list of allowed values
      public boolean show = true; // is it included in the current configuration ?
      public boolean orgSel = false;
      public boolean confInclude = false; // is it included in the current configuration ?
      public boolean hasRange = false; // is there a range for values
      public double lowerLimit = 0.0;
      public double upperLimit = 0.0;
      public boolean isInt = false; // is this an integer property
   }
   

   /**
    * Property table data model, representing MMCore data
    */
   class PropertyTableData extends AbstractTableModel {
      private static final long serialVersionUID = -5582899855072387637L;

      final public String columnNames_[] = {
            "Property",
            "Value",
            "Use"
      };
      
      ArrayList<PropertyItem> propList_ = new ArrayList<PropertyItem>();
      private CMMCore core_ = null;
      Configuration groupData_[];
      PropertySetting groupSignature_[];

      private String[] presetNames_;
      
      public PropertyTableData(CMMCore core, ShowFlags flags) {
         core_ = core;
         flags_ = flags;
         updateStatus();
      }
      
      public String findMatchingPreset() {
         // find selected rows
         ArrayList<PropertyItem> selected = new ArrayList<PropertyItem>();
         for (int i=0; i<propList_.size(); i++) {
            PropertyItem item = propList_.get(i);
            if (item.confInclude)
               selected.add(item);
         }
                
         for (int i=0; i<groupData_.length; i++) {
            int matchCount = 0;
            for (int j=0; j<selected.size(); j++) {
               PropertyItem pi = selected.get(j);
               PropertySetting ps = new PropertySetting(pi.device, pi.name, pi.value);
               if (groupData_[i].isSettingIncluded(ps)) {
                  matchCount++;
               }
            }
            if (matchCount == selected.size())
               return presetNames_[i];
         }
         
         return null;
      }
      
      public boolean applySettings(String oldLabel, String newLabel, String pixelSize) {
         // If Label was changed, delete the old one
         if (! oldLabel.equals(newLabel)) {
            try {
               if (core_.isPixelSizeConfigDefined(oldLabel))
                  core_.deletePixelSizeConfig(oldLabel);
            } catch (Exception e) {
               handleException(e);
               return false;
            }
         }
         // find selected rows
         ArrayList<PropertyItem> selected = new ArrayList<PropertyItem>();
         for (int i=0; i<propList_.size(); i++) {
            PropertyItem item = propList_.get(i);
            if (item.confInclude)
               selected.add(item);
         }
                  
         ArrayList<PropertyItem> mismatched = new ArrayList<PropertyItem>();
         ArrayList<PropertySetting> missing = new ArrayList<PropertySetting>();
         
         // check signature for mismatched or missing items
         if (!isEditingGroup()) {
            // mismatched
            for (int i=0; i<selected.size(); i++) {
               PropertyItem item = selected.get(i);
               int j;
               for (j=0; j<groupSignature_.length; j++) {
                  if (item.device.compareTo(groupSignature_[j].getDeviceLabel()) == 0 &&
                      item.name.compareTo(groupSignature_[j].getPropertyName()) == 0 ) {
                     break;
                  }
               }
               if (groupSignature_.length == j) {
                  mismatched.add(item);
               }
            }
            
            // missing
            for (int i=0; i<groupSignature_.length; i++) {
               int j;
               for (j=0; j<selected.size(); j++) {
                  PropertyItem item = selected.get(j);
                  if (item.device.compareTo(groupSignature_[i].getDeviceLabel()) == 0 &&
                      item.name.compareTo(groupSignature_[i].getPropertyName()) == 0 ) {
                     break;
                  }
               }
               if (selected.size() == j) {
                  missing.add(groupSignature_[i]);
               }
            }
         }
         
         if (mismatched.size() > 0 || missing.size() > 0) {
            String mismatchedList = new String("");
            for (int i=0; i<mismatched.size(); i++)
               mismatchedList += mismatched.get(i).device + "-" + mismatched.get(i).name + "\n";
            String missingList = new String("");
            for (int i=0; i<missing.size(); i++)
               missingList += missing.get(i).getDeviceLabel() + "-" + missing.get(i).getPropertyName() + "\n";
            
            String msgText = "All presets within a group should operate on the same set of device properties.\n" +
                             "Based on the previously defined presets in this group, the following inconsistencises were detected:\n\n" +
                             (mismatched.size() > 0 ? "Properties not previously defined:\n" + mismatchedList : "\n") +
                             (missing.size() > 0 ? "Properties prviously defined but missing from this preset:\n" + missingList : "\n") +
                             "\nPlease revise all presets in a single group to use the same set of properties.\n" +
                             "Otherwise some of the presets within the group may appear to be ambiguous";
            
            JOptionPane.showMessageDialog(null, msgText);
         }
         
         try {
            // first check if the setting exists and remove it if it does
            if (core_.isPixelSizeConfigDefined(newLabel))
               core_.deletePixelSizeConfig(newLabel);
            
            // define a new preset
            for (int i=0; i<selected.size(); i++) {
               PropertyItem item = selected.get(i);
               core_.definePixelSizeConfig(newLabel, item.device, item.name, item.value);
            }
            core_.setPixelSizeUm(newLabel, Double.parseDouble(pixelSize));
            
         } catch (Exception e) {
            handleException(e);
            return false;
         }
         
         return true;
      }
      
      public boolean verifyPresetSignature() {
         return true;
      }
      
      public void deletePixelSizeConfig(String configName) {
         try {
            core_.deletePixelSizeConfig(configName);
         } catch (Exception e) {
            handleException(e);
         }
      }
  
      public int getRowCount() {
         return propList_.size();
      }
      
      public int getColumnCount() {
         if (isEditingGroup())
            return columnNames_.length;
         else
            return columnNames_.length-1;
      }
      
      public boolean isEditingGroup() {
         return groupSignature_.length == 0;
      }
      
      public PropertyItem getPropertyItem(int row) {
         return propList_.get(row);
      }
      
      public Object getValueAt(int row, int col) {
         
         PropertyItem item = propList_.get(row);
         if (col == 0)
            return item.device + "-" + item.name;
         else if (col == 1)
            return item.value;
         else if (col == 2) {
            //JCheckBox cb = new JCheckBox();
            //cb.setSelected(item.confInclude);
            return new Boolean(item.confInclude);
         }
         
         return null;
      }
      
      public void setValueAt(Object value, int row, int col) {
         PropertyItem item = propList_.get(row);
         if (col == 1) {
            try {
               core_.setProperty(item.device, item.name, value.toString());
               //item.m_value = core_.getProperty(item.m_device, item.m_name);
               core_.waitForDevice(item.device);
               refresh();
               //item.m_value = value.toString();
               if (parentGUI_ != null)
                  parentGUI_.updateGUI(true);              
               fireTableCellUpdated(row, col);
            } catch (Exception e) {
               handleException(e);
            }
         }  else if (col == 2)  {
            item.confInclude = ((Boolean)value).booleanValue();
             // fireTableCellUpdated(row, col);
         }
      }
      
      public String getColumnName(int column) {
         return columnNames_[column];
      }
      
      public boolean isCellEditable(int nRow, int nCol) {
         if (nCol == 1)
            return !propList_.get(nRow).readOnly;
         else if (nCol == 2) {
            if (!isEditingGroup())
               return false;
            return true;
         } else
            return false;
      }
      
      /*
      String getConfig(String group) {
         String config = "";
         try {
            config = core_.getCurrentConfig(group);
         } catch (Exception e) {
            handleException(e);
         }
         return config;
      }
      */
      
      StrVector getAvailablePixelSizeConfigs() {
          return core_.getAvailablePixelSizeConfigs();
      }
      
      public void refresh(){
         try {            
            for (int i=0; i<propList_.size(); i++){
               PropertyItem item = propList_.get(i);
               item.value = core_.getProperty(item.device, item.name);
            }
            this.fireTableDataChanged();
         } catch (Exception e) {
            handleException(e);
         }
      }

      public void updateStatus(){
         
         // determine the group signature
         groupData_ = new Configuration[0];
         StrVector cfgNames = new StrVector(0);
         try {
                if (core_.isPixelSizeConfigDefined(label_)) {
                   initialCfg_ = core_.getPixelSizeConfigData(label_);
                }
                // collect the group data
               cfgNames = core_.getAvailablePixelSizeConfigs();
               groupData_ = new Configuration[(int)cfgNames.size()];
               presetNames_ = new String[(int)cfgNames.size()];
               for (int i=0; i<groupData_.length; i++) {
                  groupData_[i] = core_.getPixelSizeConfigData(cfgNames.get(i));
                  presetNames_[i] = cfgNames.get(i);
               }
               
               if (groupData_.length > 0) {
                  // extract the signature
                  groupSignature_ = new PropertySetting[(int)groupData_[0].size()];
                  for (int i=0; i < groupData_[0].size(); i++) {
                     groupSignature_[i] = groupData_[0].getSetting(i);
                  }
               } else {
                  groupSignature_ = new PropertySetting[0];
               }
         } catch (Exception e) {
            handleException(e);
         }
         
         try {
            
            StrVector devices = core_.getLoadedDevices();
            propList_.clear();
            
            for (int i=0; i<devices.size(); i++){               
               // select which devices to display
               DeviceType dtype = core_.getDeviceType(devices.get(i));
               boolean showDevice = false;
               if (dtype == DeviceType.MagnifierDevice)
                  showDevice = false;
               else if (dtype == DeviceType.CameraDevice)
                  showDevice = flags_.cameras_;
               else if (dtype == DeviceType.ShutterDevice)
                  showDevice = flags_.shutters_;
               else if (dtype == DeviceType.StageDevice)
                  showDevice = flags_.stages_;
               else if (dtype == DeviceType.StateDevice)
                  showDevice = flags_.state_;
               else
                  showDevice = flags_.other_;
               
               if (showDevice) {
                  StrVector properties = core_.getDevicePropertyNames(devices.get(i));
                  
                  for (int j=0; j<properties.size(); j++) {
                     PropertyItem item = new PropertyItem();
                     item.device = devices.get(i);
                     item.name = properties.get(j);
                     item.value = core_.getProperty(devices.get(i), properties.get(j));
                     item.readOnly = core_.isPropertyReadOnly(devices.get(i), properties.get(j));
                     // Hack to make Camera Binning invisible:
                     if ((dtype == DeviceType.CameraDevice) && (item.name.equals("Binning"))) {
                        item.readOnly = true;
                     }

                     item.preInit = core_.isPropertyPreInit(devices.get(i), properties.get(j));
                     item.hasRange = core_.hasPropertyLimits(devices.get(i), properties.get(j));
                     item.lowerLimit = core_.getPropertyLowerLimit(devices.get(i), properties.get(j));
                     item.upperLimit = core_.getPropertyUpperLimit(devices.get(i), properties.get(j));
                     item.isInt = PropertyType.Integer == core_.getPropertyType(item.device, item.name);
                     StrVector values = core_.getAllowedPropertyValues(devices.get(i), properties.get(j));
                     item.allowed = new String[(int)values.size()];
                     for (int k=0; k<values.size(); k++){
                        item.allowed[k] = values.get(k);
                     }
                     
                     if (!item.readOnly && !item.preInit) {
                        if(initialCfg_.isPropertyIncluded(item.device, item.name)){
                           item.orgSel  = true;
                           item.confInclude = true;
                        } else {
                           item.orgSel = false;
                           item.confInclude = false;
                        }
                        
                        if (isMatchingSignature(item)) {
                           propList_.add(item);
                           if (!isEditingGroup())
                              item.confInclude = true;
                        }
                     }
                  }
               }
            }
         } catch (Exception e) {
            handleException(e);
         }
         this.fireTableStructureChanged();
      }
      
      private boolean isMatchingSignature(PropertyItem item) {
         if (isEditingGroup())
            return true;
         else {
            int j;
            for (j=0; j<groupSignature_.length; j++) {
               if (item.device.compareTo(groupSignature_[j].getDeviceLabel()) == 0 &&
                   item.name.compareTo(groupSignature_[j].getPropertyName()) == 0 ) {
                  break;
               }
            }
            if (j == groupSignature_.length)
               return false;
            else
               return true;
         }
      }

      /**
       * Make sure that flags show all devices contained in the original selection.
       *
       */
      private void updateFlags() {
         try {
            // get properties contained in the current config
            if (core_.isPixelSizeConfigDefined(label_)) {
               initialCfg_ = core_.getPixelSizeConfigData(label_);
            }
            
            // change 'show' flags to always show contained devices
            for (int i=0; i< initialCfg_.size(); i++) {
               DeviceType dtype = core_.getDeviceType(initialCfg_.getSetting(i).getDeviceLabel());
               if (dtype == DeviceType.CameraDevice) {
                  flags_.cameras_ = true;
                  showCamerasCheckBox_.setSelected(true);
               } else if (dtype == DeviceType.ShutterDevice) {
                  flags_.shutters_ = true;
                  showShuttersCheckBox_.setSelected(true);
               } else if (dtype == DeviceType.StageDevice) {
                  flags_.stages_ = true;
                  showStagesCheckBox_.setSelected(true);
               } else if (dtype == DeviceType.StateDevice) {
                  flags_.state_ = true;
                  showStateDevicesCheckBox_.setSelected(true);
               } else {
                  showOtherCheckBox_.setSelected(true);
                  flags_.other_ = true;;
               }
            }
         } catch (Exception e) {
            handleException(e);
         }
      }
      
      /**
       * Restore selection to refelect the original configuration.
       *
       */
      public void showOriginalSelection() {
         // set appropriate selection
         for (int i=0; i<propList_.size(); i++) {
            PropertyItem item = propList_.get(i);
            if (initialCfg_.size() == 0)
               item.confInclude = false;
            else {
               if(initialCfg_.isPropertyIncluded(item.device, item.name)) {
                  item.confInclude = true;
               } else
                  item.confInclude = false;
            }
         }
         this.fireTableDataChanged();
      }
   }
   
   /**
    * Cell editing using either JTextField or JComboBox depending on whether the
    * property enforces a set of allowed values.
    */
   public class PropertyCellEditor extends AbstractCellEditor implements TableCellEditor {
      private static final long serialVersionUID = 1L;
      // This is the component that will handle the editing of the cell value
      JTextField text_ = new JTextField();
      JComboBox combo_ = new JComboBox();
      JCheckBox check_ = new JCheckBox();
      int editingCol_;
      PropertyItem item_;
      SliderPanel slider_ = new SliderPanel();
      
      public PropertyCellEditor() {
         super();
         check_.setSelected(false);
         check_.addActionListener(new ActionListener() {
            public void actionPerformed(ActionEvent e) {
               fireEditingStopped();
            }
         });
         // end editing on selection change
         combo_.addActionListener(new ActionListener() {
            public void actionPerformed(ActionEvent e) {
               fireEditingStopped();
            }
         });
         
         slider_.addEditActionListener(new ActionListener() {
            public void actionPerformed(ActionEvent e) {
               fireEditingStopped();
            }            
         });
         slider_.addSliderMouseListener(new MouseAdapter() {
            public void mouseReleased(MouseEvent e) {
               fireEditingStopped();
            }
         });
      }

      // This method is called when a cell value is edited by the user.
      public Component getTableCellEditorComponent(JTable table, Object value,
            boolean isSelected, int rowIndex, int colIndex) {
                 
         if (isSelected) {
            // cell (and perhaps other cells) are selected
         }
         
         editingCol_ = colIndex;
                  
         PropertyTableData data = (PropertyTableData)table.getModel();
         item_ = data.getPropertyItem(rowIndex);
         // Configure the component with the specified value
         
         if (colIndex == 1) {
            if (item_.allowed.length == 0) {
               if (item_.hasRange) {
                  if (item_.isInt)
                     slider_.setLimits((int)item_.lowerLimit, (int)item_.upperLimit);
                  else
                     slider_.setLimits(item_.lowerLimit, item_.upperLimit);
                  slider_.setText((String)value);
                  return slider_;
               } else {
                  text_.setText((String)value);
                  return text_;
               }
            }
         
            ActionListener[] l = combo_.getActionListeners();
            for (int i=0; i<l.length; i++)
               combo_.removeActionListener(l[i]);
            combo_.removeAllItems();
            for (int i=0; i<item_.allowed.length; i++){
               combo_.addItem(item_.allowed[i]);
            }
            combo_.setSelectedItem(item_.value);
                                   
            return combo_;
         } else if (colIndex == 2) {
            check_.setSelected(item_.confInclude);
            return check_;
         }
         return null;
      }
      
      // This method is called when editing is completed.
      // It must return the new value to be stored in the cell.
      public Object getCellEditorValue() {
         if (editingCol_ == 1) {
            if (item_.allowed.length == 0) {
               if (item_.hasRange)
                  return slider_.getText();
               else
                  return text_.getText();
            } else {
               return combo_.getSelectedItem();
            }
         } else if (editingCol_ == 2)
            return new Boolean(check_.isSelected());
         
         return null;
      }
   }
   
   public class PropertyCellRenderer implements TableCellRenderer {
      // This method is called each time a cell in a column
      // using this renderer needs to be rendered.
      PropertyItem item_;
      
     
      public Component getTableCellRendererComponent(JTable table, Object value,
            boolean isSelected, boolean hasFocus, int rowIndex, int colIndex) {
         
         PropertyTableData data = (PropertyTableData)table.getModel();
         item_ = data.getPropertyItem(rowIndex);
         
         JLabel lab = new JLabel();
         //lab.setOpaque(true);
         lab.setHorizontalAlignment(JLabel.LEFT);
//         if (isSelected) {
//            lab.setBackground(Color.LIGHT_GRAY);
//         } else {
//            lab.setBackground(Color.WHITE);
//         }
         
         if (hasFocus) {
            // this cell is the anchor and the table has the focus
         }
         
         Component comp;
         
         if (colIndex == 0) {
            lab.setText((String)value + (item_.orgSel ? "*" : ""));
            comp = lab;
         } else if (colIndex == 1) {
            if (item_.hasRange) {
               SliderPanel slider = new SliderPanel();
               slider.setLimits(item_.lowerLimit, item_.upperLimit);
               slider.setText((String)value);
               slider.setToolTipText((String)value);
               comp = slider;
            } else {
               lab.setText(item_.value.toString());
               comp = lab;
            }
         } else if (colIndex == 2) {
            JCheckBox cb = new JCheckBox();
            cb.setSelected(item_.confInclude);
            if (item_.readOnly)
               cb.setEnabled(false);
            comp = cb;
         } else {
            lab.setText("Undefined");
            comp = lab;
         }
      
         return comp;
      }
      
      // The following methods override the defaults for performance reasons
      public void validate() {}
      public void revalidate() {}
      protected void firePropertyChange(String propertyName, Object oldValue, Object newValue) {}
      public void firePropertyChange(String propertyName, boolean oldValue, boolean newValue) {}
      public PropertyCellRenderer() {
         super();
         //setFont(new Font("Arial", Font.PLAIN, 10));
      }
   }

   public boolean isChanged() {
      return changed_.booleanValue();
   }
}
