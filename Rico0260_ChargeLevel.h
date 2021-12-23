/*
  MIT License
  
  Copyright (c) 2021 Rico0260
  
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:
  
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
  
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

/* 
 *  Battery charge level for Lithium 18650
 */

#ifndef Rico0260_ChargeLevel_h
#define Rico0260_ChargeLevel_h

#include <Arduino.h>

class Rico0260_ChargeLevel
{
  public:
    /*
    * Constructor
    */
    Rico0260_ChargeLevel(void);
  
    /*
    * Get the battery level (0-100)
    * @return the calculated battery level
    */
    int getChargeLevel_18650(double volts);
    //int getChargeLevel_Lead(double volts);
  
  private:
    double _TableauTensions_18650[101]; //Tableau des tensions
    //double _TableauTensions_Lead[101]; //Tableau des tensions
  
    void _initArray_18650(void);
    //void _initArray_Lead(void);
};

#endif
