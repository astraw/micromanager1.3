///////////////////////////////////////////////////////////////////////////////
//FILE:          MMStudioMainFrame.java
//PROJECT:       Micro-Manager
//SUBSYSTEM:     mmstudio
//-----------------------------------------------------------------------------

//AUTHOR:       Nenad Amodaj, nenad@amodaj.com, Jul 18, 2005

//COPYRIGHT:    University of California, San Francisco, 2006
//100X Imaging Inc, www.100ximaging.com, 2008

//LICENSE:      This file is distributed under the BSD license.
//License text is included with the source distribution.

//This file is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty
//of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

//IN NO EVENT SHALL THE COPYRIGHT OWNER OR
//CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
//INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES.

//CVS:          $Id$

package org.micromanager;

import ij.IJ;
import ij.ImageJ;
import ij.ImagePlus;
import ij.WindowManager;
import ij.gui.ImageCanvas;
import ij.gui.ImageWindow;
import ij.gui.Line;
import ij.gui.Roi;
import ij.measure.Calibration;
import ij.process.ByteProcessor;
import ij.process.ColorProcessor;
import ij.process.ImageProcessor;
import ij.process.ImageStatistics;
import ij.process.ShortProcessor;

import java.awt.Color;
import java.awt.Dimension;
import java.awt.Font;
import java.awt.Insets;
import java.awt.Point;
import java.awt.Rectangle;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.event.FocusAdapter;
import java.awt.event.FocusEvent;
import java.awt.event.WindowAdapter;
import java.awt.event.WindowEvent;
import java.awt.event.WindowListener;
import java.awt.geom.Point2D;
import java.awt.image.ColorModel;
import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.logging.Level;
import java.util.prefs.Preferences;

import javax.swing.JButton;
import javax.swing.JCheckBox;
import javax.swing.JCheckBoxMenuItem;
import javax.swing.JComboBox;
import javax.swing.JFileChooser;
import javax.swing.JFrame;
import javax.swing.JLabel;
import javax.swing.JMenu;
import javax.swing.JMenuBar;
import javax.swing.JMenuItem;
import javax.swing.JOptionPane;
import javax.swing.JTextField;
import javax.swing.JToggleButton;
import javax.swing.SpringLayout;
import javax.swing.SwingConstants;
import javax.swing.SwingUtilities;
import javax.swing.Timer;
import javax.swing.UIManager;
import javax.swing.border.LineBorder;

import mmcorej.CMMCore;
import mmcorej.DeviceType;
import mmcorej.MMCoreJ;
import mmcorej.MMEventCallback;
import mmcorej.Metadata;
import mmcorej.MetadataSingleTag;
import mmcorej.MetadataTag;
import mmcorej.StrVector;

import org.micromanager.acquisition.AcquisitionManager;
import org.micromanager.acquisition.MMAcquisition;
import org.micromanager.acquisition.MMAcquisitionSnap;
import org.micromanager.api.AcquisitionEngine;
import org.micromanager.api.Autofocus;
import org.micromanager.api.DeviceControlGUI;
import org.micromanager.api.MMPlugin;
import org.micromanager.api.ScriptInterface;
import org.micromanager.conf.ConfiguratorDlg;
import org.micromanager.conf.MMConfigFileException;
import org.micromanager.conf.MicroscopeModel;
import org.micromanager.graph.ContrastPanel;
import org.micromanager.graph.GraphData;
import org.micromanager.graph.GraphFrame;
import org.micromanager.image5d.ChannelCalibration;
import org.micromanager.image5d.ChannelControl;
import org.micromanager.image5d.ChannelDisplayProperties;
import org.micromanager.image5d.Crop_Image5D;
import org.micromanager.image5d.Duplicate_Image5D;
import org.micromanager.image5d.Image5D;
import org.micromanager.image5d.Image5DWindow;
import org.micromanager.image5d.Image5D_Channels_to_Stacks;
import org.micromanager.image5d.Image5D_Stack_to_RGB;
import org.micromanager.image5d.Image5D_Stack_to_RGB_t;
import org.micromanager.image5d.Image5D_to_Stack;
import org.micromanager.image5d.Image5D_to_VolumeViewer;
import org.micromanager.image5d.Make_Montage;
import org.micromanager.image5d.Split_Image5D;
import org.micromanager.image5d.Z_Project;
import org.micromanager.metadata.AcquisitionData;
import org.micromanager.metadata.DisplaySettings;
import org.micromanager.metadata.MMAcqDataException;
import org.micromanager.metadata.SummaryKeys;
import org.micromanager.metadata.WellAcquisitionData;
import org.micromanager.navigation.CenterAndDragListener;
import org.micromanager.navigation.PositionList;
import org.micromanager.navigation.ZWheelListener;
import org.micromanager.navigation.XYZKeyListener;
import org.micromanager.utils.CfgFileFilter;
import org.micromanager.utils.ContrastSettings;
import org.micromanager.utils.GUIColors;
import org.micromanager.utils.GUIUtils;
import org.micromanager.utils.LargeMessageDlg;
import org.micromanager.utils.MMImageWindow;
import org.micromanager.utils.MMLogger;
import org.micromanager.utils.MMScriptException;
import org.micromanager.utils.MMSnapshotWindow;
import org.micromanager.utils.ProgressBar;
import org.micromanager.utils.TextUtils;
import org.micromanager.utils.WaitDialog;

import bsh.EvalError;
import bsh.Interpreter;

import com.swtdesigner.SwingResourceManager;

/*
 * Main panel and application class for the MMStudio.
 */
