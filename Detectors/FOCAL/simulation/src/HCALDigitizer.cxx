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

#include "FOCALSimulation/HCALDigitizer.h"
#include "FOCALSimulation/HCALSimParam.h"
#include "FOCALSimulation/SiPMResponse.h"
#include "SimConfig/DigiParams.h"
#include <chrono>
#include <algorithm>
#include <fairlogger/Logger.h>

ClassImp(o2::focal::HCALDigitizer);

using namespace o2::focal;

//_______________________________________________________________________
void HCALDigitizer::init()
{
  auto& param = HCALSimParam::Instance();

  // Set a random seed based on time, unless DigiParams.seed is specified at non-zero value (for reproducibility in testing)
  auto randomSeed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
  if (o2::conf::DigiParams::Instance().seed != 0) {
    randomSeed = o2::conf::DigiParams::Instance().seed;
  }
  setRandomSeed(randomSeed);

  mSiPMResponse.setRandomGenerator(&mRandomGenerator);
  mSiPMResponse.init();

  // TODO WHEN WRITTEN: HGCROCResponse::init();

  if (param.mDebugStream) {
    mDebugStream = std::make_unique<o2::utils::TreeStreamRedirector>("focalhdigitsDebug.root", "RECREATE");
  }
}

//_______________________________________________________________________
void HCALDigitizer::clear()
{
  mDigits.clear();
  mTriggerRecords.clear();
  mLabels.clear();
}

//_______________________________________________________________________
void HCALDigitizer::setEventTime(const o2::InteractionTimeRecord& interaction)
{
  mCurrentInteraction = interaction;

  // Long term: expand this when pileip is implemented
}

//_______________________________________________________________________
void HCALDigitizer::finish()
{
  // Long term: digit buffer here when pileup is implemented
}

//_______________________________________________________________________
void HCALDigitizer::process(const std::vector<o2::focal::LabeledSDigit>& sdigits)
{
  std::map<int, TowerSignal> hcalTowers;
  const int firstDigit = mDigits.size();

  // Filter out non-HCAL SDigits: will be redundant since this also happens inherently in FOCALDigitizerSpec, but kept as additional safety check and for testing
  for (const auto& dig : sdigits) {
    if (dig.getSubsystem() != SDigit::kHCAL) {
      continue;
    }
    auto& tower = hcalTowers[dig.getIndex()];
    tower.visibleEnergy = dig.getEnergy();
    tower.meanPe = mSiPMResponse.getMeanPhotoelectrons(dig.getEnergy());
    tower.time = dig.getTimeStamp();
    tower.labels = dig.getLabels();
  }

  // TODO WHEN WRITTEN: HgcrocResponse::startCollision()
  const auto hcalIndex = mGeometry->getHCALIndex();
  std::map<int, TowerSignal> hcalTowersXT = applyBoardCrosstalk(hcalTowers, hcalIndex);

  // Tower loop
  for (const auto& [towerID, tower] : hcalTowersXT) {

    SiPMSignal signal = mSiPMResponse.getResponse(tower.meanPeXT, tower.time);

    // TODO WHEN WRITTEN: HgcrocResponse section

    const int digitIndex = mDigits.size();
    mDigits.emplace_back(towerID, 0, 0, 0, 0); // TODO WHEN WRITTEN: remove placeholder 0s and instead put values returned by the HGCROCResponse class
    mLabels.addElements(digitIndex, tower.labels);

    if (mDebugStream) {
      auto [row, col] = hcalIndex.getRowColFromIndex(towerID);
      (*mDebugStream).GetFile()->cd();
      (*mDebugStream) << "HCALTowers"
                      << "Tower=" << towerID
                      << "Row=" << row
                      << "Col=" << col
                      << "Evis=" << tower.visibleEnergy
                      << "MeanPe=" << tower.meanPe
                      << "MeanPeXT=" << tower.meanPeXT
                      << "Time=" << tower.time
                      << "N=" << signal.mPhotoelectrons
                      << "Nfired=" << signal.mFiredPixels
                      << "Q=" << signal.mCharge
                      << "\n"; // TODO WHEN WRITTEN: add ADC, TOT and TOA to debug stream output
    }
  }

  mTriggerRecords.emplace_back(mCurrentInteraction, firstDigit, mDigits.size() - firstDigit);
}

//_______________________________________________________________________
std::map<int, HCALDigitizer::TowerSignal> HCALDigitizer::applyBoardCrosstalk(const std::map<int, TowerSignal>& meanPhotoelectrons, const HCALIndex& hcalIndex) const
{
  std::map<int, TowerSignal> meanPhotoelectronsXT = meanPhotoelectrons;

  auto& param = HCALSimParam::Instance();
  double xt = param.mBoardCrosstalk;

  // When no crosstalk is considered in SimParam, return here to prevent the creation of unnecessary crosstalk-only digits that shouldn't be made
  if (xt == 0) {
    return meanPhotoelectronsXT;
  }

  // First pass through tower map: reduce each tower's meanPe to account for the photoelectrons lost to crosstalk
  for (const auto& [towerID, tower] : meanPhotoelectrons) {
    meanPhotoelectronsXT[towerID].meanPeXT = (1 - 4 * xt) * tower.meanPe;
  }

  for (const auto& [towerID, tower] : meanPhotoelectrons) {
    std::vector<int> neighbourIDs;

    auto [row, col] = hcalIndex.getRowColFromIndex(towerID);
    neighbourIDs.push_back(hcalIndex.getIndexFromRowCol(row + 1, col));
    neighbourIDs.push_back(hcalIndex.getIndexFromRowCol(row - 1, col));
    neighbourIDs.push_back(hcalIndex.getIndexFromRowCol(row, col + 1));
    neighbourIDs.push_back(hcalIndex.getIndexFromRowCol(row, col - 1));

    for (int nID : neighbourIDs) {
      if (nID == -1) {
        continue;
      }

      // Define a "neighbour" tower and insert it if it doesn't exist already (this is the case for the first time a crosstalk-only tower is added)
      auto [entry, inserted] = meanPhotoelectronsXT.try_emplace(nID);
      auto& neighbour = entry->second;

      // Check if the neighbour is a crosstalk-only channel
      const bool xtOnly = (meanPhotoelectrons.find(nID) == meanPhotoelectrons.end());

      // Set time for crosstalk-only towers based on their earliest neighbour
      if (xtOnly) {
        if (inserted) {
          neighbour.time = tower.time;
          neighbour.labels = {MCLabel(true, 1.0)};
        } else {
          neighbour.time = std::min(neighbour.time, tower.time);
        }
      }

      neighbour.meanPeXT += xt * tower.meanPe;
    }
  }

  return meanPhotoelectronsXT;
}
