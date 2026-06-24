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

#include <FOCALReconstruction/HCALMapper.h>

#include <string>
#include <algorithm>
#include <iostream>
#include <cctype>
#include <fstream>
#include <sstream>
#include <vector>

using namespace o2::focal;

namespace
{
inline std::string trim(std::string s)
{
  auto notSpace = [](unsigned char c) { return !std::isspace(c); };
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
  s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
  return s;
}
} // namespace

// ---------------- Exceptions ----------------

HCALMapper::PositionException::PositionException(unsigned int column, unsigned int row)
  : mColumn(column), mRow(row)
{
  mMessage = "Invalid position: col (" + std::to_string(mColumn) +
             "), row (" + std::to_string(mRow) + ")";
}

HCALMapper::ChannelIDException::ChannelIDException(unsigned int channelID)
  : mChannelID(channelID)
{
  mMessage = "Invalid channelID: " + std::to_string(mChannelID);
}

void HCALMapper::PositionException::print(std::ostream& stream) const
{
  stream << mMessage;
}

void HCALMapper::ChannelIDException::print(std::ostream& stream) const
{
  stream << mMessage;
}

std::ostream& o2::focal::operator<<(std::ostream& stream, const HCALMapper::PositionException& except)
{
  except.print(stream);
  return stream;
}

std::ostream& o2::focal::operator<<(std::ostream& stream, const HCALMapper::ChannelIDException& except)
{
  except.print(stream);
  return stream;
}

// ---------------- HCALMapper ----------------

HCALMapper::HCALMapper(const std::string& mappingFile)
{
  loadMapping(mappingFile);
}

void HCALMapper::loadMapping(const std::string& mappingFile)
{
  std::ifstream in(mappingFile);
  if (!in.is_open()) {
    throw MappingFileException("HCALMapper: cannot open mapping file: " + mappingFile);
  }

  std::string line;
  unsigned int row = 0;

  while (std::getline(in, line)) {
    // Strip comments beginning with //
    const auto commentPos = line.find("//");
    if (commentPos != std::string::npos) {
      line = line.substr(0, commentPos);
    }

    line = trim(line);
    if (line.empty()) {
      continue;
    }

    if (row >= NROW) {
      throw MappingFileException("HCALMapper: too many rows in file " + mappingFile +
                                " (expected " + std::to_string(NROW) + ")");
    }

    // Parse CSV-ish line: 8 comma-separated ints
    std::vector<unsigned int> values;
    values.reserve(NCOLUMN);

    std::stringstream ss(line);
    std::string token;
    while (std::getline(ss, token, ',')) {
      token = trim(token);
      if (token.empty()) {
        continue;
      }
      try {
        values.push_back(static_cast<unsigned int>(std::stoul(token)));
      } catch (...) {
        throw MappingFileException("HCALMapper: invalid integer token '" + token +
                                  "' in file " + mappingFile);
      }
    }

    if (values.size() != NCOLUMN) {
      throw MappingFileException("HCALMapper: wrong number of columns in file " + mappingFile +
                                " at row " + std::to_string(row) +
                                " (got " + std::to_string(values.size()) +
                                ", expected " + std::to_string(NCOLUMN) + ")");
    }

    // File is row-major: each line is one row, values[col]
    for (unsigned int col = 0; col < NCOLUMN; ++col) {
      mMapping[col][row] = values[col];
    }

    ++row;
  }

  if (row != NROW) {
    throw MappingFileException("HCALMapper: not enough rows in file " + mappingFile +
                              " (got " + std::to_string(row) +
                              ", expected " + std::to_string(NROW) + ")");
  }

  buildInverseMapping();
  mLoaded = true;
}

void HCALMapper::buildInverseMapping()
{
  mInverseMapping.clear();
  mInverseMapping.reserve(NCHANNELS);

  for (unsigned int col = 0; col < NCOLUMN; ++col) {
    for (unsigned int row = 0; row < NROW; ++row) {
      const unsigned int ch = mMapping[col][row];

      // Ensure uniqueness (inverse mapping must be 1-to-1)
      auto [it, inserted] = mInverseMapping.emplace(ch, std::make_tuple(col, row));
      if (!inserted) {
        throw MappingFileException("HCALMapper: duplicate channelID " + std::to_string(ch) +
                                  " appears more than once in mapping");
      }
    }
  }
}