public class MMStudioMainFrame extends JFrame implements DeviceControlGUI,
		ScriptInterface {

	private static final String MICRO_MANAGER_TITLE = "Micro-Manager 1.3";
	private static final String VERSION = "1.3.20 (beta)";
	private static final long serialVersionUID = 3556500289598574541L;

	private static final String MAIN_FRAME_X = "x";
	private static final String MAIN_FRAME_Y = "y";
	private static final String MAIN_FRAME_WIDTH = "width";
	private static final String MAIN_FRAME_HEIGHT = "height";

	private static final String MAIN_EXPOSURE = "exposure";
	private static final String SYSTEM_CONFIG_FILE = "sysconfig_file";
	private static final String MAIN_STRETCH_CONTRAST = "stretch_contrast";

	private static final String CONTRAST_SETTINGS_8_MIN = "contrast8_MIN";
	private static final String CONTRAST_SETTINGS_8_MAX = "contrast8_MAX";
	private static final String CONTRAST_SETTINGS_16_MIN = "contrast16_MIN";
	private static final String CONTRAST_SETTINGS_16_MAX = "contrast16_MAX";
	private static final String OPEN_ACQ_DIR = "openDataDir";

	private static final String SCRIPT_CORE_OBJECT = "mmc";
	private static final String SCRIPT_ACQENG_OBJECT = "acq";
	private static final String SCRIPT_GUI_OBJECT = "gui";

	// GUI components
	// private JTextField textFieldGain_;
	private JComboBox comboBinning_;
	private JComboBox shutterComboBox_;
	private JTextField textFieldExp_;
	private SpringLayout springLayout_;
	private JLabel labelImageDimensions_;
	private JToggleButton toggleButtonLive_;
	private JCheckBox autoShutterCheckBox_;
	private boolean autoShutterOrg_;
	private boolean shutterOrg_;
	private MMOptions options_;
	private boolean runsAsPlugin_;

	private JButton buttonSnap_;
	private JToggleButton toggleButtonShutter_;

	private GUIColors guiColors_;
	private ColorModel currentColorModel_;

	private GraphFrame profileWin_;
	private PropertyEditor propertyBrowser_;
	private CalibrationListDlg calibrationListDlg_;
	private AcqControlDlg acqControlWin_;
	private ArrayList<PluginItem> plugins_;

	private final static String DEFAULT_CONFIG_FILE_NAME = "MMConfig_demo.cfg";

	private String sysConfigFile_;
	private String startupScriptFile_;
	private String sysStateFile_ = "MMSystemState.cfg";

	private ConfigGroupPad configPad_;
	private ContrastPanel contrastPanel_;

	// Timer interval - image display interval
	private double interval_ = 40;
	private Timer timer_;
	private GraphData lineProfileData_;
   private Object img_;

	// labels for standard devices
	private String cameraLabel_;
	private String zStageLabel_;
	private String shutterLabel_;
	private String xyStageLabel_;

	// applications settings
	private Preferences mainPrefs_;

	// MMcore
	private CMMCore core_;
	private AcquisitionEngine engine_;
	private PositionList posList_;
	private PositionListDlg posListDlg_;
	private String openAcqDirectory_ = "";
	private boolean running_;
	private boolean liveRunning_ = false;
	private boolean configChanged_ = false;
	private StrVector shutters_ = null;
	private JButton saveConfigButton_;
	private FastAcqDlg fastAcqWin_;
	private ScriptPanel scriptPanel_;
	private SplitView splitView_;
	private CenterAndDragListener centerAndDragListener_;
	private ZWheelListener zWheelListener_;
	private XYZKeyListener xyzKeyListener_;
	private AcquisitionManager acqMgr_;
	private static MMImageWindow imageWin_;
	private int snapCount_ = -1;
	private double lastImageTimeMs_=0;
	private boolean liveModeSuspended_;
	
	public static MMImageWindow getLiveWin() {
		return imageWin_;
	}

	// Callback
	private CoreEventCallback cb_;

	private JMenuBar menuBar_;

	/**
	 * Callback to update GUI when a change happens in the MMCore.
	 */
	public class CoreEventCallback extends MMEventCallback {

		public CoreEventCallback() {
			super();
		}

		public void onPropertiesChanged() {
			updateGUI(true);
			if (propertyBrowser_ != null)
				propertyBrowser_.updateStatus();
			MMLogger.getLogger().info("Notification from MMCore!");
		}
	}

	private class PluginItem {
		public String menuItem = "undefined";
		public MMPlugin plugin = null;
	}

	/**
	 * Main procedure for stand alone operation.
	 */
	public static void main(String args[]) {
		try {
			UIManager.setLookAndFeel(UIManager
					.getCrossPlatformLookAndFeelClassName());
			MMStudioMainFrame frame = new MMStudioMainFrame(false);
			frame.setVisible(true);
			frame.setDefaultCloseOperation(JFrame.DO_NOTHING_ON_CLOSE);
		} catch (Exception e) {
			e.printStackTrace();
			System.exit(1);
		}
	}

	public MMStudioMainFrame(boolean pluginStatus) {
		super();

		options_ = new MMOptions();
		options_.loadSettings();

		guiColors_ = new GUIColors();

		plugins_ = new ArrayList<PluginItem>();

		runsAsPlugin_ = pluginStatus;
		setIconImage(SwingResourceManager.getImage(MMStudioMainFrame.class,
				"icons/microscope.gif"));
		running_ = true;
		// !!! contrastSettings8_ = new ContrastSettings();
		// contrastSettings16_ = new ContrastSettings();

		acqMgr_ = new AcquisitionManager();

		sysConfigFile_ = new String(System.getProperty("user.dir") + "/"
				+ DEFAULT_CONFIG_FILE_NAME);
		startupScriptFile_ = new String(System.getProperty("user.dir") + "/"
				+ options_.startupScript);
		// set the location for app preferences
		mainPrefs_ = Preferences.userNodeForPackage(this.getClass());

		// show registration dialog if not already registered
		// first check user preferences (for legacy compatibility reasons)
		boolean userReg = mainPrefs_.getBoolean(RegistrationDlg.REGISTRATION,
				false);
		if (!userReg) {

			// now check system preferences
			Preferences systemPrefs = Preferences.systemNodeForPackage(this
					.getClass());
			boolean systemReg = systemPrefs.getBoolean(
					RegistrationDlg.REGISTRATION, false);
			if (!systemReg) {
				// prompt for registration info
				RegistrationDlg dlg = new RegistrationDlg();
				dlg.setVisible(true);
			}
		}

		// initialize timer
		ActionListener timerHandler = new ActionListener() {
			public void actionPerformed(ActionEvent evt) {
				if (!isImageWindowOpen()) {
					// stop live acquisition if user closed the window
					enableLiveMode(false);
					return;
				}
				if(!isNewImageAvailable())
				{
					return;
				}
				try {
               Object img;
					// ToDo: implement color and multi-channel camera support
					// use core_.getLastImage()
					long channels = core_.getNumberOfChannels();
					if (channels == 1)
						img = core_.getLastImage();
					else {
						img = core_.getRGB32Image();
					}
               if (img != img_) {
                  img_ = img;
                  displayImage(img_);
               }
				} catch (Exception e) {
					e.printStackTrace();
					return;
				}
			}
		};

		timer_ = new Timer((int) interval_, timerHandler);
		timer_.stop();

		// load application preferences
		// NOTE: only window size and position preferences are loaded,
		// not the settings for the camera and live imaging -
		// attempting to set those automatically on startup may cause problems
		// with the hardware
		int x = mainPrefs_.getInt(MAIN_FRAME_X, 100);
		int y = mainPrefs_.getInt(MAIN_FRAME_Y, 100);
		boolean stretch = mainPrefs_.getBoolean(MAIN_STRETCH_CONTRAST, true);

		ContrastSettings s8 = new ContrastSettings(mainPrefs_.getDouble(
				CONTRAST_SETTINGS_8_MIN, 0.0), mainPrefs_.getDouble(
				CONTRAST_SETTINGS_8_MAX, 0.0));
		ContrastSettings s16 = new ContrastSettings(mainPrefs_.getDouble(
				CONTRAST_SETTINGS_16_MIN, 0.0), mainPrefs_.getDouble(
				CONTRAST_SETTINGS_16_MAX, 0.0));
		MMImageWindow.setContrastSettings(s8, s16);

		openAcqDirectory_ = mainPrefs_.get(OPEN_ACQ_DIR, "");

		setBounds(x, y, 580, 451);
		setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
		springLayout_ = new SpringLayout();
		getContentPane().setLayout(springLayout_);
		setTitle(MICRO_MANAGER_TITLE);
		setBackground(guiColors_.background.get((options_.displayBackground)));

		// Snap button
		// -----------
		buttonSnap_ = new JButton();
		buttonSnap_.setIconTextGap(6);
		buttonSnap_.setText("Snap");
		buttonSnap_.setIcon(SwingResourceManager.getIcon(
				MMStudioMainFrame.class, "/org/micromanager/icons/camera.png"));
		buttonSnap_.setFont(new Font("", Font.PLAIN, 10));
		buttonSnap_.setToolTipText("Snap single image");
		buttonSnap_.setMaximumSize(new Dimension(0, 0));
		buttonSnap_.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent e) {
				doSnap();
			}
		});
		getContentPane().add(buttonSnap_);
		springLayout_.putConstraint(SpringLayout.SOUTH, buttonSnap_, 25,
				SpringLayout.NORTH, getContentPane());
		springLayout_.putConstraint(SpringLayout.NORTH, buttonSnap_, 4,
				SpringLayout.NORTH, getContentPane());
		springLayout_.putConstraint(SpringLayout.EAST, buttonSnap_, 95,
				SpringLayout.WEST, getContentPane());
		springLayout_.putConstraint(SpringLayout.WEST, buttonSnap_, 7,
				SpringLayout.WEST, getContentPane());

		// Initialize
		// ----------

		// Exposure field
		// ---------------
		final JLabel label_1 = new JLabel();
		label_1.setFont(new Font("Arial", Font.PLAIN, 10));
		label_1.setText("Exposure [ms]");
		getContentPane().add(label_1);
		springLayout_.putConstraint(SpringLayout.EAST, label_1, 198,
				SpringLayout.WEST, getContentPane());
		springLayout_.putConstraint(SpringLayout.WEST, label_1, 111,
				SpringLayout.WEST, getContentPane());
		springLayout_.putConstraint(SpringLayout.SOUTH, label_1, 39,
				SpringLayout.NORTH, getContentPane());
		springLayout_.putConstraint(SpringLayout.NORTH, label_1, 23,
				SpringLayout.NORTH, getContentPane());

		textFieldExp_ = new JTextField();
		textFieldExp_.addFocusListener(new FocusAdapter() {
			public void focusLost(FocusEvent fe) {
            setExposure();
			}
		});
		textFieldExp_.setFont(new Font("Arial", Font.PLAIN, 10));
		textFieldExp_.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent e) {
            setExposure();
			}
		});
		getContentPane().add(textFieldExp_);
		springLayout_.putConstraint(SpringLayout.SOUTH, textFieldExp_, 40,
				SpringLayout.NORTH, getContentPane());
		springLayout_.putConstraint(SpringLayout.NORTH, textFieldExp_, 21,
				SpringLayout.NORTH, getContentPane());
		springLayout_.putConstraint(SpringLayout.EAST, textFieldExp_, 275,
				SpringLayout.WEST, getContentPane());
		springLayout_.putConstraint(SpringLayout.WEST, textFieldExp_, 203,
				SpringLayout.WEST, getContentPane());

		// Live button
		// -----------
		toggleButtonLive_ = new JToggleButton();
		toggleButtonLive_.setMargin(new Insets(2, 2, 2, 2));
		toggleButtonLive_.setIconTextGap(1);
		toggleButtonLive_.setIcon(SwingResourceManager.getIcon(
				MMStudioMainFrame.class,
				"/org/micromanager/icons/camera_go.png"));
		toggleButtonLive_.setIconTextGap(6);
		toggleButtonLive_.setToolTipText("Continuous live view");
		toggleButtonLive_.setFont(new Font("Arial", Font.PLAIN, 10));
		toggleButtonLive_.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent e) {
				if (!IsLiveModeOn()) {
               // Display interval for Live Mode changes as well
               interval_ = 33.0;
               try {
                  if (core_.getExposure() > 33.0)
                     interval_ = core_.getExposure();
               } catch (Exception ex) {
                  handleException(ex);
               }

               timer_.setDelay((int)interval_);
				}
				enableLiveMode(!IsLiveModeOn());
			}
		});

		toggleButtonLive_.setText("Live");
		getContentPane().add(toggleButtonLive_);
		springLayout_.putConstraint(SpringLayout.SOUTH, toggleButtonLive_, 47,
				SpringLayout.NORTH, getContentPane());
		springLayout_.putConstraint(SpringLayout.NORTH, toggleButtonLive_, 26,
				SpringLayout.NORTH, getContentPane());
		springLayout_.putConstraint(SpringLayout.EAST, toggleButtonLive_, 95,
				SpringLayout.WEST, getContentPane());
		springLayout_.putConstraint(SpringLayout.WEST, toggleButtonLive_, 7,
				SpringLayout.WEST, getContentPane());

		// Acquire button
		// -----------
		JButton acquireButton = new JButton();
		acquireButton.setMargin(new Insets(2, 2, 2, 2));
		acquireButton.setIconTextGap(1);
		acquireButton.setIcon(SwingResourceManager.getIcon(
				MMStudioMainFrame.class,
				"/org/micromanager/icons/snapAppend.png"));
		acquireButton.setIconTextGap(6);
		acquireButton.setToolTipText("Acquire single frame");
		acquireButton.setFont(new Font("Arial", Font.PLAIN, 10));
		acquireButton.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent e) {
				snapAndAddToImage5D(null);
			}			
		});

		acquireButton.setText("Acquire");
		getContentPane().add(acquireButton);
		springLayout_.putConstraint(SpringLayout.SOUTH, acquireButton, 69,
				SpringLayout.NORTH, getContentPane());
		springLayout_.putConstraint(SpringLayout.NORTH, acquireButton, 48,
				SpringLayout.NORTH, getContentPane());
		springLayout_.putConstraint(SpringLayout.EAST, acquireButton, 95,
				SpringLayout.WEST, getContentPane());
		springLayout_.putConstraint(SpringLayout.WEST, acquireButton, 7,
				SpringLayout.WEST, getContentPane());

		// Shutter button
		// --------------

		toggleButtonShutter_ = new JToggleButton();
		toggleButtonShutter_.addActionListener(new ActionListener() {
			public void actionPerformed(final ActionEvent e) {
				try {
					if (toggleButtonShutter_.isSelected()) {
						setShutterButton(true);
						core_.setShutterOpen(true);
					} else {
						core_.setShutterOpen(false);
						setShutterButton(false);
					}
				} catch (Exception e1) {
					// TODO Auto-generated catch block
					e1.printStackTrace();
				}
			}
		});
		toggleButtonShutter_.setToolTipText("Open/close the shutter");
		toggleButtonShutter_.setIconTextGap(6);
		toggleButtonShutter_.setFont(new Font("Arial", Font.BOLD, 10));
		toggleButtonShutter_.setText("Open");
		getContentPane().add(toggleButtonShutter_);
		springLayout_.putConstraint(SpringLayout.EAST, toggleButtonShutter_,
				275, SpringLayout.WEST, getContentPane());
		springLayout_.putConstraint(SpringLayout.WEST, toggleButtonShutter_,
				203, SpringLayout.WEST, getContentPane());
		springLayout_.putConstraint(SpringLayout.SOUTH, toggleButtonShutter_,
				138 - 21, SpringLayout.NORTH, getContentPane());
		springLayout_.putConstraint(SpringLayout.NORTH, toggleButtonShutter_,
				117 - 21, SpringLayout.NORTH, getContentPane());

		// Active shutter label
		final JLabel activeShutterLabel = new JLabel();
		activeShutterLabel.setFont(new Font("Arial", Font.PLAIN, 10));
		activeShutterLabel.setText("Shutter");
		getContentPane().add(activeShutterLabel);
		springLayout_.putConstraint(SpringLayout.SOUTH, activeShutterLabel,
				108 - 22, SpringLayout.NORTH, getContentPane());
		springLayout_.putConstraint(SpringLayout.NORTH, activeShutterLabel,
				95 - 22, SpringLayout.NORTH, getContentPane());
		springLayout_.putConstraint(SpringLayout.EAST, activeShutterLabel,
				160 - 2, SpringLayout.WEST, getContentPane());
		springLayout_.putConstraint(SpringLayout.WEST, activeShutterLabel,
				113 - 2, SpringLayout.WEST, getContentPane());

		// Active shutter Combo Box
		shutterComboBox_ = new JComboBox();
		shutterComboBox_.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent arg0) {
				try {
					if (shutterComboBox_.getSelectedItem() != null)
						core_.setShutterDevice((String) shutterComboBox_
								.getSelectedItem());
				} catch (Exception e) {
					handleException(e);
				}
				return;
			}
		});
		getContentPane().add(shutterComboBox_);
		springLayout_.putConstraint(SpringLayout.SOUTH, shutterComboBox_,
				114 - 22, SpringLayout.NORTH, getContentPane());
		springLayout_.putConstraint(SpringLayout.NORTH, shutterComboBox_,
				92 - 22, SpringLayout.NORTH, getContentPane());
		springLayout_.putConstraint(SpringLayout.EAST, shutterComboBox_, 275,
				SpringLayout.WEST, getContentPane());
		springLayout_.putConstraint(SpringLayout.WEST, shutterComboBox_, 170,
				SpringLayout.WEST, getContentPane());

		menuBar_ = new JMenuBar();
		setJMenuBar(menuBar_);

		final JMenu fileMenu = new JMenu();
		fileMenu.setText("File");
		menuBar_.add(fileMenu);

		final JMenuItem openMenuItem = new JMenuItem();
		openMenuItem.addActionListener(new ActionListener() {
			public void actionPerformed(final ActionEvent e) {
				openAcquisitionData();
			}
		});
		openMenuItem.setText("Open Acquisition Data as Image5D......");
		fileMenu.add(openMenuItem);

		fileMenu.addSeparator();

		final JMenuItem loadState = new JMenuItem();
		loadState.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent e) {
				loadSystemState();
			}
		});
		loadState.setText("Load System State...");
		fileMenu.add(loadState);

		final JMenuItem saveStateAs = new JMenuItem();
		fileMenu.add(saveStateAs);
		saveStateAs.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent e) {
				saveSystemState();
			}
		});
		saveStateAs.setText("Save System State As...");

		fileMenu.addSeparator();

		final JMenuItem exitMenuItem = new JMenuItem();
		exitMenuItem.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent e) {
				closeSequence();
			}
		});
		fileMenu.add(exitMenuItem);
		exitMenuItem.setText("Exit");

		final JMenu image5dMenu = new JMenu();
		image5dMenu.setText("Image5D");
		menuBar_.add(image5dMenu);

		final JMenuItem closeAllMenuItem = new JMenuItem();
		closeAllMenuItem.addActionListener(new ActionListener() {
			public void actionPerformed(final ActionEvent e) {
				WindowManager.closeAllWindows();
			}
		});
		closeAllMenuItem.setText("Close All");
		image5dMenu.add(closeAllMenuItem);

		final JMenuItem duplicateMenuItem = new JMenuItem();
		duplicateMenuItem.addActionListener(new ActionListener() {
			public void actionPerformed(final ActionEvent e) {
				Duplicate_Image5D duplicate = new Duplicate_Image5D();
				duplicate.run("");
			}
		});
		duplicateMenuItem.setText("Duplicate");
		image5dMenu.add(duplicateMenuItem);

		final JMenuItem cropMenuItem = new JMenuItem();
		cropMenuItem.addActionListener(new ActionListener() {
			public void actionPerformed(final ActionEvent e) {
				Crop_Image5D crop = new Crop_Image5D();
				crop.run("");
			}
		});
		cropMenuItem.setText("Crop");
		image5dMenu.add(cropMenuItem);

		final JMenuItem makeMontageMenuItem = new JMenuItem();
		makeMontageMenuItem.addActionListener(new ActionListener() {
			public void actionPerformed(final ActionEvent e) {
				Make_Montage makeMontage = new Make_Montage();
				makeMontage.run("");
			}
		});
		makeMontageMenuItem.setText("Make Montage");
		image5dMenu.add(makeMontageMenuItem);

		final JMenuItem zProjectMenuItem = new JMenuItem();
		zProjectMenuItem.addActionListener(new ActionListener() {
			public void actionPerformed(final ActionEvent e) {
				// IJ.runPlugIn("Z_Project", "");
				Z_Project projection = new Z_Project();
				projection.run("");
			}
		});
		zProjectMenuItem.setText("Z Project");
		image5dMenu.add(zProjectMenuItem);

		final JMenuItem convertToRgbMenuItem = new JMenuItem();
		convertToRgbMenuItem.addActionListener(new ActionListener() {
			public void actionPerformed(final ActionEvent e) {
				// IJ.runPlugIn("org/micromanager/Image5D_Stack_to_RGB", "");
				Image5D_Stack_to_RGB stackToRGB = new Image5D_Stack_to_RGB();
				stackToRGB.run("");
			}
		});
		convertToRgbMenuItem.setText("Copy to RGB Stack(z)");
		image5dMenu.add(convertToRgbMenuItem);

		final JMenuItem convertToRgbtMenuItem = new JMenuItem();
		convertToRgbtMenuItem.addActionListener(new ActionListener() {
			public void actionPerformed(final ActionEvent e) {
				// IJ.runPlugIn("Image5D_Stack_to_RGB_t", "");
				Image5D_Stack_to_RGB_t stackToRGB_t = new Image5D_Stack_to_RGB_t();
				stackToRGB_t.run("");

			}
		});
		convertToRgbtMenuItem.setText("Copy to RGB Stack(t)");
		image5dMenu.add(convertToRgbtMenuItem);

		final JMenuItem convertToStackMenuItem = new JMenuItem();
		convertToStackMenuItem.addActionListener(new ActionListener() {
			public void actionPerformed(final ActionEvent e) {
				// IJ.runPlugIn("Image5D_to_Stack", "");
				Image5D_to_Stack image5DToStack = new Image5D_to_Stack();
				image5DToStack.run("");
			}
		});
		convertToStackMenuItem.setText("Copy to Stack");
		image5dMenu.add(convertToStackMenuItem);

		final JMenuItem convertToStacksMenuItem = new JMenuItem();
		convertToStacksMenuItem.addActionListener(new ActionListener() {
			public void actionPerformed(final ActionEvent e) {
				// IJ.runPlugIn("Image5D_to_Stack", "");
				Image5D_Channels_to_Stacks image5DToStacks = new Image5D_Channels_to_Stacks();
				image5DToStacks.run("");
			}
		});
		convertToStacksMenuItem.setText("Copy to Stacks (channels)");
		image5dMenu.add(convertToStacksMenuItem);

		final JMenuItem volumeViewerMenuItem = new JMenuItem();
		volumeViewerMenuItem.addActionListener(new ActionListener() {
			public void actionPerformed(final ActionEvent e) {
				Image5D_to_VolumeViewer volumeViewer = new Image5D_to_VolumeViewer();
				volumeViewer.run("");
			}
		});
		volumeViewerMenuItem.setText("VolumeViewer");
		image5dMenu.add(volumeViewerMenuItem);

		final JMenuItem splitImageMenuItem = new JMenuItem();
		splitImageMenuItem.addActionListener(new ActionListener() {
			public void actionPerformed(final ActionEvent e) {
				Split_Image5D splitImage = new Split_Image5D();
				splitImage.run("");
			}
		});
		splitImageMenuItem.setText("SplitView");
		image5dMenu.add(splitImageMenuItem);

		final JMenu toolsMenu = new JMenu();
		toolsMenu.setText("Tools");
		menuBar_.add(toolsMenu);

		final JMenuItem refreshMenuItem = new JMenuItem();
		refreshMenuItem.setIcon(SwingResourceManager.getIcon(
				MMStudioMainFrame.class, "icons/arrow_refresh.png"));
		refreshMenuItem.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent e) {
				updateGUI(true);
			}
		});
		refreshMenuItem.setText("Refresh GUI");
		toolsMenu.add(refreshMenuItem);

		final JMenuItem rebuildGuiMenuItem = new JMenuItem();
		rebuildGuiMenuItem.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent e) {
				initializeGUI();
			}
		});
		rebuildGuiMenuItem.setText("Rebuild GUI");
		toolsMenu.add(rebuildGuiMenuItem);

		toolsMenu.addSeparator();

		// final JMenuItem scriptingConsoleMenuItem = new JMenuItem();
		// toolsMenu.add(scriptingConsoleMenuItem);
		// scriptingConsoleMenuItem.addActionListener(new ActionListener() {
		// public void actionPerformed(ActionEvent e) {
		// createScriptingConsole();
		// }
		// });
		// scriptingConsoleMenuItem.setText("Scripting Console...");

		final JMenuItem scriptPanelMenuItem = new JMenuItem();
		toolsMenu.add(scriptPanelMenuItem);
		scriptPanelMenuItem.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent e) {
				createScriptPanel();

			}
		});
		scriptPanelMenuItem.setText("Script Panel");

		final JMenuItem propertyEditorMenuItem = new JMenuItem();
		toolsMenu.add(propertyEditorMenuItem);
		propertyEditorMenuItem.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent e) {
				createPropertyEditor();
			}
		});
		propertyEditorMenuItem.setText("Device/Property Browser...");

		toolsMenu.addSeparator();

		final JMenuItem xyListMenuItem = new JMenuItem();
		xyListMenuItem.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent arg0) {
				showXYPositionList();
			}
		});
		xyListMenuItem.setIcon(SwingResourceManager.getIcon(
				MMStudioMainFrame.class, "icons/application_view_list.png"));
		xyListMenuItem.setText("XY List...");
		toolsMenu.add(xyListMenuItem);

		final JMenuItem acquisitionMenuItem = new JMenuItem();
		acquisitionMenuItem.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent e) {
				openAcqControlDialog();
			}
		});
		acquisitionMenuItem.setIcon(SwingResourceManager.getIcon(
				MMStudioMainFrame.class, "icons/film.png"));
		acquisitionMenuItem.setText("Acquisition...");
		toolsMenu.add(acquisitionMenuItem);

		final JMenuItem sequenceMenuItem = new JMenuItem();
		sequenceMenuItem.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent e) {
				openSequenceDialog();
			}
		});
		sequenceMenuItem.setText("Burst Acquisition...");
		toolsMenu.add(sequenceMenuItem);

		final JMenuItem splitViewMenuItem = new JMenuItem();
		splitViewMenuItem.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent e) {
				splitViewDialog();
			}
		});
		splitViewMenuItem.setText("Split View...");
		toolsMenu.add(splitViewMenuItem);

		final JCheckBoxMenuItem centerAndDragMenuItem = new JCheckBoxMenuItem();

		centerAndDragMenuItem.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent e) {
				if (centerAndDragListener_ == null)
					centerAndDragListener_ = new CenterAndDragListener(core_);
				if (!centerAndDragListener_.isRunning()) {
					centerAndDragListener_.start();
					centerAndDragMenuItem.setSelected(true);
				} else {
					centerAndDragListener_.stop();
					centerAndDragMenuItem.setSelected(false);
				}
			}
		});
		centerAndDragMenuItem.setText("Mouse Moves Stage");
		centerAndDragMenuItem.setSelected(false);
		toolsMenu.add(centerAndDragMenuItem);

		toolsMenu.addSeparator();

		final JMenuItem configuratorMenuItem = new JMenuItem();
		configuratorMenuItem.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent arg0) {
				try {
					// unload all devices before starting configurator

					// NS: Save config presets if they were changed.
					if (configChanged_) {
						Object[] options = { "Yes", "No" };
						int n = JOptionPane.showOptionDialog(null,
								"Save Changed Configuration?", "Micro-Manager",
								JOptionPane.YES_NO_OPTION,
								JOptionPane.QUESTION_MESSAGE, null, options,
								options[0]);
						if (n == JOptionPane.YES_OPTION)
							saveConfigPresets();
						configChanged_ = false;
					}
					core_.reset();

					// run Configurator
					ConfiguratorDlg configurator = new ConfiguratorDlg(core_,
							sysConfigFile_);
					configurator.setVisible(true);

					// re-initialize the system with the new configuration file
					sysConfigFile_ = configurator.getFileName();
					mainPrefs_.put(SYSTEM_CONFIG_FILE, sysConfigFile_);
					loadSystemConfiguration();
					initializeGUI();
				} catch (Exception e) {
					handleException(e);
					return;
				}
			}
		});
		configuratorMenuItem.setText("Hardware Configuration Wizard...");
		toolsMenu.add(configuratorMenuItem);

		final JMenuItem calibrationMenuItem = new JMenuItem();
		toolsMenu.add(calibrationMenuItem);
		calibrationMenuItem.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent e) {
				createCalibrationListDlg();
			}
		});
		calibrationMenuItem.setText("Pixel Size Calibration...");
		toolsMenu.add(calibrationMenuItem);

		final JMenuItem loadSystemConfigMenuItem = new JMenuItem();
		toolsMenu.add(loadSystemConfigMenuItem);
		loadSystemConfigMenuItem.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent e) {
				loadConfiguration();
				updateTitle();
				initializeGUI();
			}
		});
		loadSystemConfigMenuItem.setText("Load Hardware Configuration...");

		final JMenuItem saveConfigurationPresetsMenuItem = new JMenuItem();
		saveConfigurationPresetsMenuItem
				.addActionListener(new ActionListener() {
					public void actionPerformed(ActionEvent arg0) {
						saveConfigPresets();
						updateChannelCombos();
					}
				});
		saveConfigurationPresetsMenuItem.setText("Save Configuration Presets");
		toolsMenu.add(saveConfigurationPresetsMenuItem);

		final JMenuItem optionsMenuItem = new JMenuItem();
		final MMStudioMainFrame thisInstance = this;
		optionsMenuItem.addActionListener(new ActionListener() {
			public void actionPerformed(final ActionEvent e) {
				int oldBufsize = options_.circularBufferSizeMB;
				OptionsDlg dlg = new OptionsDlg(options_, core_, mainPrefs_,
						thisInstance);
				dlg.setVisible(true);
				// adjust memory footprint if necessary
				if (oldBufsize != options_.circularBufferSizeMB)
					try {
						core_
								.setCircularBufferMemoryFootprint(options_.circularBufferSizeMB);
					} catch (Exception exc) {
						handleException(exc);
					}
			}
		});
		optionsMenuItem.setText("Options...");
		toolsMenu.add(optionsMenuItem);

		final JLabel binningLabel = new JLabel();
		binningLabel.setFont(new Font("Arial", Font.PLAIN, 10));
		binningLabel.setText("Binning");
		getContentPane().add(binningLabel);
		springLayout_.putConstraint(SpringLayout.SOUTH, binningLabel, 64,
				SpringLayout.NORTH, getContentPane());
		springLayout_.putConstraint(SpringLayout.NORTH, binningLabel, 43,
				SpringLayout.NORTH, getContentPane());
		springLayout_.putConstraint(SpringLayout.EAST, binningLabel, 200 - 1,
				SpringLayout.WEST, getContentPane());
		springLayout_.putConstraint(SpringLayout.WEST, binningLabel, 112 - 1,
				SpringLayout.WEST, getContentPane());

		labelImageDimensions_ = new JLabel();
		labelImageDimensions_.setFont(new Font("Arial", Font.PLAIN, 10));
		getContentPane().add(labelImageDimensions_);
		springLayout_.putConstraint(SpringLayout.SOUTH, labelImageDimensions_,
				-5, SpringLayout.SOUTH, getContentPane());
		springLayout_.putConstraint(SpringLayout.NORTH, labelImageDimensions_,
				-25, SpringLayout.SOUTH, getContentPane());
		springLayout_.putConstraint(SpringLayout.EAST, labelImageDimensions_,
				-5, SpringLayout.EAST, getContentPane());
		springLayout_.putConstraint(SpringLayout.WEST, labelImageDimensions_,
				5, SpringLayout.WEST, getContentPane());

		comboBinning_ = new JComboBox();
		comboBinning_.setFont(new Font("Arial", Font.PLAIN, 10));
		comboBinning_.setMaximumRowCount(4);
		comboBinning_.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent e) {
				changeBinning();
			}
		});
		getContentPane().add(comboBinning_);
		springLayout_.putConstraint(SpringLayout.EAST, comboBinning_, 275,
				SpringLayout.WEST, getContentPane());
		springLayout_.putConstraint(SpringLayout.WEST, comboBinning_, 200,
				SpringLayout.WEST, getContentPane());
		springLayout_.putConstraint(SpringLayout.SOUTH, comboBinning_, 66,
				SpringLayout.NORTH, getContentPane());
		springLayout_.putConstraint(SpringLayout.NORTH, comboBinning_, 43,
				SpringLayout.NORTH, getContentPane());

		configPad_ = new ConfigGroupPad();
		configPad_.setGUI(this);
		// configPad_.setDisplayStyle(options_.displayBackground, guiColors_);
		configPad_.setFont(new Font("", Font.PLAIN, 10));
		getContentPane().add(configPad_);
		springLayout_.putConstraint(SpringLayout.EAST, configPad_, -4,
				SpringLayout.EAST, getContentPane());
		springLayout_.putConstraint(SpringLayout.WEST, configPad_, 5,
				SpringLayout.EAST, comboBinning_);

		final JLabel cameraSettingsLabel = new JLabel();
		cameraSettingsLabel.setFont(new Font("Arial", Font.BOLD, 11));
		cameraSettingsLabel.setText("Camera settings");
		getContentPane().add(cameraSettingsLabel);
		springLayout_.putConstraint(SpringLayout.EAST, cameraSettingsLabel,
				211, SpringLayout.WEST, getContentPane());
		springLayout_.putConstraint(SpringLayout.NORTH, cameraSettingsLabel, 6,
				SpringLayout.NORTH, getContentPane());
		springLayout_.putConstraint(SpringLayout.WEST, cameraSettingsLabel,
				109, SpringLayout.WEST, getContentPane());

		final JLabel stateDeviceLabel = new JLabel();
		stateDeviceLabel.setFont(new Font("Arial", Font.BOLD, 11));
		stateDeviceLabel.setText("Configuration Presets");
		getContentPane().add(stateDeviceLabel);
		springLayout_.putConstraint(SpringLayout.SOUTH, stateDeviceLabel, 21,
				SpringLayout.NORTH, getContentPane());
		springLayout_.putConstraint(SpringLayout.NORTH, stateDeviceLabel, 5,
				SpringLayout.NORTH, getContentPane());
		springLayout_.putConstraint(SpringLayout.EAST, stateDeviceLabel, 455,
				SpringLayout.WEST, getContentPane());
		springLayout_.putConstraint(SpringLayout.WEST, stateDeviceLabel, 305,
				SpringLayout.WEST, getContentPane());

		final JButton buttonAcqSetup = new JButton();
		buttonAcqSetup.setMargin(new Insets(2, 2, 2, 2));
		buttonAcqSetup.setIconTextGap(1);
		buttonAcqSetup.setIcon(SwingResourceManager.getIcon(
				MMStudioMainFrame.class, "/org/micromanager/icons/film.png"));
		buttonAcqSetup.setToolTipText("Open Acquistion dialog");
		buttonAcqSetup.setFont(new Font("Arial", Font.PLAIN, 10));
		buttonAcqSetup.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent e) {
				openAcqControlDialog();
			}
		});
		buttonAcqSetup.setText("Multi-D Acq.");
		getContentPane().add(buttonAcqSetup);
		springLayout_.putConstraint(SpringLayout.SOUTH, buttonAcqSetup, 91,
				SpringLayout.NORTH, getContentPane());
		springLayout_.putConstraint(SpringLayout.NORTH, buttonAcqSetup, 70,
				SpringLayout.NORTH, getContentPane());
		springLayout_.putConstraint(SpringLayout.EAST, buttonAcqSetup, 95,
				SpringLayout.WEST, getContentPane());
		springLayout_.putConstraint(SpringLayout.WEST, buttonAcqSetup, 7,
				SpringLayout.WEST, getContentPane());

		autoShutterCheckBox_ = new JCheckBox();
		autoShutterCheckBox_.setFont(new Font("Arial", Font.PLAIN, 10));
		autoShutterCheckBox_.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent e) {
				core_.setAutoShutter(autoShutterCheckBox_.isSelected());
				if (shutterLabel_.length() > 0)
					try {
						setShutterButton(core_.getShutterOpen());
					} catch (Exception e1) {
						// do not complain here
					}
				if (autoShutterCheckBox_.isSelected())
					toggleButtonShutter_.setEnabled(false);
				else
					toggleButtonShutter_.setEnabled(true);

			}
		});
		autoShutterCheckBox_.setIconTextGap(6);
		autoShutterCheckBox_.setHorizontalTextPosition(SwingConstants.LEADING);
		autoShutterCheckBox_.setText("Auto shutter");
		getContentPane().add(autoShutterCheckBox_);
		springLayout_.putConstraint(SpringLayout.EAST, autoShutterCheckBox_,
				202 - 3, SpringLayout.WEST, getContentPane());
		springLayout_.putConstraint(SpringLayout.WEST, autoShutterCheckBox_,
				110 - 3, SpringLayout.WEST, getContentPane());
		springLayout_.putConstraint(SpringLayout.SOUTH, autoShutterCheckBox_,
				141 - 22, SpringLayout.NORTH, getContentPane());
		springLayout_.putConstraint(SpringLayout.NORTH, autoShutterCheckBox_,
				118 - 22, SpringLayout.NORTH, getContentPane());

		final JButton burstButton = new JButton();
		burstButton.setMargin(new Insets(2, 2, 2, 2));
		burstButton.setIconTextGap(1);
		burstButton
				.setIcon(SwingResourceManager.getIcon(MMStudioMainFrame.class,
						"/org/micromanager/icons/film_go.png"));
		burstButton.setFont(new Font("Arial", Font.PLAIN, 10));
		burstButton
				.setToolTipText("Open Burst dialog for fast sequence acquisition");
		burstButton.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent e) {
				openSequenceDialog();
			}
		});
		burstButton.setText("Burst");
		getContentPane().add(burstButton);
		springLayout_.putConstraint(SpringLayout.SOUTH, burstButton, 113,
				SpringLayout.NORTH, getContentPane());
		springLayout_.putConstraint(SpringLayout.NORTH, burstButton, 92,
				SpringLayout.NORTH, getContentPane());
		springLayout_.putConstraint(SpringLayout.EAST, burstButton, 95,
				SpringLayout.WEST, getContentPane());
		springLayout_.putConstraint(SpringLayout.WEST, burstButton, 7,
				SpringLayout.WEST, getContentPane());

		final JButton refreshButton = new JButton();
		refreshButton.setMargin(new Insets(2, 2, 2, 2));
		refreshButton.setIconTextGap(1);
		refreshButton.setIcon(SwingResourceManager.getIcon(
				MMStudioMainFrame.class,
				"/org/micromanager/icons/arrow_refresh.png"));
		refreshButton.setFont(new Font("Arial", Font.PLAIN, 10));
		refreshButton
				.setToolTipText("Refresh all GUI controls directly from the hardware");
		refreshButton.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent e) {
				updateGUI(true);
			}
		});
		refreshButton.setText("Refresh");
		getContentPane().add(refreshButton);
		springLayout_.putConstraint(SpringLayout.SOUTH, refreshButton, 135,
				SpringLayout.NORTH, getContentPane());
		springLayout_.putConstraint(SpringLayout.NORTH, refreshButton, 114,
				SpringLayout.NORTH, getContentPane());
		springLayout_.putConstraint(SpringLayout.EAST, refreshButton, 95,
				SpringLayout.WEST, getContentPane());
		springLayout_.putConstraint(SpringLayout.WEST, refreshButton, 7,
				SpringLayout.WEST, getContentPane());

		// add window listeners
		addWindowListener(new WindowAdapter() {
			public void windowClosing(WindowEvent e) {
				running_ = false;
				closeSequence();
			}

			public void windowOpened(WindowEvent e) {
				// -------------------
				// initialize hardware
				// -------------------
				core_ = new CMMCore();
				core_.enableDebugLog(options_.debugLogEnabled);
				// core_.clearLog();
				cameraLabel_ = new String("");
				shutterLabel_ = new String("");
				zStageLabel_ = new String("");
				xyStageLabel_ = new String("");
				engine_ = new MMAcquisitionEngineMT();

				// register callback for MMCore notifications, this is a global
				// to avoid garbage collection
				cb_ = new CoreEventCallback();
				core_.registerCallback(cb_);

				try {
					core_
							.setCircularBufferMemoryFootprint(options_.circularBufferSizeMB);
				} catch (Exception exc) {
					handleException(exc);
				}

				engine_.setCore(core_);
				posList_ = new PositionList();
				engine_.setPositionList(posList_);
				MMStudioMainFrame parent = (MMStudioMainFrame) e.getWindow();
				if (parent != null)
					engine_.setParentGUI(parent);

				// load configuration from the file
				sysConfigFile_ = mainPrefs_.get(SYSTEM_CONFIG_FILE,
						sysConfigFile_);
				// startupScriptFile_ = mainPrefs_.get(STARTUP_SCRIPT_FILE,
				// startupScriptFile_);

				if (!options_.doNotAskForConfigFile) {
					MMIntroDlg introDlg = new MMIntroDlg(VERSION);
					introDlg.setConfigFile(sysConfigFile_);
					// introDlg.setScriptFile(startupScriptFile_);
					introDlg.setVisible(true);
					sysConfigFile_ = introDlg.getConfigFile();
				}
				// startupScriptFile_ = introDlg.getScriptFile();
				mainPrefs_.put(SYSTEM_CONFIG_FILE, sysConfigFile_);
				// mainPrefs_.put(STARTUP_SCRIPT_FILE, startupScriptFile_);

				paint(MMStudioMainFrame.this.getGraphics());

				// TODO: If there is an error loading the config file, make sure
				// we prompt next time at startup
				loadSystemConfiguration();
				executeStartupScript();

				configPad_.setCore(core_);
				if (parent != null)
					configPad_.setParentGUI(parent);

				// initialize controls
				initializeGUI();
				initializePluginMenu();

			}
		});

		final JButton setRoiButton = new JButton();
		setRoiButton.setIcon(SwingResourceManager.getIcon(
				MMStudioMainFrame.class,
				"/org/micromanager/icons/shape_handles.png"));
		setRoiButton.setFont(new Font("Arial", Font.PLAIN, 10));
		setRoiButton
				.setToolTipText("Set Region Of Interest to selected rectangle");
		setRoiButton.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent e) {
				setROI();
			}
		});
		getContentPane().add(setRoiButton);
		springLayout_.putConstraint(SpringLayout.EAST, setRoiButton, 48,
				SpringLayout.WEST, getContentPane());
		springLayout_.putConstraint(SpringLayout.WEST, setRoiButton, 7,
				SpringLayout.WEST, getContentPane());
		springLayout_.putConstraint(SpringLayout.SOUTH, setRoiButton, 174,
				SpringLayout.NORTH, getContentPane());
		springLayout_.putConstraint(SpringLayout.NORTH, setRoiButton, 154,
				SpringLayout.NORTH, getContentPane());

		final JButton clearRoiButton = new JButton();
		clearRoiButton.setIcon(SwingResourceManager.getIcon(
				MMStudioMainFrame.class,
				"/org/micromanager/icons/arrow_out.png"));
		clearRoiButton.setFont(new Font("Arial", Font.PLAIN, 10));
		clearRoiButton.setToolTipText("Reset Region of Interest to full frame");
		clearRoiButton.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent e) {
				clearROI();
			}
		});
		getContentPane().add(clearRoiButton);
		springLayout_.putConstraint(SpringLayout.EAST, clearRoiButton, 93,
				SpringLayout.WEST, getContentPane());
		springLayout_.putConstraint(SpringLayout.WEST, clearRoiButton, 51,
				SpringLayout.WEST, getContentPane());
		springLayout_.putConstraint(SpringLayout.SOUTH, clearRoiButton, 174,
				SpringLayout.NORTH, getContentPane());
		springLayout_.putConstraint(SpringLayout.NORTH, clearRoiButton, 154,
				SpringLayout.NORTH, getContentPane());

		final JLabel regionOfInterestLabel = new JLabel();
		regionOfInterestLabel.setFont(new Font("Arial", Font.BOLD, 11));
		regionOfInterestLabel.setText("ROI");
		getContentPane().add(regionOfInterestLabel);
		springLayout_.putConstraint(SpringLayout.SOUTH, regionOfInterestLabel,
				154, SpringLayout.NORTH, getContentPane());
		springLayout_.putConstraint(SpringLayout.NORTH, regionOfInterestLabel,
				140, SpringLayout.NORTH, getContentPane());
		springLayout_.putConstraint(SpringLayout.EAST, regionOfInterestLabel,
				71, SpringLayout.WEST, getContentPane());
		springLayout_.putConstraint(SpringLayout.WEST, regionOfInterestLabel,
				8, SpringLayout.WEST, getContentPane());

		/*
		 * final JLabel gainLabel = new JLabel(); gainLabel.setFont(new
		 * Font("Arial", Font.PLAIN, 10)); gainLabel.setText("Gain");
		 * getContentPane().add(gainLabel);
		 * springLayout_.putConstraint(SpringLayout.SOUTH, gainLabel, 108,
		 * SpringLayout.NORTH, getContentPane());
		 * springLayout_.putConstraint(SpringLayout.NORTH, gainLabel, 95,
		 * SpringLayout.NORTH, getContentPane());
		 * springLayout_.putConstraint(SpringLayout.EAST, gainLabel, 136,
		 * SpringLayout.WEST, getContentPane());
		 * springLayout_.putConstraint(SpringLayout.WEST, gainLabel, 113,
		 * SpringLayout.WEST, getContentPane());
		 * 
		 * textFieldGain_ = new JTextField(); textFieldGain_.setFont(new
		 * Font("Arial", Font.PLAIN, 10)); textFieldGain_.addActionListener(new
		 * ActionListener() { public void actionPerformed(ActionEvent e) { try {
		 * if (isCameraAvailable()) { core_.setProperty(core_.getCameraDevice(),
		 * MMCoreJ.getG_Keyword_Gain(), textFieldGain_.getText()); } } catch
		 * (Exception exp) { handleException(exp); } } });
		 * getContentPane().add(textFieldGain_);
		 * springLayout_.putConstraint(SpringLayout.SOUTH, textFieldGain_, 112,
		 * SpringLayout.NORTH, getContentPane());
		 * springLayout_.putConstraint(SpringLayout.NORTH, textFieldGain_, 93,
		 * SpringLayout.NORTH, getContentPane());
		 * springLayout_.putConstraint(SpringLayout.EAST, textFieldGain_, 275,
		 * SpringLayout.WEST, getContentPane());
		 * springLayout_.putConstraint(SpringLayout.WEST, textFieldGain_, 203,
		 * SpringLayout.WEST, getContentPane());
		 */
		contrastPanel_ = new ContrastPanel();
		contrastPanel_.setFont(new Font("", Font.PLAIN, 10));
		contrastPanel_.setContrastStretch(stretch);
		contrastPanel_.setBorder(new LineBorder(Color.black, 1, false));
		getContentPane().add(contrastPanel_);
		springLayout_.putConstraint(SpringLayout.SOUTH, contrastPanel_, -26,
				SpringLayout.SOUTH, getContentPane());
		springLayout_.putConstraint(SpringLayout.NORTH, contrastPanel_, 176,
				SpringLayout.NORTH, getContentPane());
		springLayout_.putConstraint(SpringLayout.EAST, contrastPanel_, -4,
				SpringLayout.EAST, getContentPane());
		springLayout_.putConstraint(SpringLayout.WEST, contrastPanel_, 7,
				SpringLayout.WEST, getContentPane());

		final JLabel regionOfInterestLabel_1 = new JLabel();
		regionOfInterestLabel_1.setFont(new Font("Arial", Font.BOLD, 11));
		regionOfInterestLabel_1.setText("Zoom");
		getContentPane().add(regionOfInterestLabel_1);
		springLayout_.putConstraint(SpringLayout.SOUTH,
				regionOfInterestLabel_1, 154, SpringLayout.NORTH,
				getContentPane());
		springLayout_.putConstraint(SpringLayout.NORTH,
				regionOfInterestLabel_1, 140, SpringLayout.NORTH,
				getContentPane());
		springLayout_.putConstraint(SpringLayout.EAST, regionOfInterestLabel_1,
				164, SpringLayout.WEST, getContentPane());
		springLayout_.putConstraint(SpringLayout.WEST, regionOfInterestLabel_1,
				101, SpringLayout.WEST, getContentPane());

		final JButton zoomInButton = new JButton();
		zoomInButton.addActionListener(new ActionListener() {
			public void actionPerformed(final ActionEvent e) {
				zoomIn();
			}
		});
		zoomInButton
				.setIcon(SwingResourceManager.getIcon(MMStudioMainFrame.class,
						"/org/micromanager/icons/zoom_in.png"));
		zoomInButton
				.setToolTipText("Set Region Of Interest to selected rectangle");
		zoomInButton.setFont(new Font("Arial", Font.PLAIN, 10));
		getContentPane().add(zoomInButton);
		springLayout_.putConstraint(SpringLayout.SOUTH, zoomInButton, 174,
				SpringLayout.NORTH, getContentPane());
		springLayout_.putConstraint(SpringLayout.NORTH, zoomInButton, 154,
				SpringLayout.NORTH, getContentPane());
		springLayout_.putConstraint(SpringLayout.EAST, zoomInButton, 141,
				SpringLayout.WEST, getContentPane());
		springLayout_.putConstraint(SpringLayout.WEST, zoomInButton, 100,
				SpringLayout.WEST, getContentPane());

		final JButton zoomOutButton = new JButton();
		zoomOutButton.addActionListener(new ActionListener() {
			public void actionPerformed(final ActionEvent e) {
				zoomOut();
			}
		});
		zoomOutButton.setIcon(SwingResourceManager
				.getIcon(MMStudioMainFrame.class,
						"/org/micromanager/icons/zoom_out.png"));
		zoomOutButton.setToolTipText("Reset Region of Interest to full frame");
		zoomOutButton.setFont(new Font("Arial", Font.PLAIN, 10));
		getContentPane().add(zoomOutButton);
		springLayout_.putConstraint(SpringLayout.SOUTH, zoomOutButton, 174,
				SpringLayout.NORTH, getContentPane());
		springLayout_.putConstraint(SpringLayout.NORTH, zoomOutButton, 154,
				SpringLayout.NORTH, getContentPane());
		springLayout_.putConstraint(SpringLayout.EAST, zoomOutButton, 186,
				SpringLayout.WEST, getContentPane());
		springLayout_.putConstraint(SpringLayout.WEST, zoomOutButton, 144,
				SpringLayout.WEST, getContentPane());

		// Profile
		// -------

		final JLabel profileLabel_ = new JLabel();
		profileLabel_.setFont(new Font("Arial", Font.BOLD, 11));
		profileLabel_.setText("Profile");
		getContentPane().add(profileLabel_);
		springLayout_.putConstraint(SpringLayout.SOUTH, profileLabel_, 154,
				SpringLayout.NORTH, getContentPane());
		springLayout_.putConstraint(SpringLayout.NORTH, profileLabel_, 140,
				SpringLayout.NORTH, getContentPane());
		springLayout_.putConstraint(SpringLayout.EAST, profileLabel_, 257,
				SpringLayout.WEST, getContentPane());
		springLayout_.putConstraint(SpringLayout.WEST, profileLabel_, 194,
				SpringLayout.WEST, getContentPane());

		final JButton buttonProf = new JButton();
		buttonProf.setIcon(SwingResourceManager.getIcon(
				MMStudioMainFrame.class,
				"/org/micromanager/icons/chart_curve.png"));
		buttonProf.setFont(new Font("Arial", Font.PLAIN, 10));
		buttonProf
				.setToolTipText("Open line profile window (requires line selection)");
		buttonProf.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent e) {
				openLineProfileWindow();
			}
		});
		// buttonProf.setText("Profile");
		getContentPane().add(buttonProf);
		springLayout_.putConstraint(SpringLayout.SOUTH, buttonProf, 174,
				SpringLayout.NORTH, getContentPane());
		springLayout_.putConstraint(SpringLayout.NORTH, buttonProf, 154,
				SpringLayout.NORTH, getContentPane());
		springLayout_.putConstraint(SpringLayout.EAST, buttonProf, 235,
				SpringLayout.WEST, getContentPane());
		springLayout_.putConstraint(SpringLayout.WEST, buttonProf, 193,
				SpringLayout.WEST, getContentPane());

		final JButton addGroupButton_ = new JButton();
		addGroupButton_.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent arg0) {
				if (configPad_.addGroup()) {
					configChanged_ = true;
					setConfigSaveButtonStatus(configChanged_);
				}
				updateGUI(true);
			}
		});
		addGroupButton_.setToolTipText("Add new group of presets");
		if (System.getProperty("os.name").indexOf("Mac OS X") != -1)
			addGroupButton_.setIcon(SwingResourceManager
					.getIcon(MMStudioMainFrame.class,
							"/org/micromanager/icons/plus.png"));
		else
			addGroupButton_.setText("+");
		getContentPane().add(addGroupButton_);
		springLayout_.putConstraint(SpringLayout.EAST, addGroupButton_, 337,
				SpringLayout.WEST, getContentPane());
		springLayout_.putConstraint(SpringLayout.WEST, addGroupButton_, 295,
				SpringLayout.WEST, getContentPane());
		springLayout_.putConstraint(SpringLayout.SOUTH, addGroupButton_, 173,
				SpringLayout.NORTH, getContentPane());
		springLayout_.putConstraint(SpringLayout.NORTH, addGroupButton_, 155,
				SpringLayout.NORTH, getContentPane());

		final JButton removeGroupButton_ = new JButton();
		removeGroupButton_.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent arg0) {
				if (configPad_.removeGroup()) {
					configChanged_ = true;
					updateGUI(true);
					setConfigSaveButtonStatus(configChanged_);
				}
			}
		});
		removeGroupButton_.setToolTipText("Remove selected group of presets");
		if (System.getProperty("os.name").indexOf("Mac OS X") != -1)
			removeGroupButton_.setIcon(SwingResourceManager.getIcon(
					MMStudioMainFrame.class,
					"/org/micromanager/icons/minus.png"));
		else
			removeGroupButton_.setText("-");
		getContentPane().add(removeGroupButton_);
		springLayout_.putConstraint(SpringLayout.SOUTH, removeGroupButton_,
				173, SpringLayout.NORTH, getContentPane());
		springLayout_.putConstraint(SpringLayout.NORTH, removeGroupButton_,
				155, SpringLayout.NORTH, getContentPane());
		springLayout_.putConstraint(SpringLayout.EAST, removeGroupButton_, 382,
				SpringLayout.WEST, getContentPane());
		springLayout_.putConstraint(SpringLayout.WEST, removeGroupButton_, 340,
				SpringLayout.WEST, getContentPane());

		final JButton editPreset_ = new JButton();
		editPreset_.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent arg0) {
				if (configPad_.editPreset()) {
					configChanged_ = true;
					updateGUI(true);
					setConfigSaveButtonStatus(configChanged_);
				}
			}
		});
		editPreset_.setToolTipText("Edit selected preset");
		editPreset_.setText("Edit");
		getContentPane().add(editPreset_);
		springLayout_.putConstraint(SpringLayout.EAST, editPreset_, -2,
				SpringLayout.EAST, getContentPane());
		springLayout_.putConstraint(SpringLayout.WEST, editPreset_, -72,
				SpringLayout.EAST, getContentPane());
		springLayout_.putConstraint(SpringLayout.SOUTH, configPad_, 0,
				SpringLayout.NORTH, editPreset_);
		springLayout_.putConstraint(SpringLayout.NORTH, configPad_, 21,
				SpringLayout.NORTH, getContentPane());
		springLayout_.putConstraint(SpringLayout.SOUTH, editPreset_, 173,
				SpringLayout.NORTH, getContentPane());
		springLayout_.putConstraint(SpringLayout.NORTH, editPreset_, 155,
				SpringLayout.NORTH, getContentPane());

		final JButton addPresetButton_ = new JButton();
		addPresetButton_.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent arg0) {
				if (configPad_.addPreset()) {
					configChanged_ = true;
					updateGUI(true);
					setConfigSaveButtonStatus(configChanged_);
				}
			}
		});
		addPresetButton_.setToolTipText("Add preset");
		if (System.getProperty("os.name").indexOf("Mac OS X") != -1)
			addPresetButton_.setIcon(SwingResourceManager
					.getIcon(MMStudioMainFrame.class,
							"/org/micromanager/icons/plus.png"));
		else
			addPresetButton_.setText("+");
		getContentPane().add(addPresetButton_);
		springLayout_.putConstraint(SpringLayout.EAST, addPresetButton_, -114,
				SpringLayout.EAST, getContentPane());
		springLayout_.putConstraint(SpringLayout.WEST, addPresetButton_, -156,
				SpringLayout.EAST, getContentPane());
		springLayout_.putConstraint(SpringLayout.SOUTH, addPresetButton_, 173,
				SpringLayout.NORTH, getContentPane());
		springLayout_.putConstraint(SpringLayout.NORTH, addPresetButton_, 155,
				SpringLayout.NORTH, getContentPane());

		final JButton removePresetButton_ = new JButton();
		removePresetButton_.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent arg0) {
				if (configPad_.removePreset()) {
					configChanged_ = true;
					updateGUI(true);
					setConfigSaveButtonStatus(configChanged_);
				}
			}
		});
		removePresetButton_.setToolTipText("Remove currently selected preset");
		if (System.getProperty("os.name").indexOf("Mac OS X") != -1)
			removePresetButton_.setIcon(SwingResourceManager.getIcon(
					MMStudioMainFrame.class,
					"/org/micromanager/icons/minus.png"));
		else
			removePresetButton_.setText("-");
		getContentPane().add(removePresetButton_);
		springLayout_.putConstraint(SpringLayout.EAST, removePresetButton_,
				-72, SpringLayout.EAST, getContentPane());
		springLayout_.putConstraint(SpringLayout.WEST, removePresetButton_,
				-114, SpringLayout.EAST, getContentPane());
		springLayout_.putConstraint(SpringLayout.SOUTH, removePresetButton_,
				173, SpringLayout.NORTH, getContentPane());
		springLayout_.putConstraint(SpringLayout.NORTH, removePresetButton_,
				155, SpringLayout.NORTH, getContentPane());

		saveConfigButton_ = new JButton();
		saveConfigButton_.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent arg0) {
				saveConfigPresets();
			}
		});
		saveConfigButton_
				.setToolTipText("Save current presets to the configuration file");
		saveConfigButton_.setText("Save");
		saveConfigButton_.setEnabled(false);
		getContentPane().add(saveConfigButton_);
		springLayout_.putConstraint(SpringLayout.SOUTH, saveConfigButton_, 20,
				SpringLayout.NORTH, getContentPane());
		springLayout_.putConstraint(SpringLayout.NORTH, saveConfigButton_, 2,
				SpringLayout.NORTH, getContentPane());
		springLayout_.putConstraint(SpringLayout.EAST, saveConfigButton_, 510,
				SpringLayout.WEST, getContentPane());
		springLayout_.putConstraint(SpringLayout.WEST, saveConfigButton_, 435,
				SpringLayout.WEST, getContentPane());

		/*
		 * final JButton xyListButton_ = new JButton();
		 * xyListButton_.setIcon(SwingResourceManager.getIcon(
		 * MMStudioMainFrame.class, "icons/application_view_list.png"));
		 * xyListButton_.addActionListener(new ActionListener() { public void
		 * actionPerformed(ActionEvent arg0) { showXYPositionList(); } });
		 * xyListButton_
		 * .setToolTipText("Refresh all GUI controls directly from the hardware"
		 * ); xyListButton_.setFont(new Font("Arial", Font.PLAIN, 10));
		 * xyListButton_.setText("XY List");
		 * getContentPane().add(xyListButton_);
		 * springLayout_.putConstraint(SpringLayout.EAST, xyListButton_, 95,
		 * SpringLayout.WEST, getContentPane());
		 * springLayout_.putConstraint(SpringLayout.WEST, xyListButton_, 7,
		 * SpringLayout.WEST, getContentPane());
		 * springLayout_.putConstraint(SpringLayout.SOUTH, xyListButton_, 135,
		 * SpringLayout.NORTH, getContentPane());
		 * springLayout_.putConstraint(SpringLayout.NORTH, xyListButton_, 114,
		 * SpringLayout.NORTH, getContentPane());
		 */
	}

	private void handleException(Exception e) {
		String errText = "";
		if (options_.debugLogEnabled)
			errText = "Exception occurred: " + e.getMessage();
		else {
			errText = "Exception occurred: " + e.toString() + "\n";
			e.printStackTrace();
		}
		handleError(errText);
	}

	private void handleError(String message) {
		if (IsLiveModeOn()) {
			// Should we always stop live mode on any error?
			enableLiveMode(false);
		}
		JOptionPane.showMessageDialog(this, message);
		MMLogger.getLogger().log(Level.SEVERE, message);
	}

   private void setExposure()  {
      try {
         core_.setExposure(Double.parseDouble(textFieldExp_
               .getText()));
         // Display interval for Live Mode changes as well
         interval_ = 33.0;
         if (core_.getExposure() > 33.0)
            interval_ = core_.getExposure();
         timer_.setDelay((int)interval_);
      } catch (Exception exp) {
         handleException(exp);
      }
   }

	private void updateTitle() {
		this.setTitle("System: " + sysConfigFile_);
	}

	public void updateHistogram() {
		// !!! ImagePlus imp = WindowManager.getCurrentImage();
		if (isImageWindowOpen()) {
			ImagePlus imp = imageWin_.getImagePlus();

			if (imp != null) {
				contrastPanel_.setContrastSettings(MMImageWindow
						.getContrastSettings8(), MMImageWindow
						.getContrastSettings16());
				contrastPanel_.update();
			}
		}
	}

	private void updateLineProfile() {
		if (!isImageWindowOpen() || profileWin_ == null
				|| !profileWin_.isShowing())
			return;

		calculateLineProfileData(imageWin_.getImagePlus());
		profileWin_.setData(lineProfileData_);
	}

	private void openLineProfileWindow() {
		if (imageWin_ == null || imageWin_.isClosed())
			return;
		calculateLineProfileData(imageWin_.getImagePlus());
		if (lineProfileData_ == null)
			return;
		profileWin_ = new GraphFrame();
		profileWin_.setDefaultCloseOperation(JFrame.DISPOSE_ON_CLOSE);
		profileWin_.setData(lineProfileData_);
		profileWin_.setAutoScale();
		profileWin_.setTitle("Live line profile");
		profileWin_.setBackground(guiColors_.background
				.get((options_.displayBackground)));
		profileWin_.setVisible(true);
	}

	private void calculateLineProfileData(ImagePlus imp) {
		// generate line profile
		Roi roi = imp.getRoi();
		if (roi == null || !roi.isLine()) {

			// if there is no line ROI, create one
			Rectangle r = imp.getProcessor().getRoi();
			int iWidth = r.width;
			int iHeight = r.height;
			int iXROI = r.x;
			int iYROI = r.y;
			if (roi == null) {
				iXROI += iWidth / 2;
				iYROI += iHeight / 2;
			}

			roi = new Line(iXROI - iWidth / 4, iYROI - iWidth / 4, iXROI
					+ iWidth / 4, iYROI + iHeight / 4);
			imp.setRoi(roi);
			roi = imp.getRoi();
		}

		ImageProcessor ip = imp.getProcessor();
		ip.setInterpolate(true);
		Line line = (Line) roi;

		if (lineProfileData_ == null)
			lineProfileData_ = new GraphData();
		lineProfileData_.setData(line.getPixels());
	}

	private void setROI() {
		ImagePlus curImage = WindowManager.getCurrentImage();
		if (curImage == null)
			return;

		Roi roi = curImage.getRoi();

		try {
			if (roi == null) {
				// if there is no ROI, create one
				Rectangle r = curImage.getProcessor().getRoi();
				int iWidth = r.width;
				int iHeight = r.height;
				int iXROI = r.x;
				int iYROI = r.y;
				if (roi == null) {
					iWidth /= 2;
					iHeight /= 2;
					iXROI += iWidth / 2;
					iYROI += iHeight / 2;
				}

				curImage.setRoi(iXROI, iYROI, iWidth, iHeight);
				roi = curImage.getRoi();
			}

			if (roi.getType() != Roi.RECTANGLE) {
				handleError("ROI must be a rectangle.\nUse the ImageJ rectangle tool to draw the ROI.");
				return;
			}

			Rectangle r = roi.getBoundingRect();
			// Stop (and restart) live mode if it is running
			boolean liveRunning = false;
			if (liveRunning_) {
				liveRunning = liveRunning_;
				enableLiveMode(false);
			}
			core_.setROI(r.x, r.y, r.width, r.height);
			updateStaticInfo();
			if (liveRunning)
				enableLiveMode(true);

		} catch (Exception e) {
			handleException(e);
		}
	}

	private void clearROI() {
		try {
			boolean liveRunning = false;
			if (liveRunning_) {
				liveRunning = liveRunning_;
				enableLiveMode(false);
			}
			core_.clearROI();
			updateStaticInfo();
			if (liveRunning)
				enableLiveMode(true);

		} catch (Exception e) {
			handleException(e);
		}
	}

	private MMImageWindow createImageWindow() {
		MMImageWindow win = imageWin_;
		imageWin_ = null;
		try {
			if (win != null) {
				win.saveAttributes();
				WindowManager.removeWindow(imageWin_);
				win.dispose();
				win = null;
			}
			win = new MMImageWindow(core_, this, contrastPanel_);

			MMLogger.getLogger().log(Level.INFO, "createImageWin1");

			win.setBackground(guiColors_.background
					.get((options_.displayBackground)));
			setIJCal(win);

			// listeners
			if (centerAndDragListener_ != null
					&& centerAndDragListener_.isRunning())
				centerAndDragListener_.attach(win.getImagePlus().getWindow());
			if (zWheelListener_ != null && zWheelListener_.isRunning())
				zWheelListener_.attach(win.getImagePlus().getWindow());
			if (xyzKeyListener_ != null && xyzKeyListener_.isRunning())
				xyzKeyListener_.attach(win.getImagePlus().getWindow());

			win.getCanvas().requestFocus();
			imageWin_ = win;

		} catch (Exception e) {
			if (win != null) {
				win.saveAttributes();
				WindowManager.removeWindow(win);
				win.dispose();
			}
			handleException(e);
		}
		return imageWin_;
	}

	/**
	 * Returns instance of the core uManager object;
	 */
	public CMMCore getMMCore() {
		return core_;
	}

	protected void saveConfigPresets() {
		MicroscopeModel model = new MicroscopeModel();
		try {
			model.loadFromFile(sysConfigFile_);
			model.createSetupConfigsFromHardware(core_);
			model.createResolutionsFromHardware(core_);
			JFileChooser fc = new JFileChooser();
			boolean saveFile = true;
			File f;
			do {
				fc.setSelectedFile(new File(model.getFileName()));
				int retVal = fc.showSaveDialog(this);
				if (retVal == JFileChooser.APPROVE_OPTION) {
					f = fc.getSelectedFile();

					// check if file already exists
					if (f.exists()) {
						int sel = JOptionPane.showConfirmDialog(this,
								"Overwrite " + f.getName(), "File Save",
								JOptionPane.YES_NO_OPTION);

						if (sel == JOptionPane.YES_OPTION)
							saveFile = true;
						else
							saveFile = false;
					}
				} else {
					return;
				}
			} while (saveFile == false);

			model.saveToFile(f.getAbsolutePath());
			sysConfigFile_ = f.getAbsolutePath();
			configChanged_ = false;
			setConfigSaveButtonStatus(configChanged_);
		} catch (MMConfigFileException e) {
			handleException(e);
		}
	}

	protected void setConfigSaveButtonStatus(boolean changed) {
		saveConfigButton_.setEnabled(changed);
	}

	/**
	 * Open an existing acquisition directory and build image5d window.
	 * 
	 */
	protected void openAcquisitionData() {

		// choose the directory
		// --------------------
		JFileChooser fc = new JFileChooser();
		fc.setFileSelectionMode(JFileChooser.FILES_AND_DIRECTORIES);
		fc.setSelectedFile(new File(openAcqDirectory_));
		int retVal = fc.showOpenDialog(this);
		if (retVal == JFileChooser.APPROVE_OPTION) {
			File f = fc.getSelectedFile();
			if (f.isDirectory()) {
				openAcqDirectory_ = f.getAbsolutePath();
			} else {
				openAcqDirectory_ = f.getParent();
			}

			ProgressBar progressBar = null;
			AcquisitionData ad = new AcquisitionData();
			try {

				// attempt to open metafile
				ad.load(openAcqDirectory_);

				// create image 5d
				Image5D img5d = new Image5D(openAcqDirectory_, ad
						.getImageJType(), ad.getImageWidth(), ad
						.getImageHeight(), ad.getNumberOfChannels(), ad
						.getNumberOfSlices(), ad.getNumberOfFrames(), false);

				img5d.setCalibration(ad.ijCal());

				Color colors[] = ad.getChannelColors();
				String names[] = ad.getChannelNames();
				if (colors != null && names != null)
					for (int i = 0; i < ad.getNumberOfChannels(); i++) {

						ChannelCalibration chcal = new ChannelCalibration();
						// set channel name
						chcal.setLabel(names[i]);
						img5d.setChannelCalibration(i + 1, chcal);

						// set color
						img5d.setChannelColorModel(i + 1,
								ChannelDisplayProperties
										.createModelFromColor(colors[i]));
					}

				progressBar = new ProgressBar("Opening File...", 0, ad
						.getNumberOfChannels()
						* ad.getNumberOfFrames() * ad.getNumberOfSlices());
				// set pixels
				int singleImageCounter = 0;
				for (int i = 0; i < ad.getNumberOfFrames(); i++) {
					for (int j = 0; j < ad.getNumberOfChannels(); j++) {
						for (int k = 0; k < ad.getNumberOfSlices(); k++) {
							img5d.setCurrentPosition(0, 0, j, k, i);
							// read the file

							// insert pixels into the 5d image
							Object img = ad.getPixels(i, j, k);
							if (img != null) {
								img5d.setPixels(img);

								// set display settings for channels
								if (k == 0 && i == 0) {
									DisplaySettings ds[] = ad
											.getChannelDisplaySettings();
									if (ds != null) {
										// display properties are recorded in
										// metadata use them...
										double min = ds[j].min;
										double max = ds[j].max;
										img5d.setChannelMinMax(j + 1, min, max);
									} else {
										// ...if not, autoscale channels based
										// on the first slice of the first frame
										ImageStatistics stats = img5d
												.getStatistics(); // get
										// uncalibrated
										// stats
										double min = stats.min;
										double max = stats.max;
										img5d.setChannelMinMax(j + 1, min, max);
									}
								}
							} else {
								// gap detected, let's try to fill in by using
								// the most recent channel data
								// NOTE: we assume that the gap is only in the
								// frame dimension
								// we don't know how to deal with z-slice gaps
								// !!!!
								// TODO: handle the case with Z-position gaps
								if (i > 0) {
									Object previousImg = img5d.getPixels(j + 1,
											k + 1, i);
									if (previousImg != null)
										img5d.setPixels(previousImg, j + 1,
												k + 1, i + 1);
								}
							}
						}
					}
					singleImageCounter++;
					progressBar.setProgress(singleImageCounter);
					progressBar.update(progressBar.getGraphics());
				}
				// pop-up 5d image window
				Image5DWindow i5dWin = new Image5DWindow(img5d);
				i5dWin.setBackground(guiColors_.background
						.get((options_.displayBackground)));
				if (ad.getNumberOfChannels() == 1)
					img5d.setDisplayMode(ChannelControl.ONE_CHANNEL_COLOR);
				else
					img5d.setDisplayMode(ChannelControl.OVERLAY);
				i5dWin.setAcquitionEngine(engine_);
				i5dWin.setAcquisitionData(ad);
				i5dWin.setAcqSavePath(openAcqDirectory_);
				i5dWin.addWindowListener(new WindowAdapter() {
					public void windowGainedFocus(WindowEvent e) {
						updateHistogram();
					}
				});

				i5dWin.addWindowListener(new WindowAdapter() {
					public void windowGainedFocus(WindowEvent e) {
						updateHistogram();
					}
				});
				i5dWin.addWindowListener(new WindowAdapter() {
					public void windowActivated(WindowEvent e) {
						updateHistogram();
					}
				});

				img5d.changes = false;
			} catch (MMAcqDataException e) {
				handleError(e.getMessage());
			} finally {
				if (progressBar != null) {
					progressBar.setVisible(false);
					progressBar = null;
				}
			}
		}
	}

	protected void zoomOut() {
		ImageWindow curWin = WindowManager.getCurrentWindow();
		if (curWin != null) {
			ImageCanvas canvas = curWin.getCanvas();
			Rectangle r = canvas.getBounds();
			canvas.zoomOut(r.width / 2, r.height / 2);
		}
	}

	protected void zoomIn() {
		ImageWindow curWin = WindowManager.getCurrentWindow();
		if (curWin != null) {
			ImageCanvas canvas = curWin.getCanvas();
			Rectangle r = canvas.getBounds();
			canvas.zoomIn(r.width / 2, r.height / 2);
		}
	}

	protected void changeBinning() {
		try {
			boolean liveRunning = false;
			if (liveRunning_) {
				liveRunning = liveRunning_;
				enableLiveMode(false);
			}

			if (isCameraAvailable()) {
				Object item = comboBinning_.getSelectedItem();
				if (item != null)
					core_.setProperty(cameraLabel_, MMCoreJ
							.getG_Keyword_Binning(), item.toString());
			}

			updateStaticInfo();

			if (liveRunning) {
				enableLiveMode(true);
			}

		} catch (Exception e) {
			handleException(e);
		}

	}

	private void createPropertyEditor() {
		if (propertyBrowser_ != null)
			propertyBrowser_.dispose();

		propertyBrowser_ = new PropertyEditor();
		propertyBrowser_.setGui(this);
		propertyBrowser_.setVisible(true);
		propertyBrowser_.setDefaultCloseOperation(JFrame.DISPOSE_ON_CLOSE);
		propertyBrowser_.setCore(core_);
		propertyBrowser_.setParentGUI(this);
	}

	private void createCalibrationListDlg() {
		if (calibrationListDlg_ != null)
			calibrationListDlg_.dispose();

		calibrationListDlg_ = new CalibrationListDlg(core_, options_);
		calibrationListDlg_.setVisible(true);
		calibrationListDlg_.setDefaultCloseOperation(JFrame.DISPOSE_ON_CLOSE);
		// calibrationListDlg_.setCore(core_);
		calibrationListDlg_.setParentGUI(this);
	}

	private void createScriptPanel() {
		if (scriptPanel_ == null) {
			scriptPanel_ = new ScriptPanel(core_, options_);
			scriptPanel_.insertScriptingObject(SCRIPT_CORE_OBJECT, core_);
			scriptPanel_.insertScriptingObject(SCRIPT_ACQENG_OBJECT, engine_);
			scriptPanel_.setParentGUI(this);
			scriptPanel_.setBackground(guiColors_.background
					.get((options_.displayBackground)));
		}
		scriptPanel_.setVisible(true);

	}

	private void updateStaticInfo() {
		try {
			double zPos = 0.0;
			String dimText = "Image size: " + core_.getImageWidth() + " X "
					+ core_.getImageHeight() + " X " + core_.getBytesPerPixel()
					+ ", Intensity range: " + core_.getImageBitDepth()
					+ " bits";
			double pixSizeUm = core_.getPixelSizeUm();
			if (pixSizeUm > 0.0)
				dimText += ", " + TextUtils.FMT0.format(pixSizeUm * 1000)
						+ "nm/pix";
			// dimText += ", " + TextUtils.FMT3.format(pixSizeUm) + "um/pix";
			else
				dimText += ", uncalibrated";
			if (zStageLabel_.length() > 0) {
				zPos = core_.getPosition(zStageLabel_);
				dimText += ", Z=" + TextUtils.FMT2.format(zPos) + "um";
			}
			if (xyStageLabel_.length() > 0) {
				double x[] = new double[1];
				double y[] = new double[1];
				core_.getXYPosition(xyStageLabel_, x, y);
				dimText += ", XY=(" + TextUtils.FMT2.format(x[0]) + ","
						+ TextUtils.FMT2.format(y[0]) + ")um";
			}

			labelImageDimensions_.setText(dimText);

		} catch (Exception e) {
			handleException(e);
		}
	}

	private void setShutterButton(boolean state) {
		if (state) {
			toggleButtonShutter_.setSelected(true);
			toggleButtonShutter_.setText("Close");
		} else {
			toggleButtonShutter_.setSelected(false);
			toggleButtonShutter_.setText("Open");
		}
	}

	// //////////////////////////////////////////////////////////////////////////
	// public interface available for scripting access
	// //////////////////////////////////////////////////////////////////////////

	public void snapSingleImage() {
		doSnap();
	}

	public Object getPixels() {
		if (imageWin_ != null)
			return imageWin_.getImagePlus().getProcessor().getPixels();

		return null;
	}

	public void setPixels(Object obj) {
		if (imageWin_ == null) {
			return;
		}
		imageWin_.getImagePlus().getProcessor().setPixels(obj);
	}

	public int getImageHeight() {
		if (imageWin_ != null)
			return imageWin_.getImagePlus().getHeight();
		return 0;
	}

	public int getImageWidth() {
		if (imageWin_ != null)
			return imageWin_.getImagePlus().getWidth();
		return 0;
	}

	public int getImageDepth() {
		if (imageWin_ != null)
			return imageWin_.getImagePlus().getBitDepth();
		return 0;
	}

	public ImageProcessor getImageProcessor() {
		if (imageWin_ == null)
			return null;
		return imageWin_.getImagePlus().getProcessor();
	}

	private boolean isCameraAvailable() {
		return cameraLabel_.length() > 0;
	}

	public boolean isImageWindowOpen() {
		boolean ret = imageWin_ != null;
		ret = ret && !imageWin_.isClosed();
		if (ret) {
			try {
				imageWin_.getGraphics().clearRect(0, 0, imageWin_.getWidth(),
						40);
				imageWin_.drawInfo(imageWin_.getGraphics());
			} catch (Exception e) {
				WindowManager.removeWindow(imageWin_);
				imageWin_.saveAttributes();
				imageWin_.dispose();
				imageWin_ = null;
				ret = false;
			}
		}
		return ret;
	}

	public void updateImageGUI() {
		updateHistogram();
	}

	boolean IsLiveModeOn() {
		return timer_ != null && timer_.isRunning();
	}

	public void enableLiveMode(boolean enable) {
		if (enable) {
			if (IsLiveModeOn())
				return;
			try {
				if (!isImageWindowOpen())
					imageWin_ = createImageWindow();

				// this is needed to clear the subtitle, should be folded into
				// drawInfo
				imageWin_.getGraphics().clearRect(0, 0, imageWin_.getWidth(),
						40);
				imageWin_.drawInfo(imageWin_.getGraphics());
				imageWin_.toFront();

				// turn off auto shutter and open the shutter
				autoShutterOrg_ = core_.getAutoShutter();
				if (shutterLabel_.length() > 0)
					shutterOrg_ = core_.getShutterOpen();
				core_.setAutoShutter(false);

				// Hide the autoShutter Checkbox
				autoShutterCheckBox_.setEnabled(false);

				shutterLabel_ = core_.getShutterDevice();
				// only open the shutter when we have one and the Auto shutter
				// checkbox was checked
				if ((shutterLabel_.length() > 0) && autoShutterOrg_)
					core_.setShutterOpen(true);
				// attach mouse wheel listener to control focus:
				if (zWheelListener_ == null)
					zWheelListener_ = new ZWheelListener(core_);
				zWheelListener_.start(imageWin_);

				// attach key listener to control the stage and focus:
				if (xyzKeyListener_ == null)
					xyzKeyListener_ = new XYZKeyListener(core_);
				xyzKeyListener_.start(imageWin_);

            // Do not display more often than dictated by the exposire time
            interval_ = 33.0;
            if (core_.getExposure() > 33.0)
               interval_ = core_.getExposure();
            timer_.setDelay((int)interval_);

				core_.startContinuousSequenceAcquisition(0.0);
				timer_.start();
				// Only hide the shutter checkbox if we are in autoshuttermode
				buttonSnap_.setEnabled(false);
				if (autoShutterOrg_)
					toggleButtonShutter_.setEnabled(false);
				liveRunning_ = true;
			} catch (Exception err) {
				JOptionPane.showMessageDialog(this,
						"Exception while enabling Live mode "
								+ err.getMessage());
				MMLogger.getLogger().log(Level.INFO, err.getMessage());
				if (imageWin_ != null) {
					imageWin_.saveAttributes();
					WindowManager.removeWindow(imageWin_);
					imageWin_.dispose();
					imageWin_ = null;
				}
			}
		} else {
			if (!IsLiveModeOn())
				return;
			try {
				timer_.stop();
				core_.stopSequenceAcquisition();

				if (zWheelListener_ != null)
					zWheelListener_.stop();
				if (xyzKeyListener_ != null)
					xyzKeyListener_.stop();

				// restore auto shutter and close the shutter
				if (shutterLabel_.length() > 0)
					core_.setShutterOpen(shutterOrg_);
				core_.setAutoShutter(autoShutterOrg_);
				if (autoShutterOrg_)
					toggleButtonShutter_.setEnabled(false);
				else
					toggleButtonShutter_.setEnabled(true);
				liveRunning_ = false;
				buttonSnap_.setEnabled(true);
				autoShutterCheckBox_.setEnabled(true);
				// This is here to avoid crashes when changing ROI in live mode
				// with Sensicam
				// Should be removed when underlying problem is dealt with
				Thread.sleep(100);
			} catch (Exception err) {
				JOptionPane.showMessageDialog(this,
						"Exception while disabling Live mode "
								+ err.getMessage());
				MMLogger.getLogger().log(Level.INFO, err.getMessage());
				if (imageWin_ != null) {
					WindowManager.removeWindow(imageWin_);
					imageWin_.dispose();
					imageWin_ = null;
				}
			}
		}
		toggleButtonLive_.setIcon(IsLiveModeOn() ? SwingResourceManager
				.getIcon(MMStudioMainFrame.class,
						"/org/micromanager/icons/cancel.png")
				: SwingResourceManager.getIcon(MMStudioMainFrame.class,
						"/org/micromanager/icons/camera_go.png"));
		toggleButtonLive_.setSelected(IsLiveModeOn());
		toggleButtonLive_.setText(IsLiveModeOn() ? "Stop Live" : "Live");

	}

	public boolean getLiveMode() {
		return liveRunning_;
	}

	public boolean updateImage() {
		try {
			if (!isImageWindowOpen()) {
				// stop live acquistion if the window is not open
				if (IsLiveModeOn()) {
					enableLiveMode(false);
					return true; // nothing to do
				}
			}

			long channels = core_.getNumberOfChannels();
			long bpp = core_.getBytesPerPixel();

			if (imageWin_.windowNeedsResizing()) {
				createImageWindow();
			}

			if (!isCurrentImageFormatSupported())
				return false;

			/*
			 * //!!! if (channels > 1) { if (channels != 4 && bpp != 1) {
			 * handleError("Unsupported image format."); return false; } }
			 */

			core_.snapImage();
			Object img;
			if (channels == 1)
				img = core_.getImage();
			else {
				img = core_.getRGB32Image();
			}

			imageWin_.newImage(img);
			updateLineProfile();
		} catch (Exception e) {
			handleException(e);
			return false;
		}

		return true;
	}

	public boolean displayImage(Object pixels) {
		try {
			if (!isImageWindowOpen()
					|| imageWin_.getImageWindowByteLength() != imageWin_
							.imageByteLenth(pixels)) {
				createImageWindow();
			}

			imageWin_.newImage(pixels);
			updateLineProfile();
		} catch (Exception e) {
			handleException(e);
			e.printStackTrace();
			return false;
		}

		return true;
	}

	private boolean isCurrentImageFormatSupported() {
		boolean ret = false;
		long channels = core_.getNumberOfChannels();
		long bpp = core_.getBytesPerPixel();

		if (channels > 1 && channels != 4 && bpp != 1) {
			handleError("Unsupported image format.");
		} else
			ret = true;
		return ret;
	}

	private void doSnap() {
		try {
			if (!isImageWindowOpen())
				//if (null == createImageWindow())
			//		handleError("Image window open failed");

					imageWin_ = createImageWindow();
          
			imageWin_.toFront();

			setIJCal(imageWin_);
			// this is needed to clear the subtite, should be folded into
			// drawInfo
			imageWin_.getGraphics().clearRect(0, 0, imageWin_.getWidth(), 40);
			imageWin_.drawInfo(imageWin_.getGraphics());

			String expStr = textFieldExp_.getText();
			if (expStr.length() > 0) {
				core_.setExposure(Double.parseDouble(expStr));
				updateImage();
			} else
				handleError("Exposure field is empty!");
		} catch (Exception e) {
			handleException(e);
		}
	}

	public void initializeGUI() {
		try {

			// establish device roles
			cameraLabel_ = core_.getCameraDevice();
			shutterLabel_ = core_.getShutterDevice();
			zStageLabel_ = core_.getFocusDevice();
			xyStageLabel_ = core_.getXYStageDevice();
			engine_.setZStageDevice(zStageLabel_);

			if (cameraLabel_.length() > 0) {
				ActionListener[] listeners;

				// binning combo
				if (comboBinning_.getItemCount() > 0)
					comboBinning_.removeAllItems();
				StrVector binSizes = core_.getAllowedPropertyValues(
						cameraLabel_, MMCoreJ.getG_Keyword_Binning());
				listeners = comboBinning_.getActionListeners();
				for (int i = 0; i < listeners.length; i++)
					comboBinning_.removeActionListener(listeners[i]);
				for (int i = 0; i < binSizes.size(); i++) {
					comboBinning_.addItem(binSizes.get(i));
				}

				comboBinning_.setMaximumRowCount((int) binSizes.size());
				if (binSizes.size() == 0) {
					comboBinning_.setEditable(true);
				} else {
					comboBinning_.setEditable(false);
				}

				for (int i = 0; i < listeners.length; i++)
					comboBinning_.addActionListener(listeners[i]);

			}

			// active shutter combo
			try {
				shutters_ = core_
						.getLoadedDevicesOfType(DeviceType.ShutterDevice);
			} catch (Exception e) {
				// System.println(DeviceType.ShutterDevice);
				e.printStackTrace();
				handleException(e);
			}

			if (shutters_ != null) {
				String items[] = new String[(int) shutters_.size()];
				// items[0] = "";
				for (int i = 0; i < shutters_.size(); i++)
					items[i] = shutters_.get(i);

				GUIUtils.replaceComboContents(shutterComboBox_, items);
				String activeShutter = core_.getShutterDevice();
				if (activeShutter != null)
					shutterComboBox_.setSelectedItem(activeShutter);
				else
					shutterComboBox_.setSelectedItem("");
			}

			updateGUI(true);
		} catch (Exception e) {
			handleException(e);
		}
	}

	private void initializePluginMenu() {
		// add plugin menu items
		if (plugins_.size() > 0) {
			final JMenu pluginMenu = new JMenu();
			pluginMenu.setText("Plugins");
			menuBar_.add(pluginMenu);

			for (int i = 0; i < plugins_.size(); i++) {
				final JMenuItem newMenuItem = new JMenuItem();
				newMenuItem.addActionListener(new ActionListener() {
					public void actionPerformed(final ActionEvent e) {
						System.out.println("Plugin command: "
								+ e.getActionCommand());
						// find the coresponding plugin
						for (int i = 0; i < plugins_.size(); i++)
							if (plugins_.get(i).menuItem.equals(e
									.getActionCommand())) {
								plugins_.get(i).plugin.show();
								break;
							}
						// if (plugins_.get(i).plugin == null) {
						// hcsPlateEditor_ = new
						// PlateEditor(MMStudioMainFrame.this);
						// }
						// hcsPlateEditor_.setVisible(true);
					}
				});
				newMenuItem.setText(plugins_.get(i).menuItem);
				pluginMenu.add(newMenuItem);
			}
		}

		// add help menu item
		final JMenu helpMenu = new JMenu();
		helpMenu.setText("Help");
		menuBar_.add(helpMenu);

		final JMenuItem aboutMenuItem = new JMenuItem();
		aboutMenuItem.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent e) {
				MMAboutDlg dlg = new MMAboutDlg();
				String versionInfo = "MM Studio version: " + VERSION;
				versionInfo += "\n" + core_.getVersionInfo();
				versionInfo += "\n" + core_.getAPIVersionInfo();
				versionInfo += "\nUser: " + core_.getUserId();
				versionInfo += "\nHost: " + core_.getHostName();

				dlg.setVersionInfo(versionInfo);
				dlg.setVisible(true);
			}
		});
		aboutMenuItem.setText("About...");
		helpMenu.add(aboutMenuItem);
	}

	public void updateGUI(boolean updateConfigPadStructure) {

		try {
			// establish device roles
			cameraLabel_ = core_.getCameraDevice();
			shutterLabel_ = core_.getShutterDevice();
			zStageLabel_ = core_.getFocusDevice();
			xyStageLabel_ = core_.getXYStageDevice();

			// camera settings
			if (isCameraAvailable()) {
				double exp = core_.getExposure();
				textFieldExp_.setText(Double.toString(exp));
				// textFieldGain_.setText(core_.getProperty(cameraLabel_,
				// MMCoreJ.getG_Keyword_Gain()));
				String binSize = core_.getProperty(cameraLabel_, MMCoreJ
						.getG_Keyword_Binning());
				GUIUtils.setComboSelection(comboBinning_, binSize);

				long bitDepth = 8;
				if (imageWin_ != null) {
					long hsz = imageWin_.getRawHistogramSize();
					bitDepth = (long) Math.log(hsz);
				}

				// bitDepth = core_.getImageBitDepth();
				// !!! contrastPanel_.setPixelBitDepth((int) bitDepth, false);
			}

			if (!timer_.isRunning()) {
				autoShutterCheckBox_.setSelected(core_.getAutoShutter());
				boolean shutterOpen = core_.getShutterOpen();
				setShutterButton(shutterOpen);
				if (autoShutterCheckBox_.isSelected()) {
					toggleButtonShutter_.setEnabled(false);
				} else {
					toggleButtonShutter_.setEnabled(true);
				}

				autoShutterOrg_ = core_.getAutoShutter();
			}

			// active shutter combo
			if (shutters_ != null) {
				String activeShutter = core_.getShutterDevice();
				if (activeShutter != null)
					shutterComboBox_.setSelectedItem(activeShutter);
				else
					shutterComboBox_.setSelectedItem("");
			}

			// state devices
			if (updateConfigPadStructure && (configPad_ != null))
				configPad_.refreshStructure();

			// update Channel menus in Multi-dimensional acquisition dialog
			updateChannelCombos();

		} catch (Exception e) {
			handleException(e);
		}

		updateStaticInfo();
		updateTitle();
	}

	public boolean okToAcquire() {
		return !timer_.isRunning();
	}

	public void stopAllActivity() {
		enableLiveMode(false);
	}

	public void refreshImage() {
		if (imageWin_ != null)
			imageWin_.getImagePlus().updateAndDraw();
	}

	private void cleanupOnClose() {
		// NS: Save config presets if they were changed.
		if (configChanged_) {
			Object[] options = { "Yes", "No" };
			int n = JOptionPane.showOptionDialog(null,
					"Save Changed Configuration?", "Micro-Manager",
					JOptionPane.YES_NO_OPTION, JOptionPane.QUESTION_MESSAGE,
					null, options, options[0]);
			if (n == JOptionPane.YES_OPTION)
				saveConfigPresets();
		}
		timer_.stop();
		if (imageWin_ != null) {
			imageWin_.close();
			imageWin_.dispose();
			imageWin_ = null;
		}

		if (profileWin_ != null)
			profileWin_.dispose();

		if (scriptPanel_ != null)
			scriptPanel_.closePanel();

		if (propertyBrowser_ != null)
			propertyBrowser_.dispose();

		if (acqControlWin_ != null) 
         acqControlWin_.close();

		if (engine_ != null)
			engine_.shutdown();

		// dispose plugins
		for (int i = 0; i < plugins_.size(); i++)
			plugins_.get(i).plugin.dispose();

		try {
			core_.reset();
		} catch (Exception err) {
			handleException(err);
		}
	}

	private void saveSettings() {
		Rectangle r = this.getBounds();

		mainPrefs_.putInt(MAIN_FRAME_X, r.x);
		mainPrefs_.putInt(MAIN_FRAME_Y, r.y);
		mainPrefs_.putInt(MAIN_FRAME_WIDTH, r.width);
		mainPrefs_.putInt(MAIN_FRAME_HEIGHT, r.height);

		mainPrefs_.putDouble(CONTRAST_SETTINGS_8_MIN, MMImageWindow
				.getContrastSettings8().min);
		mainPrefs_.putDouble(CONTRAST_SETTINGS_8_MAX, MMImageWindow
				.getContrastSettings8().max);
		mainPrefs_.putDouble(CONTRAST_SETTINGS_16_MIN, MMImageWindow
				.getContrastSettings16().min);
		mainPrefs_.putDouble(CONTRAST_SETTINGS_16_MAX, MMImageWindow
				.getContrastSettings16().max);
		mainPrefs_.putBoolean(MAIN_STRETCH_CONTRAST, contrastPanel_
				.isContrastStretch());

		mainPrefs_.put(OPEN_ACQ_DIR, openAcqDirectory_);

		// save field values from the main window
		// NOTE: automatically restoring these values on startup may cause
		// problems
		mainPrefs_.put(MAIN_EXPOSURE, textFieldExp_.getText());
		// NOTE: do not save auto shutter state
		// mainPrefs_.putBoolean(MAIN_AUTO_SHUTTER,
		// autoShutterCheckBox_.isSelected());
	}

	private void loadConfiguration() {
		JFileChooser fc = new JFileChooser();
		fc.addChoosableFileFilter(new CfgFileFilter());
		fc.setSelectedFile(new File(sysConfigFile_));
		int retVal = fc.showOpenDialog(this);
		if (retVal == JFileChooser.APPROVE_OPTION) {
			File f = fc.getSelectedFile();
			sysConfigFile_ = f.getAbsolutePath();
			configChanged_ = false;
			setConfigSaveButtonStatus(configChanged_);
			mainPrefs_.put(SYSTEM_CONFIG_FILE, sysConfigFile_);
			loadSystemConfiguration();
		}
	}

	private void loadSystemState() {
		JFileChooser fc = new JFileChooser();
		fc.addChoosableFileFilter(new CfgFileFilter());
		fc.setSelectedFile(new File(sysStateFile_));
		int retVal = fc.showOpenDialog(this);
		if (retVal == JFileChooser.APPROVE_OPTION) {
			File f = fc.getSelectedFile();
			sysStateFile_ = f.getAbsolutePath();
			try {
				// WaitDialog waitDlg = new
				// WaitDialog("Loading saved state, please wait...");
				// waitDlg.showDialog();
				core_.loadSystemState(sysStateFile_);
				// waitDlg.closeDialog();
				initializeGUI();
			} catch (Exception e) {
				handleException(e);
				return;
			}
		}
	}

	private void saveSystemState() {
		JFileChooser fc = new JFileChooser();
		boolean saveFile = true;
		File f;

		do {
			fc.setSelectedFile(new File(sysStateFile_));
			int retVal = fc.showSaveDialog(this);
			if (retVal == JFileChooser.APPROVE_OPTION) {
				f = fc.getSelectedFile();

				// check if file already exists
				if (f.exists()) {
					int sel = JOptionPane.showConfirmDialog(this, "Overwrite "
							+ f.getName(), "File Save",
							JOptionPane.YES_NO_OPTION);

					if (sel == JOptionPane.YES_OPTION)
						saveFile = true;
					else
						saveFile = false;
				}
			} else {
				return;
			}
		} while (saveFile == false);

		sysStateFile_ = f.getAbsolutePath();

		try {
			core_.saveSystemState(sysStateFile_);
		} catch (Exception e) {
			handleException(e);
			return;
		}
	}

	public void closeSequence() {
		if (engine_ != null && engine_.isAcquisitionRunning()) {
			int result = JOptionPane
					.showConfirmDialog(
							this,
							"Acquisition in progress. Are you sure you want to exit and discard all data?",
							"Micro-Manager", JOptionPane.YES_NO_OPTION,
							JOptionPane.INFORMATION_MESSAGE);

			if (result == JOptionPane.NO_OPTION)
				return;
		}

		stopAllActivity();

		cleanupOnClose();
		saveSettings();
		configPad_.saveSettings();
		options_.saveSettings();
		dispose();
		if (!runsAsPlugin_)
			System.exit(0);
		else {
			ImageJ ij = IJ.getInstance();
			if (ij != null)
				ij.quit();
		}

	}

	public void applyContrastSettings(ContrastSettings contrast8,
			ContrastSettings contrast16) {
		contrastPanel_.applyContrastSettings(contrast8, contrast16);
	}

	public ContrastSettings getContrastSettings() {
		// TODO Auto-generated method stub
		return null;
	}

	public boolean is16bit() {
		if (isImageWindowOpen()
				&& imageWin_.getImagePlus().getProcessor() instanceof ShortProcessor)
			return true;
		return false;
	}

	public boolean isRunning() {
		return running_;
	}

	/**
	 * Executes the beanShell script. This script instance only supports
	 * commands directed to the core object.
	 */
	private void executeStartupScript() {
		// execute startup script
		File f = new File(startupScriptFile_);

		if (startupScriptFile_.length() > 0 && f.exists()) {
			WaitDialog waitDlg = new WaitDialog(
					"Executing startup script, please wait...");
			waitDlg.showDialog();
			Interpreter interp = new Interpreter();
			try {
				// insert core object only
				interp.set(SCRIPT_CORE_OBJECT, core_);
				interp.set(SCRIPT_ACQENG_OBJECT, engine_);
				interp.set(SCRIPT_GUI_OBJECT, this);

				// read text file and evaluate
				interp.eval(TextUtils.readTextFile(startupScriptFile_));
			} catch (IOException exc) {
				handleException(exc);
			} catch (EvalError exc) {
				handleException(exc);
			} finally {
				waitDlg.closeDialog();
			}
		}
	}

	/**
	 * Loads sytem configuration from the cfg file.
	 */
	private void loadSystemConfiguration() {
		WaitDialog waitDlg = new WaitDialog(
				"Loading system configuration, please wait...");
		waitDlg.showDialog();
		try {

			if (sysConfigFile_.length() > 0) {
				// remember the selected file name
				core_.loadSystemConfiguration(sysConfigFile_);
				// waitDlg.closeDialog();
			}
		} catch (Exception err) {
			// handleException(err);
			// handle long error messages
			waitDlg.closeDialog();
			LargeMessageDlg dlg = new LargeMessageDlg(
					"Configuration error log", err.getMessage());
			dlg.setVisible(true);
		}
		waitDlg.closeDialog();
	}

	/**
	 * Opens Acquisition dialog.
	 */
	private void openAcqControlDialog() {
		try {
			if (acqControlWin_ == null) {
				acqControlWin_ = new AcqControlDlg(engine_, mainPrefs_, this);
			}
			if (acqControlWin_.isActive())
				acqControlWin_.setTopPosition();

			acqControlWin_.setVisible(true);

			// TODO: this call causes a strange exception the first time the
			// dialog is created
			// something to do with the order in which combo box creation is
			// performed

			// acqControlWin_.updateGroupsCombo();
		} catch (Exception exc) {
			exc.printStackTrace();
			handleError(exc.getMessage()
					+ "\nAcquistion window failed to open due to invalid or corrupted settings.\n"
					+ "Try resetting registry settings to factory defaults (Menu Tools|Options).");
		}
	}

	/**
	 * Opens streaming sequence acquisition dialog.
	 */
	protected void openSequenceDialog() {
		try {
			if (fastAcqWin_ == null) {
				fastAcqWin_ = new FastAcqDlg(core_, this);
			}
			fastAcqWin_.setVisible(true);
		} catch (Exception exc) {
			exc.printStackTrace();
			handleError(exc.getMessage()
					+ "\nSequence window failed to open due to internal error.");
		}
	}

	/**
	 * Opens Split View dialog.
	 */
	protected void splitViewDialog() {
		try {
			if (splitView_ == null) {
				splitView_ = new SplitView(core_, options_);
			}
			splitView_.setVisible(true);
		} catch (Exception exc) {
			exc.printStackTrace();
			handleError(exc.getMessage()
					+ "\nSplit View Window failed to open due to internal error.");
		}
	}

	/**
	 * /** Opens a dialog to record stage positions
	 */
	public void showXYPositionList() {
		if (posListDlg_ == null) {
			posListDlg_ = new PositionListDlg(core_, posList_, options_);
			posListDlg_.setBackground(guiColors_.background
					.get((options_.displayBackground)));
		}
		posListDlg_.setVisible(true);
	}

	private void updateChannelCombos() {
		if (this.acqControlWin_ != null)
			this.acqControlWin_.updateChannelAndGroupCombo();
	}

	public void setConfigChanged(boolean status) {
		configChanged_ = status;
		setConfigSaveButtonStatus(configChanged_);
	}

	/*
	 * Changes background color of this window
	 */
	public void setBackgroundStyle(String backgroundType) {
		setBackground(guiColors_.background.get((options_.displayBackground)));
		paint(MMStudioMainFrame.this.getGraphics());
		// configPad_.setDisplayStyle(options_.displayBackground, guiColors_);
		if (acqControlWin_ != null)
			acqControlWin_.setBackgroundStyle(options_.displayBackground);
		if (profileWin_ != null)
			profileWin_.setBackground(guiColors_.background
					.get((options_.displayBackground)));
		if (posListDlg_ != null)
			posListDlg_.setBackground(guiColors_.background
					.get((options_.displayBackground)));
		if (imageWin_ != null)
			imageWin_.setBackground(guiColors_.background
					.get((options_.displayBackground)));
		if (fastAcqWin_ != null)
			fastAcqWin_.setBackground(guiColors_.background
					.get((options_.displayBackground)));
		if (scriptPanel_ != null)
			scriptPanel_.setBackground(guiColors_.background
					.get((options_.displayBackground)));
		if (splitView_ != null)
			splitView_.setBackground(guiColors_.background
					.get((options_.displayBackground)));
	}

	public String getBackgroundStyle() {
		return options_.displayBackground;
	}

	// Set ImageJ pixel calibration
	private void setIJCal(MMImageWindow imageWin) {
		if (imageWin != null) {
			imageWin.setIJCal();
		}
	}

	// //////////////////////////////////////////////////////////////////////////
	// Scripting interface
	// //////////////////////////////////////////////////////////////////////////

	private class ExecuteAcq implements Runnable {

		public ExecuteAcq() {
		}

		public void run() {
			if (acqControlWin_ != null)
				acqControlWin_.runAcquisition();
		}
	}

	private class LoadAcq implements Runnable {
		private String filePath_;

		public LoadAcq(String path) {
			filePath_ = path;
		}

		public void run() {
			// stop current acquisition if any
			engine_.shutdown();

			// load protocol
			if (acqControlWin_ != null)
				acqControlWin_.loadAcqSettingsFromFile(filePath_);
		}
	}

	private class RefreshPositionList implements Runnable {

		public RefreshPositionList() {
		}

		public void run() {
			if (posListDlg_ != null) {
				posListDlg_.setPositionList(posList_);
				engine_.setPositionList(posList_);
			}
		}
	}

	private void testForAbortRequests() throws MMScriptException {
		if (scriptPanel_ != null) {
			if (scriptPanel_.stopRequestPending())
				throw new MMScriptException("Script interrupted by the user!");
		}

	}

	public void startBurstAcquisition() throws MMScriptException {
		testForAbortRequests();
		if (fastAcqWin_ != null) {
			fastAcqWin_.start();
		}
	}

	public void runBurstAcquisition() throws MMScriptException {
		testForAbortRequests();
		if (fastAcqWin_ == null)
			return;
		fastAcqWin_.start();
		try {
			while (fastAcqWin_.isBusy()) {
				Thread.sleep(20);
			}
		} catch (InterruptedException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
	}

	public boolean isBurstAcquisitionRunning() throws MMScriptException {
		testForAbortRequests();
		if (fastAcqWin_ != null)
			return fastAcqWin_.isBusy();
		else
			return false;
	}

	public void startAcquisition() throws MMScriptException {
		testForAbortRequests();
		SwingUtilities.invokeLater(new ExecuteAcq());
	}

	public void runAcquisition() throws MMScriptException {
		testForAbortRequests();
		if (acqControlWin_ != null) {
			acqControlWin_.runAcquisition();
			try {
				while (acqControlWin_.isAcquisitionRunning()) {
					Thread.sleep(50);
				}
			} catch (InterruptedException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}
		} else {
			throw new MMScriptException(
					"Acquisition window must be open for this command to work.");
		}
	}

	public void runAcqusition(String name, String root)
			throws MMScriptException {
		testForAbortRequests();
		if (acqControlWin_ != null) {
			acqControlWin_.runAcquisition(name, root);
			try {
				while (acqControlWin_.isAcquisitionRunning()) {
					Thread.sleep(100);
				}
			} catch (InterruptedException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}
		} else {
			throw new MMScriptException(
					"Acquisition window must be open for this command to work.");
		}
	}

	public void loadAcquisition(String path) throws MMScriptException {
		testForAbortRequests();
		SwingUtilities.invokeLater(new LoadAcq(path));
	}

	public void setPositionList(PositionList pl) throws MMScriptException {
		testForAbortRequests();
		// use serialization to clone the PositionList object
		posList_ = PositionList.newInstance(pl);
		SwingUtilities.invokeLater(new RefreshPositionList());
	}

	public PositionList getPositionList() throws MMScriptException {
		testForAbortRequests();
		// use serialization to clone the PositionList object
		return PositionList.newInstance(posList_);
	}

	public void sleep(long ms) throws MMScriptException {
		if (scriptPanel_ != null) {
			if (scriptPanel_.stopRequestPending())
				throw new MMScriptException("Script interrupted by the user!");
			scriptPanel_.sleep(ms);
		}
	}

	public void openAcquisition(String name, String rootDir, int nrFrames,
			int nrChannels, int nrSlices) throws MMScriptException {
		acqMgr_.openAcquisition(name, rootDir);
		MMAcquisition acq = acqMgr_.getAcquisition(name);
		acq.setDimensions(nrFrames, nrChannels, nrSlices);
	}

	public void openAcquisition(String name, String rootDir, int nrFrames,
			int nrChannels, int nrSlices, boolean show)
			throws MMScriptException {
		acqMgr_.openAcquisition(name, rootDir, show);
		MMAcquisition acq = acqMgr_.getAcquisition(name);
		acq.setDimensions(nrFrames, nrChannels, nrSlices);
	}

	private void openAcquisitionSnap(String name, String rootDir, boolean show)
			throws MMScriptException {
		MMAcquisition acq = acqMgr_.openAcquisitionSnap(name, rootDir, this,
				show);
		acq.setDimensions(0, 1, 1);
		try {
			// acq.getAcqData().setPixelSizeUm(core_.getPixelSizeUm());
			acq.setProperty(SummaryKeys.IMAGE_PIXEL_SIZE_UM, String
					.valueOf(core_.getPixelSizeUm()));

		} catch (Exception e) {
			handleException(e);
		}

	}

	public void initializeAcquisition(String name, int width, int height,
			int depth) throws MMScriptException {
		MMAcquisition acq = acqMgr_.getAcquisition(name);
		acq.setImagePhysicalDimensions(width, height, depth);
		acq.initialize();
	}

	public void closeAcquisition(String name) throws MMScriptException {
		acqMgr_.closeAcquisition(name);

	}

	public void closeAcquisitionImage5D(String title) throws MMScriptException {
		acqMgr_.closeImage5D(title);
	}

	public void loadBurstAcquisition(String path) {
		// TODO Auto-generated method stub

	}

	public void refreshGUI() {
		updateGUI(true);
	}

	public void setAcquisitionProperty(String acqName, String propertyName,
			String value) throws MMScriptException {
		MMAcquisition acq = acqMgr_.getAcquisition(acqName);
		acq.setProperty(propertyName, value);
	}

	public void setImageProperty(String acqName, int frame, int channel,
			int slice, String propName, String value) throws MMScriptException {
		MMAcquisition acq = acqMgr_.getAcquisition(acqName);
		acq.setProperty(frame, channel, slice, propName, value);
	}

	public void snapAndAddImage(String name, int frame, int channel, int slice)
			throws MMScriptException {

		Metadata md = new Metadata();
		try {
			Object img;
			if (core_.isSequenceRunning()) {
				img = core_.getLastImage();
				core_.getLastImageMD(0, 0, md);
			} else {
				core_.snapImage();
				// ToDo: get metadata of the snapped image
				long channels = core_.getNumberOfChannels();
				if (channels == 1)
					img = core_.getImage();
				else
					img = core_.getRGB32Image();
			}

			MMAcquisition acq = acqMgr_.getAcquisition(name);

			long width = core_.getImageWidth();
			long height = core_.getImageHeight();
			long depth = core_.getBytesPerPixel();

			if (!acq.isInitialized()) {

				acq.setImagePhysicalDimensions((int) width, (int) height,
						(int) depth);
				acq.initialize();
			}

			acq.insertImage(img, frame, channel, slice);

		} catch (Exception e) {
			handleException(e);
		}

	}

	public void addToSnapSeries(Object img, String acqName) {
		try {
			boolean liveRunning = liveRunning_;
			if (liveRunning)
				enableLiveMode(false);
			if (acqName == null)
				acqName = "Snap" + snapCount_;
			Boolean newSnap = false;
			// gui.closeAllAcquisitions();
			core_.setExposure(Double.parseDouble(textFieldExp_.getText()));
			long width = core_.getImageWidth();
			long height = core_.getImageHeight();
			long depth = core_.getBytesPerPixel();
			MMAcquisitionSnap acq = null;

			if (acqMgr_.hasActiveImage5D(acqName)) {
				acq = (MMAcquisitionSnap) acqMgr_.getAcquisition(acqName);
				newSnap = !acq.isCompatibleWithCameraSettings();
			} else
				newSnap = true;

			if (newSnap) {
				snapCount_++;
				acqName = "Snap" + snapCount_;
				this.openAcquisitionSnap(acqName, null, true); // (dir=null) ->
																// keep in
																// memory; don't
																// save to file.
				initializeAcquisition(acqName, (int) width, (int) height,
						(int) depth);

			}
			setChannelColor(acqName, 0, Color.WHITE);
			setChannelName(acqName, 0, "Snap");

			acq = (MMAcquisitionSnap) acqMgr_.getAcquisition(acqName);
			acq.appendImage(img);

			if (liveRunning)
				enableLiveMode(true);

			// closeAcquisition(acqName);
		} catch (Exception e) {
			handleException(e);
		}

	}

	public void addImage(String name, Object img, int frame, int channel,
			int slice) throws MMScriptException {
		MMAcquisition acq = acqMgr_.getAcquisition(name);
		acq.insertImage(img, frame, channel, slice);
	}

	public void closeAllAcquisitions() {
		acqMgr_.closeAll();
	}

	private class ScriptConsoleMessage implements Runnable {
		String msg_;

		public ScriptConsoleMessage(String text) {
			msg_ = text;
		}

		public void run() {
			scriptPanel_.message(msg_);
		}
	}

	public void message(String text) throws MMScriptException {
		if (scriptPanel_ != null) {
			if (scriptPanel_.stopRequestPending())
				throw new MMScriptException("Script interrupted by the user!");

			SwingUtilities.invokeLater(new ScriptConsoleMessage(text));
		}
	}

	public void clearMessageWindow() throws MMScriptException {
		if (scriptPanel_ != null) {
			if (scriptPanel_.stopRequestPending())
				throw new MMScriptException("Script interrupted by the user!");
			scriptPanel_.clearOutput();
		}
	}

	public void clearOutput() throws MMScriptException {
		clearMessageWindow();
	}

	public void clear() throws MMScriptException {
		clearMessageWindow();
	}

	public void setChannelContrast(String title, int channel, int min, int max)
			throws MMScriptException {
		MMAcquisition acq = acqMgr_.getAcquisition(title);
		acq.setChannelContrast(channel, min, max);
	}

	public void setChannelName(String title, int channel, String name)
			throws MMScriptException {
		MMAcquisition acq = acqMgr_.getAcquisition(title);
		acq.setChannelName(channel, name);

	}

	public void setChannelColor(String title, int channel, Color color)
			throws MMScriptException {
		MMAcquisition acq = acqMgr_.getAcquisition(title);
		acq.setChannelColor(channel, color.getRGB());
	}

	public void setContrastBasedOnFrame(String title, int frame, int slice)
			throws MMScriptException {
		MMAcquisition acq = acqMgr_.getAcquisition(title);
		acq.setContrastBasedOnFrame(frame, slice);
	}

	public void runWellScan(WellAcquisitionData wad) throws MMScriptException {
		testForAbortRequests();
		if (acqControlWin_ == null)
			openAcqControlDialog();

		engine_.setPositionList(posList_);
		if (acqControlWin_.runWellScan(wad) == false)
			throw new MMScriptException("Scanning error.");
		try {
			while (acqControlWin_.isAcquisitionRunning()) {
				Thread.sleep(500);
			}
		} catch (InterruptedException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
	}

	public void setXYStagePosition(double x, double y) throws MMScriptException {
		try {
			core_.setXYPosition(core_.getXYStageDevice(), x, y);
			core_.waitForDevice(core_.getXYStageDevice());
		} catch (Exception e) {
			throw new MMScriptException(e.getMessage());
		}

	}

	public Point2D.Double getXYStagePosition() throws MMScriptException {
		String stage = core_.getXYStageDevice();
		if (stage.length() == 0)
			return null;

		double x[] = new double[1];
		double y[] = new double[1];
		try {
			core_.getXYPosition(stage, x, y);
			Point2D.Double pt = new Point2D.Double(x[0], y[0]);
			return pt;
		} catch (Exception e) {
			throw new MMScriptException(e.getMessage());
		}
	}

	public void autofocus() throws MMScriptException {
		Autofocus af = engine_.getAutofocus();
		if (af == null)
			throw new MMScriptException("Autofocus plugin not installed!");

		af.setMMCore(core_);
		af.fullFocus();
	}

	public void autofocus(double coarseStep, int numCoarse, double fineStep,
			int numFine) throws MMScriptException {
		Autofocus af = engine_.getAutofocus();
		if (af == null)
			throw new MMScriptException("Autofocus plugin not installed!");

		af.setMMCore(core_);
		af.focus(coarseStep, numCoarse, fineStep, numFine);
	}

	public String getXYStageName() {
		return core_.getXYStageDevice();
	}

	public void setXYOrigin(double x, double y) throws MMScriptException {
		String xyStage = core_.getXYStageDevice();
		try {
			core_.setAdapterOriginXY(xyStage, x, y);
		} catch (Exception e) {
			throw new MMScriptException(e);
		}
	}

	public String installPlugin(String className, String menuName) {
		// instantiate auto-focusing module
		String msg = new String(className + " module loaded.");
		try {
			Class cl = Class.forName(className);
			PluginItem pi = new PluginItem();
			pi.menuItem = menuName;
			pi.plugin = (MMPlugin) cl.newInstance();
			pi.plugin.setApp(this);
			plugins_.add(pi);
		} catch (ClassNotFoundException e) {
			msg = className + " plugin not found.";
		} catch (InstantiationException e) {
			msg = className + " instantiation to MMPlugin interface failed.";
		} catch (IllegalAccessException e) {
			msg = "Illegal access exception!";
		} catch (NoClassDefFoundError e) {
			msg = className + " class definition nor found.";
		}
		System.out.println(msg);

		return msg;
	}

	public CMMCore getCore() {
		return core_;
	}
	
	public void snapAndAddToImage5D(String acqName) {
		Object img;
		try {
			boolean liveRunning = liveRunning_;
			if (liveRunning)
				enableLiveMode(false);

			core_.snapImage();
			img = core_.getImage();

			if (liveRunning)
				enableLiveMode(true);

			addToSnapSeries(img, acqName);
		}	catch (Exception e) {
			handleException(e);
		}
	}

	//Returns true if there is a newer image to display that can be get from MMCore
	//Implements "optimistic" approach: returns true even 
	//if there was an error while getting the image time stamp 
	private boolean isNewImageAvailable()
	{
		boolean ret=true;
/* disabled until metadata-related methods in MMCoreJ can handle exceptions 		
		Metadata md = new Metadata();
		MetadataSingleTag tag = null; 
		try
		{
			core_.getLastImageMD(0, 0, md);
			String strTag=MMCoreJ.getG_Keyword_Elapsed_Time_ms();
			tag = md.GetSingleTag(strTag);
			if(tag != null)
			{
				double newFrameTimeStamp = Double.valueOf(tag.GetValue());
				ret = newFrameTimeStamp > lastImageTimeMs_;
				if (ret)
				{
					lastImageTimeMs_ = newFrameTimeStamp;
				}
			}
		}
		catch(Exception e)
		{
			e.printStackTrace();
		}
*/		
		return ret;
	};

	public void suspendLiveMode() {
		liveModeSuspended_ = IsLiveModeOn();
		enableLiveMode(false);
	}
	
	public void resumeLiveMode() {
		if (liveModeSuspended_)
				enableLiveMode(true);
	}
}
