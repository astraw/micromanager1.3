///////////////////////////////////////////////////////////////////////////////
//FILE:          NumberUtils.java
//PROJECT:       Micro-Manager
//SUBSYSTEM:     mmstudio
//-----------------------------------------------------------------------------
//
// AUTHOR:       Nico Stuurman, nico@cmp.ucsf.edu, March 21, 2009
//
// COPYRIGHT:    University of California, San Francisco, 2009
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
//
package org.micromanager.utils;

import java.text.NumberFormat;

public class NumberUtils {
   private static NumberFormat format_;

   static {
      format_ = NumberFormat.getInstance();
   }

   public static String NumberToString(int number) {
      return format_.format(number);
   }

   public static String NumberToString(double number) {
      return format_.format(number);
   }

   public static double StringToDouble(String numberString) throws java.text.ParseException {
      return format_.parse(numberString).doubleValue();
   }

   public static int StringToInt(String numberString) throws java.text.ParseException {
      return format_.parse(numberString).intValue();
   }

}