unsigned int HCALMapper::getChannelID(unsigned int col, unsigned int row) const
{
  if (!mLoaded) {
    throw MappingFileException("HCALMapper: mapping not loaded (call loadMapping())");
  }
  if (col >= NCOLUMN || row >= NROW) {
    throw PositionException(col, row);
  }
  return mMapping[col][row];
}

std::tuple<unsigned int, unsigned int> HCALMapper::getRowColFromChannelID(unsigned int channelID) const
{
  if (!mLoaded) {
    throw MappingFileException("HCALMapper: mapping not loaded (call loadMapping())");
  }

  auto it = mInverseMapping.find(channelID);
  if (it == mInverseMapping.end()) {
    throw ChannelIDException(channelID);
  }
  return it->second;
}

unsigned int HCALMapper::getRow(unsigned int channelID) const
{
  return std::get<1>(getRowColFromChannelID(channelID));
}

unsigned int HCALMapper::getColumn(unsigned int channelID) const
{
  return std::get<0>(getRowColFromChannelID(channelID));
}

void HCALMapper::loadPCBMapping(const std::string& mappingFile)
{
  std::ifstream in(mappingFile);
  if (!in.is_open()) {
    throw PCBMappingException("HCALMapper: cannot open PCB mapping file: " + mappingFile);
  }

  mPCBWiring.clear();
  std::string line;
  int lineNum = 0;

  while (std::getline(in, line)) {
    ++lineNum;
    // strip // and # comments
    for (const auto& marker : {std::string("//"), std::string("#")}) {
      const auto pos = line.find(marker);
      if (pos != std::string::npos) {
        line = line.substr(0, pos);
      }
    }
    line = trim(line);
    if (line.empty()) {
      continue;
    }

    std::vector<unsigned int> values;
    std::stringstream ss(line);
    std::string token;
    while (std::getline(ss, token, ',')) {
      token = trim(token);
      if (token.empty()) {
        continue;
      }
      try {
        values.push_back(static_cast<unsigned int>(std::stoul(token)));
      } catch (...) {
        throw PCBMappingException("HCALMapper: invalid token '" + token +
                                  "' in PCB mapping file " + mappingFile +
                                  " at line " + std::to_string(lineNum));
      }
    }

    if (values.size() != 3) {
      throw PCBMappingException("HCALMapper: expected 3 values (pcbIndex, asicIndex, halfIndex) in " +
                                mappingFile + " at line " + std::to_string(lineNum) +
                                " (got " + std::to_string(values.size()) + ")");
    }

    auto [it, inserted] = mPCBWiring.emplace(values[0], PCBLocation{values[1], values[2]});
    if (!inserted) {
      throw PCBMappingException("HCALMapper: duplicate PCB index " + std::to_string(values[0]) +
                                " in file " + mappingFile);
    }
  }

  mPCBMappingLoaded = !mPCBWiring.empty();
}

void HCALMapper::setPCBWiring(unsigned int pcbIndex, unsigned int asicIndex, unsigned int halfIndex)
{
  mPCBWiring[pcbIndex] = PCBLocation{asicIndex, halfIndex};
  mPCBMappingLoaded = true;
}

HCALMapper::PCBLocation HCALMapper::getPCBLocation(unsigned int pcbIndex) const
{
  if (!mPCBMappingLoaded) {
    throw PCBMappingException("HCALMapper: PCB mapping not loaded (call loadPCBMapping() or setPCBWiring())");
  }
  const auto it = mPCBWiring.find(pcbIndex);
  if (it == mPCBWiring.end()) {
    throw PCBMappingException("HCALMapper: unknown PCB index " + std::to_string(pcbIndex));
  }
  return it->second;
}
