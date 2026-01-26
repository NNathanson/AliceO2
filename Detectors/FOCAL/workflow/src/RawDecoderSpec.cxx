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

#include "Framework/CCDBParamSpec.h"
#include "Framework/ConfigParamRegistry.h"
#include "Framework/ControlService.h"
#include "Framework/InputRecordWalker.h"
#include "Framework/DataRefUtils.h"
#include "Framework/Logger.h"
#include "Framework/WorkflowSpec.h"

#include "CommonConstants/Triggers.h"
#include "DetectorsRaw/RDHUtils.h"

#include "FOCALReconstruction/PadWord.h"
#include "FOCALReconstruction/HCALWord.h"
#include "FOCALWorkflow/RawDecoderSpec.h"

#include "ITSMFTReconstruction/GBTWord.h"

#include <algorithm>
#include <iostream>
#include <sstream>
#include <set>

using namespace o2::focal::reco_workflow;

void RawDecoderSpec::init(framework::InitContext& ctx)
{
  if (ctx.options().get<bool>("filterIncomplete")) {
    LOG(info) << "Enabling filtering of incomplete events in the pixel data";
    mFilterIncomplete = true;
  }
  if (ctx.options().get<bool>("displayInconsistent")) {
    LOG(info) << "Display additional information in case of inconsistency between pixel links";
    mDisplayInconsistent = true;
  }

  auto mappingfile = ctx.options().get<std::string>("pixelmapping");
  PixelMapper::MappingType_t mappingtype = PixelMapper::MappingType_t::MAPPING_UNKNOWN;
  auto chiptype = ctx.options().get<std::string>("pixeltype");

  if (chiptype == "IB") {
    LOG(info) << "Using mapping type: IB";
    mappingtype = PixelMapper::MappingType_t::MAPPING_IB;
  } else if (chiptype == "OB") {
    LOG(info) << "Using mapping type: OB";
    mappingtype = PixelMapper::MappingType_t::MAPPING_OB;
  } else {
    LOG(fatal) << "Unknown mapping type for pixels: " << chiptype;
  }

  if (mappingfile == "default") {
    LOG(info) << "Using default pixel mapping for pixel type " << chiptype;
    mPixelMapping = std::make_unique<PixelMapper>(mappingtype);
  } else {
    LOG(info) << "Using user-defined mapping: " << mappingfile;
    mPixelMapping = std::make_unique<PixelMapper>(PixelMapper::MappingType_t::MAPPING_UNKNOWN);
    mPixelMapping->setMappingFile(mappingfile, mappingtype);
  }
}

