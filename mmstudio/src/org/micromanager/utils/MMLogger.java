package org.micromanager.utils;

import java.io.IOException;
import java.util.logging.FileHandler;
import java.util.logging.Logger;

public class MMLogger {
	private static Logger logger_;

	public MMLogger()
	// !!! ToDo: specify exceptions throws SecurityException, IOException {
	{
		try {
			if (logger_ == null) {
				logger_ = Logger.getLogger(this.getClass().getName());
				logger_.addHandler(new FileHandler("MMStudio.log"));
			}
		} catch (Exception e) {
			e.printStackTrace();
		}
	}

	public static Logger getLogger() {
		// !!! ToDo: specify exceptions throws SecurityException, IOException {
		try {
			if (logger_ == null)
				new MMLogger();
		} catch (Exception e) {
			e.printStackTrace();
		}
		return logger_;
	}

}
