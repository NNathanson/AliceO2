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
#ifndef ALICEO2_FOCAL_DIGIT_H_
#define ALICEO2_FOCAL_DIGIT_H_

#include <iosfwd>
#include <cstdint>
#include "Rtypes.h"

namespace o2
{

namespace focal
{

/// \class Digit
/// \brief FOCAL electronics-level digit
/// \ingroup FOCALDataFormat

class Digit
{
 public:
  /// \enum Flag_t
  /// \brief Bits of mFlags
  enum Flag_t : uint8_t {
    kTOTPresent = 1 << 0,  ///< TOT present
    kADCSaturated = 1 << 1 ///< ADC saturated
  };

  Digit() = default;

  /// \param absID Channel ID (HCAL: tower ID)
  /// \param adc ADC value (0-1023)
  /// \param toa TOA value (0-1023)
  /// \param tot TOT value (0-4095)
  /// \param flags Flag bits (Flag_t)
  Digit(int absID, int adc, int toa, int tot, int flags);
  ~Digit() = default;

  void setAbsID(int absID) { mAbsID = absID; }
  int getAbsID() const { return mAbsID; }

  void setADC(int adc) { mADC = adc; }
  int getADC() const { return mADC; }

  void setTOA(int toa) { mTOA = toa; }
  int getTOA() const { return mTOA; }

  void setTOT(int tot) { mTOT = tot; }
  int getTOT() const { return mTOT; }

  void setFlags(int flags) { mFlags = flags; }
  int getFlags() const { return mFlags; }

  bool TOTPresent() const { return mFlags & kTOTPresent; }
  bool ADCSaturated() const { return mFlags & kADCSaturated; }

  void PrintStream(std::ostream& stream) const;

 private:
  uint32_t mAbsID = 0; ///< Tower ID (= SDigit index)
  uint16_t mADC = 0;   ///< ADC (0-1023)
  uint16_t mTOA = 0;   ///< TOA (0-1023)
  uint16_t mTOT = 0;   ///< TOT (0-4095)
  uint8_t mFlags = 0;  ///< See enum Flag_t above for bit definitions

  ClassDefNV(Digit, 1);
};

std::ostream& operator<<(std::ostream& stream, const Digit& digit);
} // namespace focal
} // namespace o2
#endif
