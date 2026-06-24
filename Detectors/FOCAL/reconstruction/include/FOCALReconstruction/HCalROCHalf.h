#ifndef ALICEO2_FOCAL_HCalROCDataLink_H
#define ALICEO2_FOCAL_HCalROCDataLink_H

#include <array>
#include <exception>
#include <string>
#include <vector>

#include <gsl/span>

#include "Rtypes.h"

namespace o2::focal {

  class HCalROCDataLink {
    public:
      HCalROCDataLink() = default;
      ~HCalROCDataLink() = default;

    private:
      HCalDAQHeader mHeader;
      HCalChannel mChannels[36];
      HCalChannel mCommonMode;
      HCalChannel mCalibration;
      unsigned int mCRC;
  }

}
