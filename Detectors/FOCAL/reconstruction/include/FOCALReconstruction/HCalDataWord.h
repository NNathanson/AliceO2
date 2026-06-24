#ifndef ALICEO2_FOCAL_HCalDataWord_H
#define ALICEO2_FOCAL_HCalDataWord_H

#include <cstdint>

namespace o2::focal {

  struct HCalDataWord {
    uint32_t data = 0;

    // DAQ header word fields
    uint32_t tr() const { return  data        & 0xF;   } // trailer pattern (0101)
    uint32_t hm() const { return (data >>  4) & 0x7;   } // hamming decoding error bits
    uint32_t ob() const { return (data >>  7) & 0x7;   } // orbit counter
    uint32_t ec() const { return (data >> 10) & 0x3F;  } // event counter
    uint32_t bx() const { return (data >> 16) & 0xFFF; } // bunch crossing counter
    uint32_t hd() const { return (data >> 28) & 0xF;   } // header pattern (1111)

    // Channel data word fields
    uint32_t toa() const { return  data        & 0x3FF; } // time of arrival
    uint32_t tot() const { return (data >> 10) & 0x3FF; } // time over threshold
    uint32_t adc() const { return (data >> 20) & 0x3FF; } // adc count
    uint32_t tp()  const { return (data >> 30) & 0x1;   } // "tot in progress" flag
    uint32_t tc()  const { return (data >> 31) & 0x1;   } // "tot complete" flag

    constexpr operator uint32_t() const noexcept { return data; }

  };

  struct HCalDAQHeader : public HCalDataWord {
    HCalDAQHeader() {
      data = 0;
    }
    
    HCalDAQHeader(unsigned int w) {
      data = w;
    }
  };

  struct HCalChannel : public HCalDataWord {
    HCalChannel() {
      data = 0;
    }
    
    HCalChannel(unsigned int w) {
      data = w;
    }

  };

  // Struct representing a single line of payload: eight 32-bit words
  struct HCalGBTLine {
    HCalDataWord words[8]; 

    uint32_t hdr()      const { return  words[0]        & 0xFF;  }
    uint32_t link_id()  const { return (words[0] >> 8)  & 0xFF;  }
    uint32_t bx_cntr()  const { return (words[0] >> 16) & 0xFFF; }
    uint32_t ob_cntr()  const { return  words[1]               ; }
  };

} // namespace o2::focal

#endif 
