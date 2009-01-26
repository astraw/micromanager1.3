import ij.IJ;
import ij.plugin.PlugIn;
import mmcorej.CMMCore;

/*
 * Created on Dec 8, 2006
 * author: Nenad Amodaj
 */

/**
 * ImageJ plugin wrapper for uManager.
 */
public class MMLiveTrackerPlugin_ implements PlugIn {
   
   public void run(String arg) {
      
      CMMCore core = MMStudioPlugin.getMMCoreInstance();
      if (core == null) {
         IJ.error("Micro-Manager Studio must be running!");
         return;
      }
          
      TrackerControlDlg tdlg = new TrackerControlDlg(core);
      tdlg.setVisible(true);       
   }
   
}