void RawDecoderSpec::run(framework::ProcessingContext& ctx)
{
  LOG(info) << "Running FOCAL decoding";
  resetContainers();

  mTimeframeHasPadData = false;
  mTimeframeHasPixelData = false;
  mTimeframeHasHcalData = false;

  constexpr uint16_t FEE_PADS = 0xcafe;
  constexpr uint16_t FEE_HCAL = 0xbeef; // TODO: replace with real HCAL FEE ID

  int inputs = 0;
  std::vector<char> rawbuffer;
  uint16_t currentfee = 0;
  o2::InteractionRecord currentIR;

  std::unordered_map<int, int> numHBFFEE, numEventsFEE;
  std::unordered_map<int, std::vector<int>> numEventsHBFFEE;

  int numHBFPadsTF = 0, numEventsPadsTF = 0;
  int numHBFHcalTF = 0, numEventsHcalTF = 0;

  std::vector<int> expectFEEs;

  for (const auto& rawData : framework::InputRecordWalker(ctx.inputs())) {
    if (rawData.header != nullptr && rawData.payload != nullptr) {
      const auto payloadSize = o2::framework::DataRefUtils::getPayloadSize(rawData);
      auto header = o2::framework::DataRefUtils::getHeader<o2::header::DataHeader*>(rawData);

      LOG(debug) << "Channel " << header->dataOrigin.str << "/"
                 << header->dataDescription.str << "/"
                 << header->subSpecification;

      gsl::span<const char> databuffer(rawData.payload, payloadSize);
      int currentpos = 0;
      bool firstHBF = true;

      while (currentpos < static_cast<int>(databuffer.size())) {
        auto rdh = reinterpret_cast<const o2::header::RDHAny*>(databuffer.data() + currentpos);
        if (mDebugMode) {
          o2::raw::RDHUtils::printRDH(rdh);
        }

        if (o2::raw::RDHUtils::getMemorySize(rdh) > o2::raw::RDHUtils::getHeaderSize(rdh)) {
          auto payloadsize = o2::raw::RDHUtils::getMemorySize(rdh) - o2::raw::RDHUtils::getHeaderSize(rdh);
          auto fee = o2::raw::RDHUtils::getFEEID(rdh);

          LOG(debug) << "Next RDH: ";
          LOG(debug) << "Found fee                   0x" << std::hex << fee << std::dec
                     << " (System " << (fee == FEE_PADS ? "Pads" : (fee == FEE_HCAL ? "HCAL" : "Pixels")) << ")";
          LOG(debug) << "Found trigger BC:           " << o2::raw::RDHUtils::getTriggerBC(rdh);
          LOG(debug) << "Found trigger Orbit:        " << o2::raw::RDHUtils::getTriggerOrbit(rdh);
          LOG(debug) << "Found payload size:         " << payloadsize;
          LOG(debug) << "Found offset to next:       " << o2::raw::RDHUtils::getOffsetToNext(rdh);
          LOG(debug) << "Stop bit:                   " << (o2::raw::RDHUtils::getStop(rdh) ? "yes" : "no");

          size_t wordSize = 0;
          if (fee == FEE_PADS) {
            wordSize = sizeof(o2::focal::PadGBTWord);
          } else if (fee == FEE_HCAL) {
            wordSize = sizeof(o2::focal::HCALGBTWord);
          } else {
            wordSize = sizeof(o2::itsmft::GBTWord);
          }
          LOG(debug) << "Number of GBT words:        " << (wordSize ? (payloadsize / wordSize) : 0);

          auto page_payload = databuffer.subspan(currentpos + o2::raw::RDHUtils::getHeaderSize(rdh), payloadsize);
          std::copy(page_payload.begin(), page_payload.end(), std::back_inserter(rawbuffer));
        }

        auto trigger = o2::raw::RDHUtils::getTriggerType(rdh);
        if (trigger & o2::trigger::HB) {
          if (o2::raw::RDHUtils::getStop(rdh)) {
            LOG(debug) << "Stop bit received - processing payload";

            if (!rawbuffer.empty()) {
              if (currentfee == FEE_PADS) {
                if (mUsePadData) {
                  auto nEventsPads = decodePadData(rawbuffer, currentIR);
                  mTimeframeHasPadData = true;

                  mNumEventsHBFPads[nEventsPads] += 1;
                  numEventsPadsTF += nEventsPads;
                  numHBFPadsTF++;
                }
              } else if (currentfee == FEE_HCAL) {
                if (mUseHcalData) {
                  auto nEventsHcal = decodeHcalData(rawbuffer, currentIR);
                  mTimeframeHasHcalData = true;

                  mNumEventsHBFHcal[nEventsHcal] += 1;
                  numEventsHcalTF += nEventsHcal;
                  numHBFHcalTF++;
                }
              } else {
                if (mUsePixelData) {
                  auto feeID = o2::raw::RDHUtils::getFEEID(rdh);

                  if (firstHBF) {
                    if (std::find(expectFEEs.begin(), expectFEEs.end(), feeID) == expectFEEs.end()) {
                      expectFEEs.emplace_back(feeID);
                    }
                    firstHBF = false;
                  }

                  auto neventsPixels = decodePixelData(rawbuffer, currentIR, feeID);
                  mTimeframeHasPixelData = true;

                  numHBFFEE[feeID] += 1;
                  numEventsFEE[feeID] += neventsPixels;
                  numEventsHBFFEE[feeID].push_back(neventsPixels);
                }
              }
            } else {
              LOG(debug) << "Payload size 0 - skip empty HBF";
            }

            rawbuffer.clear();
          } else {
            currentIR.bc = o2::raw::RDHUtils::getTriggerBC(rdh);
            currentIR.orbit = o2::raw::RDHUtils::getTriggerOrbit(rdh);
            currentfee = o2::raw::RDHUtils::getFEEID(rdh);

            LOG(debug) << "New HBF " << currentIR.orbit << " / " << currentIR.bc
                       << ", FEE 0x" << std::hex << currentfee << std::dec;
          }
        }

        currentpos += o2::raw::RDHUtils::getOffsetToNext(rdh);
      }
    } else {
      LOG(error) << "Input " << inputs << ": Either header or payload is nullptr";
    }
    inputs++;
  }

  int numHBFPixelsTF = 0;
  if (mTimeframeHasPixelData) {
    if (!consistencyCheckPixelFEE(numHBFFEE)) {
      LOG(alarm) << "Mismatch in number of HBF / TF between pixel FEEs";
      if (mDisplayInconsistent) {
        printCounters(numHBFFEE);
      }
      mNumInconsistencyPixelHBF++;
    }

    numHBFPixelsTF = maxCounter(numHBFFEE);
    mNumHBFPixels += numHBFPixelsTF;

    if (!consistencyCheckPixelFEE(numEventsFEE)) {
      LOG(alarm) << "Mismatch in number of events / TF between pixel FEEs";
      if (mDisplayInconsistent) {
        printCounters(numEventsFEE);
      }
      mNumInconsistencyPixelEvent++;
    }

    mNumEventsPixels += maxCounter(numEventsFEE);

    if (!checkEventsHBFConsistency(numEventsHBFFEE)) {
      LOG(alarm) << "Mismatch number of events / HBF between pixel FEEs";
      if (mDisplayInconsistent) {
        printEvents(numEventsHBFFEE);
      }
      mNumInconsistencyPixelEventHBF++;
    }

    fillPixelEventHBFCount(numEventsHBFFEE);

    if (mFilterIncomplete) {
      for (auto& hbf : mHBFs) {
        auto numErased = filterIncompletePixelsEventsHBF(hbf.second, expectFEEs);
        mNumEventsPixels -= numErased;
      }
    }
  }

  buildEvents();
  sendOutput(ctx);

  mNumEventsPads += numEventsPadsTF;
  mNumHBFPads += numHBFPadsTF;

  mNumEventsHcal += numEventsHcalTF;
  mNumHBFHcal += numHBFHcalTF;

  mNumTimeframes++;

  mNumHBFperTFPads[numHBFPadsTF] += 1;
  mNumHBFperTFHcal[numHBFHcalTF] += 1;
  mNumHBFperTFPixels[numHBFPixelsTF] += 1;
}

