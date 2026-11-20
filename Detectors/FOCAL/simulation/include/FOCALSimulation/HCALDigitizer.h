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
#ifndef ALICEO2_FOCAL_HCALDIGITIZER_H_
#define ALICEO2_FOCAL_HCALDIGITIZER_H_

#include <map>
#include <memory>
#include <vector>

#include "Rtypes.h"
#include "TRandom3.h"

#include "CommonDataFormat/InteractionRecord.h"
#include "CommonUtils/TreeStreamRedirector.h"
#include "DataFormatsFOCAL/Digit.h"
#include "DataFormatsFOCAL/MCLabel.h"
#include "DataFormatsFOCAL/TriggerRecord.h"
#include "FOCALBase/Geometry.h"
#include "FOCALSimulation/LabeledSDigit.h"
#include "FOCALSimulation/SiPMResponse.h"
#include "SimulationDataFormat/MCTruthContainer.h"

namespace o2
{
namespace focal
{

/// \class HCALDigitizer
/// \brief FOCAL-H digitizer: HCAL SDigits of one collision -> digits
/// \ingroup FOCALsimulation
class HCALDigitizer
{
 public:
  HCALDigitizer() = default;
  ~HCALDigitizer() = default;
  HCALDigitizer(const HCALDigitizer&) = delete;
  HCALDigitizer& operator=(const HCALDigitizer&) = delete;

  void init();
  void clear();
  void finish();

  void setGeometry(const o2::focal::Geometry* gm) { mGeometry = gm; }
  void setRandomSeed(unsigned int seed) { mRandomGenerator.SetSeed(seed); }
  void setEventTime(const o2::InteractionTimeRecord& interaction); // Included already so that integration of pile-up buffer in future is easier

  /// \brief Process the SDigits of a single collision: appends its digits, labels and trigger record to the timeframe output
  /// \param sdigits Labeled SDigits
  void process(const std::vector<o2::focal::LabeledSDigit>& sdigits);

  void endDebugStream() { mDebugStream.reset(); }

  const std::vector<Digit>& getDigits() const { return mDigits; }
  const std::vector<TriggerRecord>& getTriggerRecords() const { return mTriggerRecords; }
  const o2::dataformats::MCTruthContainer<MCLabel>& getMCLabels() const { return mLabels; }

 private:
  struct TowerSignal {
    double visibleEnergy = 0.;   // eV, 0 for crosstalk-only towers
    double meanPe = 0.;          // mu before board crosstalk
    double meanPeXT = 0.;        // mu after board crosstalk
    double time = 0.;            // in the case of crosstalk-only towers, time is set based on the contributing neighbour
    std::vector<MCLabel> labels; // labels of the SDigit
  };

  /// \brief Board crosstalk: each tower keeps (1 - 4c) of its mu and gets c of the mu of each of its neighbours
  /// \param meanPhotoelectrons mu per tower ID, before crosstalk
  /// \param hcalIndex SiPM grid, used for row/col calculations
  /// \return mu per tower ID, after crosstalk (may contain towers without a dedicated SDigit)
  std::map<int, TowerSignal> applyBoardCrosstalk(const std::map<int, TowerSignal>& meanPhotoelectrons, const HCALIndex& hcalIndex) const;

  const Geometry* mGeometry = nullptr;                           //! geometry (not owned)
  TRandom3 mRandomGenerator;                                     //! random genrator, shared by the SiPM and H2GCROC responses
  SiPMResponse mSiPMResponse;                                    ///< SiPM response
  std::unique_ptr<o2::utils::TreeStreamRedirector> mDebugStream; //! focalhdigitsDebug.root, if HCALSimParam::mDebugStream

  std::vector<Digit> mDigits;                         //! digits of the timeframe
  std::vector<TriggerRecord> mTriggerRecords;         //! one per collision
  o2::dataformats::MCTruthContainer<MCLabel> mLabels; //! labels, indexed like mDigits
  o2::InteractionTimeRecord mCurrentInteraction;      //! interaction record of the collision being processed

  ClassDefNV(HCALDigitizer, 1);
};

} // namespace focal
} // namespace o2

#endif
