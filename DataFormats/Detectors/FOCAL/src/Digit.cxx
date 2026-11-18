// Copyright 2019-2020 CERN and copyright holders of ALICE O2.
// See https://alice-o2.web.cern.ch/copyright for details of the copyright holders.
// All rights not expressly granted are reserved.
//
// This software is distributed under the terms of the GNU General Public
// License v3 (GPL Version 3), copied verbatim in the file "COPYING".
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

#include <iostream>
#include "DataFormatsFOCAL/Digit.h"

using namespace o2::focal;

Digit::Digit(int absID, int adc, int toa, int tot, int flags)
  : mAbsID(absID), mADC(adc), mTOA(toa), mTOT(tot), mFlags(flags)
{
}

void Digit::PrintStream(std::ostream& stream) const
{
  stream << "FOCAL Digit: Tower " << mAbsID << ", ADC " << mADC << ", TOA " << mTOA << ", TOT " << mTOT << ", Flag " << static_cast<int>(mFlags);
}

std::ostream& o2::focal::operator<<(std::ostream& stream, const Digit& digit)
{
  digit.PrintStream(stream);
  return stream;
}