int RawDecoderSpec::decodePadData(const gsl::span<const char> padWords, o2::InteractionRecord& hbIR)
{
  constexpr std::size_t EVENTSIZEPADGBT = 1180;
  constexpr std::size_t EVENTSIZECHAR = EVENTSIZEPADGBT * sizeof(PadGBTWord);

  auto nevents = static_cast<int>(padWords.size() / EVENTSIZECHAR);
  for (int ievent = 0; ievent < nevents; ievent++) {
    decodePadEvent(padWords.subspan(EVENTSIZECHAR * ievent, EVENTSIZECHAR), hbIR);
  }
  return nevents;
}

void RawDecoderSpec::decodePadEvent(const gsl::span<const char> padWords, o2::InteractionRecord& hbIR)
{
  gsl::span<const PadGBTWord> padWordsGBT(reinterpret_cast<const PadGBTWord*>(padWords.data()),
                                         padWords.size() / sizeof(PadGBTWord));

  mPadDecoder.reset();
  mPadDecoder.decodeEvent(padWordsGBT);

  auto foundHBF = mHBFs.find(hbIR);
  if (foundHBF == mHBFs.end()) {
    auto res = mHBFs.insert({hbIR, HBFData{}});
    foundHBF = res.first;
  }

  foundHBF->second.mPadEvents.push_back(createPadLayerEvent(mPadDecoder.getData()));
}

int RawDecoderSpec::decodeHcalData(const gsl::span<const char> hcalWords, o2::InteractionRecord& hbIR)
{
  constexpr std::size_t EVENTSIZEHCALGBT = 1180;
  constexpr std::size_t EVENTSIZECHAR = EVENTSIZEHCALGBT * sizeof(HCALGBTWord);

  auto nevents = static_cast<int>(hcalWords.size() / EVENTSIZECHAR);
  for (int ievent = 0; ievent < nevents; ievent++) {
    decodeHcalEvent(hcalWords.subspan(EVENTSIZECHAR * ievent, EVENTSIZECHAR), hbIR);
  }
  return nevents;
}

void RawDecoderSpec::decodeHcalEvent(const gsl::span<const char> hcalWords, o2::InteractionRecord& hbIR)
{
  gsl::span<const HCALGBTWord> hcalWordsGBT(reinterpret_cast<const HCALGBTWord*>(hcalWords.data()),
                                           hcalWords.size() / sizeof(HCALGBTWord));

  mHcalDecoder.reset();
  mHcalDecoder.decodeEvent(hcalWordsGBT);

  auto foundHBF = mHBFs.find(hbIR);
  if (foundHBF == mHBFs.end()) {
    auto res = mHBFs.insert({hbIR, HBFData{}});
    foundHBF = res.first;
  }

  // IMPORTANT FIX: HCALDecoder exposes getData(), not getPCBData()
  foundHBF->second.mHCALEvents.push_back(createHcalPCBEvent(mHcalDecoder.getData()));
}

