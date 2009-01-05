/**
 * 
 */
package org.micromanager.utils;

import ij.ImagePlus;

/**
 * @author OD
 *
 */
public interface ImageController {
	public void setImagePlus(ImagePlus imp);
	public void setContrastSettings(ContrastSettings contrastSettings8_,
			ContrastSettings contrastSettings16_);
	public void update();
}