std::array<HCALPCBEvent, constants::HCAL_NPCBS>
RawDecoderSpec::createHcalPCBEvent(const o2::focal::HCALData& data) const
{
  std::array<HCALPCBEvent, constants::HCAL_NPCBS> result{};
  std::array<uint8_t, 8> triggertimes{};

  for (std::size_t ipcb = 0; ipcb < constants::HCAL_NPCBS; ipcb++) {
    const auto& asic = data.getDataForASIC(static_cast<int>(ipcb)).getASIC();

    for (int ihalf = 0; ihalf < o2::focal::ASICData::NHALVES; ihalf++) {
      auto header = asic.getHeader(ihalf);
      auto calib  = asic.getCalib(ihalf);
      auto cmn    = asic.getCMN(ihalf);

      result[ipcb].setHeader(ihalf, header.getHeader(), header.getBCID(),
                             header.getWadd(), header.getFourbit(), header.getTrailer());
      result[ipcb].setCalib(ihalf, calib.getADC(), calib.getTOA(), calib.getTOT());
      result[ipcb].setCMN(ihalf, cmn.getADC(), cmn.getTOA(), cmn.getTOT());
    }

    for (int ich = 0; ich < o2::focal::ASICData::NCHANNELS; ich++) {
      auto channel = asic.getChannel(ich);
      result[ipcb].setChannel(ich, channel.getADC(), channel.getTOA(), channel.getTOT());
    }

    auto triggers = data.getDataForASIC(static_cast<int>(ipcb)).getTriggerWords();
    const auto nwin = std::min<std::size_t>(triggers.size(), constants::HCAL_WINDOW_LENGTH);

    for (std::size_t window = 0; window < nwin; window++) {
      triggertimes.fill(0);
      triggertimes[0] = triggers[window].mTrigger0;
      triggertimes[1] = triggers[window].mTrigger1;
      triggertimes[2] = triggers[window].mTrigger2;
      triggertimes[3] = triggers[window].mTrigger3;
      triggertimes[4] = triggers[window].mTrigger4;
      triggertimes[5] = triggers[window].mTrigger5;
      triggertimes[6] = triggers[window].mTrigger6;
      triggertimes[7] = triggers[window].mTrigger7;

      result[ipcb].setTrigger(window, triggers[window].mHeader0, triggers[window].mHeader1, triggertimes);
    }
  }

  return result;
}

// IMPORTANT: in getRawDecoderSpec(), pass useHcalData into RawDecoderSpec
o2::framework::DataProcessorSpec o2::focal::reco_workflow::getRawDecoderSpec(bool askDISTSTF,
                                                                            uint32_t outputSubspec,
                                                                            bool usePadData,
                                                                            bool usePixelData,
                                                                            bool useHcalData,
                                                                            bool debugMode)
{
  constexpr auto originFOC = o2::header::gDataOriginFOC;

  std::vector<o2::framework::OutputSpec> outputs;
  outputs.emplace_back(originFOC, "PADLAYERS", outputSubspec, o2::framework::Lifetime::Timeframe);
  outputs.emplace_back(originFOC, "HCALPCBS",  outputSubspec, o2::framework::Lifetime::Timeframe);
  outputs.emplace_back(originFOC, "PIXELHITS", outputSubspec, o2::framework::Lifetime::Timeframe);
  outputs.emplace_back(originFOC, "PIXELCHIPS", outputSubspec, o2::framework::Lifetime::Timeframe);
  outputs.emplace_back(originFOC, "TRIGGERS", outputSubspec, o2::framework::Lifetime::Timeframe);

  std::vector<o2::framework::InputSpec> inputs{
    {"stf", o2::framework::ConcreteDataTypeMatcher{originFOC, o2::header::gDataDescriptionRawData},
     o2::framework::Lifetime::Timeframe}
  };
  if (askDISTSTF) {
    inputs.emplace_back("stdDist", "FLP", "DISTSUBTIMEFRAME", 0, o2::framework::Lifetime::Timeframe);
  }

  return o2::framework::DataProcessorSpec{
    "FOCALRawDecoderSpec",
    inputs,
    outputs,
    o2::framework::adaptFromTask<o2::focal::reco_workflow::RawDecoderSpec>(
      outputSubspec, usePadData, usePixelData, useHcalData, debugMode
    ),
    o2::framework::Options{
      {"filterIncomplete",    o2::framework::VariantType::Bool,   false, {"Filter incomplete pixel events"}},
      {"displayInconsistent", o2::framework::VariantType::Bool,   false, {"Display information about inconsistent timeframes"}},
      {"pixeltype",           o2::framework::VariantType::String, "OB",  {"Pixel mapping type"}},
      {"pixelmapping",        o2::framework::VariantType::String, "default", {"File with pixel mapping"}}
    }
  };
}
